#include "storage.h"

//------------------- Lưu/đọc loại bao -------------------
void saveBagTypesToFile() {
  File f = LittleFS.open(BAGTYPES_FILE, "w");
  if (!f) return;
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  for (auto& type : bagTypes) arr.add(type);
  serializeJson(doc, f);
  f.close();
}
void loadBagTypesFromFile() {
  File f = LittleFS.open(BAGTYPES_FILE, "r");
  if (!f) return;
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, f);
  if (!err) {
    bagTypes.clear();
    for (JsonVariant v : doc.as<JsonArray>()) bagTypes.push_back(v.as<String>());
  }
  f.close();
}

//------------------- Lưu/đọc thông tin batch -------------------
#define BATCH_FILE "/batch_info.json"

void saveBatchInfoToFile() {
  File f = LittleFS.open(BATCH_FILE, "w");
  if (!f) return;
  DynamicJsonDocument doc(2048);
  doc["batchName"] = currentBatchName;
  doc["batchId"] = currentBatchId;
  doc["batchDescription"] = currentBatchDescription;
  doc["totalOrders"] = totalOrdersInBatch;
  doc["batchTotalTarget"] = batchTotalTarget;
  serializeJson(doc, f);
  f.close();
  Serial.println("Batch info saved to file: " + currentBatchName);
}

void loadBatchInfoFromFile() {
  File f = LittleFS.open(BATCH_FILE, "r");
  if (!f) return;
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, f);
  if (!err) {
    currentBatchName = doc["batchName"].as<String>();
    currentBatchId = doc["batchId"].as<String>();
    currentBatchDescription = doc["batchDescription"].as<String>();
    totalOrdersInBatch = doc["totalOrders"].as<int>();
    batchTotalTarget = doc["batchTotalTarget"].as<int>();
    Serial.println("Batch info loaded from file: " + currentBatchName);
  }
  f.close();
}

//------------------- Lưu/đọc cấu hình từng loại -------------------
void saveBagConfigsToFile() {
  File f = LittleFS.open(BAGCONFIGS_FILE, "w");
  if (!f) return;
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  for (auto& c : bagConfigs) {
    JsonObject o = arr.createNestedObject();
    o["type"] = c.type;
    o["target"] = c.target;
    o["warn"] = c.warn;
    o["status"] = c.status;
  }
  serializeJson(doc, f);
  f.close();
}
void loadBagConfigsFromFile() {
  File f = LittleFS.open(BAGCONFIGS_FILE, "r");
  if (!f) return;
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, f);
  if (!err) {
    bagConfigs.clear();
    for (JsonObject o : doc.as<JsonArray>()) {
      BagConfig c;
      c.type = o["type"].as<String>();
      c.target = o["target"] | 20;
      c.warn = o["warn"] | 10;
      c.status = o["status"] | "WAIT";
      bagConfigs.push_back(c);
    }
  }
  f.close();
}

//------------------- Lưu/đọc cài đặt chung -------------------
void copyJsonObject(JsonObject src, JsonObject dst) {
  for (JsonPair p : src) {
    if (p.value().is<JsonObject>()) {
      JsonObject nested = dst.createNestedObject(p.key());
      copyJsonObject(p.value().as<JsonObject>(), nested);
    } else if (p.value().is<JsonArray>()) {
      JsonArray srcArray = p.value().as<JsonArray>();
      JsonArray dstArray = dst.createNestedArray(p.key());
      for (JsonVariant v : srcArray) {
        if (v.is<JsonObject>()) {
          JsonObject nestedObj = dstArray.createNestedObject();
          copyJsonObject(v.as<JsonObject>(), nestedObj);
        } else {
          dstArray.add(v);
        }
      }
    } else {
      dst[p.key()] = p.value();
    }
  }
}

// Mã SP trên đơn: ưu tiên product.code, fallback productCode phẳng (đồng bộ web)
String orderProductCodeFromJson(JsonObject order) {
  if (order.containsKey("product") && order["product"].is<JsonObject>() &&
      order["product"].containsKey("code")) {
    return order["product"]["code"].as<String>();
  }
  if (order.containsKey("productCode")) {
    return order["productCode"].as<String>();
  }
  return "";
}

