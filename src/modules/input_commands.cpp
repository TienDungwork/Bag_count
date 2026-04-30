#include "input_commands.h"

//----------------------------------------IR Remote functions
unsigned long mapIRButton(unsigned long code) {
  if (code == 0xFFA25D || code == 0xE318261B) return 1;  // Nút 1 - Start
  if (code == 0x511DBB || code == 0xFF629D) return 2;    // Nút 2 - Pause
  if (code == 0xFFE21D || code == 0xEE886D7F) return 3;  // Nút 3 - Reset
  if (code == 0xFF22DD || code == 0x52A3D41F) return 4;   // Nút 4 - (Đóng relay 5s)
  return 0;
}

void handleIRCommand(int button) {
  DynamicJsonDocument doc(2048);
  String msg;
  String action = "";
  
  switch(button) {
    case 1: // Start
      Serial.println("🎛️ IR Remote: START");
      isRunning = true;
      isTriggerEnabled = true;
      isCountingEnabled = true;
      isStartAuthorized = true;
      waitForSensorClearOnStart = true;
      currentSystemStatus = "RUNNING";
      isRunning = true;
      isTriggerEnabled = true;
      isCountingEnabled = true;
      currentSystemStatus = "RUNNING";
      action = "START";
      
      if (time(nullptr) > 24 * 3600) {
        startTimeStr = getTimeStr();
        timeWaitingForSync = false;
      } else {
        startTimeStr = "Waiting for time sync...";
        timeWaitingForSync = true;
      }
      
      // Chỉ load từ ordersData nếu bagType trống (chưa được set từ web)
      if (bagType.isEmpty()) {
        loadCurrentOrderForDisplay();
      }
      
      for (auto& cfg : bagConfigs) {
        cfg.status = "RUNNING";
      }
      saveBagConfigsToFile();
      updateStartLED();
      needUpdate = true;
      break;
      
    case 2: // Pause
      Serial.println("IR Remote: PAUSE");
      isRunning = false;
      isTriggerEnabled = false;
      isCountingEnabled = false;
      isStartAuthorized = false;
      currentSystemStatus = "PAUSE";
      action = "PAUSE";
      
      for (auto& cfg : bagConfigs) {
        cfg.status = "PAUSE";
      }
      saveBagConfigsToFile();
      updateStartLED();
      needUpdate = true;
      break;
      
    case 3: // Reset
      Serial.println("IR Remote: RESET");
      totalCount = 0;
      isLimitReached = false;
      isRunning = false;
      isTriggerEnabled = false;
      isCountingEnabled = false;
      isStartAuthorized = false;
      history.clear();
      startTimeStr = "";
      timeWaitingForSync = false;
      currentSystemStatus = "RESET";
      action = "RESET";
      
      for (auto& cfg : bagConfigs) {
        cfg.status = "RESET";
      }
      saveBagConfigsToFile();
      updateStartLED();
      updateDoneLED();
      needUpdate = true;
      break;
      saveBagConfigsToFile();
      updateStartLED();
      updateDoneLED();
      needUpdate = true;
      break;
      case 4: // Custom - Đóng relay 5s
        Serial.println(" IR Remote: Đóng relay 5s");
        digitalWrite(START_LED_PIN, HIGH); // Đóng relay
        unsigned long relayCustomStart = millis();
        while (millis() - relayCustomStart < 10000) {
          delay(10);
        }
        digitalWrite(START_LED_PIN, LOW); // Ngắt relay
        break;
  }
  
  // GỬI LỆNH REALTIME NHƯ WEB
  doc.clear();
  doc["source"] = "IR_REMOTE";
  doc["action"] = action;
  doc["status"] = isRunning ? "RUNNING" : "STOPPED";
  doc["count"] = totalCount;
  doc["timestamp"] = millis();
  doc["startTime"] = startTimeStr;
  msg = "";
  serializeJson(doc, msg);
  
  broadcastRealtimeMessage(TOPIC_IR_CMD, msg);
  Serial.println("IR Command sent to realtime web: " + action);
  
  updateDisplay();
  publishStatusMQTT();
  
  lastIRCommand = action;
  lastIRTimestamp = millis();
  hasNewIRCommand = true;

  Serial.println("IR Command " + action + " sent to web via WebSocket");
}

