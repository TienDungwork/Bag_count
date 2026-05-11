#include "mqtt2.h"

//----------------------------------------MQTT2 Setup và Functions (Server anh Dũng)
static String mqtt2JsonString(JsonObject obj, const char* key);

static String mqtt2Topic(const char* action) {
  return "devices/" + mqtt2_password + "/" + String(action);
}

static String mqtt2ClientId() {
  uint64_t chipId = ESP.getEfuseMac();
  char id[24];
  snprintf(id, sizeof(id), "ESP32%012llX", (unsigned long long)chipId);
  return String(id);
}

static bool connectMQTT2WithClientId(const String& clientId, const String& statusTopic,
                                     const char* offlinePayload) {
  String displayClientId = clientId.length() > 0 ? clientId : "(empty - broker assigned)";
  Serial.println("Trying MQTT2 ClientId: " + displayClientId);

  bool connected = mqtt2.connect(clientId.c_str(), mqtt2_username.c_str(), mqtt2_password.c_str(),
                                 statusTopic.c_str(), 0, true, offlinePayload);
  if (!connected) {
    Serial.println("MQTT2 connect failed with ClientId '" + displayClientId + "', rc=" + String(mqtt2.state()));
    mqtt2.disconnect();
    ethClient2.stop();
    delay(100);
  }

  return connected;
}

static bool publishMQTT2ConnectionStatus(bool online) {
  if (mqtt2_password.length() == 0 || !mqtt2.connected()) {
    return false;
  }

  String topic = mqtt2Topic("status");
  String payload = online ? "{\"status\":\"online\"}" : "{\"status\":\"offline\"}";
  bool published = mqtt2.publish(topic.c_str(), payload.c_str(), true);

  if (published) {
    Serial.println("MQTT2 status published:");
    Serial.println("Topic: " + topic);
    Serial.println("Data: " + payload);
  } else {
    Serial.println("MQTT2 status publish failed, rc=" + String(mqtt2.state()));
    Serial.println("Topic: " + topic);
  }

  return published;
}

static String mqtt2ProductGroupForHistory(JsonObject entry) {
  String group = mqtt2JsonString(entry, "productGroup");
  if (group.length() > 0) {
    return group;
  }

  String entryProductCode = mqtt2JsonString(entry, "productCode");
  String entryProductName = mqtt2JsonString(entry, "productName");

  for (size_t i = 0; i < productsData.size(); i++) {
    JsonObject product = productsData[i];
    String catalogProductCode = mqtt2JsonString(product, "code");
    String catalogProductName = mqtt2JsonString(product, "name");
    bool codeMatch = entryProductCode.length() > 0 && catalogProductCode == entryProductCode;
    bool nameMatch = entryProductName.length() > 0 && catalogProductName == entryProductName;

    if (codeMatch || nameMatch) {
      return mqtt2JsonString(product, "group");
    }
  }

  return "";
}

static int mqtt2SetModeForHistory(JsonObject entry) {
  if (entry.containsKey("setModeNumber")) {
    return entry["setModeNumber"].as<int>();
  }

  String mode = mqtt2JsonString(entry, "setMode");
  if (mode == "input") {
    return 1;
  }
  if (mode == "output") {
    return 2;
  }

  return currentMode == "input" ? 1 : 2;
}

static bool ensureMQTT2Connected() {
  if (currentNetworkMode == WIFI_AP_MODE || mqtt2_password.length() == 0) {
    return false;
  }

  if (!mqtt2.connected()) {
    setupMQTT2();
  }

  return mqtt2.connected();
}