// Tìm unitWeight theo đơn hiện tại (ưu tiên ordersData, sau đó productsData)
float resolveUnitWeightFromData(const String& wantedOrderCode, const String& wantedProductCode, const String& wantedProductName) {
  // 1) Ưu tiên tra trong ordersData vì đơn hàng luôn có product.unitWeight
  for (size_t i = 0; i < ordersData.size(); i++) {
    if (!ordersData[i].containsKey("orders")) continue;
    JsonArray orders = ordersData[i]["orders"];
    for (size_t j = 0; j < orders.size(); j++) {
      JsonObject order = orders[j];
      String orderCodeCheck = order["orderCode"].as<String>();
      String productCodeCheck = orderProductCodeFromJson(order);
      String productNameCheck = order["productName"].as<String>();

      bool orderMatch = !wantedOrderCode.isEmpty() && orderCodeCheck == wantedOrderCode;
      bool codeMatch = !wantedProductCode.isEmpty() && productCodeCheck == wantedProductCode;
      bool nameMatch = !wantedProductName.isEmpty() && productNameCheck == wantedProductName;
      if (!(orderMatch && (codeMatch || nameMatch))) continue;

      if (order.containsKey("product") && order["product"].is<JsonObject>() && order["product"].containsKey("unitWeight")) {
        float w = order["product"]["unitWeight"].as<float>();
        if (w > 0.0f) return w;
      }
    }
  }

  // 2) Fallback productsData
  for (size_t i = 0; i < productsData.size(); i++) {
    JsonObject product = productsData[i];
    String nameCheck = product["name"].as<String>();
    String codeCheck = product["code"].as<String>();
    bool codeMatch = !wantedProductCode.isEmpty() && codeCheck == wantedProductCode;
    bool nameMatch = !wantedProductName.isEmpty() && nameCheck == wantedProductName;
    if ((codeMatch || nameMatch) && product.containsKey("unitWeight")) {
      float w = product["unitWeight"].as<float>();
      if (w > 0.0f) return w;
    }
  }
  return 0.0f;
}

void saveSettingsToFile() {
  Serial.println("Saving settings to file...");
  
  DynamicJsonDocument doc(2048);
  
  // Network settings
  doc["ipAddress"] = local_IP.toString();
  doc["gateway"] = gateway.toString();
  doc["subnet"] = subnet.toString();
  doc["dns1"] = primaryDNS.toString();
  doc["dns2"] = secondaryDNS.toString();
  
  // System settings
  doc["conveyorName"] = conveyorName;
  doc["location"] = location;
  doc["brightness"] = 100;
  doc["sensorDelay"] = sensorDelayMs;
  doc["bagDetectionDelay"] = bagDetectionDelay;
  doc["minBagInterval"] = minBagInterval;
  doc["autoReset"] = autoReset;
  doc["relayDelayAfterComplete"] = relayDelayAfterComplete;
  doc["bagTimeMultiplier"] = bagTimeMultiplier;
  doc["countSensorActiveLevel"] = countSensorActiveLevel;
  doc["inputSensorActiveLevel"] = inputSensorActiveLevel;
  doc["outputSensorActiveLevel"] = outputSensorActiveLevel;
  
  // MQTT2 settings (Server anh Dũng)
  doc["mqtt2Server"] = mqtt_server2;
  doc["mqtt2Port"] = mqtt_port2;
  doc["mqtt2Username"] = mqtt2_username;
  doc["mqtt2Password"] = mqtt2_password;
  
  // Weight-based Detection Delay settings
  doc["enableWeightBasedDelay"] = enableWeightBasedDelay;
  JsonArray weightRulesArray = doc.createNestedArray("weightDelayRules");
  for (const auto& rule : weightDelayRules) {
    JsonObject ruleObj = weightRulesArray.createNestedObject();
    ruleObj["weight"] = rule.weight;
    ruleObj["delay"] = rule.delay;
  }
  
  File file = LittleFS.open("/settings.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    Serial.println("Settings saved to /settings.json");
  } else {
    Serial.println("Failed to save settings");
  }
}

