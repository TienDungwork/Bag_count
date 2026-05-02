#include "realtime.h"

//----------------------------------------Realtime Callback & Publish Functions
void handleRealtimeMessage(const String& topicStr, const String& message) {
  Serial.println("Realtime message received:");
  Serial.println("  Topic: " + topicStr);
  Serial.println("  Message: " + message);
  
  // Xử lý các lệnh điều khiển
  if (topicStr == TOPIC_CMD_START) {
    DynamicJsonDocument sourceDoc(256);
    if (deserializeJson(sourceDoc, message) == DeserializationError::Ok) {
  String source = sourceDoc["source"].as<String>();
      if (source == "IR_REMOTE") {
        Serial.println("START command from IR remote");
        return;
      }
    }
    Serial.println("MQTT Command: START from Web");
    handleWebCommand(1); // Start command from web
    
  } else if (topicStr == TOPIC_CMD_PAUSE) {
    // Kiểm tra source để tránh xử lý lại lệnh từ chính mình
    DynamicJsonDocument sourceDoc(256);
    if (deserializeJson(sourceDoc, message) == DeserializationError::Ok) {
  String source = sourceDoc["source"].as<String>();
      if (source == "IR_REMOTE") {
        Serial.println("PAUSE command from IR remote");
        return;
      }
    }
    Serial.println("MQTT Command: PAUSE from Web");
    handleWebCommand(2); // Pause command from web
    
  } else if (topicStr == TOPIC_CMD_RESET) {
    // Kiểm tra source để tránh xử lý lại lệnh từ chính mình
    DynamicJsonDocument sourceDoc(256);
    if (deserializeJson(sourceDoc, message) == DeserializationError::Ok) {
  String source = sourceDoc["source"].as<String>();
      if (source == "IR_REMOTE") {
        Serial.println("RESET command from IR remote");
        return;
      }
    }
    Serial.println("MQTT Command: RESET from Web");
    handleWebCommand(3); // Reset command from web
    
  } else if (topicStr == "bagcounter/ws/current_order") {
    Serial.println("Realtime Command: CURRENT ORDER via WS");
    // Parse JSON để chọn đơn hàng
    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, message) == DeserializationError::Ok) {
      String orderType = doc["type"].as<String>();
      String selectedOrderCode = doc["orderCode"].as<String>();
      String selectedProductCode = doc["productCode"].as<String>();
      String selectedCustomerName = doc["customerName"].as<String>();
      int target = doc["target"] | 20;
      int warn = doc["warn"] | 10;
      bool keepCount = doc["keepCount"] | false;
      int currentCountFromWeb = doc["currentCount"] | 0;
      
      if (orderType.length() > 0) {
        bagType = orderType;
        if (selectedProductCode.length() > 0) {
          productCode = selectedProductCode;
        }
        if (selectedOrderCode.length() > 0) {
          orderCode = selectedOrderCode;
        }
        if (selectedCustomerName.length() > 0) {
          customerName = selectedCustomerName;
        }
        targetCount = target;
        
        // Reset trạng thái cho đơn mới; khi resume thì không để count web cũ = 0 ghi đè count thật trên ESP32.
        if (keepCount) {
          if (currentCountFromWeb > 0) {
            totalCount = currentCountFromWeb;
          }
        } else {
          totalCount = 0;
        }
        isRunning = false;
        isTriggerEnabled = false;
        isCountingEnabled = false;
        isLimitReached = false;
        isStartAuthorized = false;
        waitForSensorClearOnStart = true;
        isBagDetected = false;
        waitingForInterval = false;
        bagStartTime = 0;
        lastBagTime = 0;
        lastDebounceTime = 0;
        
        // Cập nhật bagConfig
        bool found = false;
        for (auto& cfg : bagConfigs) {
          if (cfg.type == orderType) {
            cfg.target = target;
            cfg.warn = warn;
            cfg.status = "SELECTED";
            found = true;
            break;
          }
        }
        
        if (!found) {
          BagConfig newCfg = {orderType, target, warn, "SELECTED"};
          bagConfigs.push_back(newCfg);
        }
        
        saveBagConfigsToFile();
        needUpdate = true;

        // Đồng bộ order đang chọn vào ordersData để tránh mất context khi resume
        for (size_t i = 0; i < ordersData.size(); i++) {
          JsonArray orders = ordersData[i]["orders"];
          for (size_t j = 0; j < orders.size(); j++) {
            JsonObject order = orders[j];
            String oCode = order["orderCode"].as<String>();
            String oProductCode = orderProductCodeFromJson(order);
            String oProductName = order["productName"].as<String>();

            bool isMatch = false;
            if (selectedOrderCode.length() > 0 && selectedProductCode.length() > 0) {
              isMatch = (oCode == selectedOrderCode && oProductCode == selectedProductCode);
            } else if (selectedOrderCode.length() > 0) {
              isMatch = (oCode == selectedOrderCode);
            } else if (selectedProductCode.length() > 0) {
              isMatch = (oProductCode == selectedProductCode);
            } else {
              isMatch = (oProductName == orderType);
            }

            if (isMatch) {
              order["selected"] = true;
              order["status"] = keepCount ? "paused" : "waiting";
              if (keepCount) {
                if (currentCountFromWeb > 0) {
                  order["currentCount"] = currentCountFromWeb;
                  order["executeCount"] = currentCountFromWeb;
                }
              } else {
                order["currentCount"] = 0;
                order["executeCount"] = 0;
              }
            }
          }
        }
        saveOrdersToFile();
        
        // Publish confirmation
        publishStatusMQTT();
        
        Serial.println("Order selected MQTT: " + orderType + " | orderCode=" + orderCode + " | productCode=" + productCode + " | currentCount=" + String(totalCount));
      }
    }
    
  } else if (topicStr == TOPIC_CONFIG) {
    Serial.println("MQTT Command: CONFIG UPDATE");
    // Parse JSON config update - ÁP DỤNG SETTINGS TỪNG BỘ PHẬN
    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, message) == DeserializationError::Ok) {
      bool settingsChanged = false;
      
      if (doc.containsKey("brightness")) {
        displayBrightness = 100;
        if (dma_display) dma_display->setBrightness8(255);
        Serial.println("MQTT: Brightness fixed at 100%");
        settingsChanged = true;
      }
      
      if (doc.containsKey("sensorDelay")) {
        sensorDelayMs = doc["sensorDelay"];
        debounceDelay = sensorDelayMs;
        Serial.println("MQTT: Applied sensorDelay: " + String(sensorDelayMs) + "ms");
        settingsChanged = true;
      }
      
      if (doc.containsKey("bagDetectionDelay")) {
        ::bagDetectionDelay = doc["bagDetectionDelay"];
        Serial.println("MQTT: Applied bagDetectionDelay: " + String(::bagDetectionDelay) + "ms");
        settingsChanged = true;
      }
      
      if (doc.containsKey("bagTimeMultiplier")) {
        ::bagTimeMultiplier = doc["bagTimeMultiplier"];
        Serial.println("MQTT: Applied bagTimeMultiplier: " + String(::bagTimeMultiplier) + "%");
        settingsChanged = true;
      }
      
      if (doc.containsKey("minBagInterval")) {
        ::minBagInterval = doc["minBagInterval"];
        Serial.println("MQTT: Applied minBagInterval: " + String(::minBagInterval) + "ms");
        settingsChanged = true;
      }
      
      if (doc.containsKey("autoReset")) {
        ::autoReset = doc["autoReset"];
        Serial.println("MQTT: Applied autoReset: " + String(::autoReset ? "true" : "false"));
        settingsChanged = true;
      }
      
      if (doc.containsKey("relayDelayAfterComplete")) {
        ::relayDelayAfterComplete = doc["relayDelayAfterComplete"];
        Serial.println("MQTT: Applied relayDelayAfterComplete: " + String(::relayDelayAfterComplete) + "ms");
        settingsChanged = true;
      }
      
      if (doc.containsKey("conveyorName")) {
        conveyorName = doc["conveyorName"].as<String>();
        Serial.println("MQTT: Applied conveyorName: " + conveyorName);
        settingsChanged = true;
      }
      
      if (doc.containsKey("location")) {
        location = doc["location"].as<String>();
        Serial.println("MQTT: Applied location: " + location);
        settingsChanged = true;
      }
      
      // Lưu settings vào file nếu có thay đổi
      if (settingsChanged) {
        DynamicJsonDocument settingsDoc(1024);
        settingsDoc["conveyorName"] = conveyorName;
        settingsDoc["location"] = location;
        settingsDoc["brightness"] = 100;
        settingsDoc["sensorDelay"] = sensorDelayMs;
        settingsDoc["bagDetectionDelay"] = ::bagDetectionDelay;
        settingsDoc["minBagInterval"] = ::minBagInterval;
        settingsDoc["autoReset"] = ::autoReset;
        settingsDoc["relayDelayAfterComplete"] = ::relayDelayAfterComplete;
        settingsDoc["bagTimeMultiplier"] = ::bagTimeMultiplier;
        
        File file = LittleFS.open("/settings.json", "w");
        if (file) {
          serializeJson(settingsDoc, file);
          file.close();
          Serial.println("MQTT: Settings saved to file");
        }
      }
      
      // Legacy targets
      if (doc.containsKey("target")) {
        targetCount = doc["target"];
        Serial.println("Target updated via MQTT: " + String(targetCount));
        needUpdate = true;
      }
      
      // Thêm xử lý resetLimit để ESP32 tiếp tục đếm
      if (doc.containsKey("resetLimit") && doc["resetLimit"]) {
        isLimitReached = false;
        Serial.println("Limit reset via MQTT - continuing count");
        needUpdate = true;
      }
    }
    
  } else if (topicStr == TOPIC_CMD_BATCH) {
    Serial.println("MQTT Command: BATCH INFO");
    // Parse JSON batch information
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, message) == DeserializationError::Ok) {
      if (doc.containsKey("firstOrder")) {
        JsonObject firstOrder = doc["firstOrder"];
        if (firstOrder.containsKey("productName")) {
          bagType = firstOrder["productName"].as<String>();
          Serial.println("MQTT: Set first order product: " + bagType);
        }
        if (firstOrder.containsKey("customerName")) {
          String customerName = firstOrder["customerName"].as<String>();
          Serial.println("MQTT: Customer: " + customerName);
        }
        // Cập nhật target từ firstOrder (đơn hàng hiện tại) chứ không phải totalTarget
        if (firstOrder.containsKey("quantity")) {
          targetCount = firstOrder["quantity"] | 20;
          Serial.println("MQTT: Set current order target: " + String(targetCount));
        }
      }
      
      needUpdate = true;
    }
  }
}