// Load thông tin đơn hàng hiện tại để hiển thị trên LED khi IR Remote START
void loadCurrentOrderForDisplay() {
  Serial.println("Loading order display...");
  
  // Tìm đơn hàng đang đếm hoặc chờ trong ordersData
  for (size_t i = 0; i < ordersData.size(); i++) {
    JsonArray orders = ordersData[i]["orders"];
    
    // Tìm đơn hàng đang counting hoặc selected
    for (size_t j = 0; j < orders.size(); j++) {
      JsonObject order = orders[j];
      String status = order["status"].as<String>();
      bool selected = order["selected"] | false;
      
      if ((status == "counting" || status == "waiting" || status == "paused") && selected) {
        // Load thông tin đơn hàng này
        String productName = order["productName"].as<String>();
        String productCodeFromOrder = "";
        
        // Lấy product code từ product object nếu có
        if (order.containsKey("product") && order["product"].containsKey("code")) {
          productCodeFromOrder = order["product"]["code"].as<String>();
        }
        
        int quantity = order["quantity"] | 20;
        int warningQuantity = order["warningQuantity"].as<int>() | 5; // Mặc định 5 nếu không có
        
        bagType = productName;
        productCode = productCodeFromOrder;
        
        // Cập nhật biến global cho API customer
        if (order.containsKey("orderCode")) {
          orderCode = order["orderCode"].as<String>();
          Serial.println("Updated global orderCode: " + orderCode);
        }
        if (order.containsKey("customerName")) {
          customerName = order["customerName"].as<String>();
          Serial.println("Updated global customerName: " + customerName);
        }
        
        targetCount = quantity;
        
        // Cập nhật bagConfig với warningQuantity từ order
        for (auto& cfg : bagConfigs) {
          if (cfg.type == productName) {
            cfg.target = quantity;
            cfg.warn = warningQuantity;
            Serial.println("Updated warning threshold : " + String(warningQuantity));
            break;
          }
        }
        
        Serial.println("Loaded order:");
        Serial.println("   Product: " + productName);
        Serial.println("   Code: " + productCodeFromOrder);
        Serial.println("   Target: " + String(quantity));
        Serial.println("   Warning: " + String(warningQuantity));
        
        needUpdate = true;
        return;
      }
    }
  }
  
  Serial.println("No current order for display");
}

// Handle commands from Web
void handleWebCommand(int button) {
  String action = "";
  
  switch(button) {
    case 1: // Start
      isRunning = true;
      isTriggerEnabled = true;
      isCountingEnabled = true;
      isStartAuthorized = true;
      waitForSensorClearOnStart = true;
      currentSystemStatus = "RUNNING";
      action = "START";
      
      if (time(nullptr) > 24 * 3600) {
        startTimeStr = getTimeStr();
        timeWaitingForSync = false;
      } else {
        startTimeStr = "Waiting for time sync...";
        timeWaitingForSync = true;
      }
      
      // LUÔN LOAD THÔNG TIN ĐƠN HÀNG HIỆN TẠI KHI START
      loadCurrentOrderForDisplay();
      
      for (auto& cfg : bagConfigs) {
        cfg.status = "RUNNING";
      }
      saveBagConfigsToFile();
      updateStartLED();
      needUpdate = true;
      break;
      
    case 2: // Pause
      isRunning = false;
      isTriggerEnabled = false;
      isCountingEnabled = false;
      isStartAuthorized = false;
      waitForSensorClearOnStart = false;
      currentSystemStatus = "PAUSE";
      action = "PAUSE";
      
      for (auto& cfg : bagConfigs) {
        cfg.status = "PAUSE";
      }
      saveBagConfigsToFile();
      updateStartLED();
      needUpdate = true;
      break;
      
    case 3: // Reset
      totalCount = 0;
      isLimitReached = false;
      isRunning = false;
      isTriggerEnabled = false;
      isCountingEnabled = false;
      isStartAuthorized = false;
      waitForSensorClearOnStart = false;
      history.clear();
      startTimeStr = "";
      timeWaitingForSync = false;
      currentSystemStatus = "RESET";
      action = "RESET";
      
      // CLEAR RELAY DELAY STATE
      isOrderComplete = false;
      isRelayDelayActive = false;
      orderCompleteTime = 0;
      Serial.println("CLEAR RELAY DELAY STAT");
      
      // CLEAR WARNING THRESHOLD STATE
      isWarningLedActive = false;
      hasReachedWarningThreshold = false;
      warningLedStartTime = 0;
      Serial.println("CLEAR WARNING THRESHOLD STATE");
      
      for (auto& cfg : bagConfigs) {
        cfg.status = "RESET";
      }
      saveBagConfigsToFile();
      updateStartLED();
      updateDoneLED();
      needUpdate = true;
      break;
  }

  publishStatusMQTT();
}
  