void loadSettingsFromFile() {
  Serial.println("Loading settings from file");
  
  // KIỂM TRA VÀ TẠO FILE MẶC ĐỊNH NẾU CHƯA CÓ
  if (!LittleFS.exists("/settings.json")) {
    Serial.println("No settings");
    createDefaultSettingsFile();
  }
  
  // LUÔN LOAD TỪ FILE
  File file = LittleFS.open("/settings.json", "r");
  if (file) {
    String content = file.readString();
    file.close();
    
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, content) == DeserializationError::Ok) {
      Serial.println("Found settings file, loading saved values:");
      
      // Load Ethernet IP config
  String ethIP = doc["ipAddress"].as<String>();
  String ethGateway = doc["gateway"].as<String>();
  String ethSubnet = doc["subnet"].as<String>();
  String ethDNS1 = doc["dns1"].as<String>();
  String ethDNS2 = doc["dns2"].as<String>();
      
      if (ethIP.length() > 0) {
        IPAddress newIP, newGateway, newSubnet, newDNS1, newDNS2;
        if (newIP.fromString(ethIP)) local_IP = newIP;
        if (newGateway.fromString(ethGateway)) gateway = newGateway;
        if (newSubnet.fromString(ethSubnet)) subnet = newSubnet;
        if (newDNS1.fromString(ethDNS1)) primaryDNS = newDNS1;
        if (newDNS2.fromString(ethDNS2)) secondaryDNS = newDNS2;
        
        Serial.println("   Network config loaded:");
        Serial.println("    IP: " + ethIP);
        Serial.println("    Gateway: " + ethGateway);
        Serial.println("    Subnet: " + ethSubnet);
      }
      
      // Load all settings from file
      conveyorName = doc["conveyorName"].as<String>();
      location = doc["location"].as<String>();
      displayBrightness = 100;
      sensorDelayMs = doc["sensorDelay"].as<int>();
      bagDetectionDelay = doc["bagDetectionDelay"].as<int>();
      minBagInterval = doc["minBagInterval"].as<int>();
      autoReset = doc["autoReset"].as<bool>();
      relayDelayAfterComplete = doc["relayDelayAfterComplete"].as<int>();
      bagTimeMultiplier = doc["bagTimeMultiplier"] | 25;  // Default 25% nếu không có
      countSensorActiveLevel = doc["countSensorActiveLevel"] | DEFAULT_SENSOR_DETECTED_LEVEL;
      inputSensorActiveLevel = doc["inputSensorActiveLevel"] | DEFAULT_SENSOR_DETECTED_LEVEL;
      outputSensorActiveLevel = doc["outputSensorActiveLevel"] | DEFAULT_SENSOR_DETECTED_LEVEL;
      
      // DEBUG: In giá trị minBagInterval được load
      Serial.println("🔧 LOADED minBagInterval từ settings: " + String(minBagInterval) + "ms");
      
      // Load MQTT2 settings (Server anh Dũng)
      if (doc.containsKey("mqtt2Server")) {
        mqtt_server2 = doc["mqtt2Server"].as<String>();
      }
      if (doc.containsKey("mqtt2Port")) {
        mqtt_port2 = doc["mqtt2Port"].as<int>();
      }
      if (doc.containsKey("mqtt2Username")) {
        mqtt2_username = doc["mqtt2Username"].as<String>();
      }
      if (doc.containsKey("mqtt2Password")) {
        mqtt2_password = doc["mqtt2Password"].as<String>();
      }
      
      // Load Weight-based Detection Delay settings
      if (doc.containsKey("enableWeightBasedDelay")) {
        enableWeightBasedDelay = doc["enableWeightBasedDelay"].as<bool>();
      }
      
      if (doc.containsKey("weightDelayRules") && doc["weightDelayRules"].is<JsonArray>()) {
        weightDelayRules.clear();
        JsonArray rulesArray = doc["weightDelayRules"];
        for (JsonObject ruleObj : rulesArray) {
          WeightDelayRule rule;
          rule.weight = ruleObj["weight"].as<float>();
          rule.delay = ruleObj["delay"].as<int>();
          weightDelayRules.push_back(rule);
        }
        
        // Sort rules by weight descending để tìm kiếm đúng thứ tự (nặng → nhẹ)
        std::sort(weightDelayRules.begin(), weightDelayRules.end(), 
                  [](const WeightDelayRule& a, const WeightDelayRule& b) {
                    return a.weight > b.weight;
                  });
        
        Serial.println("    Loaded " + String(weightDelayRules.size()) + " weight delay rules (sorted by weight)");
        
        // Log các rules để debug
        for (size_t i = 0; i < weightDelayRules.size(); i++) {
          Serial.println("      Rule " + String(i+1) + ": " + String(weightDelayRules[i].weight) + "kg → " + String(weightDelayRules[i].delay) + "ms");
        }
      }
      
      // Sync delay
      debounceDelay = sensorDelayMs;
      
      Serial.println("    Settings loaded from file:");
      Serial.println("    conveyorName: " + conveyorName);
      Serial.println("    location: " + location);
      Serial.println("    brightness: " + String(displayBrightness) + "%");
      Serial.println("    sensorDelay: " + String(sensorDelayMs) + "ms");
      Serial.println("    bagDetectionDelay: " + String(bagDetectionDelay) + "ms");
      Serial.println("    minBagInterval: " + String(minBagInterval) + "ms");
      Serial.println("    autoReset: " + String(autoReset ? "true" : "false"));
      Serial.println("    relayDelayAfterComplete: " + String(relayDelayAfterComplete) + "ms");
      Serial.println("    countSensorActiveLevel: " + String(sensorLevelName(countSensorActiveLevel)));
      Serial.println("    inputSensorActiveLevel: " + String(sensorLevelName(inputSensorActiveLevel)));
      Serial.println("    outputSensorActiveLevel: " + String(sensorLevelName(outputSensorActiveLevel)));
      Serial.println("All settings loaded from file successfully");
    } else {
      Serial.println("Failed to parse settings JSON - recreating file");
      createDefaultSettingsFile();
      loadSettingsFromFile();
    }
  } else {
    Serial.println("Failed to open settings file - recreating file");
    createDefaultSettingsFile();
    loadSettingsFromFile();
  }
}