static bool publishMQTT2HistoryEntry(JsonObject entry) {
  if (!ensureMQTT2Connected()) {
    Serial.println("MQTT2 sync: broker not connected, will retry later");
    return false;
  }

  String topic = mqtt2Topic("Transaction");

  String payloadName = mqtt2JsonString(entry, "batchName");
  if (payloadName.length() == 0) {
    payloadName = mqtt2JsonString(entry, "orderCode");
  }
  if (payloadName.length() == 0) {
    payloadName = mqtt2JsonString(entry, "productName");
  }

  String entryLocation = mqtt2JsonString(entry, "location");
  if (entryLocation.length() == 0) {
    entryLocation = location;
  }

  DynamicJsonDocument doc(1024);
  doc["Name"] = payloadName;
  doc["OrderCode"] = mqtt2JsonString(entry, "orderCode");
  doc["ProductName"] = mqtt2JsonString(entry, "productName");
  doc["ProductGroup"] = mqtt2ProductGroupForHistory(entry);
  doc["ProductCode"] = mqtt2JsonString(entry, "productCode");
  doc["CustomerName"] = mqtt2JsonString(entry, "customerName");
  doc["CustomerPhone"] = mqtt2JsonString(entry, "customerPhone");
  doc["StartTime"] = mqtt2JsonString(entry, "startTime");
  doc["SetMode"] = mqtt2SetModeForHistory(entry);
  doc["Location"] = entryLocation;
  doc["PlannedCount"] = entry["plannedQuantity"] | entry["planned"] | entry["target"] | 0;
  doc["ActualCount"] = entry["actualCount"] | entry["actual"] | entry["count"] | 0;

  String message;
  serializeJson(doc, message);

  bool published = mqtt2.publish(topic.c_str(), message.c_str());
  if (published) {
    Serial.println("MQTT2 sync published history entry:");
    Serial.println("Topic: " + topic);
    Serial.println("Data: " + message);
  } else {
    Serial.println("MQTT2 sync publish failed, rc=" + String(mqtt2.state()) +
                   ", payloadLen=" + String(message.length()) +
                   ", bufferSize=" + String(mqtt2.getBufferSize()));
    Serial.println("Topic: " + topic);
    Serial.println("Data: " + message);
  }

  return published;
}

void processMQTT2SyncQueue() {
  static unsigned long lastSyncAttempt = 0;
  const unsigned long syncIntervalMs = 5000;

  if (millis() - lastSyncAttempt < syncIntervalMs) {
    return;
  }
  lastSyncAttempt = millis();

  if (currentNetworkMode == WIFI_AP_MODE || mqtt2_password.length() == 0) {
    return;
  }

  if (!LittleFS.exists("/history.json")) {
    return;
  }

  if (!ensureMQTT2Connected()) {
    return;
  }

  File file = LittleFS.open("/history.json", "r");
  if (!file) {
    Serial.println("MQTT2 sync: cannot open /history.json for reading");
    return;
  }

  String content = file.readString();
  file.close();

  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, content);
  if (err || !doc.is<JsonArray>()) {
    Serial.println("MQTT2 sync: invalid /history.json, cannot parse queue");
    return;
  }

  JsonArray historyArray = doc.as<JsonArray>();
  for (JsonObject entry : historyArray) {
    bool isSynced = entry["IsSyncServer"] | false;
    if (isSynced) {
      continue;
    }

    String orderCodeToSync = mqtt2JsonString(entry, "orderCode");
    Serial.println("MQTT2 sync: found unsynced history entry, orderCode=" + orderCodeToSync);

    if (!publishMQTT2HistoryEntry(entry)) {
      return;
    }

    entry["IsSyncServer"] = true;
    entry["syncServerAt"] = getTimeStr();

    File out = LittleFS.open("/history.json", "w");
    if (!out) {
      Serial.println("MQTT2 sync: publish succeeded but cannot mark history as synced");
      return;
    }

    size_t written = serializeJson(doc, out);
    out.close();
    Serial.println("MQTT2 sync: marked one history entry as synced, bytes written: " + String(written));
    return;
  }
}

//----------------------------------------MQTT2 Setup và Functions (Server anh Dũng)