void publishStatusMQTT() {
  static unsigned long lastPublish = 0;
  
  // Debounce - chỉ publish mỗi 500ms
  if (millis() - lastPublish < 500) {
    return;
  }
  lastPublish = millis();
  
  DynamicJsonDocument doc(1024);
  doc["deviceId"] = conveyorName;   
  doc["status"] = currentSystemStatus; 
  doc["count"] = totalCount;
  doc["target"] = targetCount;
  doc["type"] = bagType;
  doc["productCode"] = productCode;
  doc["startTime"] = startTimeStr;
  doc["timestamp"] = getTimeStr();
  doc["uptime"] = millis() / 1000;
  doc["isWarning"] = false;
  doc["limitReached"] = isLimitReached;
  doc["sensorEnabled"] = isCountingEnabled;
  doc["triggerEnabled"] = isTriggerEnabled;
  doc["lastMeasuredTime"] = lastMeasuredTime;
  doc["isMeasuringSensor"] = isMeasuringSensor;
  int sensorReading = digitalRead(SENSOR_PIN);
  doc["sensorCurrentState"] = sensorRawStateName(sensorReading);
  doc["sensorBlocked"] = isSensorBlocked(sensorReading);
  doc["sensorActiveLevel"] = "LOW";
  if (isMeasuringSensor && sensorActiveStartTime > 0) {
    doc["currentMeasuringTime"] = millis() - sensorActiveStartTime;
  }
  
  // Kiểm tra cảnh báo
  for (auto& cfg : bagConfigs) {
    if (cfg.type == bagType) {
      int warningThreshold = cfg.target - cfg.warn;
      doc["isWarning"] = (totalCount >= warningThreshold);
      doc["warningThreshold"] = warningThreshold;
      break;
    }
  }
  
  String message;
  serializeJson(doc, message);

  broadcastRealtimeMessage(TOPIC_STATUS, message);
}