// TẠO FILE CÀI ĐẶT MẶC ĐỊNH
void createDefaultSettingsFile() {
  Serial.println("Creating default settings file");
  
  DynamicJsonDocument doc(2048);
  
  // Network settings - default values
  doc["ipAddress"] = "192.168.1.198";
  doc["gateway"] = "192.168.1.1";
  doc["subnet"] = "255.255.255.0";
  doc["dns1"] = "8.8.8.8";
  doc["dns2"] = "8.8.4.4";
  
  // Application settings - default values
  doc["conveyorName"] = "BT-001";
  doc["location"] = "Khu vực 1";
  doc["brightness"] = 100;
  doc["sensorDelay"] = 0;
  doc["bagDetectionDelay"] = 200;
  doc["minBagInterval"] = 100;
  doc["autoReset"] = true;
  doc["relayDelayAfterComplete"] = 10000;
  doc["bagTimeMultiplier"] = 25;  // Default 25%
  doc["countSensorActiveLevel"] = DEFAULT_SENSOR_DETECTED_LEVEL;
  doc["inputSensorActiveLevel"] = DEFAULT_SENSOR_DETECTED_LEVEL;
  doc["outputSensorActiveLevel"] = DEFAULT_SENSOR_DETECTED_LEVEL;
  
  // Weight-based Detection Delay settings - default values
  doc["enableWeightBasedDelay"] = false;
  JsonArray defaultRules = doc.createNestedArray("weightDelayRules");
  JsonObject rule1 = defaultRules.createNestedObject();
  rule1["weight"] = 50.0;
  rule1["delay"] = 3000;
  JsonObject rule2 = defaultRules.createNestedObject();
  rule2["weight"] = 40.0;
  rule2["delay"] = 2500;
  JsonObject rule3 = defaultRules.createNestedObject();
  rule3["weight"] = 30.0;
  rule3["delay"] = 2000;
  JsonObject rule4 = defaultRules.createNestedObject();
  rule4["weight"] = 20.0;
  rule4["delay"] = 1500;
  
  // Add creation timestamp
  doc["_created"] = "ESP32_Default_Config";
  doc["_version"] = "1.0";
  
  File file = LittleFS.open("/settings.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    Serial.println("Default settings created successfully");
  } else {
    Serial.println("Failed create default settings file");
  }
}

// HÀM TẠO CÁC FILE MẶC ĐỊNH LẦN ĐẦU
void createDefaultDataFiles() {
  Serial.println("Creating default data files");
  
  // Create default products.json if not exists
  if (!LittleFS.exists("/products.json")) {
    Serial.println("Creating default products.json");
    File file = LittleFS.open("/products.json", "w");
    if (file) {
      file.println("[]"); // Empty array
      file.close();
      Serial.println("Default products.json created");
    }
  }
  
  // Create default orders.json if not exists
  if (!LittleFS.exists("/orders.json")) {
    Serial.println("Creating default orders.json");
    File file = LittleFS.open("/orders.json", "w");
    if (file) {
      file.println("[]"); // Empty array
      file.close();
      Serial.println("Default orders.json created");
    }
  }
  
  // Create default history.json if not exists
  if (!LittleFS.exists("/history.json")) {
    Serial.println("Creating default history.json");
    File file = LittleFS.open("/history.json", "w");
    if (file) {
      file.println("[]"); // Empty array
      file.close();
      Serial.println("Default history.json created");
    }
  }
  
  Serial.println("All default data files created");
}