void setupMQTT2() {
  // KIỂM TRA NETWORK MODE - Chỉ hoạt động khi có Internet
  if (currentNetworkMode == WIFI_AP_MODE) {
    Serial.println("MQTT2: Cannot setup in AP mode");
    return;
  }

  mqtt_server2.trim();
  mqtt2_username.trim();
  mqtt2_password.trim();
  
  // Kiểm tra KeyLogin có được cấu hình chưa
  if (mqtt2_password.length() == 0) {
    Serial.println("MQTT2: KeyLogin not configured, skipping connection");
    return;
  }
  
  Serial.println("Setting up MQTT Client 2 (Server anh Dũng)...");
  
  mqtt2.setServer(mqtt_server2.c_str(), mqtt_port2);
  mqtt2.setCallback(onMqttMessage2);
  mqtt2.setBufferSize(2048);
  mqtt2.setKeepAlive(60);
  
  String keyLoginClientId = mqtt2_password;
  keyLoginClientId.trim();
  String chipClientId = mqtt2ClientId();
  
  Serial.print("Connecting to MQTT broker 2: ");
  Serial.println(mqtt_server2 + ":" + String(mqtt_port2));
  Serial.println("Username: " + mqtt2_username);
  Serial.println("KeyLogin: " + mqtt2_password);
  
  String statusTopic = mqtt2Topic("status");
  const char* offlinePayload = "{\"status\":\"offline\"}";

  bool connected = connectMQTT2WithClientId(keyLoginClientId, statusTopic, offlinePayload);

  if (!connected && mqtt2.state() == 2 && chipClientId != keyLoginClientId) {
    connected = connectMQTT2WithClientId(chipClientId, statusTopic, offlinePayload);
  }

  if (!connected && mqtt2.state() == 2) {
    connected = connectMQTT2WithClientId("", statusTopic, offlinePayload);
  }

  if (connected) {
    Serial.println("MQTT Client 2 connected successfully!");
    Serial.println("Kết nối broker " + mqtt_server2 + ":" + String(mqtt_port2) + " thành công!");

    publishMQTT2ConnectionStatus(true);
  } else {
    Serial.println("MQTT Client 2 connection failed, rc=" + String(mqtt2.state()));
    
    switch(mqtt2.state()) {
      case -4: Serial.println("  Error: Connection timeout"); break;
      case -3: Serial.println("  Error: Connection lost"); break;
      case -2: Serial.println("  Error: Connect failed"); break;
      case 1: Serial.println("  Error: Wrong protocol version"); break;
      case 2: Serial.println("  Error: Client ID rejected"); break;
      case 3: Serial.println("  Error: Server unavailable"); break;
      case 4: Serial.println("  Error: Bad username/password"); break;
      case 5: Serial.println("  Error: Not authorized"); break;
      default: Serial.println("  Error: Unknown"); break;
    }
  }
}

// Callback cho MQTT Client 2 (Server anh Dũng)
void onMqttMessage2(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  String topicStr = String(topic);
  Serial.println("MQTT2 Message received:");
  Serial.println("  Topic: " + topicStr);
  Serial.println("  Message: " + message);
}

static String mqtt2JsonString(JsonObject obj, const char* key) {
  if (obj.isNull() || !obj.containsKey(key)) {
    return "";
  }
  return obj[key].as<String>();
}

static String mqtt2OrderProductGroup(JsonObject order) {
  String group = mqtt2JsonString(order, "productGroup");
  if (group.length() > 0) {
    return group;
  }

  if (order.containsKey("product") && order["product"].is<JsonObject>()) {
    JsonObject product = order["product"];
    group = mqtt2JsonString(product, "group");
    if (group.length() > 0) {
      return group;
    }
  }

  return "";
}

static void mqtt2ApplyOrderProduct(JsonObject order, String& resolvedProductName,
                                   String& resolvedProductCode, String& resolvedProductGroup) {
  String orderProductName = mqtt2JsonString(order, "productName");
  String orderProductCode = orderProductCodeFromJson(order);
  String orderProductGroup = mqtt2OrderProductGroup(order);

  if (resolvedProductName.length() == 0 && orderProductName.length() > 0) {
    resolvedProductName = orderProductName;
  }
  if (resolvedProductCode.length() == 0 && orderProductCode.length() > 0) {
    resolvedProductCode = orderProductCode;
  }
  if (resolvedProductGroup.length() == 0 && orderProductGroup.length() > 0) {
    resolvedProductGroup = orderProductGroup;
  }
}

