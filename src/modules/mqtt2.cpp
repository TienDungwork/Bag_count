#include "mqtt2.h"

//----------------------------------------MQTT2 Setup và Functions (Server anh Dũng)
void setupMQTT2() {
  // KIỂM TRA NETWORK MODE - Chỉ hoạt động khi có Internet
  if (currentNetworkMode == WIFI_AP_MODE) {
    Serial.println("MQTT2: Cannot setup in AP mode");
    return;
  }
  
  // Kiểm tra KeyLogin có được cấu hình chưa
  if (mqtt2_password.length() == 0) {
    Serial.println("MQTT2: KeyLogin not configured, skipping connection");
    return;
  }
  
  Serial.println("Setting up MQTT Client 2 (Server anh Dũng)...");
  
  mqtt2.setServer(mqtt_server2.c_str(), mqtt_port2);
  mqtt2.setCallback(onMqttMessage2);
  // mqtt2.setBufferSize(512);  // Không có trong bản PubSubClient cũ
  // mqtt2.setKeepAlive(60);    // Khuyến nghị 60 giây
  
  // Tạo client ID random như khuyến nghị
  String clientId2 = "ESP32Device_" + String(random(100000, 999999));
  
  Serial.print("Connecting to MQTT broker 2: ");
  Serial.println(mqtt_server2 + ":" + String(mqtt_port2));
  Serial.println("Username: " + mqtt2_username);
  Serial.println("KeyLogin: " + mqtt2_password);
  
  // Kết nối với username/password (đơn giản, không cần LWT)
  if (mqtt2.connect(clientId2.c_str(), mqtt2_username.c_str(), mqtt2_password.c_str())) {
    Serial.println("MQTT Client 2 connected successfully!");
    Serial.println("Kết nối broker " + mqtt_server2 + ":" + String(mqtt_port2) + " thành công!");
    
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

// PAYLOAD gửi lên MQTT2
void publishMQTT2OrderComplete() {
  // Kiểm tra điều kiện kết nối
  if (currentNetworkMode == WIFI_AP_MODE || !mqtt2.connected() || mqtt2_password.length() == 0) {
    Serial.println("MQTT2: Không thể gửi - chưa kết nối hoặc chưa có KeyLogin");
    return;
  }
  
  // Tạo topic theo format: devices/{KeyLogin}/Transaction
  String mqtt2_topic_transaction = "devices/" + mqtt2_password + "/Transaction";
  
  DynamicJsonDocument doc(768);
  
  // 1. orderCode - Mã đơn hàng hiện tại
  doc["orderCode"] = orderCode;
  
  // 2. productGroup - Nhóm sản phẩm (từ sản phẩm hiện tại)
  String currentProductGroup = "";
  if (productCode.length() > 0) {
    // Tìm productGroup từ productsData
    JsonArray products = productsData.as<JsonArray>();
    for (JsonObject product : products) {
      if (product["code"] == productCode) {
        currentProductGroup = product["group"] | "";
        break;
      }
    }
  }
  doc["productGroup"] = currentProductGroup;
  
  // 3. productCode - Mã sản phẩm hiện tại
  doc["productCode"] = productCode;
  
  // 4. batchName - Tên khách hàng
  String currentBatchName = "";
  if (orderCode.length() > 0) {
    currentBatchName = orderCode;
  } else if (customerName.length() > 0) {
    currentBatchName = customerName;
  }
  doc["customerName"] = currentBatchName;
  
  // 5. customerName - SDT hiện tại
  doc["customerPhone"] = customerName;
  
  // 6. startTime - Thời gian bắt đầu đếm
  doc["startTime"] = startTimeStr;
  
  // 7. setMode - Chế độ hiển thị hiện tại (input/output)
  doc["setMode"] = currentMode;
  
  // 8. location - Địa điểm đặt băng tải
  doc["location"] = location;

  // 9. target - Mục tiêu đếm hiện tại
  doc["target"] = (int)targetCount;

  // 10. count - Số lượng đã đếm được
  doc["count"] = (long)totalCount;

  // 11. băng tải
  doc["conveyor"] = conveyorName;

  // 12. thời gian sensor đo được cho bao/nhóm bao gần nhất
  doc["sensorTimeMs"] = (long)lastMeasuredTime;
  
  // Serialize JSON
  String message;
  serializeJson(doc, message);
  
  // Publish với QoS 1
  bool published = mqtt2.publish(mqtt2_topic_transaction.c_str(), message.c_str(), 1);
  
  if (published) {
    Serial.println("MQTT2 Order Complete published successfully!");
    Serial.println("Topic: " + mqtt2_topic_transaction);
    Serial.println("Data: " + message);
  } else {
    Serial.println("MQTT2 Order Complete publish failed!");
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