void debugSettingsFile() {
  Serial.println("DEBUG: Checking settings file");
  
  if (LittleFS.exists("/settings.json")) {
    File file = LittleFS.open("/settings.json", "r");
    if (file) {
      String content = file.readString();
      file.close();
      
      Serial.println("Settings file content:");
      Serial.println(content);
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, content);
      
      if (error) {
        Serial.println("JSON Parse Error: " + String(error.c_str()));
      } else {
        Serial.println("JSON is valid");
        Serial.println("Keys found:");
        for (JsonPair p : doc.as<JsonObject>()) {
          Serial.println("  - " + String(p.key().c_str()) + ": " + p.value().as<String>());
        }
      }
    } else {
      Serial.println("Cannot open settings file");
    }
  } else {
    Serial.println("Settings file does not exist");
  }
  
  Serial.println("Current variables in memory:");
  Serial.println("  - conveyorName: " + conveyorName);
  Serial.println("  - location: " + location);
  Serial.println("  - brightness: " + String(displayBrightness));
  Serial.println("  - sensorDelay: " + String(sensorDelayMs));
  Serial.println("  - bagDetectionDelay: " + String(bagDetectionDelay));
  Serial.println("  - minBagInterval: " + String(minBagInterval));
  Serial.println("  - autoReset: " + String(autoReset));
  Serial.println("  - countSensorActiveLevel: " + String(sensorLevelName(countSensorActiveLevel)));
  Serial.println("  - inputSensorActiveLevel: " + String(sensorLevelName(inputSensorActiveLevel)));
  Serial.println("  - outputSensorActiveLevel: " + String(sensorLevelName(outputSensorActiveLevel)));
}

//----------------------------------------Data Storage Functions
void saveProductsToFile() {
  File file = LittleFS.open("/products.json", "w");
  if (file) {
    serializeJson(productsData, file);
    file.close();
    Serial.println("Products saved to /products.json");
  } else {
    Serial.println("Failed to save products.json");
  }
}

void loadProductsFromFile() {
  // ĐẢMBẢO FILE TỒN TẠI - TẠO NẾU CHƯA CÓ
  if (!LittleFS.exists("/products.json")) {
    Serial.println("products.json not found - creating empty products file");
    File file = LittleFS.open("/products.json", "w");
    if (file) {
      file.println("[]");
      file.close();
      Serial.println("Empty products.json created");
    }
  }
  
  // LUÔN LOAD TỪ FILE
  File file = LittleFS.open("/products.json", "r");
  if (file) {
    DeserializationError error = deserializeJson(productsData, file);
    file.close();
    
    if (error) {
      Serial.println("Failed to parse products.json: " + String(error.c_str()) + " - recreating file");
      File newFile = LittleFS.open("/products.json", "w");
      if (newFile) {
        newFile.println("[]");
        newFile.close();
      }
      productsData.clear();
      productsData.to<JsonArray>();
    } else {
      Serial.println("Products loaded from /products.json");
      Serial.println("   Found " + String(productsData.size()) + " products");
      
      // DEBUG: Log chi tiết từng product
      for (size_t i = 0; i < productsData.size(); i++) {
        JsonObject prod = productsData[i];
        String prodName = prod["name"].as<String>();
        String prodCode = prod["code"].as<String>();
        float prodWeight = prod.containsKey("unitWeight") ? prod["unitWeight"].as<float>() : 0.0;
        
        Serial.println("   Product " + String(i+1) + ": name='" + prodName + "', code='" + prodCode + "', unitWeight=" + String(prodWeight, 2) + "kg");
      }
    }
  } else {
    Serial.println("Failed to open products.json - creating empty array");
    productsData.clear();
    productsData.to<JsonArray>();
  }
}

static const char* ORDERS_FILE = "/orders.json";
static const char* ORDERS_TMP_FILE = "/orders.tmp";
static const char* ORDERS_BAK_FILE = "/orders.bak";
static const size_t MAX_STORED_ORDERS = 100;
static const size_t MAX_STORED_HISTORY_ENTRIES = 100;

void trimHistoryArrayToLimit(JsonArray historyArray) {
  while (historyArray.size() > MAX_STORED_HISTORY_ENTRIES) {
    historyArray.remove(0);
  }
}

static size_t countStoredOrders() {
  size_t total = 0;

  for (JsonVariant item : ordersData.as<JsonArray>()) {
    if (item.is<JsonObject>() &&
        item.as<JsonObject>().containsKey("orders") &&
        item.as<JsonObject>()["orders"].is<JsonArray>()) {
      total += item.as<JsonObject>()["orders"].as<JsonArray>().size();
    } else {
      total++;
    }
  }

  return total;
}