static void mqtt2ResolveProductInfo(String& resolvedProductName, String& resolvedProductCode,
                                    String& resolvedProductGroup) {
  resolvedProductName = bagType;
  resolvedProductCode = productCode;
  resolvedProductGroup = "";

  int bestScore = 0;
  JsonObject bestOrder;

  for (size_t i = 0; i < ordersData.size(); i++) {
    JsonArray orders = ordersData[i]["orders"];
    for (JsonObject order : orders) {
      String orderProductName = mqtt2JsonString(order, "productName");
      String orderProductCode = orderProductCodeFromJson(order);
      String orderOrderCode = mqtt2JsonString(order, "orderCode");
      String status = mqtt2JsonString(order, "status");
      bool selected = order["selected"] | false;

      int score = 0;
      if (orderCode.length() > 0 && orderOrderCode == orderCode) score += 8;
      if (resolvedProductCode.length() > 0 && orderProductCode == resolvedProductCode) score += 4;
      if (resolvedProductName.length() > 0 && orderProductName == resolvedProductName) score += 2;
      if (selected) score += 1;
      if (status == "counting") score += 2;
      if (status == "waiting" || status == "paused") score += 1;

      if (score > bestScore) {
        bestScore = score;
        bestOrder = order;
      }
    }
  }

  if (!bestOrder.isNull()) {
    mqtt2ApplyOrderProduct(bestOrder, resolvedProductName, resolvedProductCode, resolvedProductGroup);
  }

  for (size_t i = 0; i < productsData.size(); i++) {
    JsonObject product = productsData[i];
    String catalogProductName = mqtt2JsonString(product, "name");
    String catalogProductCode = mqtt2JsonString(product, "code");
    bool codeMatch = resolvedProductCode.length() > 0 && catalogProductCode == resolvedProductCode;
    bool nameMatch = resolvedProductName.length() > 0 && catalogProductName == resolvedProductName;

    if (codeMatch || nameMatch) {
      if (resolvedProductName.length() == 0) {
        resolvedProductName = catalogProductName;
      }
      if (resolvedProductCode.length() == 0) {
        resolvedProductCode = catalogProductCode;
      }
      if (resolvedProductGroup.length() == 0) {
        resolvedProductGroup = mqtt2JsonString(product, "group");
      }
      break;
    }
  }
}

// PAYLOAD gửi lên MQTT2
void publishMQTT2OrderComplete() {
  // Kiểm tra điều kiện kết nối
  if (currentNetworkMode == WIFI_AP_MODE) {
    Serial.println("MQTT2: Không thể gửi - đang ở AP mode");
    return;
  }

  if (mqtt2_password.length() == 0) {
    Serial.println("MQTT2: Không thể gửi - chưa có KeyLogin");
    return;
  }

  if (!mqtt2.connected()) {
    Serial.println("MQTT2: Chưa kết nối, thử reconnect trước khi gửi...");
    setupMQTT2();
  }

  if (!mqtt2.connected()) {
    Serial.println("MQTT2: Không thể gửi - reconnect thất bại, rc=" + String(mqtt2.state()));
    return;
  }
  
  // Tạo topic theo format: devices/{KeyLogin}/Transaction
  String mqtt2_topic_transaction = mqtt2Topic("Transaction");
  
  DynamicJsonDocument doc(1024);
  
  // Payload theo sample/doc.md: devices/{KeyLogin}/Transaction
  String resolvedProductName;
  String resolvedProductCode;
  String resolvedProductGroup;
  mqtt2ResolveProductInfo(resolvedProductName, resolvedProductCode, resolvedProductGroup);

  String customerDisplayName = currentBatchName;
  if (customerDisplayName.length() == 0) {
    customerDisplayName = orderCode;
  }

  doc["Name"] = customerDisplayName;
  doc["OrderCode"] = orderCode;
  doc["ProductName"] = resolvedProductName;
  doc["ProductGroup"] = resolvedProductGroup;
  doc["ProductCode"] = resolvedProductCode;
  doc["CustomerName"] = customerDisplayName;
  doc["CustomerPhone"] = customerName;
  doc["StartTime"] = startTimeIsoStr;
  doc["SetMode"] = currentMode == "input" ? 1 : 2;
  doc["Location"] = location;
  doc["PlannedCount"] = (int)targetCount;
  doc["ActualCount"] = (long)totalCount;

  Serial.println("MQTT2 resolved product:");
  Serial.println("  ProductName: " + resolvedProductName);
  Serial.println("  ProductGroup: " + resolvedProductGroup);
  Serial.println("  ProductCode: " + resolvedProductCode);
  
  // Serialize JSON
  String message;
  serializeJson(doc, message);
  
  // PubSubClient publish() dùng tham số thứ 3 là retained, không phải QoS.
  bool published = mqtt2.publish(mqtt2_topic_transaction.c_str(), message.c_str());
  
  if (published) {
    Serial.println("MQTT2 Order Complete published successfully!");
    Serial.println("Topic: " + mqtt2_topic_transaction);
    Serial.println("Data: " + message);
  } else {
    Serial.println("MQTT2 Order Complete publish failed, rc=" + String(mqtt2.state()) +
                   ", payloadLen=" + String(message.length()) +
                   ", bufferSize=" + String(mqtt2.getBufferSize()));
    Serial.println("Topic: " + mqtt2_topic_transaction);
    Serial.println("Data: " + message);
  }
}