void publishCountUpdate() {
  // Throttle count updates để tránh spam MQTT
  unsigned long now = millis();
  if (now - lastCountPublish < COUNT_PUBLISH_THROTTLE) {
    return;
  }
  lastCountPublish = now;

  DynamicJsonDocument doc(256);
  doc["deviceId"] = conveyorName;
  doc["count"] = totalCount;
  doc["target"] = targetCount;
  doc["type"] = bagType;
  doc["productCode"] = productCode;
  doc["timestamp"] = getTimeStr();
  doc["progress"] = targetCount > 0 ? ((float)totalCount / targetCount) * 100 : 0;
  
  String message;
  serializeJson(doc, message);

  broadcastRealtimeMessage(TOPIC_COUNT, message);
}

void publishAlert(String alertType, String message) {
  DynamicJsonDocument doc(256);
  doc["deviceId"] = conveyorName;
  doc["alertType"] = alertType; // "WARNING", "COMPLETED", "ERROR"
  doc["message"] = message;
  doc["count"] = totalCount;
  doc["target"] = targetCount;
  doc["type"] = bagType;
  doc["timestamp"] = getTimeStr();
  
  String alertMessage;
  serializeJson(doc, alertMessage);

  broadcastRealtimeMessage(TOPIC_ALERTS, alertMessage);
  Serial.println("Alert published: " + alertType + " - " + message);
}