static void removeEmptyOrderBatches() {
  JsonArray batches = ordersData.as<JsonArray>();

  for (int i = (int)batches.size() - 1; i >= 0; i--) {
    JsonVariant item = batches[i];
    if (!item.is<JsonObject>() ||
        !item.as<JsonObject>().containsKey("orders") ||
        !item.as<JsonObject>()["orders"].is<JsonArray>()) {
      continue;
    }

    JsonArray orders = item.as<JsonObject>()["orders"].as<JsonArray>();
    if (orders.size() == 0) {
      String batchName = item.as<JsonObject>()["name"] | "";
      Serial.println("Removing empty order batch: " + batchName);
      batches.remove(i);
    }
  }
}

static void trimStoredOrdersToLimit() {
  size_t totalOrders = countStoredOrders();
  if (totalOrders <= MAX_STORED_ORDERS) {
    removeEmptyOrderBatches();
    return;
  }

  Serial.println("Orders exceed limit: " + String(totalOrders) +
                 ". Trimming to " + String(MAX_STORED_ORDERS) + " newest orders.");

  JsonArray batches = ordersData.as<JsonArray>();
  while (totalOrders > MAX_STORED_ORDERS && batches.size() > 0) {
    JsonVariant firstItem = batches[0];

    if (firstItem.is<JsonObject>() &&
        firstItem.as<JsonObject>().containsKey("orders") &&
        firstItem.as<JsonObject>()["orders"].is<JsonArray>()) {
      JsonArray orders = firstItem.as<JsonObject>()["orders"].as<JsonArray>();

      if (orders.size() == 0) {
        batches.remove(0);
        continue;
      }

      JsonObject oldestOrder = orders[0];
      String orderCode = oldestOrder["orderCode"] | "";
      String productName = oldestOrder["productName"] | "";
      Serial.println("Removing old order due to 100-order limit: " + orderCode + " - " + productName);
      orders.remove(0);
      totalOrders--;

      if (orders.size() == 0) {
        batches.remove(0);
      }
    } else {
      batches.remove(0);
      totalOrders--;
    }
  }

  removeEmptyOrderBatches();
}

static bool verifyJsonArrayFile(const char* path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Cannot open file for JSON verification: " + String(path));
    return false;
  }

  DynamicJsonDocument verifyDoc(65536);
  DeserializationError error = deserializeJson(verifyDoc, file);
  file.close();

  if (error) {
    Serial.println("JSON verification failed for " + String(path) + ": " + String(error.c_str()));
    return false;
  }
  if (!verifyDoc.is<JsonArray>()) {
    Serial.println("JSON verification failed for " + String(path) + ": root is not array");
    return false;
  }

  return true;
}

static bool loadOrdersFromPath(const char* path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open " + String(path));
    return false;
  }

  ordersData.clear();
  DeserializationError error = deserializeJson(ordersData, file);
  file.close();

  if (error || !ordersData.is<JsonArray>()) {
    Serial.println("Failed to parse " + String(path) + ": " + String(error.c_str()));
    ordersData.clear();
    ordersData.to<JsonArray>();
    return false;
  }

  Serial.println("Orders loaded from " + String(path));
  Serial.println("   Found " + String(ordersData.size()) + " orders");
  return true;
}

static bool replaceOrdersFileWithTmp() {
  if (!LittleFS.exists(ORDERS_TMP_FILE)) {
    Serial.println("Cannot replace orders file: temp file missing");
    return false;
  }

  if (LittleFS.exists(ORDERS_BAK_FILE)) {
    LittleFS.remove(ORDERS_BAK_FILE);
  }

  bool hadOrdersFile = LittleFS.exists(ORDERS_FILE);
  if (hadOrdersFile && !LittleFS.rename(ORDERS_FILE, ORDERS_BAK_FILE)) {
    Serial.println("Failed to move current orders.json to orders.bak");
    return false;
  }

  if (!LittleFS.rename(ORDERS_TMP_FILE, ORDERS_FILE)) {
    Serial.println("Failed to promote orders.tmp to orders.json");
    if (hadOrdersFile && LittleFS.exists(ORDERS_BAK_FILE)) {
      LittleFS.rename(ORDERS_BAK_FILE, ORDERS_FILE);
      Serial.println("Restored orders.json from orders.bak after failed promote");
    }
    return false;
  }

  return true;
}