void setupTime() {
  Serial.println("Configuring NTP time...");
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // máy chủ NTP
  
  // Đợi đồng bộ thời gian
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  int timeout = 30; // 30 giây timeout
  while (now < 24 * 3600 && timeout > 0) {
    delay(1000);
    Serial.print(".");
    now = time(nullptr);
    timeout--;
  }
  Serial.println();
  
  if (now > 24 * 3600) {
    Serial.println("NTP successfully!");
    Serial.print("Current time: ");
    Serial.println(getTimeStr());
  } else {
    Serial.println("Failed NTP time");
  }
}

String getTimeStr() {
  time_t now = time(nullptr);
  
  // Kiểm tra xem thời gian đã được đồng bộ chưa
  if (now < 24 * 3600) {
    return "Syncing...";
  }
  
  struct tm* t = localtime(&now);
  char buf[32];
  strftime(buf, sizeof(buf), "%H:%M - %d/%m/%Y", t);
  return String(buf);
}

String getIsoTimeStr() {
  time_t now = time(nullptr);
  if (now < 24 * 3600) {
    return "";
  }

  struct tm* t = gmtime(&now);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", t);
  return String(buf);
}

void broadcastRealtimeMessage(const char* topic, const String& payloadJson) {
  if (realtimeSocket.count() == 0) {
    return;
  }

  String message = "{\"topic\":\"" + String(topic) + "\",\"data\":" + payloadJson + "}";
  realtimeSocket.textAll(message);
}

void sendRealtimeSnapshot(AsyncWebSocketClient *client) {
  if (client == nullptr || !client->canSend()) {
    return;
  }

  publishStatusMQTT();
  publishCountUpdate();
  publishSensorData();
  publishHeartbeat();
}

void onRealtimeSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  (void)server;

  if (type == WS_EVT_CONNECT) {
    Serial.println("Realtime WebSocket client connected: " + String(client->id()));
    sendRealtimeSnapshot(client);
    return;
  }

  if (type == WS_EVT_DISCONNECT) {
    Serial.println("Realtime WebSocket client disconnected: " + String(client->id()));
    return;
  }

  if (type == WS_EVT_ERROR) {
    Serial.println("Realtime WebSocket error from client: " + String(client->id()));
    return;
  }

  if (type != WS_EVT_DATA) {
    return;
  }

  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info == nullptr || info->opcode != WS_TEXT || !info->final || info->index != 0 || info->len != len) {
    Serial.println("Realtime WebSocket fragmented or non-text frame ignored");
    return;
  }

  String frame;
  frame.reserve(len);
  for (size_t i = 0; i < len; i++) {
    frame += (char)data[i];
  }

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, frame);
  if (error) {
    Serial.println("Realtime WebSocket JSON parse error: " + String(error.c_str()));
    return;
  }

  String topicStr = doc["topic"] | "";
  if (topicStr.length() == 0) {
    Serial.println("Realtime WebSocket message missing topic");
    return;
  }

  String payload = "{}";
  if (!doc["data"].isNull()) {
    payload = "";
    serializeJson(doc["data"], payload);
  }

  handleRealtimeMessage(topicStr, payload);
}

void setupRealtimeServer() {
  Serial.println("Starting realtime WebSocket server on port " + String(REALTIME_WS_PORT));
  realtimeSocket.onEvent(onRealtimeSocketEvent);
  realtimeServer.addHandler(&realtimeSocket);
  realtimeServer.begin();
}