void publishSensorData() {
  DynamicJsonDocument doc(768);
  doc["deviceId"] = conveyorName;
  doc["sensorTriggered"] = isCountingEnabled;
  doc["triggerEnabled"] = isTriggerEnabled;
  doc["lastTrigger"] = millis();
  int sensorReading = digitalRead(SENSOR_PIN);
  doc["sensorState"] = isSensorBlocked(sensorReading) ? "DETECTED" : "CLEAR";
  doc["triggerState"] = digitalRead(TRIGGER_SENSOR_PIN) == LOW ? "DETECTED" : "CLEAR";
  doc["lastMeasuredTime"] = lastMeasuredTime;
  doc["isMeasuringSensor"] = isMeasuringSensor;
  doc["sensorCurrentState"] = sensorRawStateName(sensorReading);
  doc["sensorBlocked"] = isSensorBlocked(sensorReading);
  doc["sensorActiveLevel"] = "LOW";
  if (isMeasuringSensor && sensorActiveStartTime > 0) {
    doc["currentMeasuringTime"] = millis() - sensorActiveStartTime;
  }
  doc["timestamp"] = getTimeStr();
  
  String message;
  serializeJson(doc, message);

  broadcastRealtimeMessage(TOPIC_SENSOR, message);
}

void updateSensorTimingMeasurement() {
  int currentTimingState = digitalRead(SENSOR_PIN);

  if (currentTimingState == lastTimingSensorState) {
    return;
  }

  lastTimingSensorState = currentTimingState;

  if (isSensorBlocked(currentTimingState)) {
    sensorActiveStartTime = millis();
    isMeasuringSensor = true;
    Serial.println("📏 BẮT ĐẦU đo thời gian sensor LOW (có vật chắn)");
  } else {
    if (isMeasuringSensor && sensorActiveStartTime > 0) {
      unsigned long measuredDuration = millis() - sensorActiveStartTime;
      lastMeasuredTime = measuredDuration;
      Serial.print("📏 KẾT THÚC đo thời gian sensor: ");
      Serial.print(measuredDuration);
      Serial.println("ms");
    }
    sensorActiveStartTime = 0;
    isMeasuringSensor = false;
  }

  publishSensorData();
}

void publishHeartbeat() {
  DynamicJsonDocument doc(256);
  doc["deviceId"] = conveyorName;
  doc["status"] = "ONLINE";
  doc["uptime"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["wifiRSSI"] = WiFi.RSSI();
  doc["ipAddress"] = currentNetworkMode == ETHERNET_MODE ? ETH.localIP().toString() :
                     (currentNetworkMode == WIFI_AP_MODE ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
  doc["timestamp"] = getTimeStr();
  
  String message;
  serializeJson(doc, message);

  broadcastRealtimeMessage(TOPIC_HEARTBEAT, message);
}

void publishBagConfigs() {
  DynamicJsonDocument doc(2048);
  JsonArray orders = doc.createNestedArray("orders");
  
  for (auto& cfg : bagConfigs) {
    JsonObject order = orders.createNestedObject();
    order["type"] = cfg.type;
    order["target"] = cfg.target;
    order["status"] = cfg.status;
    order["warn"] = cfg.warn;
    
    // Tìm thông tin chi tiết từ ordersData
    JsonArray ordersArray = ordersData.as<JsonArray>();
    for (size_t i = 0; i < ordersArray.size(); i++) {
      JsonObject orderData = ordersArray[i];
      if (orderData["id"].as<String>() == cfg.type) {
        order["productName"] = orderData["productName"];
        order["productCode"] = orderData["productCode"];
        order["quantity"] = orderData["quantity"];
        order["executeCount"] = orderData["executeCount"];
        break;
      }
    }
  }
  
  doc["currentOrder"] = bagType;
  doc["currentCount"] = totalCount;
  doc["timestamp"] = getTimeStr();
  
  String message;
  serializeJson(doc, message);
  
  broadcastRealtimeMessage("bagcounter/orders", message);
  Serial.println("Orders configuration published to realtime web clients");
}