void saveOrdersToFile() {
  Serial.println("ordersData size: " + String(ordersData.size()) + " items");
  
  // TỰ ĐỘNG XÓA CÁC ĐƠN ĐÃ HOÀN THÀNH TRƯỚC KHI LƯU
  bool hasRemovedOrders = false;
  Serial.println("Auto removing completed orders before saving...");
  
  // Duyệt qua tất cả các batch
  for (size_t i = 0; i < ordersData.size(); i++) {
    JsonArray orders = ordersData[i]["orders"];
    String batchId = ordersData[i]["id"].as<String>();
    
    // Duyệt ngược để tránh lỗi index khi xóa
    for (int j = orders.size() - 1; j >= 0; j--) {
      JsonObject order = orders[j];
      String status = order["status"].as<String>();
      
      if (status == "completed") {
        String productName = order["productName"].as<String>();
        int orderNumber = order["orderNumber"] | 0;
        int executeCount = order["executeCount"] | 0;
        
        Serial.println("Auto-removing completed order: " + productName + " (orderNumber=" + String(orderNumber) + ", executeCount=" + String(executeCount) + ")");
        
        orders.remove(j);
        hasRemovedOrders = true;
      }
    }
  }
  
  if (hasRemovedOrders) {
    Serial.println("Completed orders auto-removed before saving.");
  }

  trimStoredOrdersToLimit();
  
  if (LittleFS.exists(ORDERS_TMP_FILE)) {
    LittleFS.remove(ORDERS_TMP_FILE);
  }

  File file = LittleFS.open(ORDERS_TMP_FILE, "w");
  if (file) {
    size_t bytesWritten = serializeJson(ordersData, file);
    file.flush();
    file.close();

    Serial.println("Orders written to /orders.tmp");
    Serial.println("Temp file size: " + String(bytesWritten) + " bytes");

    if (bytesWritten == 0 || !verifyJsonArrayFile(ORDERS_TMP_FILE)) {
      Serial.println("Orders temp file invalid; keeping previous /orders.json");
      LittleFS.remove(ORDERS_TMP_FILE);
      return;
    }

    if (replaceOrdersFileWithTmp()) {
      Serial.println("Orders saved atomically to /orders.json");
      Serial.println("Previous valid file kept as /orders.bak");
    } else {
      Serial.println("Atomic orders save failed; previous /orders.json kept when possible");
      LittleFS.remove(ORDERS_TMP_FILE);
    }
  } else {
    Serial.println("Failed to open /orders.tmp for writing");
  }
}

void loadOrdersFromFile() {
  // Nếu mất điện sau khi ghi xong orders.tmp nhưng trước bước promote, tmp là bản mới nhất.
  if (LittleFS.exists(ORDERS_TMP_FILE)) {
    if (verifyJsonArrayFile(ORDERS_TMP_FILE)) {
      Serial.println("Found valid orders.tmp from interrupted save - promoting it");
      replaceOrdersFileWithTmp();
    } else {
      Serial.println("Removing invalid stale orders.tmp");
      LittleFS.remove(ORDERS_TMP_FILE);
    }
  }

  // ĐẢMBẢO FILE TỒN TẠI - TẠO NẾU CHƯA CÓ
  if (!LittleFS.exists(ORDERS_FILE)) {
    if (LittleFS.exists(ORDERS_BAK_FILE) && loadOrdersFromPath(ORDERS_BAK_FILE)) {
      LittleFS.rename(ORDERS_BAK_FILE, ORDERS_FILE);
      Serial.println("Restored missing orders.json from orders.bak");
      return;
    }

    Serial.println("orders.json not found - creating empty orders file");
    File file = LittleFS.open(ORDERS_FILE, "w");
    if (file) {
      file.println("[]");
      file.flush();
      file.close();
      Serial.println("Empty orders.json created");
    }
  }
  
  //  LUÔN LOAD TỪ FILE, fallback sang backup nếu file chính lỗi do mất điện
  if (loadOrdersFromPath(ORDERS_FILE)) {
    return;
  }

  Serial.println("orders.json invalid - trying orders.bak");
  if (LittleFS.exists(ORDERS_BAK_FILE) && loadOrdersFromPath(ORDERS_BAK_FILE)) {
    LittleFS.remove(ORDERS_FILE);
    if (LittleFS.rename(ORDERS_BAK_FILE, ORDERS_FILE)) {
      Serial.println("Restored orders.json from valid orders.bak");
    } else {
      Serial.println("Loaded orders.bak but could not promote it to orders.json");
    }
    return;
  }

  Serial.println("No valid orders file found - recreating empty orders.json");
  File newFile = LittleFS.open(ORDERS_FILE, "w");
  if (newFile) {
    newFile.println("[]");
    newFile.flush();
    newFile.close();
  }
  ordersData.clear();
  ordersData.to<JsonArray>();
}

void addNewProduct(String code, String name) {
  JsonArray arr = productsData.as<JsonArray>();
  
  // KIỂM TRA DUPLICATE TRƯỚC KHI THÊM
  for (JsonObject product : arr) {
    String existingCode = product["code"];
    String existingName = product["name"];
    if (existingCode == code || (existingCode == code && existingName == name)) {
      Serial.println("Product already exists: " + code + " - " + name + " (skipped)");
      return;
    }
  }
  
  // Tìm ID lớn nhất
  int maxId = 0;
  for (JsonObject product : arr) {
    int id = product["id"];
    if (id > maxId) maxId = id;
  }
  
  // Thêm sản phẩm mới
  JsonObject newProduct = arr.createNestedObject();
  newProduct["id"] = maxId + 1;
  newProduct["code"] = code;
  newProduct["name"] = name;
  
  saveProductsToFile();
  Serial.println("Add new product: " + code + " - " + name + " (ID: " + String(maxId + 1) + ")");
}

void deleteProduct(int productId) {
  JsonArray arr = productsData.as<JsonArray>();
  
  for (size_t i = 0; i < arr.size(); i++) {
    if (arr[i]["id"] == productId) {
      arr.remove(i);
      saveProductsToFile();
      Serial.println("Deleted product ID: " + String(productId));
      return;
    }
  }
  Serial.println("Product ID not found: " + String(productId));
}

void addNewOrder(String productCode, String customerName, int quantity, String notes) {
  JsonArray arr = ordersData.as<JsonArray>();
  
  // Tìm ID lớn nhất
  int maxId = 0;
  for (JsonObject order : arr) {
    int id = order["id"];
    if (id > maxId) maxId = id;
  }
  
  // Thêm đơn hàng mới
  JsonObject newOrder = arr.createNestedObject();
  newOrder["id"] = maxId + 1;
  newOrder["productCode"] = productCode;
  newOrder["customerName"] = customerName;
  newOrder["quantity"] = quantity;
  newOrder["notes"] = notes;
  newOrder["status"] = "pending";
  newOrder["createdDate"] = getTimeStr();
  newOrder["completedCount"] = 0;
  
  saveOrdersToFile();
  Serial.println("Added new order: " + productCode + " x" + String(quantity) + " for " + customerName);
}

void deleteOrder(int orderId) {
  JsonArray arr = ordersData.as<JsonArray>();
  
  for (size_t i = 0; i < arr.size(); i++) {
    if (arr[i]["id"] == orderId) {
      arr.remove(i);
      saveOrdersToFile();
      Serial.println("Deleted order ID: " + String(orderId));
      return;
    }
  }
  Serial.println("Order ID not found: " + String(orderId));
}

void printDataStatus() {
  Serial.println("DATA STORAGE STATUS:");
  
  // Kiểm tra files tồn tại
  Serial.println("Files on LittleFS:");
  Serial.println("  - /settings.json: " + String(LittleFS.exists("/settings.json") ? "✅ EXISTS" : "❌ MISSING"));
  Serial.println("  - /products.json: " + String(LittleFS.exists("/products.json") ? "✅ EXISTS" : "❌ MISSING"));
  Serial.println("  - /orders.json: " + String(LittleFS.exists("/orders.json") ? "✅ EXISTS" : "❌ MISSING"));
  Serial.println("  - /bag_types.json: " + String(LittleFS.exists("/bag_types.json") ? "✅ EXISTS" : "❌ MISSING"));
  Serial.println("  - /bag_configs.json: " + String(LittleFS.exists("/bag_configs.json") ? "✅ EXISTS" : "❌ MISSING"));
  
  // Hiển thị dữ liệu trong memory
  Serial.println("Data in Memory:");
  Serial.println("  - Products: " + String(productsData.size()) + " items");
  Serial.println("  - Orders: " + String(ordersData.size()) + " items");
  Serial.println("  - Bag Types: " + String(bagTypes.size()) + " items");
  Serial.println("  - Bag Configs: " + String(bagConfigs.size()) + " items");
  
  // Hiển thị chi tiết products
  if (productsData.size() > 0) {
    Serial.println("Products Details:");
    JsonArray arr = productsData.as<JsonArray>();
    for (JsonObject product : arr) {
      Serial.println("    ID:" + String(product["id"].as<int>()) + 
                     " Code:" + product["code"].as<String>() + 
                     " Name:" + product["name"].as<String>());
    }
  }
  
  // Hiển thị chi tiết orders
  if (ordersData.size() > 0) {
    Serial.println("Orders Details:");
    JsonArray arr = ordersData.as<JsonArray>();
    for (JsonObject order : arr) {
      Serial.println("    ID:" + String(order["id"].as<int>()) + 
                     " Product:" + order["productCode"].as<String>() + 
                     " Customer:" + order["customerName"].as<String>() +
                     " Qty:" + String(order["quantity"].as<int>()));
    }
  }
}
