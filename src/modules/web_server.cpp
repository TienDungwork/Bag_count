#include "web_server.h"

//----------------------------------------Web server API
void setupWebServer() {
  server.on("/", HTTP_GET, [](){
    // Nếu đang ở chế độ WiFi STA và được truy cập qua AP IP
    if (currentNetworkMode == WIFI_STA_MODE && server.client().localIP() == WiFi.softAPIP()) {
      String redirectUrl = "http://" + WiFi.localIP().toString() + "/";
      server.sendHeader("Location", redirectUrl);
      server.send(302, "text/plain", "Redirecting to: " + redirectUrl);
      Serial.println("Redirected from AP IP to STA IP: " + redirectUrl);
      return;
    }
    
    if (LittleFS.exists("/index.html")) {
      File file = LittleFS.open("/index.html", "r");
      server.streamFile(file, "text/html");
      file.close();
      Serial.println("Served index.html from LittleFS");
    } else {
      // Backup HTML với thông tin IP hiện tại
      String currentIP = (currentNetworkMode == WIFI_STA_MODE) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
      String networkMode = (currentNetworkMode == WIFI_STA_MODE) ? "WiFi STA" : 
                          (currentNetworkMode == ETHERNET_MODE) ? "Ethernet" : "WiFi AP";
      
      String html = "<!DOCTYPE html><html><head><title>Bag Counter</title>";
      html += "<meta http-equiv='refresh' content='5'></head><body>"; // Auto refresh mỗi 5s
      html += "<h1>🎒 Bag Counter System</h1>";
      html += "<div style='background: #f0f0f0; padding: 10px; margin: 10px 0;'>";
      html += "<p><strong>📍 Current IP:</strong> " + currentIP + "</p>";
      html += "<p><strong>🌐 Network Mode:</strong> " + networkMode + "</p>";
      html += "<p><strong>⚡ System Status:</strong> <span style='color: green'>Connected</span></p>";
      html += "</div>";
      html += "<p>🔧 Web interface is working!</p>";
      if (currentNetworkMode == WIFI_STA_MODE) {
        html += "<p>✅ <a href='http://" + WiFi.localIP().toString() + "/test'>Test Page</a></p>";
      }
      html += "</body></html>";
      server.send(200, "text/html", html);
      Serial.println("Served backup HTML (index.html not found)");
    }
  });

  // Serve CSS files
  server.on("/style.css", HTTP_GET, [](){
    if (LittleFS.exists("/style.css")) {
      File file = LittleFS.open("/style.css", "r");
      server.streamFile(file, "text/css");
      file.close();
    } else {
      server.send(404, "text/plain", "CSS not found");
    }
  });

  // Serve JS files
  server.on("/script.js", HTTP_GET, [](){
    if (LittleFS.exists("/script.js")) {
      File file = LittleFS.open("/script.js", "r");
      server.streamFile(file, "application/javascript");
      file.close();
    } else {
      server.send(404, "text/plain", "JS not found");
    }
  });

  // Serve FontAwesome CSS
  server.on("/all.min.css", HTTP_GET, [](){
    if (LittleFS.exists("/all.min.css")) {
      File file = LittleFS.open("/all.min.css", "r");
      server.streamFile(file, "text/css");
      file.close();
      Serial.println("Served all.min.css from LittleFS");
    } else {
      server.send(404, "text/plain", "all.min.css not found");
      Serial.println("ERROR: all.min.css not found in LittleFS");
    }
  });
  
  //Webfonts
  server.on("/webfonts/fa-solid-900.woff2", HTTP_GET, [](){
  if (LittleFS.exists("/webfonts/fa-solid-900.woff2")) {
    File file = LittleFS.open("/webfonts/fa-solid-900.woff2", "r");
    server.streamFile(file, "font/woff2");
    file.close();
  } else {
    server.send(404, "text/plain", "Font not found");
  }
});

server.on("/webfonts/fa-solid-900.ttf", HTTP_GET, [](){
  if (LittleFS.exists("/webfonts/fa-solid-900.ttf")) {
    File file = LittleFS.open("/webfonts/fa-solid-900.ttf", "r");
    server.streamFile(file, "font/ttf");
    file.close();
  } else {
    server.send(404, "text/plain", "Font not found");
  }
});

  // Serve test customer API page
  server.on("/test-customer-api", HTTP_GET, [](){
    if (LittleFS.exists("/test_customer_api.html")) {
      File file = LittleFS.open("/test_customer_api.html", "r");
      server.streamFile(file, "text/html");
      file.close();
      Serial.println("Served test_customer_api.html from LittleFS");
    } else {
      server.send(404, "text/plain", "Test customer API page not found");
    }
  });
  
  // API trạng thái hiện tại - Real-time polling
  server.on("/api/status", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    
    DynamicJsonDocument doc(512);
    
    // TRẢ VỀ STATUS ĐÚNG THEO TRẠNG THÁI THỰC TẾ CỦA HỆ THỐNG
    String currentStatus = "WAIT";  // Default
    
    // Nếu đang chạy thì trả về RUNNING
    if (isRunning) {
      currentStatus = "RUNNING";
    } else {
      // Kiểm tra status từ bagConfigs - lấy status đầu tiên khác WAIT
      for (auto& cfg : bagConfigs) {
        if (cfg.status != "WAIT") {
          currentStatus = cfg.status;  // PAUSE, RESET, DONE
          break;
        }
      }
      
      // kiểm tra bagType hiện tại
      if (currentStatus == "WAIT") {
        for (auto& cfg : bagConfigs) {
          if (cfg.type == bagType) {
            currentStatus = cfg.status;
            break;
          }
        }
      }
    }
    
    // // Serial.print("API Status returning: ");
    // Serial.print(currentStatus);
    // Serial.print(" (isRunning: ");
    // Serial.print(isRunning);
    // Serial.print(", bagType: ");
    // Serial.print(bagType);
    // Serial.println(")");
    
    doc["status"] = currentStatus;
    doc["count"] = totalCount;
    doc["startTime"] = startTimeStr;
    doc["currentType"] = bagType;
    doc["target"] = targetCount;
    doc["isWarning"] = false;
    doc["timestamp"] = millis();
    doc["sensorEnabled"] = isCountingEnabled;
    doc["triggerEnabled"] = isTriggerEnabled;
    doc["limitReached"] = isLimitReached;
    doc["currentTime"] = getTimeStr();
    
    // Extended info
    doc["conveyorId"] = conveyorName;
    doc["ipAddress"] = currentNetworkMode == ETHERNET_MODE ? ETH.localIP().toString() : WiFi.localIP().toString();
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["realtimeConnected"] = realtimeSocket.count() > 0;
    
    // LED states
    doc["startLedOn"] = startLedOn;
    doc["doneLedOn"] = doneLedOn;
    
    // Thêm tên băng tải từ settings
    if (LittleFS.exists("/settings.json")) {
      File file = LittleFS.open("/settings.json", "r");
      if (file) {
        String content = file.readString();
        file.close();
        DynamicJsonDocument settingsDoc(1024);
        if (deserializeJson(settingsDoc, content) == DeserializationError::Ok) {
          if (settingsDoc.containsKey("conveyorName")) {
            doc["conveyorName"] = settingsDoc["conveyorName"].as<String>();
          }
        }
      }
    }
    
    // Thêm trạng thái bagConfig hiện tại để web sync được
    for (auto& cfg : bagConfigs) {
      if (cfg.type == bagType) {
        doc["bagConfigStatus"] = cfg.status;  // WAIT, RUNNING, DONE
        int warningThreshold = cfg.target - cfg.warn;
        doc["isWarning"] = (totalCount >= warningThreshold);
        doc["warningThreshold"] = warningThreshold;
        break;
      }
    }
    
    // THONG TIN IR COMMAND CHO WEB
    doc["lastIRCommand"] = lastIRCommand;
    doc["lastIRTimestamp"] = lastIRTimestamp;
    doc["hasNewIRCommand"] = hasNewIRCommand;
    
    // THÔNG TIN BATCH HIỆN TẠI
    doc["currentBatchName"] = currentBatchName;
    doc["currentBatchId"] = currentBatchId;
    doc["currentBatchDescription"] = currentBatchDescription;
    doc["totalOrdersInBatch"] = totalOrdersInBatch;
    doc["batchTotalTarget"] = batchTotalTarget;
    
    // Reset flag sau khi gửi cho web
    if (hasNewIRCommand) {
      hasNewIRCommand = false;
      Serial.println("IR Command flag reset after sending to web");
    }
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // API kiểm tra thay đổi từ IR Remote - DEPRECATED: Thay bằng MQTT
  // server.on("/api/ir_status", HTTP_GET, [](){
  //   // Đã chuyển sang MQTT real-time
  // });

  // API lấy thời gian hiện tại
  server.on("/api/current_time", HTTP_GET, [](){
    DynamicJsonDocument doc(256);
    time_t now = time(nullptr);
    bool isTimeSynced = (now > 24 * 3600);
    
    doc["currentTime"] = getTimeStr();
    doc["timestamp"] = now;
    doc["isTimeSynced"] = isTimeSynced;
    doc["uptimeSeconds"] = millis() / 1000;
    
    if (isTimeSynced) {
      struct tm* t = localtime(&now);
      doc["year"] = t->tm_year + 1900;
      doc["month"] = t->tm_mon + 1;
      doc["day"] = t->tm_mday;
      doc["hour"] = t->tm_hour;
      doc["minute"] = t->tm_min;
      doc["second"] = t->tm_sec;
    }
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });
  
  // API GET orders - Trả về orders data (KHÔNG phải bagConfigs)
  server.on("/api/orders", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    // Serial.println("GET /api/orders called");
    // Serial.println("ordersData size: " + String(ordersData.size()) + " items");
    
    // Trả về orders data từ LittleFS
    String out;
    serializeJson(ordersData, out);
    
    // Serial.println("Sending orders data (size: " + String(out.length()) + " chars)");
    // Commented out to reduce log spam
    // if (out.length() > 0 && out.length() < 300) {
    //   Serial.println("Orders content: " + out);
    // } else if (out.length() >= 300) {
    //   Serial.println("Orders content preview: " + out.substring(0, 200) + "...");
    // } else {
    //   Serial.println("Orders content: EMPTY");
    // }
    
    server.send(200, "application/json", out);
  });
  
  // API bagConfigs - Trả về bag configurations (khác với orders)
  server.on("/api/bagconfigs", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    DynamicJsonDocument doc(2048);
    JsonArray arr = doc.to<JsonArray>();
    for (auto& cfg : bagConfigs) {
      JsonObject o = arr.createNestedObject();
      o["type"] = cfg.type;
      o["target"] = cfg.target;
      o["warn"] = cfg.warn;
      o["status"] = cfg.status;
      o["isCurrent"] = (cfg.type == bagType);
      o["count"] = (cfg.type == bagType) ? totalCount : 0;
    }
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // API quản lý đơn hàng
  server.on("/api/order-list", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    // Trả về danh sách đơn hàng từ LittleFS
    String out;
    serializeJson(ordersData, out);
    server.send(200, "application/json", out);
    
    Serial.println("Order-list API" + String(ordersData.size()) + " orders");
  });

  server.on("/api/order-list", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, server.arg("plain"));
      
  String productCode = doc["productCode"].as<String>();
  String customerName = doc["customerName"].as<String>(); 
      int quantity = doc["quantity"];
      String notes = doc["notes"] | "";
      
      if (productCode.length() > 0 && customerName.length() > 0 && quantity > 0) {
        addNewOrder(productCode, customerName, quantity, notes);
        server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Order added successfully\"}");
      } else {
        server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Product code, customer name and quantity are required\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"No data provided\"}");
    }
  });

  server.on("/api/order-list", HTTP_DELETE, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (server.hasArg("id")) {
      String orderIdStr = server.arg("id");
      int orderId = orderIdStr.toInt();
      
      if (orderId > 0) {
        deleteOrder(orderId);
        server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Order deleted successfully\"}");
      } else {
        server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid order ID\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Missing order ID\"}");
    }
  });

  // API lịch sử đếm - đọc từ file
  server.on("/api/history", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (!LittleFS.exists("/history.json")) {
      server.send(200, "application/json", "[]");
      Serial.println("History API called - no history file found");
      return;
    }

    File file = LittleFS.open("/history.json", "r");
    if (!file) {
      server.send(200, "application/json", "[]");
      Serial.println("History API called - file exists but cannot be opened");
      return;
    }

    String content = file.readString();
    file.close();
    
    Serial.println("History API: file size = " + String(content.length()) + " bytes");
    Serial.println("History API: content preview = " + content.substring(0, min(200, (int)content.length())));

    // Try to parse and normalize older formats into the expected rich format
    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, content);
    if (err) {
      // If parse fails, return raw content (best-effort)
      Serial.println("History API: failed to parse JSON, returning raw content");
      server.send(200, "application/json", content);
      return;
    }

    DynamicJsonDocument outDoc(16384);
    JsonArray out = outDoc.to<JsonArray>();

    if (doc.is<JsonArray>()) {
      JsonArray arr = doc.as<JsonArray>();
      Serial.println("History API: found " + String(arr.size()) + " entries in array");
      for (JsonVariant v : arr) {
        JsonObject obj = out.createNestedObject();

        // Normalize fields with fallbacks for old schema
        const char* timestamp = v["timestamp"] | v["time"] | "";
        const char* customer = v["customerName"] | v["customer"] | "";
        const char* product = v["productName"] | v["product"] | v["batchType"] | "";
        const char* orderCode = v["orderCode"] | "";
        const char* vehicle = v["vehicleNumber"] | v["vehicle"] | "";
        int planned = v["plannedQuantity"] | v["plannedQuantity"] | v["planned"] | v["target"] | 0;
        int actual = v["actualCount"] | v["actual"] | v["totalCounted"] | v["count"] | 0;

        obj["timestamp"] = timestamp;
        obj["customerName"] = customer;
        obj["productName"] = product;
        obj["orderCode"] = orderCode;
        obj["vehicleNumber"] = vehicle;
        obj["plannedQuantity"] = planned;
        obj["actualCount"] = actual;
        obj["isBatch"] = v["isBatch"] | false;
      }
    } else if (doc.is<JsonObject>()) {
      // Single object - normalize it into an array
      JsonObject v = doc.as<JsonObject>();
      JsonObject obj = out.createNestedObject();

      const char* timestamp = v["timestamp"] | v["time"] | "";
      const char* customer = v["customerName"] | v["customer"] | "";
      const char* product = v["productName"] | v["product"] | v["batchType"] | "";
      const char* orderCode = v["orderCode"] | "";
      const char* vehicle = v["vehicleNumber"] | v["vehicle"] | "";
      int planned = v["plannedQuantity"] | v["plannedQuantity"] | v["planned"] | v["target"] | 0;
      int actual = v["actualCount"] | v["actual"] | v["totalCounted"] | v["count"] | 0;

      obj["timestamp"] = timestamp;
      obj["customerName"] = customer;
      obj["productName"] = product;
      obj["orderCode"] = orderCode;
      obj["vehicleNumber"] = vehicle;
      obj["plannedQuantity"] = planned;
      obj["actualCount"] = actual;
      obj["isBatch"] = v["isBatch"] | false;
    }

    // Serialize normalized array and send
    String outStr;
    serializeJson(outDoc, outStr);
    Serial.println("History API: returning " + String(out.size()) + " normalized entries");
    Serial.println("History API: response size = " + String(outStr.length()) + " bytes");
    server.send(200, "application/json", outStr);
    Serial.println("History API called - returned normalized history array");
  });

  // API điều khiển cơ bản
  server.on("/api/cmd", HTTP_POST, [](){
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, server.arg("plain"));
      String cmd = doc["cmd"];
      
      Serial.println("Received API command: " + cmd);
      
      // DEBUG: Print full command payload
      if (cmd == "UPDATE_ORDER" || cmd == "DELETE_ORDER") {
        Serial.println("DEBUG: Full command payload:");
        String debugPayload;
        serializeJson(doc, debugPayload);
        Serial.println(debugPayload);
      }
      
      if (cmd == "start") {
        Serial.println("Web Start command - delegating to handleWebCommand()");
        handleWebCommand(1); // Gọi chung logic với IR command
      } else if (cmd == "pause") {
        Serial.println("Web Pause command - delegating to handleWebCommand()");  
        handleWebCommand(2); // Gọi chung logic với IR command
      } else if (cmd == "reset") {
        Serial.println("Web Reset command - delegating to handleWebCommand()");
        handleWebCommand(3); // Gọi chung logic với IR command
      } else if (cmd == "reset_count_only") {
        Serial.println("Reset count only command received");
        // CHỈ RESET COUNT, KHÔNG THAY ĐỔI TRẠNG THÁI KHÁC
        totalCount = 0;
        isLimitReached = false;
        history.clear();
        
        // CLEAR RELAY DELAY STATE (khi reset count)
        isOrderComplete = false;
        isRelayDelayActive = false;
        orderCompleteTime = 0;
        Serial.println("RELAY DELAY STATE CLEARED (count reset)");
        
        // CLEAR WARNING THRESHOLD STATE (khi reset count)
        isWarningLedActive = false;
        hasReachedWarningThreshold = false;
        warningLedStartTime = 0;
        Serial.println("WARNING LED STATE CLEARED (count reset)");
        
        // GIỮ NGUYÊN TRẠNG THÁI isRunning, isTriggerEnabled
        // CHỈ CẬP NHẬT COUNT DISPLAY
        updateDoneLED();
        needUpdate = true;
        
        Serial.println("Count reset to 0, keeping current running state");
      } else if (cmd == "set_current_order") {
        // Cập nhật thông tin đơn hàng hiện tại để hiển thị trên LED
        String productName = doc["productName"].as<String>();
        String customerNameFromWeb = doc["customerName"].as<String>();
        String orderCodeFromWeb = doc["orderCode"].as<String>();
        String productCodeFromWeb = doc["productCode"].as<String>();  // Nhận mã sản phẩm từ web
        int target = doc["target"] | 20;
        int warningQuantity = doc["warningQuantity"] | 5;
        float unitWeight = doc["unitWeight"] | 0.0;
        bool keepCount = doc["keepCount"] | false;
        bool isRunningOrder = doc["isRunning"] | false;
        int existingCount = doc["currentCount"] | 0;  // Nhận currentCount từ web
        
        Serial.println("Setting current order:");
        Serial.println("Product: " + productName);
        Serial.println("Product Code: " + productCodeFromWeb);
        Serial.println("Customer: " + customerNameFromWeb);
        Serial.println("Order Code: " + orderCodeFromWeb);
        Serial.println("Target: " + String(target));
        Serial.println("Warning: " + String(warningQuantity));
        Serial.println("UnitWeight: " + String(unitWeight, 3));
        Serial.println("Keep Count: " + String(keepCount));
        Serial.println("Existing Count: " + String(existingCount));
        Serial.println("Is Running: " + String(isRunningOrder));
        
        // Cập nhật biến hiển thị
        bagType = productName;
        productCode = productCodeFromWeb;  // Cập nhật mã sản phẩm
        orderCode = orderCodeFromWeb;      // Cập nhật biến global
        customerName = customerNameFromWeb; // Cập nhật biến global
        Serial.println("Updated global orderCode: " + orderCode);
        Serial.println("Updated global customerName: " + customerName);
        targetCount = target; 
        if (unitWeight <= 0.0f) {
          unitWeight = resolveUnitWeightFromData(orderCodeFromWeb, productCodeFromWeb, productName);
        }
        currentOrderUnitWeight = unitWeight;
        Serial.println("Resolved UnitWeight: " + String(currentOrderUnitWeight, 3));
        
        // KHÔNG RESET COUNT NẾU keepCount = true
        if (!keepCount) {
          totalCount = 0;
          isLimitReached = false;
          Serial.println("Reset totalCount = 0 (keepCount = false)");
        } else if (existingCount > 0) {
          totalCount = existingCount;
          isLimitReached = false; // Reset flag to allow continued counting
          Serial.println("Set totalCount to existing count: " + String(existingCount));
          
          // Reset sensor states to ensure counting can continue
          isBagDetected = false;
          waitingForInterval = false;
          lastBagTime = 0;
          bagStartTime = 0;
          Serial.println("Reset sensor states for continued counting");
        } else {
          Serial.println("Keep existing totalCount: " + String(totalCount) + " (keepCount = true)");
        }
        
        // MỖI LẦN ĐỔI ĐƠN: reset trạng thái cảm biến để không dính detectionDuration của đơn trước
        isBagDetected = false;
        waitingForInterval = false;
        lastBagTime = 0;
        bagStartTime = 0;
        lastDebounceTime = 0;
        waitForSensorClearOnStart = true;
        Serial.println("Reset sensor states for order switch");

        // ĐẶT TRẠNG THÁI RUNNING NẾU isRunning = true
        if (isRunningOrder) {
          isRunning = true;
          isTriggerEnabled = true;
          isStartAuthorized = true;
          Serial.println("Set running state to RUNNING");
        }
        
        // Tìm và cập nhật bagConfig
        bool found = false;
        for (auto& cfg : bagConfigs) {
          if (cfg.type == productName) {
            cfg.target = target;
            cfg.warn = warningQuantity;
            if (isRunningOrder) {
              cfg.status = "RUNNING";
            }
            found = true;
            break;
          }
        }
        
        // Tạo mới nếu không tìm thấy
        if (!found) {
          BagConfig newCfg;
          newCfg.type = productName;
          newCfg.target = target;
          newCfg.warn = warningQuantity;
          newCfg.status = isRunningOrder ? "RUNNING" : "WAIT";
          bagConfigs.push_back(newCfg);
        }
        
        // Tìm và đánh dấu đơn hiện tại là SELECTED
        for (size_t i = 0; i < ordersData.size(); i++) {
          JsonArray orders = ordersData[i]["orders"];
          
          for (size_t j = 0; j < orders.size(); j++) {
            JsonObject order = orders[j];
            String orderProductCode = "";
            if (order.containsKey("product") && order["product"].containsKey("code")) {
              orderProductCode = order["product"]["code"].as<String>();
            }
            String orderOrderCode = order["orderCode"].as<String>();
            
            // Đánh dấu đơn hiện tại là SELECTED
            if (orderProductCode == productCodeFromWeb && orderOrderCode == orderCodeFromWeb) {
              order["selected"] = true;
              order["status"] = isRunningOrder ? "counting" : (keepCount ? "paused" : "waiting");
              order["currentCount"] = keepCount ? existingCount : 0;
              order["executeCount"] = keepCount ? existingCount : 0;
              Serial.println("Marked order with productCode " + productCodeFromWeb + " as SELECTED");
            }
          }
        }
        
        // Lưu thay đổi vào file
        saveOrdersToFile();
        
        saveBagConfigsToFile();
        needUpdate = true;
        
        Serial.println("Current order updated successfully");
      } else if (cmd == "set_mode") {
        // XỬ LÝ THAY ĐỔI CHẾ ĐỘ HIỂN THỊ
        String mode = doc["mode"].as<String>();
        
        if (mode == "output" || mode == "input") {
          currentMode = mode;
          needUpdate = true;
          updateDisplay();
          
          Serial.println("Mode changed to: " + mode);
          Serial.println("Display updated with new mode");
        } else {
          Serial.println("Invalid mode: " + mode);
          server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid mode. Use 'output' or 'input'\"}");
          return;
        }
      } else if (cmd == "next_order") {
        // XỬ LÝ CHUYỂN SANG ĐƠN HÀNG TIẾP THEO
        Serial.println("Next order command received");
        
        String productName = doc["productName"].as<String>();
        String customerNameFromWeb = doc["customerName"].as<String>();
        String orderCodeFromWeb = doc["orderCode"].as<String>();
        String productCodeFromWeb = doc["productCode"].as<String>();  // Nhận mã sản phẩm từ web
        int target = doc["target"] | 20;
        int warningQuantity = doc["warningQuantity"] | 5;
        float unitWeight = doc["unitWeight"] | 0.0;
        bool keepCount = doc["keepCount"] | false;
        
        Serial.println("Switching to next order:");
        Serial.println("Product: " + productName);
        Serial.println("Product Code: " + productCodeFromWeb);
        Serial.println("Customer: " + customerNameFromWeb);
        Serial.println("Order Code: " + orderCodeFromWeb);
        Serial.println("Target: " + String(target));
        Serial.println("UnitWeight: " + String(unitWeight, 3));
        Serial.println("Keep Count: " + String(keepCount));
        
        // CẬP NHẬT THÔNG TIN ĐƠN HÀNG MỚI
        bagType = productName;
        productCode = productCodeFromWeb;  // Cập nhật mã sản phẩm
        orderCode = orderCodeFromWeb;      // Cập nhật biến global
        customerName = customerNameFromWeb; // Cập nhật biến global
        Serial.println("Updated global orderCode: " + orderCode);
        Serial.println("Updated global customerName: " + customerName);
        targetCount = target;
        if (unitWeight <= 0.0f) {
          unitWeight = resolveUnitWeightFromData(orderCodeFromWeb, productCodeFromWeb, productName);
        }
        currentOrderUnitWeight = unitWeight;
        Serial.println("Resolved UnitWeight: " + String(currentOrderUnitWeight, 3));
        
        // KHÔNG RESET COUNT NẾU keepCount = true (để tiếp tục đếm multi-order)
        if (!keepCount) {
          totalCount = 0;
          isLimitReached = false;
        }

        // Đổi đơn tiếp theo: luôn reset chu kỳ cảm biến để tránh cộng dồn thời gian đơn trước
        isBagDetected = false;
        waitingForInterval = false;
        lastBagTime = 0;
        bagStartTime = 0;
        lastDebounceTime = 0;
        waitForSensorClearOnStart = true;
        Serial.println("Reset sensor states for next order");
        
        // ĐẢM BẢO TRẠNG THÁI ĐANG CHẠY
        isRunning = true;
        isTriggerEnabled = true;
        isStartAuthorized = true;
        // isCountingEnabled sẽ được set khi cảm biến kích hoạt
        
        // TÌM VÀ CẬP NHẬT BAGCONFIG
        bool found = false;
        for (auto& cfg : bagConfigs) {
          if (cfg.type == productName) {
            cfg.target = target;
            cfg.warn = warningQuantity;
            cfg.status = "RUNNING";  // ĐẢM BẢO TRẠNG THÁI RUNNING
            found = true;
            Serial.println("Updated existing bagConfig to RUNNING");
            break;
          }
        }
        
        // TẠO MỚI NẾU KHÔNG TÌM THẤY
        if (!found) {
          BagConfig newCfg;
          newCfg.type = productName;
          newCfg.target = target;
          newCfg.warn = warningQuantity;
          newCfg.status = "RUNNING";
          bagConfigs.push_back(newCfg);
          Serial.println("Created new bagConfig with RUNNING status");
        }
        

        // Để đảm bảo /api/status trả về đúng
        for (auto& cfg : bagConfigs) {
          if (cfg.type == bagType) {  // bagType hiện tại đang active
            cfg.status = "RUNNING";
            Serial.println("Updated current bagType status to RUNNING");
            break;
          }
        }
        
        saveBagConfigsToFile();
        updateStartLED();
        needUpdate = true;
        
        Serial.println("Next order setup completed - Status: RUNNING");
      } else if (cmd == "select") {
        String type = doc["type"];
        int target = doc["target"] | 20;
        int warn = doc["warn"] | 10;
        orderCode = doc["orderCode"].as<String>();  // Cập nhật biến global
        Serial.println("Updated global orderCode: " + orderCode);
        
        // Cập nhật hoặc tạo mới bagConfig cho đơn hàng này
        bool found = false;
        for (auto& cfg : bagConfigs) {
          if (cfg.type == type || (orderCode.length() > 0 && cfg.type.indexOf(orderCode) >= 0)) {
            bagType = cfg.type;
            targetCount = target > 0 ? target : cfg.target;
            found = true;
            
            // Reset trạng thái cho đơn hàng mới
            isRunning = false;
            isTriggerEnabled = false;
            isCountingEnabled = false;
            isLimitReached = false;
            totalCount = 0;
            finishedBlinking = false;
            blinkCount = 0;
            isBlinking = false;
            startTimeStr = "";
            timeWaitingForSync = false;
            updateStartLED();
            updateDoneLED();
            
            // Cập nhật trạng thái
            cfg.status = "RUNNING";
            break;
          }
        }
        
        if (!found && type.length() > 0) {
          // Tạo mới nếu không tìm thấy
          BagConfig newCfg;
          newCfg.type = type;
          newCfg.target = target;
          newCfg.warn = warn;
          newCfg.status = "RUNNING";
          bagConfigs.push_back(newCfg);
          
          bagType = type;
          targetCount = target;
          
          // Reset trạng thái
          isRunning = false;
          isTriggerEnabled = false;
          isCountingEnabled = false;
          isLimitReached = false;
          totalCount = 0;
          finishedBlinking = false;
          blinkCount = 0;
          isBlinking = false;
          startTimeStr = "";
          timeWaitingForSync = false;
          updateStartLED();
          updateDoneLED();
        }
        
        // Đánh dấu các loại khác là WAIT hoặc giữ nguyên DONE
        for (auto& c : bagConfigs) {
          if (c.type != bagType && c.status != "DONE") {
            c.status = "WAIT";
          }
        }
        
        saveBagConfigsToFile();
        needUpdate = true;
        
        Serial.println("Order selected: " + bagType);
        Serial.println("Target: " + String(targetCount));
        Serial.println("Warning: " + String(warn));
      } else if (cmd == "REMOTE") {
        String button = doc["button"];
        Serial.println("Remote command received: " + button);
        
        // Xử lý các lệnh remote với function handleIRCommand
        if (button == "START") {
          handleIRCommand(1);  // Nút 1 - Start
        } else if (button == "STOP") {
          handleIRCommand(2);  // Nút 2 - Pause  
        } else if (button == "RESET") {
          handleIRCommand(3);  // Nút 3 - Reset
        }
      } else if (cmd == "set_product") {
        // LỆNH ĐƠN GIẢN ĐỂ SET SẢN PHẨM HIỆN TẠI
        String productName = doc["productName"];
        int target = doc["target"] | 0;
        
        if (productName.length() > 0) {
          Serial.println("Setting current product: " + productName);
          Serial.println("Target: " + String(target));
          
          bagType = productName;
          if (target > 0) {
            targetCount = target;
          }
          
          needUpdate = true;  // Force update display
          updateDisplay();    // Update ngay
          
          Serial.println("Product set - bagType: " + bagType + ", targetCount: " + String(targetCount));
        }
      } else if (cmd == "batch_info") {
        // XỬ LÝ THÔNG TIN BATCH TỪ WEB
        if (doc.containsKey("batchTotalTarget")) {
          batchTotalTarget = doc["batchTotalTarget"].as<int>();
          Serial.println("Batch Total Target set to: " + String(batchTotalTarget));
        }
        
        if (doc.containsKey("firstOrder")) {
          JsonObject firstOrder = doc["firstOrder"];
          String productName = firstOrder["productName"];
          int target = firstOrder["quantity"] | 0;
          
          if (productName.length() > 0) {
            Serial.println("Setting product from batch info: " + productName);
            bagType = productName;
            if (target > 0) {
              targetCount = target; // Hiển thị số lượng của đơn hàng hiện tại (không phải tổng batch)
            }
            needUpdate = true;
            updateDisplay();
            Serial.println("Batch product set - bagType: " + bagType + ", targetCount: " + String(targetCount));
          }
        }
      } else if (cmd == "ping") {
        // LỆNH PING ĐỂ TEST CONNECTIVITY
        Serial.println("Ping command received from web");
        server.send(200, "text/plain", "PONG - ESP32 is alive!");
        return;
      } else if (cmd == "test") {
        // LỆNH TEST ĐỂ DEBUG COMMUNICATION
        Serial.println("Test command received from web");
        Serial.println("Current state:");
        Serial.println("   isRunning: " + String(isRunning));
        Serial.println("   isTriggerEnabled: " + String(isTriggerEnabled));
        Serial.println("   isCountingEnabled: " + String(isCountingEnabled));
        Serial.println("   totalCount: " + String(totalCount));
        Serial.println("   Realtime clients: " + String(realtimeSocket.count()));
        server.send(200, "text/plain", "TEST OK - Check Serial Monitor for details");
        return;
      } else if (cmd == "clear_batch") {
        // XỬ LÝ LỆNH XÓA BATCH
        Serial.println("Command payload:");
        String debugPayload;
        serializeJson(doc, debugPayload);
        Serial.println(debugPayload);
        
        long long batchId = doc["batchId"].as<long long>();
        Serial.println("Extracted batch ID: " + String((unsigned long long)batchId));
        
        if (batchId > 0) {
          Serial.println("Processing clear batch for ID: " + String((unsigned long long)batchId));
          Serial.println("Current ordersData size: " + String(ordersData.size()));
          
          // Debug: Log all existing batch IDs
          Serial.println("Existing batch IDs in ordersData:");
          for (size_t i = 0; i < ordersData.size(); i++) {
            long long existingId = ordersData[i]["id"].as<long long>();
            String existingName = ordersData[i]["name"] | "Unknown";
            Serial.println("   Batch " + String(i) + ": ID=" + String((unsigned long long)existingId) + ", Name=" + existingName);
          }
          
          // Tìm và xóa batch từ ordersData
          bool found = false;
          for (size_t i = 0; i < ordersData.size(); i++) {
            long long currentBatchId = ordersData[i]["id"].as<long long>();
            if (currentBatchId == batchId) {
              String batchName = ordersData[i]["name"] | "Unknown";
              ordersData.remove(i);
              found = true;
              Serial.println("BATCH FOUND AND REMOVED:");
              Serial.println("   - Batch ID: " + String((unsigned long long)batchId));
              Serial.println("   - Batch Name: " + batchName);
              Serial.println("   - Removed from index: " + String(i));
              break;
            }
          }
          
          if (found) {
            // Lưu thay đổi vào file
            saveOrdersToFile();
            Serial.println("Orders file updated after batch deletion");
            
            // Reset trạng thái nếu batch đang active
            if (currentBatchId == String((unsigned long long)batchId)) {
              currentBatchId = "";
              batchTotalTarget = 0;
              bagType = "bao";
              targetCount = 20;
              isRunning = false;
              isTriggerEnabled = false;
              isCountingEnabled = false;
              totalCount = 0;
              isLimitReached = false;
              updateStartLED();
              updateDoneLED();
              needUpdate = true;
              Serial.println("ESP32 state reset after clearing active batch");
            }
            
            Serial.println("Batch cleared successfully from ESP32");
            server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Batch cleared successfully\",\"batchId\":" + String((unsigned long long)batchId) + "}");
          } else {
            Serial.println("BATCH NOT FOUND:");
            Serial.println("   - Requested batch ID: " + String((unsigned long long)batchId));
            Serial.println("   - Available batches: " + String(ordersData.size()));
            server.send(404, "application/json", "{\"status\":\"Error\",\"message\":\"Batch not found\",\"batchId\":" + String((unsigned long long)batchId) + "}");
          }
        } else {
          Serial.println("Invalid batch ID for clear operation: " + String((unsigned long long)batchId));
          server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid batch ID\",\"received\":" + String((unsigned long long)batchId) + "}");
        }
      } else if (cmd == "UPDATE_ORDER") {
        // XỬ LÝ CẬP NHẬT ORDER
        Serial.println("Processing UPDATE_ORDER command...");
        if (doc.containsKey("batchId") && doc.containsKey("orderId") && doc.containsKey("orderData")) {
          int batchId = doc["batchId"];
          int orderId = doc["orderId"];
          JsonObject orderData = doc["orderData"];
          
          Serial.println("Updating order - Batch ID: " + String(batchId) + ", Order ID: " + String(orderId));
          
          // Tìm batch trong ordersData
          bool batchFound = false;
          for (size_t i = 0; i < ordersData.size(); i++) {
            if (ordersData[i]["id"] == batchId) {
              JsonArray orders = ordersData[i]["orders"];
              
              // Tìm order trong batch
              bool orderFound = false;
              for (size_t j = 0; j < orders.size(); j++) {
                if (orders[j]["id"] == orderId) {
                  // Cập nhật order data
                  orders[j]["productCode"] = orderData["productCode"];
                  orders[j]["productName"] = orderData["productName"];
                  orders[j]["quantity"] = orderData["quantity"];
                  orders[j]["bagType"] = orderData["bagType"];
                  
                  orderFound = true;
                  Serial.println("Order updated successfully");
                  break;
                }
              }
              
              if (!orderFound) {
                Serial.println("Order not found in batch");
                server.send(404, "application/json", "{\"status\":\"Error\",\"message\":\"Order not found\"}");
                return;
              }
              
              batchFound = true;
              break;
            }
          }
          
          if (!batchFound) {
            Serial.println("Batch not found");
            server.send(404, "application/json", "{\"status\":\"Error\",\"message\":\"Batch not found\"}");
            return;
          }
          
          // Lưu thay đổi vào file
          saveOrdersToFile();
          Serial.println("Order update saved to file");
          
        } else {
          Serial.println("Missing required parameters for UPDATE_ORDER");
          server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Missing batchId, orderId or orderData\"}");
          return;
        }
      } else if (cmd == "DELETE_ORDER") {
        // XỬ LÝ XÓA ORDER
        Serial.println("Processing DELETE_ORDER command...");
        if (doc.containsKey("batchId") && doc.containsKey("orderId")) {
          long long batchId = doc["batchId"].as<long long>();
          int orderId = doc["orderId"];
          
          Serial.println("Deleting order - Batch ID: " + String((unsigned long long)batchId) + ", Order ID: " + String(orderId));
          
          // Tìm batch trong ordersData
          bool batchFound = false;
          for (size_t i = 0; i < ordersData.size(); i++) {
            long long currentBatchId = ordersData[i]["id"].as<long long>();
            Serial.println("   - Checking batch with ID: " + String((unsigned long long)currentBatchId));
            if (currentBatchId == batchId) {
              JsonArray orders = ordersData[i]["orders"];
              
              // Tìm và xóa order trong batch
              bool orderFound = false;
              for (size_t j = 0; j < orders.size(); j++) {
                Serial.println("     - Checking order with ID: " + String(orders[j]["id"].as<int>()));
                if (orders[j]["id"] == orderId) {
                  orders.remove(j);
                  orderFound = true;
                  Serial.println("Order deleted successfully");
                  break;
                }
              }
              
              if (!orderFound) {
                Serial.println("Order not found in batch");
                server.send(404, "application/json", "{\"status\":\"Error\",\"message\":\"Order not found\"}");
                return;
              }
              
              batchFound = true;
              break;
            }
          }
          
          if (!batchFound) {
            Serial.println("Batch not found");
            server.send(404, "application/json", "{\"status\":\"Error\",\"message\":\"Batch not found\"}");
            return;
          }
          
          // Lưu thay đổi vào file
          saveOrdersToFile();
          Serial.println("Order deletion saved to file");
          
        } else {
          Serial.println("Missing required parameters for DELETE_ORDER");
          server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Missing batchId or orderId\"}");
          return;
        }
      } else if (cmd == "test_simulate_encoder") {
        // Web: mô phỏng cảm biến encoder (TRIGGER_SENSOR) — bật isCountingEnabled như khi phát hiện vật thể
        if (!isRunning) {
          server.send(409, "application/json", "{\"status\":\"Error\",\"message\":\"Chưa bắt đầu — nhấn Bắt đầu trước\"}");
          return;
        }
        isCountingEnabled = true;
        Serial.println("WEB TEST: test_simulate_encoder -> isCountingEnabled=true");
        needUpdate = true;
      } else if (cmd == "test_simulate_count_sensor") {
        // Web: mô phỏng một lần đếm từ cảm biến đếm bao (SENSOR_PIN / T61)
        if (!isRunning) {
          server.send(409, "application/json", "{\"status\":\"Error\",\"message\":\"Chưa bắt đầu — nhấn Bắt đầu trước\"}");
          return;
        }
        if (!isCountingEnabled) {
          server.send(409, "application/json", "{\"status\":\"Error\",\"message\":\"Chưa bật đếm — thử nút test cảm biến encoder trước\"}");
          return;
        }
        if (isLimitReached) {
          server.send(409, "application/json", "{\"status\":\"Error\",\"message\":\"Đã đủ target — không đếm thêm\"}");
          return;
        }
        int bagCount = doc["bagCount"] | 1;
        if (bagCount < 1) bagCount = 1;
        if (bagCount > 50) bagCount = 50;
        Serial.println("WEB TEST: test_simulate_count_sensor bagCount=" + String(bagCount));
        updateCount(bagCount);
        publishSensorData();
        needUpdate = true;
      }
    }
    server.send(200, "text/plain", "OK");
  });

  // API xóa loại bao
  server.on("/api/bagtype", HTTP_DELETE, [](){
    if (server.hasArg("type")) {
      String typeToDelete = server.arg("type");
      // Xóa khỏi danh sách loại
      bagTypes.erase(
        std::remove_if(bagTypes.begin(), bagTypes.end(),
          [&typeToDelete](const String& type) { return type == typeToDelete; }),
        bagTypes.end()
      );
      // Xóa khỏi cấu hình
      bagConfigs.erase(
        std::remove_if(bagConfigs.begin(), bagConfigs.end(),
          [&typeToDelete](const BagConfig& cfg) { return cfg.type == typeToDelete; }),
        bagConfigs.end()
      );
      // Lưu thay đổi
      saveBagTypesToFile();
      saveBagConfigsToFile();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing type parameter");
    }
  });

  // API cập nhật cấu hình
  server.on("/api/config", HTTP_POST, [](){
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, server.arg("plain"));
      String type = doc["type"];
      int target = doc["target"];
      int warn = doc["warn"];
      
      // Cập nhật cấu hình
      bool found = false;
      for (auto& cfg : bagConfigs) {
        if (cfg.type == type) {
          cfg.target = target;
          cfg.warn = warn;
          found = true;
          break;
        }
      }
      
      // Nếu chưa có cấu hình cho loại này, tạo mới
      if (!found) {
        BagConfig newCfg;
        newCfg.type = type;
        newCfg.target = target;
        newCfg.warn = warn;
        newCfg.status = "WAIT";
        bagConfigs.push_back(newCfg);
      }
      
      // Lưu thay đổi
      saveBagConfigsToFile();
    }
    server.send(200, "text/plain", "OK");
  });

  // API lấy danh sách loại bao
  server.on("/api/bagtype", HTTP_GET, [](){
    DynamicJsonDocument doc(512);
    JsonArray arr = doc.to<JsonArray>();
    for (auto& type : bagTypes) arr.add(type);
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // API thêm loại bao mới
  server.on("/api/bagtype", HTTP_POST, [](){
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, server.arg("plain"));
      String type = doc["type"];
      
      // Kiểm tra và thêm loại mới
      if (type.length() > 0 && std::find(bagTypes.begin(), bagTypes.end(), type) == bagTypes.end()) {
        bagTypes.push_back(type);
        saveBagTypesToFile();
        needUpdate = true;
      }
    }
    server.send(200, "text/plain", "OK");
  });

  // API cho sản phẩm - Trả về dữ liệu từ LittleFS
  server.on("/api/products", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    // Trả về dữ liệu sản phẩm từ LittleFS
    String out;
    serializeJson(productsData, out);
    server.send(200, "application/json", out);
    
    Serial.println("Products API called - returned " + String(productsData.size()) + " products");
  });

  // server.on("/api/products", HTTP_POST, [](){
  //   server.sendHeader("Access-Control-Allow-Origin", "*");
  //   
  //   if (server.hasArg("plain")) {
  //     DynamicJsonDocument doc(256);
  //     deserializeJson(doc, server.arg("plain"));
  //     String code = doc["code"];
  //     String name = doc["name"];
  //     
  //     Serial.println("Add Product Request: code='" + code + "', name='" + name + "'");
  //     
  //     if (code.length() > 0 && name.length() > 0) {
  //       int sizeBefore = productsData.size();
  //       
  //       // Thêm sản phẩm mới vào LittleFS
  //       addNewProduct(code, name);
  //       
  //       int sizeAfter = productsData.size();
  //       
  //       DynamicJsonDocument response(256);
  //       response["status"] = "OK";
  //       
  //       if (sizeAfter > sizeBefore) {
  //         response["message"] = "Product added successfully";
  //         response["added"] = true;
  //       } else {
  //         response["message"] = "Product already exists (not added)";
  //         response["added"] = false;
  //       }
  //       
  //       response["total_products"] = sizeAfter;
  //       
  //       String out;
  //       serializeJson(response, out);
  //       server.send(200, "application/json", out);
  //       
  //     } else {
  //       server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Code and name are required\"}");
  //     }
  //   } else {
  //     server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"No data provided\"}");
  //   }
  // });

  server.on("/api/products", HTTP_DELETE, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (server.hasArg("id")) {
      String productIdStr = server.arg("id");
      int productId = productIdStr.toInt();
      
      if (productId > 0) {
        deleteProduct(productId);
        server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Product deleted successfully\"}");
      } else {
        server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid product ID\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Missing product ID\"}");
    }
  });

  // API xóa đơn hàng
  server.on("/api/orders", HTTP_DELETE, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (server.hasArg("orderCode")) {
      String orderCode = server.arg("orderCode");
      
      // Tìm và xóa khỏi bagConfigs
      bagConfigs.erase(
        std::remove_if(bagConfigs.begin(), bagConfigs.end(),
          [&orderCode](const BagConfig& cfg) { 
            return cfg.type.indexOf(orderCode) >= 0; 
          }),
        bagConfigs.end()
      );
      
      // Lưu thay đổi
      saveBagConfigsToFile();
      
      Serial.println("Order deleted: " + orderCode);
      server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Order deleted from ESP32\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Missing order code\"}");
    }
  });

  // API cho đơn hàng
  server.on("/api/new_orders", HTTP_GET, [](){
    // Trả về danh sách đơn hàng
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    
    // Tạm thời trả về dữ liệu mẫu
    JsonObject order1 = arr.createNestedObject();
    order1["id"] = 1;
    order1["orderNumber"] = 1;
    order1["customerName"] = "Công ty ABC";
    order1["orderCode"] = "DH001";
    order1["vehicleNumber"] = "51A-12345";
    order1["productName"] = "Gạo thường";
    order1["quantity"] = 100;
    order1["currentCount"] = 0;
    order1["status"] = "waiting";
    order1["selected"] = false;
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/new_orders", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    // Check memory before processing
    size_t freeHeap = ESP.getFreeHeap();
    Serial.println("Free heap before new_orders processing: " + String(freeHeap) + " bytes");
    
    if (freeHeap < 5000) {
      Serial.println("Very low memory detected, rejecting request");
      server.send(507, "application/json", "{\"status\":\"Error\",\"message\":\"Insufficient memory\"}");
      return;
    }
    
    if (server.hasArg("plain")) {
      String jsonData = server.arg("plain");
      Serial.println("Received data size: " + String(jsonData.length()) + " bytes");
      
      // Use smaller JSON document
      DynamicJsonDocument doc(256);  // Reduced from 512
      DeserializationError error = deserializeJson(doc, jsonData);
      
      if (error) {
        Serial.println("JSON Parse Error: " + String(error.c_str()));
        server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid JSON\"}");
        return;
      }
      
      customerName = doc["customerName"].as<String>();  // Cập nhật biến global
      orderCode = doc["orderCode"].as<String>();        // Cập nhật biến global
      vehicleNumber = doc["vehicleNumber"].as<String>(); // Cập nhật biến global
      Serial.println("Updated global customerName: " + customerName);
      Serial.println("Updated global orderCode: " + orderCode);
      Serial.println("Updated global vehicleNumber: " + vehicleNumber);
      String productName = doc["productName"];
      int quantity = doc["quantity"];
      int warningQuantity = doc["warningQuantity"];
      
      // DEBUG: Log received data
      Serial.println("DEBUG: Received new order data:");
      Serial.println("  Customer: " + customerName);
      Serial.println("  OrderCode: " + orderCode);
      Serial.println("  Vehicle: " + vehicleNumber);
      Serial.println("  Product: " + productName);
      Serial.println("  Quantity: " + String(quantity));
      Serial.println("  WarningQuantity: " + String(warningQuantity));
      Serial.println("  WarningQuantity (raw): " + doc["warningQuantity"].as<String>());

      yield();
      
      // Tạo BagConfig mới từ đơn hàng
      BagConfig newConfig;
      // Sử dụng productCode để đảm bảo unique cho từng đơn hàng
      productCode = doc["product"]["code"].as<String>();  // Cập nhật biến global
      if (productCode.length() == 0) {
        productCode = String(millis()); // Fallback nếu không có productCode
      }
      Serial.println("Updated global productCode: " + productCode);
      newConfig.type = productName + "_" + productCode; // Sử dụng productCode để unique
      newConfig.target = quantity;
      newConfig.warn = warningQuantity;
      newConfig.status = "WAIT";
      
      Serial.println("Created bagConfig with type: " + newConfig.type + " (productCode: " + productCode + ")");
      
      // LUÔN LUÔN THÊM MỚI ĐƠN HÀNG
      bagConfigs.push_back(newConfig);
      Serial.println("Added new bag config: " + newConfig.type + " (Total configs: " + String(bagConfigs.size()) + ")");
      
      yield();
      
      // Thêm vào bagTypes nếu chưa có
      if (std::find(bagTypes.begin(), bagTypes.end(), productName) == bagTypes.end()) {
        bagTypes.push_back(productName);
        saveBagTypesToFile();
      }
      
      // Lưu cấu hình
      saveBagConfigsToFile();
      
      // CẬP NHẬT bagType để hiển thị tên sản phẩm mới
      bagType = productName;
      Serial.println("Updated bagType to: " + bagType);
      
      Serial.println("New order saved to ESP32:");
      Serial.println("Customer: " + customerName);
      Serial.println("Order Code: " + orderCode);
      Serial.println("Vehicle: " + vehicleNumber);
      Serial.println("Product: " + productName);
      Serial.println("Quantity: " + String(quantity));
      Serial.println("Warning: " + String(warningQuantity));
      
      // Check memory after processing
      Serial.println("Free heap after processing: " + String(ESP.getFreeHeap()) + " bytes");
      
      server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Order saved to ESP32\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"No data provided\"}");
    }
  });

  // API kích hoạt batch - nhận thông tin batch từ web
  server.on("/api/activate_batch", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    
    if(server.hasArg("plain")) {
      String body = server.arg("plain");
      Serial.println("Activating batch, received data: " + body);
      
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, body);
      
      if (!error) {
        // Lưu thông tin batch
        currentBatchName = doc["batchName"].as<String>();
        currentBatchId = doc["batchId"].as<String>();
        currentBatchDescription = doc["batchDescription"].as<String>();
        totalOrdersInBatch = doc["totalOrders"].as<int>();
        batchTotalTarget = doc["batchTotalTarget"].as<int>();  // Nhận tổng target của batch
        
        Serial.println("Batch activated:");
        Serial.println("Name: " + currentBatchName);
        Serial.println("ID: " + currentBatchId);
        Serial.println("Description: " + currentBatchDescription);
        Serial.println("Total Orders: " + String(totalOrdersInBatch));
        Serial.println("Batch Total Target: " + String(batchTotalTarget));
        
        // RESET TRẠNG THÁI KHI CHUYỂN BATCH MỚI
        totalCount = 0;
        isLimitReached = false;
        isRunning = false;
        isTriggerEnabled = false;
        isCountingEnabled = false;
        
        // LƯU THÔNG TIN BATCH VÀO FILE
        saveBatchInfoToFile();
        
        // In thông báo đã chọn batch
        Serial.println("Batch displayed: " + currentBatchName);
        
        // Gửi response thành công
        DynamicJsonDocument response(512);
        response["status"] = "OK";
        response["message"] = "Batch activated successfully";
        response["batchName"] = currentBatchName;
        response["totalOrders"] = totalOrdersInBatch;
        response["batchTotalTarget"] = batchTotalTarget;  // Thêm vào response
        
        String responseStr;
        serializeJson(response, responseStr);
        server.send(200, "application/json", responseStr);
        
      } else {
        Serial.println("JSON parse error: " + String(error.c_str()));
        server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid JSON\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"No data provided\"}");
    }
  });

  // Handle CORS preflight for activate_batch
  server.on("/api/activate_batch", HTTP_OPTIONS, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200);
  });

  // API trả về danh sách batch đã lưu - để web load khi reload
  server.on("/api/batches", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    DynamicJsonDocument doc(2048);
    JsonArray batchArray = doc.to<JsonArray>();
    
    // Nếu có batch hiện tại, thêm vào danh sách
    if (currentBatchName != "") {
      JsonObject batch = batchArray.createNestedObject();
      batch["id"] = currentBatchId;
      batch["name"] = currentBatchName;
      batch["description"] = currentBatchDescription;
      batch["totalOrders"] = totalOrdersInBatch;
      batch["batchTotalTarget"] = batchTotalTarget;  // Thêm tổng target
      batch["isActive"] = true;  // Batch hiện tại luôn là active
      batch["createdAt"] = "ESP32_STORED";
      
      Serial.println("Returning current batch: " + currentBatchName);
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // API cài đặt chung - Trả về giá trị hiện tại (từ biến global)
  server.on("/api/settings", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    DynamicJsonDocument doc(1024);
    
    // Trả về giá trị hiện tại từ biến global (đã được load từ file)
    doc["conveyorName"] = conveyorName;
    doc["location"] = location;
    doc["ipAddress"] = currentNetworkMode == ETHERNET_MODE ? ETH.localIP().toString() : WiFi.localIP().toString();
    doc["gateway"] = gateway.toString();
    doc["subnet"] = subnet.toString();
    doc["sensorDelay"] = sensorDelayMs;
    doc["bagDetectionDelay"] = bagDetectionDelay;
    doc["minBagInterval"] = minBagInterval;
    doc["autoReset"] = autoReset;
    doc["brightness"] = displayBrightness;
    doc["relayDelayAfterComplete"] = relayDelayAfterComplete;
    doc["bagTimeMultiplier"] = bagTimeMultiplier;
    
    doc["realtimePort"] = REALTIME_WS_PORT;
    doc["realtimePath"] = "/ws";
    
    // MQTT2 settings (Server anh Dũng)
    doc["mqtt2Server"] = mqtt_server2;
    doc["mqtt2Port"] = mqtt_port2;
    doc["mqtt2Username"] = mqtt2_username;
    doc["mqtt2Password"] = mqtt2_password;
    doc["_mqtt2Connected"] = mqtt2.connected();
    
    // Weight-based Detection Delay settings
    doc["enableWeightBasedDelay"] = enableWeightBasedDelay;
    JsonArray weightRulesArray = doc.createNestedArray("weightDelayRules");
    for (const auto& rule : weightDelayRules) {
      JsonObject ruleObj = weightRulesArray.createNestedObject();
      ruleObj["weight"] = rule.weight;
      ruleObj["delay"] = rule.delay;
    }
    
    // Sensor timing measurement info
    doc["lastMeasuredTime"] = lastMeasuredTime;
    doc["isMeasuringSensor"] = isMeasuringSensor;
    int sensorReading = digitalRead(SENSOR_PIN);
    doc["sensorCurrentState"] = sensorRawStateName(sensorReading);
    doc["sensorBlocked"] = isSensorBlocked(sensorReading);
    doc["sensorActiveLevel"] = "LOW";
    
    // Add debug info about settings source
    doc["_debug"] = LittleFS.exists("/settings.json") ? "file" : "defaults";
    doc["_settingsFileExists"] = LittleFS.exists("/settings.json");
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/settings", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, server.arg("plain"));
      
      Serial.println("Receiving settings from web, applying and saving...");
      
      // ÁP DỤNG SETTINGS NGAY LẬP TỨC VÀO BIẾN GLOBAL
      if (doc.containsKey("conveyorName")) {
        String oldValue = conveyorName;
        conveyorName = doc["conveyorName"].as<String>();
        Serial.println("  conveyorName: '" + oldValue + "' → '" + conveyorName + "'");
      }
      
      if (doc.containsKey("location")) {
        String oldValue = location;
        location = doc["location"].as<String>();
        Serial.println("  location: '" + oldValue + "' → '" + location + "'");
      }
      
      if (doc.containsKey("brightness")) {
        int oldValue = displayBrightness;
        displayBrightness = doc["brightness"];
        if (displayBrightness >= 10 && displayBrightness <= 100) {
          dma_display->setBrightness8(map(displayBrightness, 0, 100, 0, 255));
          Serial.println("  brightness: " + String(oldValue) + "% → " + String(displayBrightness) + "%");
        }
      }
      
      if (doc.containsKey("sensorDelay")) {
        int oldValue = sensorDelayMs;
        sensorDelayMs = doc["sensorDelay"];
        debounceDelay = sensorDelayMs;
        Serial.println("  sensorDelay: " + String(oldValue) + "ms → " + String(sensorDelayMs) + "ms");
      }
      
      if (doc.containsKey("bagDetectionDelay")) {
        int oldValue = ::bagDetectionDelay;
        ::bagDetectionDelay = doc["bagDetectionDelay"]; // Sử dụng :: để chỉ biến global
        Serial.println("  bagDetectionDelay: " + String(oldValue) + "ms → " + String(::bagDetectionDelay) + "ms");
      }
      
      if (doc.containsKey("minBagInterval")) {
        int oldValue = ::minBagInterval;
        ::minBagInterval = doc["minBagInterval"];
        Serial.println("  minBagInterval: " + String(oldValue) + "ms → " + String(::minBagInterval) + "ms");
      }
      
      if (doc.containsKey("autoReset")) {
        bool oldValue = ::autoReset;
        ::autoReset = doc["autoReset"];
        Serial.println("  autoReset: " + String(oldValue ? "true" : "false") + " → " + String(::autoReset ? "true" : "false"));
      }
      
      if (doc.containsKey("relayDelayAfterComplete")) {
        int oldValue = ::relayDelayAfterComplete;
        ::relayDelayAfterComplete = doc["relayDelayAfterComplete"];
        Serial.println("  relayDelayAfterComplete: " + String(oldValue) + "ms → " + String(::relayDelayAfterComplete) + "ms");
      }
      
      if (doc.containsKey("bagTimeMultiplier")) {
        int oldValue = ::bagTimeMultiplier;
        ::bagTimeMultiplier = doc["bagTimeMultiplier"];
        Serial.println("  bagTimeMultiplier: " + String(oldValue) + "% → " + String(::bagTimeMultiplier) + "%");
      }
      
      // MQTT2 settings (Server anh Dũng)
      bool mqtt2NeedReconnect = false;
      if (doc.containsKey("mqtt2Server")) {
        String oldValue = mqtt_server2;
        mqtt_server2 = doc["mqtt2Server"].as<String>();
        if (oldValue != mqtt_server2) {
          mqtt2NeedReconnect = true;
          Serial.println("  mqtt2Server: '" + oldValue + "' → '" + mqtt_server2 + "'");
        }
      }
      
      if (doc.containsKey("mqtt2Port")) {
        int oldValue = mqtt_port2;
        mqtt_port2 = doc["mqtt2Port"].as<int>();
        if (oldValue != mqtt_port2) {
          mqtt2NeedReconnect = true;
          Serial.println("  mqtt2Port: " + String(oldValue) + " → " + String(mqtt_port2));
        }
      }
      
      if (doc.containsKey("mqtt2Username")) {
        String oldValue = mqtt2_username;
        mqtt2_username = doc["mqtt2Username"].as<String>();
        if (oldValue != mqtt2_username) {
          mqtt2NeedReconnect = true;
          Serial.println("  mqtt2Username: '" + oldValue + "' → '" + mqtt2_username + "'");
        }
      }
      
      if (doc.containsKey("mqtt2Password")) {
        String oldValue = mqtt2_password;
        mqtt2_password = doc["mqtt2Password"].as<String>();
        if (oldValue != mqtt2_password) {
          mqtt2NeedReconnect = true;
          Serial.println("  mqtt2Password: [CHANGED]");
        }
      }
      
      // Weight-based Detection Delay settings
      if (doc.containsKey("enableWeightBasedDelay")) {
        bool oldValue = enableWeightBasedDelay;
        enableWeightBasedDelay = doc["enableWeightBasedDelay"].as<bool>();
        Serial.println("  enableWeightBasedDelay: " + String(oldValue ? "true" : "false") + " → " + String(enableWeightBasedDelay ? "true" : "false"));
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
        
        Serial.println("  weightDelayRules: Updated " + String(weightDelayRules.size()) + " rules (sorted by weight)");
        
        // Log các rules để debug
        for (size_t i = 0; i < weightDelayRules.size(); i++) {
          Serial.println("    Rule " + String(i+1) + ": " + String(weightDelayRules[i].weight) + "kg → " + String(weightDelayRules[i].delay) + "ms");
        }
      }
      
      // Cấu hình IP tĩnh Ethernet
      String ethIP = doc["ipAddress"];
      String ethGateway = doc["gateway"];
      String ethSubnet = doc["subnet"];
      String ethDNS1 = doc["dns1"];
      String ethDNS2 = doc["dns2"];
      
      // Cập nhật IP tĩnh Ethernet nếu có thay đổi
      bool needRestart = false;
      if (ethIP.length() > 0 && ethGateway.length() > 0 && ethSubnet.length() > 0) {
        IPAddress newIP, newGateway, newSubnet, newDNS1, newDNS2;
        if (newIP.fromString(ethIP) && newGateway.fromString(ethGateway) && newSubnet.fromString(ethSubnet)) {
          // Kiểm tra xem có thay đổi không
          if (local_IP != newIP || gateway != newGateway || subnet != newSubnet) {
            local_IP = newIP;
            gateway = newGateway; 
            subnet = newSubnet;
            needRestart = true;
            
            Serial.println("  Network config changed:");
            Serial.println("    New IP: " + ethIP);
            Serial.println("    New Gateway: " + ethGateway);
            Serial.println("    New Subnet: " + ethSubnet);
          }
          
          if (ethDNS1.length() > 0) newDNS1.fromString(ethDNS1);
          else newDNS1 = IPAddress(8, 8, 8, 8);
          
          if (ethDNS2.length() > 0) newDNS2.fromString(ethDNS2);
          else newDNS2 = IPAddress(8, 8, 4, 4);
          
          primaryDNS = newDNS1;
          secondaryDNS = newDNS2;
        }
      }
      
      // LƯU TẤT CẢ SETTINGS VÀO FILE
      saveSettingsToFile();
      
      Serial.println("Settings updated and saved permanently:");
      Serial.println("  - Conveyor Name: " + conveyorName);
      Serial.println("  - Location: " + location);
      Serial.println("  - Brightness: " + String(displayBrightness) + "%");
      Serial.println("  - Sensor Delay: " + String(sensorDelayMs) + "ms");
      Serial.println("  - Bag Detection Delay: " + String(::bagDetectionDelay) + "ms");
      Serial.println("  - Min Bag Interval: " + String(::minBagInterval) + "ms");
      Serial.println("  - Auto Reset: " + String(::autoReset ? "true" : "false"));
      Serial.println("  - Relay Delay After Complete: " + String(::relayDelayAfterComplete) + "ms");
      if (ethIP.length() > 0) {
        Serial.println("  - Ethernet IP: " + ethIP);
      }
      
      // Reconnect MQTT2 if needed - CHỈ KHI CÓ INTERNET
      if (mqtt2NeedReconnect && currentNetworkMode != WIFI_AP_MODE) {
        Serial.println("MQTT2 configuration changed, reconnecting...");
        Serial.println("  New MQTT2 broker: " + mqtt_server2 + ":" + String(mqtt_port2));
        Serial.println("  Username: " + mqtt2_username);
        Serial.println("  KeyLogin: [SET]");
        
        if (mqtt2.connected()) {
          mqtt2.disconnect();
        }
        // Will reconnect via setupMQTT2() in main loop
        setupMQTT2();
      } else if (mqtt2NeedReconnect && currentNetworkMode == WIFI_AP_MODE) {
        Serial.println("MQTT2 (AP mode)");
      }
      
      // Trả về response với thông báo restart nếu cần
      DynamicJsonDocument response(256);
      response["status"] = "OK";
      if (needRestart) {
        response["message"] = "Settings saved. Restart required for IP changes.";
        response["needRestart"] = true;
      } else {
        response["message"] = "Settings saved successfully";
        response["needRestart"] = false;
      }
      
      String out;
      serializeJson(response, out);
      server.send(200, "application/json", out);
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"No data provided\"}");
    }
  });

  // Individual setting endpoints - DEPRECATED: Sử dụng MQTT config thay thế
  // server.on("/brightness", HTTP_GET, [](){
  //   // Đã chuyển sang MQTT: bagcounter/config/update {"brightness": 50}
  // });
  
  // server.on("/sensorDelay", HTTP_GET, [](){
  //   // Đã chuyển sang MQTT: bagcounter/config/update {"sensorDelay": 50}
  // });
  
  // server.on("/bagDetectionDelay", HTTP_GET, [](){
  //   // Đã chuyển sang MQTT: bagcounter/config/update {"bagDetectionDelay": 200}
  // });
  
  // server.on("/minBagInterval", HTTP_GET, [](){
  //   // Đã chuyển sang MQTT: bagcounter/config/update {"minBagInterval": 100}
  // });

  // API cập nhật số đếm từ web - DEPRECATED: Chỉ sử dụng MQTT
  // server.on("/api/update_count", HTTP_POST, [](){
  //   // Đã chuyển sang MQTT real-time updates
  // });

  // API lấy trạng thái mở rộng - DEPRECATED: Merge vào /api/status
  // server.on("/api/extended_status", HTTP_GET, [](){
  //   // Đã merge vào /api/status với đầy đủ thông tin
  // });

  // WiFi configuration endpoints
  server.on("/api/wifi/scan", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Connection", "close");
    
    Serial.println("WiFi scan requested");
    
    // Ensure WiFi is initialized for scanning
    if (WiFi.getMode() == WIFI_OFF) {
      Serial.println("  Initializing WiFi for scan...");
      WiFi.mode(WIFI_STA);
      delay(100); // Allow WiFi to initialize
    }
    
    Serial.println("  Scanning networks...");
    unsigned long scanStart = millis();
    int n = WiFi.scanNetworks(false, false, false, 200); // Giảm timeout xuống 200ms mỗi channel
    unsigned long scanDuration = millis() - scanStart;
    Serial.println("Scan completed in " + String(scanDuration) + "ms");
    
    if (n == -1) {
      Serial.println("WiFi scan failed");
      String errorResponse = "{\"error\":\"WiFi scan failed\",\"networks\":[]}";
      server.send(500, "application/json", errorResponse);
      return;
    }
    
    if (n == 0) {
      Serial.println("No networks found");
      String emptyResponse = "{\"networks\":[]}";
      server.send(200, "application/json", emptyResponse);
      return;
    }
    
    Serial.println("  Found " + String(n) + " networks");
    
    // Tạo response từng phần để tránh buffer overflow
    String response = "{\"networks\":[";
    
    int maxNetworks = min(n, 6); // Giảm xuống 6 networks để response nhanh hơn
    for (int i = 0; i < maxNetworks; i++) {
      if (i > 0) response += ",";
      
      String ssid = WiFi.SSID(i);
      // Clean SSID - remove problematic characters
      ssid.replace("\"", "");
      ssid.replace("\\", "");
      ssid.replace("\n", "");
      ssid.replace("\r", "");
      
      // Keep response minimal
      response += "{";
      response += "\"ssid\":\"" + ssid + "\",";
      response += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
      response += "\"encrypted\":" + String((WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "true" : "false");
      response += "}";
      
      Serial.println("    " + ssid + " (" + String(WiFi.RSSI(i)) + " dBm)");
      
      // Check if response is getting too long
      if (response.length() > 600) { // Giảm limit xuống 600 bytes
        Serial.println("  Response getting too long, truncating at " + String(i+1) + " networks");
        break;
      }
    }
    
    response += "]}";
    
    Serial.println("Sending WiFi scan response (length: " + String(response.length()) + ")");
    
    // Send response với immediate flush
    server.sendHeader("Content-Length", String(response.length()));
    server.send(200, "application/json", response);
    server.client().flush(); // Force flush response
    
    // Clean up scan results
    WiFi.scanDelete();
    
    Serial.println("WiFi scan response sent and flushed successfully");
  });

  server.on("/api/wifi/connect", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, server.arg("plain"));
      
  String ssid = doc["ssid"].as<String>();
  String password = doc["password"].as<String>();
      bool useStaticIP = doc["use_static_ip"] | false;
      String staticIP = doc["static_ip"];
      String gateway = doc["gateway"];
      String subnet = doc["subnet"];
      String dns1 = doc["dns1"];
      String dns2 = doc["dns2"];
      
      if (ssid.length() > 0) {
        // Save WiFi config first
        saveWiFiConfig(ssid, password, useStaticIP, staticIP, gateway, subnet, dns1, dns2);
        
        // Update global variables
        wifi_ssid = ssid;
        wifi_password = password;
        wifi_use_static_ip = useStaticIP;
        if (useStaticIP && staticIP.length() > 0) {
          wifi_static_ip.fromString(staticIP);
          if (gateway.length() > 0) wifi_gateway.fromString(gateway);
          if (subnet.length() > 0) wifi_subnet.fromString(subnet);
          if (dns1.length() > 0) wifi_dns1.fromString(dns1);
          if (dns2.length() > 0) wifi_dns2.fromString(dns2);
        }
        
        // Send immediate response to avoid timeout
        DynamicJsonDocument response(256);
        response["success"] = true;
        response["message"] = "WiFi config saved. Attempting connection...";
        response["status"] = "connecting";
        
        String out;
        serializeJson(response, out);
        server.send(200, "application/json", out);
        
        // Delay a bit to ensure response is sent
        delay(100);
        
        // Now try to connect in background
        Serial.println("Attempting WiFi connection to: " + ssid);
        
        // Configure WiFi
        WiFi.mode(WIFI_STA);
        
        // Configure static IP if enabled
        if (wifi_use_static_ip) {
          Serial.println("Configuring static IP: " + wifi_static_ip.toString());
          if (!WiFi.config(wifi_static_ip, wifi_gateway, wifi_subnet, wifi_dns1, wifi_dns2)) {
            Serial.println("Failed to configure static IP");
          }
        }
        
        WiFi.begin(ssid.c_str(), password.c_str());
        
        // Check connection in background (non-blocking)
        unsigned long startTime = millis();
        bool connected = false;
        while (millis() - startTime < 15000) {
          if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            break;
          }
          delay(500);
          Serial.print(".");
        }
        
        if (connected) {
          currentNetworkMode = WIFI_STA_MODE;
          wifiConnected = true;
          Serial.println();
          Serial.println("WiFi connected successfully!");
          Serial.print("IP: ");
          Serial.println(WiFi.localIP());
          Serial.print("Gateway: ");
          Serial.println(WiFi.gatewayIP());
          Serial.print("Subnet: ");
          Serial.println(WiFi.subnetMask());
        } else {
          Serial.println();
          Serial.println("WiFi connection failed, reverting to AP mode");
          // Revert to AP mode if WiFi connection fails
          setupWiFiAP();
        }
        
      } else {
        server.send(400, "application/json", "{\"error\":\"SSID required\"}");
      }
    } else {
      server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    }
  });

  server.on("/api/network/status", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache");
    
    // Tạo response nhỏ gọn bằng String thay vì ArduinoJson
    String response = "{";
    response += "\"ethernet_connected\":" + String(ethernetConnected ? "true" : "false") + ",";
    response += "\"wifi_connected\":" + String(wifiConnected ? "true" : "false") + ",";
    
    if (currentNetworkMode == ETHERNET_MODE) {
      response += "\"current_mode\":\"ethernet\"";
      if (ethernetConnected) {
        response += ",\"ip\":\"" + ETH.localIP().toString() + "\"";
        response += ",\"gateway\":\"" + ETH.gatewayIP().toString() + "\"";
      }
    } else if (currentNetworkMode == WIFI_STA_MODE) {
      response += "\"current_mode\":\"wifi_sta\"";
      if (wifiConnected) {
        response += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
        response += ",\"ssid\":\"" + WiFi.SSID() + "\"";
      }
    } else if (currentNetworkMode == WIFI_AP_MODE) {
      response += "\"current_mode\":\"wifi_ap\"";
      response += ",\"ip\":\"" + WiFi.softAPIP().toString() + "\"";
      response += ",\"ap_ssid\":\"" + String(ap_ssid) + "\"";
    }
    
    response += "}";
    
    server.send(200, "application/json", response);
    Serial.println("Network status API - response length: " + String(response.length()));
  });

  // API restart ESP32 để áp dụng cấu hình IP mới
  server.on("/api/restart", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    DynamicJsonDocument doc(256);
    doc["status"] = "OK";
    doc["message"] = "ESP32 will restart in 2 seconds";
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
    
    Serial.println("Restart requested from web - restarting in 2 seconds...");
    delay(2000);
    ESP.restart();
  });

  // API để force restart ethernet connection
  server.on("/api/restart_ethernet", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    // Load settings từ file
    if (LittleFS.exists("/settings.json")) {
      File file = LittleFS.open("/settings.json", "r");
      if (file) {
        String content = file.readString();
        file.close();
        
        DynamicJsonDocument doc(1024);
        if (deserializeJson(doc, content) == DeserializationError::Ok) {
          String ethIP = doc["ipAddress"];
          String ethGateway = doc["gateway"];
          String ethSubnet = doc["subnet"];
          
          if (ethIP.length() > 0) {
            IPAddress newIP, newGateway, newSubnet;
            if (newIP.fromString(ethIP) && newGateway.fromString(ethGateway) && newSubnet.fromString(ethSubnet)) {
              local_IP = newIP;
              gateway = newGateway;
              subnet = newSubnet;
              
              Serial.println("Applying new Ethernet config:");
              Serial.println("IP: " + ethIP);
              Serial.println("Gateway: " + ethGateway);
              Serial.println("Subnet: " + ethSubnet);
            }
          }
        }
      }
    }
    
    server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Restarting with new IP config\"}");
    delay(1000);
    ESP.restart();
  });

  // API để kiểm tra kết quả kết nối WiFi
  server.on("/api/wifi/status", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    DynamicJsonDocument doc(256);
    doc["wifi_connected"] = wifiConnected;
    doc["current_mode"] = (currentNetworkMode == ETHERNET_MODE) ? "ethernet" : 
                         (currentNetworkMode == WIFI_STA_MODE) ? "wifi_sta" : "wifi_ap";
    
    if (currentNetworkMode == WIFI_STA_MODE && wifiConnected) {
      doc["success"] = true;
      doc["ip"] = WiFi.localIP().toString();
      doc["gateway"] = WiFi.gatewayIP().toString();
      doc["subnet"] = WiFi.subnetMask().toString();
      doc["ssid"] = WiFi.SSID();
      doc["use_static_ip"] = wifi_use_static_ip;
      doc["message"] = "WiFi connected successfully";
    } else if (currentNetworkMode == WIFI_AP_MODE) {
      doc["success"] = false;
      doc["message"] = "WiFi connection failed, AP mode active";
      doc["ap_ip"] = WiFi.softAPIP().toString();
    } else {
      doc["success"] = false;
      doc["message"] = "WiFi connection in progress or failed";
    }
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // MQTT Control API
  server.on("/api/mqtt/status", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    DynamicJsonDocument doc(512);
    doc["connected"] = realtimeSocket.count() > 0;
    doc["transport"] = "websocket";
    doc["port"] = REALTIME_WS_PORT;
    doc["path"] = "/ws";
    doc["last_publish"] = lastMqttPublish;
    doc["last_heartbeat"] = lastHeartbeat;
    
    // Topic structure
    JsonObject topics = doc.createNestedObject("topics");
    JsonArray publish_topics = topics.createNestedArray("publish");
    publish_topics.add(TOPIC_STATUS);
    publish_topics.add(TOPIC_COUNT);
    publish_topics.add(TOPIC_ALERTS);
    publish_topics.add(TOPIC_SENSOR);
    publish_topics.add(TOPIC_HEARTBEAT);
    publish_topics.add(TOPIC_IR_CMD);
    
    JsonArray subscribe_topics = topics.createNestedArray("subscribe");
    subscribe_topics.add(TOPIC_CMD_START);
    subscribe_topics.add(TOPIC_CMD_PAUSE);
    subscribe_topics.add(TOPIC_CMD_RESET);
    subscribe_topics.add(TOPIC_CMD_BATCH);
    subscribe_topics.add(TOPIC_CONFIG);
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/mqtt/publish", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, server.arg("plain"));
      
      String topic = doc["topic"];
      String message = doc["message"];
      
      if (topic.length() > 0 && message.length() > 0) {
        broadcastRealtimeMessage(topic.c_str(), message);
        bool success = true;
        
        DynamicJsonDocument response(256);
        response["success"] = success;
        response["topic"] = topic;
        response["message"] = message;
        response["timestamp"] = millis();
        
        String out;
        serializeJson(response, out);
        server.send(200, "application/json", out);
        
        Serial.println("Manual MQTT publish: " + topic + " = " + message);
      } else {
        server.send(400, "application/json", "{\"error\":\"Topic and message required\"}");
      }
    } else {
      server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    }
  });

  server.on("/api/mqtt/force_publish", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    // Force publish tất cả dữ liệu hiện tại
    publishStatusMQTT();
    publishCountUpdate();
    publishSensorData();
    publishHeartbeat();
    
    DynamicJsonDocument response(256);
    response["success"] = true;
    response["message"] = "All realtime topics published";
    response["timestamp"] = millis();
    
    String out;
    serializeJson(response, out);
    server.send(200, "application/json", out);
    
    Serial.println(" Force published all MQTT topics");
  });

  // test page
  server.on("/test", HTTP_GET, [](){
    String html = "<!DOCTYPE html><html><head><title>Test Page</title></head><body>";
    html += "<h1>Test Page Working!</h1>";
    html += "<p>AP IP: " + WiFi.softAPIP().toString() + "</p>";
    html += "<p>Free Heap: " + String(ESP.getFreeHeap()) + "</p>";
    html += "<p>Uptime: " + String(millis()) + "ms</p>";
    html += "<button onclick=\"testWiFiScan()\">Test WiFi Scan</button>";
    html += "<div id=\"scanResult\"></div>";
    html += "<script>";
    html += "function testWiFiScan() {";
    html += "  const startTime = Date.now();";
    html += "  document.getElementById('scanResult').innerHTML = 'Scanning...';";
    html += "  fetch('/api/wifi/scan')";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      const duration = Date.now() - startTime;";
    html += "      document.getElementById('scanResult').innerHTML = ";
    html += "        '<p>Scan completed in ' + duration + 'ms</p>' +";
    html += "        '<p>Found ' + (data.networks ? data.networks.length : 0) + ' networks</p>';";
    html += "    })";
    html += "    .catch(error => {";
    html += "      const duration = Date.now() - startTime;";
    html += "      document.getElementById('scanResult').innerHTML = ";
    html += "        '<p>Error after ' + duration + 'ms: ' + error.message + '</p>';";
    html += "    });";
    html += "}";
    html += "</script>";
    html += "</body></html>";
    server.send(200, "text/html", html);
    Serial.println("Served test page with WiFi scan test");
  });

  // API debug/test cho việc lưu trữ dữ liệu
  server.on("/api/debug/storage", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    DynamicJsonDocument doc(2048);
    
    // Thông tin files
    doc["files"]["settings"] = LittleFS.exists("/settings.json");
    doc["files"]["products"] = LittleFS.exists("/products.json");
    doc["files"]["orders"] = LittleFS.exists("/orders.json");
    doc["files"]["bag_types"] = LittleFS.exists("/bag_types.json");
    doc["files"]["bag_configs"] = LittleFS.exists("/bag_configs.json");
    
    // Thông tin dữ liệu trong memory
    doc["memory"]["products_count"] = productsData.size();
    doc["memory"]["orders_count"] = ordersData.size();
    doc["memory"]["bag_types_count"] = bagTypes.size();
    doc["memory"]["bag_configs_count"] = bagConfigs.size();
    
    // Current settings
    doc["current_settings"]["conveyor_name"] = conveyorName;
    doc["current_settings"]["brightness"] = displayBrightness;
    doc["current_settings"]["sensor_delay"] = sensorDelayMs;
    doc["current_settings"]["bag_detection_delay"] = bagDetectionDelay;
    doc["current_settings"]["min_bag_interval"] = minBagInterval;
    doc["current_settings"]["auto_reset"] = autoReset;
    doc["current_settings"]["bag_time_multiplier"] = bagTimeMultiplier;
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  server.on("/api/debug/reload", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    // Reload tất cả dữ liệu từ LittleFS
    Serial.println("Reloading all data from LittleFS...");
    loadSettingsFromFile();
    loadProductsFromFile();
    loadOrdersFromFile();
    loadBagTypesFromFile();
    loadBagConfigsFromFile();
    
    printDataStatus();
    
    server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"All data reloaded from storage\"}");
  });

  server.on("/api/debug/reset-products", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    Serial.println("Resetting products to default...");
    
    // Xóa file cũ
    if (LittleFS.exists("/products.json")) {
      LittleFS.remove("/products.json");
      Serial.println("Deleted /products.json");
    }
    
    // Tạo lại file trống
    productsData.clear();
    productsData.to<JsonArray>(); // Tạo array rỗng
    saveProductsToFile();
    
    Serial.println("Products reset to empty");
    
    server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Products reset to empty\",\"count\":" + String(productsData.size()) + "}");
  });

  server.on("/api/debug/reset-orders", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    Serial.println("Resetting orders...");
    
    // Xóa file cũ
    if (LittleFS.exists("/orders.json")) {
      LittleFS.remove("/orders.json");
      Serial.println("Deleted /orders.json");
    }
    
    // Tạo lại array rỗng
    ordersData.clear();
    ordersData.to<JsonArray>(); // Tạo array rỗng
    saveOrdersToFile();
    
    Serial.println("Orders reset (empty)");
    
    server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Orders reset to empty\",\"count\":" + String(ordersData.size()) + "}");
  });

  server.on("/api/debug/settings", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    debugSettingsFile();
    
    DynamicJsonDocument doc(1024);
    doc["file_exists"] = LittleFS.exists("/settings.json");
    
    if (LittleFS.exists("/settings.json")) {
      File file = LittleFS.open("/settings.json", "r");
      if (file) {
        String content = file.readString();
        file.close();
        doc["file_content"] = content;
        
        DynamicJsonDocument fileDoc(1024);
        DeserializationError error = deserializeJson(fileDoc, content);
        doc["json_valid"] = (error == DeserializationError::Ok);
        if (error != DeserializationError::Ok) {
          doc["json_error"] = error.c_str();
        }
      }
    }
    
    doc["current_memory"]["conveyorName"] = conveyorName;
    doc["current_memory"]["brightness"] = displayBrightness;
    doc["current_memory"]["sensorDelay"] = sensorDelayMs;
    doc["current_memory"]["bagDetectionDelay"] = bagDetectionDelay;
    doc["current_memory"]["minBagInterval"] = minBagInterval;
    doc["current_memory"]["autoReset"] = autoReset;
    doc["current_memory"]["bagTimeMultiplier"] = bagTimeMultiplier;
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // ========== CAPTIVE PORTAL HANDLERS ==========
  // Xử lý các URL thường được sử dụng để phát hiện captive portal
  server.on("/generate_204", HTTP_GET, [](){
    // Android captive portal detection
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  });
  
  server.on("/fwlink", HTTP_GET, [](){
    // Microsoft captive portal detection
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  });
  
  server.on("/hotspot-detect.html", HTTP_GET, [](){
    // iOS captive portal detection
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  });
  
  server.on("/connecttest.txt", HTTP_GET, [](){
    // Windows captive portal detection
    server.send(200, "text/plain", "Microsoft Connect Test");
  });
  
  // API BỔ SUNG ĐỂ ĐỒNG BỘ VỚI WEB
  
  // API lưu/xóa toàn bộ products
  server.on("/api/products", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (server.hasArg("plain")) {
      String jsonData = server.arg("plain");
      Serial.println("Receiving ALL products from web: " + jsonData);
      
      // Parse JSON array
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, jsonData);
      
      if (error) {
        server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid JSON\"}");
        return;
      }
      
      // Xóa products hiện tại và thay thế bằng dữ liệu mới
      productsData.clear();
      
      if (doc.is<JsonArray>()) {
        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject obj : arr) {
          JsonObject newProduct = productsData.createNestedObject();
          newProduct["id"] = obj["id"];
          newProduct["code"] = obj["code"];
          newProduct["name"] = obj["name"];
          if (obj.containsKey("group")) {
            newProduct["group"] = obj["group"];
          }
          if (obj.containsKey("unitWeight")) {
            newProduct["unitWeight"] = obj["unitWeight"];
          }
          if (obj.containsKey("createdAt")) {
            newProduct["createdAt"] = obj["createdAt"];
          }
        }
      }
      
      // Lưu vào file
      saveProductsToFile();
      
      Serial.println("Saved " + String(productsData.size()) + " products to ESP32");
      server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Products saved\",\"count\":" + String(productsData.size()) + "}");
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"No data provided\"}");
    }
  });
  
  // API xóa product theo ID
  server.on("/api/products/*", HTTP_DELETE, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    String uri = server.uri();
    String productIdStr = uri.substring(uri.lastIndexOf('/') + 1);
    int productId = productIdStr.toInt();
    
    if (productId > 0) {
      Serial.println("Deleting product ID: " + String(productId));
      
      // Tìm và xóa product
      for (size_t i = 0; i < productsData.size(); i++) {
        if (productsData[i]["id"] == productId) {
          productsData.remove(i);
          saveProductsToFile();
          Serial.println("Product deleted successfully");
          server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Product deleted\"}");
          return;
        }
      }
      
      server.send(404, "application/json", "{\"status\":\"Error\",\"message\":\"Product not found\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid product ID\"}");
    }
  });
  
  // API lưu/xóa toàn bộ order batches
  server.on("/api/orders", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    // Check memory before processing
    size_t freeHeap = ESP.getFreeHeap();
    Serial.println("Free heap before orders processing: " + String(freeHeap) + " bytes");
    
    if (freeHeap < 8000) {  // Giảm từ 15000 xuống 8000 bytes
      Serial.println("Low memory detected, rejecting large orders request");
      server.send(507, "application/json", "{\"status\":\"Error\",\"message\":\"Insufficient memory for orders\"}");
      return;
    }
    
    if (server.hasArg("plain")) {
      String jsonData = server.arg("plain");
      Serial.println("Receiving ALL order batches from web...");
      Serial.println("Data size: " + String(jsonData.length()) + " chars");
      Serial.println("Data preview (first 300 chars): " + jsonData.substring(0, min(300, (int)jsonData.length())));
      
      // DEBUG: Print more of the data to see if there are different orders
      if (jsonData.length() > 300) {
        Serial.println("Data continuation (chars 300-600): " + jsonData.substring(300, min(600, (int)jsonData.length())));
      }
      if (jsonData.length() > 600) {
        Serial.println("Data continuation (chars 600-900): " + jsonData.substring(600, min(900, (int)jsonData.length())));
      }
      
      // Limit data size to prevent memory overflow
      if (jsonData.length() > 8000) {  // Tăng từ 6000 lên 8000 bytes
        Serial.println("Data too large, rejecting request");
        server.send(413, "application/json", "{\"status\":\"Error\",\"message\":\"Data too large\"}");
        return;
      }
      
      // Use smaller JSON document and check available memory
      size_t docSize = min(8192, (int)freeHeap / 2); // Use 1/2 of available memory instead of 1/3
      Serial.println("Using JSON document size: " + String(docSize) + " bytes");
      
      DynamicJsonDocument doc(docSize);
      DeserializationError error = deserializeJson(doc, jsonData);
      
      if (error) {
        Serial.println("JSON Parse Error: " + String(error.c_str()));
        server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid JSON: " + String(error.c_str()) + "\"}");
        return;
      }
      
      Serial.println("JSON parsed successfully");
      
      // Feed watchdog to prevent timeout
      yield();
      
      // Xóa orders hiện tại và thay thế bằng dữ liệu mới
      Serial.println("Clearing existing orders data...");
      ordersData.clear();
      
      if (doc.is<JsonArray>()) {
        JsonArray arr = doc.as<JsonArray>();
        Serial.println("Processing " + String(arr.size()) + " order batches...");
        
        int batchCount = 0;
        for (JsonObject obj : arr) {
          Serial.println("DEBUG: Processing batch " + String(batchCount + 1));
          Serial.println("   - Batch ID: " + String(obj["id"].as<long>()));
          Serial.println("   - Batch name: " + obj["name"].as<String>());
          
          // Check orders array BEFORE copy
          if (obj.containsKey("orders") && obj["orders"].is<JsonArray>()) {
            JsonArray orders = obj["orders"];
            Serial.println("   - Orders count in this batch: " + String(orders.size()));
            
            // Log first few orders FROM ORIGINAL JSON
            Serial.println("   - DEBUG: Orders from ORIGINAL JSON:");
            for (size_t i = 0; i < min(3, (int)orders.size()); i++) {
              JsonObject order = orders[i];
              String orderCode = order["orderCode"].as<String>();
              String productName = order["productName"].as<String>();
              String productCode = "";
              if (order.containsKey("product") && order["product"].containsKey("code")) {
                productCode = order["product"]["code"].as<String>();
              }
              int quantity = order["quantity"].as<int>();
              int warningQuantity = order["warningQuantity"].as<int>();
              
              Serial.println("     Order " + String(i + 1) + ": orderCode=" + orderCode + 
                           ", productName=" + productName + ", productCode=" + productCode +
                           ", qty=" + String(quantity) + ", warning=" + String(warningQuantity));
            }
          } else {
            Serial.println("   - No orders array found or empty!");
          }
          
          JsonObject newBatch = ordersData.createNestedObject();
          copyJsonObject(obj, newBatch);
          
          // CHECK orders array AFTER copy
          if (newBatch.containsKey("orders") && newBatch["orders"].is<JsonArray>()) {
            JsonArray copiedOrders = newBatch["orders"];
            Serial.println("   - DEBUG: Orders AFTER copy to ordersData:");
            for (size_t i = 0; i < min(3, (int)copiedOrders.size()); i++) {
              JsonObject copiedOrder = copiedOrders[i];
              String orderCode = copiedOrder["orderCode"].as<String>();
              String productName = copiedOrder["productName"].as<String>();
              String productCode = "";
              if (copiedOrder.containsKey("product") && copiedOrder["product"].containsKey("code")) {
                productCode = copiedOrder["product"]["code"].as<String>();
              }
              int quantity = copiedOrder["quantity"].as<int>();
              int warningQuantity = copiedOrder["warningQuantity"].as<int>();
              
              Serial.println("     Copied Order " + String(i + 1) + ": orderCode=" + orderCode + 
                           ", productName=" + productName + ", productCode=" + productCode +
                           ", qty=" + String(quantity) + ", warning=" + String(warningQuantity));
            }
          }
          
          // Feed watchdog every few batches to prevent timeout
          if (++batchCount % 2 == 0) {
            yield();
            Serial.println("Processed " + String(batchCount) + " batches...");
          }
        }
        
        Serial.println("All batches copied to ordersData");
      } else {
        Serial.println("Data is not a JSON array");
        server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Data must be an array\"}");
        return;
      }
      
      // Feed watchdog before file save
      yield();
      
      // Lưu vào file
      Serial.println("Saving orders to file...");
      saveOrdersToFile();
      
      // Check memory after processing
      Serial.println("Free heap after orders processing: " + String(ESP.getFreeHeap()) + " bytes");
      
      Serial.println("Saved " + String(ordersData.size()) + " order batches to ESP32");
      server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Orders saved\",\"count\":" + String(ordersData.size()) + "}");
    } else {
      Serial.println("No POST data received");
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"No data provided\"}");
    }
  });
  
  // API xóa batch theo ID
  server.on("/api/orders/*", HTTP_DELETE, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    String uri = server.uri();
    String batchIdStr = uri.substring(uri.lastIndexOf('/') + 1);
    int batchId = batchIdStr.toInt();
    
    if (batchId > 0) {
      Serial.println("Deleting batch ID: " + String(batchId));
      
      // Tìm và xóa batch
      for (size_t i = 0; i < ordersData.size(); i++) {
        if (ordersData[i]["id"] == batchId) {
          ordersData.remove(i);
          saveOrdersToFile();
          Serial.println("Batch deleted successfully");
          server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Batch deleted\"}");
          return;
        }
      }
      
      server.send(404, "application/json", "{\"status\":\"Error\",\"message\":\"Batch not found\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid batch ID\"}");
    }
  });
  
  // API cập nhật đơn được chọn
  server.on("/api/select_orders", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(8192);
      DeserializationError error = deserializeJson(doc, server.arg("plain"));
      
      if (error) {
        Serial.println("JSON parsing failed: " + String(error.c_str()));
        server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid JSON\"}");
        return;
      }
      
      String batchId = doc["batchId"].as<String>();
      JsonArray selectedOrders = doc["selectedOrders"];
      int selectedCount = doc["selectedCount"] | (int)selectedOrders.size();
      JsonObject firstSelectedOrder = doc["firstSelectedOrder"];
      JsonArray selectedOrdersDetails = doc["selectedOrdersDetails"];
      JsonObject activeCountingOrder = doc["activeCountingOrder"];
      
      Serial.println("Updating selected orders for batch: " + batchId);
      Serial.println("Selected orders count: " + String(selectedOrders.size()));
      Serial.println("Selected count (from web): " + String(selectedCount));
      
      // DEBUG: In ra danh sách ID được chọn
      Serial.print("Selected order IDs: ");
      for (JsonVariant selectedId : selectedOrders) {
        Serial.print(String(selectedId.as<int>()) + " ");
      }
      Serial.println();
      
      // Tìm batch và cập nhật selected status
      bool batchFound = false;
      for (size_t i = 0; i < ordersData.size(); i++) {
        if (ordersData[i]["id"].as<String>() == batchId) {
          batchFound = true;
          JsonArray orders = ordersData[i]["orders"];
          
          // Reset tất cả về false trước
          for (size_t j = 0; j < orders.size(); j++) {
            JsonObject order = orders[j];
            order["selected"] = false;
            int orderNumber = order["orderNumber"] | 0;
            String productName = order["productName"].as<String>();
            Serial.println("Reset order " + String(orderNumber) + " (" + productName + ") selected = false");
          }
          
          // Set selected = true cho các đơn được chọn
          for (JsonVariant selectedId : selectedOrders) {
            int orderId = selectedId.as<int>();
            for (size_t j = 0; j < orders.size(); j++) {
              JsonObject order = orders[j];
              if (order["id"] == orderId) {
                order["selected"] = true;
                int orderNumber = order["orderNumber"] | 0;
                String productName = order["productName"].as<String>();
                Serial.println("Order ID " + String(orderId) + " (orderNumber=" + String(orderNumber) + ", product=" + productName + ") marked as SELECTED");
              }
            }
          }

          // Đồng bộ trạng thái chi tiết từng đơn từ web (status/currentCount/productCode)
          if (!selectedOrdersDetails.isNull() && selectedOrdersDetails.size() > 0) {
            for (JsonVariant detailVar : selectedOrdersDetails) {
              JsonObject detail = detailVar.as<JsonObject>();
              int detailId = detail["id"] | 0;
              String detailStatus = detail["status"].as<String>();
              int detailCurrentCount = detail["currentCount"] | 0;
              String detailProductCode = detail["productCode"].as<String>();

              for (size_t j = 0; j < orders.size(); j++) {
                JsonObject order = orders[j];
                if (order["id"] == detailId) {
                  if (detailStatus.length() > 0) {
                    order["status"] = detailStatus;
                  }
                  order["currentCount"] = detailCurrentCount;
                  order["executeCount"] = detailCurrentCount;

                  // Ghi productCode phẳng để match ổn định phía web
                  if (detailProductCode.length() > 0) {
                    order["productCode"] = detailProductCode;
                  }
                  break;
                }
              }
            }
          }

          // Nếu web đã chỉ ra đơn đang counting, sync context active về ESP32
          if (!activeCountingOrder.isNull()) {
            String activeOrderCode = activeCountingOrder["orderCode"].as<String>();
            String activeProductName = activeCountingOrder["productName"].as<String>();
            String activeProductCode = activeCountingOrder["productCode"].as<String>();
            int activeCurrentCount = activeCountingOrder["currentCount"] | 0;

            if (activeProductName.length() > 0) {
              bagType = activeProductName;
            }
            if (activeProductCode.length() > 0) {
              productCode = activeProductCode;
            }
            if (activeOrderCode.length() > 0) {
              orderCode = activeOrderCode;
            }
            totalCount = activeCurrentCount;
            isLimitReached = false;
          }
          break;
        }
      }
      
      if (batchFound) {
        // Nếu web gửi đơn đầu tiên đã tích, đồng bộ ngay để ESP32 biết đơn bắt đầu
        if (!firstSelectedOrder.isNull()) {
          String firstProductName = firstSelectedOrder["productName"].as<String>();
          String firstProductCode = firstSelectedOrder["productCode"].as<String>();
          String firstOrderCode = firstSelectedOrder["orderCode"].as<String>();
          String firstCustomerName = firstSelectedOrder["customerName"].as<String>();
          int firstQuantity = firstSelectedOrder["quantity"] | 0;

          if (firstProductName.length() > 0) {
            bagType = firstProductName;
            productCode = firstProductCode;
            orderCode = firstOrderCode;
            customerName = firstCustomerName;
            if (firstQuantity > 0) targetCount = firstQuantity;

            Serial.println("Synced first selected order from web:");
            Serial.println("  Product: " + bagType);
            Serial.println("  ProductCode: " + productCode);
            Serial.println("  OrderCode: " + orderCode);
            Serial.println("  Target: " + String(targetCount));
          }
        }

        saveOrdersToFile();
        server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Selected orders updated\"}");
      } else {
        server.send(404, "application/json", "{\"status\":\"Error\",\"message\":\"Batch not found\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"No data provided\"}");
    }
  });
  
  // API lưu/xóa history
  server.on("/api/history", HTTP_POST, [](){
    // Allow cross-origin requests for clients that may not be served from the ESP
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

    if (server.method() == HTTP_OPTIONS) {
      // Preflight request
      server.send(200, "application/json", "{}");
      return;
    }

    if (server.hasArg("plain")) {
      String jsonData = server.arg("plain");
      Serial.println("Receiving history from web: " + String(jsonData.length()) + " bytes");
      if (jsonData.length() > 1000) {
        Serial.println("History payload (truncated 1st 1000 chars):");
        Serial.println(jsonData.substring(0, 1000));
      } else {
        Serial.println("History payload:");
        Serial.println(jsonData);
      }

      // Lưu trực tiếp vào file
      File file = LittleFS.open("/history.json", "w");
      if (file) {
        size_t written = file.print(jsonData);
        file.close();
        Serial.println("History saved to ESP32, bytes written: " + String(written));
        server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"History saved\"}");
      } else {
        Serial.println("ERROR: Failed to open /history.json for writing");
        server.send(500, "application/json", "{\"status\":\"Error\",\"message\":\"Failed to save history\"}");
      }
    } else {
      Serial.println("WARNING: /api/history POST called with no body");
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"No data provided\"}");
    }
  });
  
  server.on("/api/history", HTTP_DELETE, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    Serial.println("Clearing all history from ESP32");
    
    // Xóa file history
    if (LittleFS.exists("/history.json")) {
      LittleFS.remove("/history.json");
    }
    
    Serial.println("History cleared from ESP32");
    server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"History cleared\"}");
  });
  
  // API reset settings về mặc định
  server.on("/api/settings/reset", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    Serial.println("Resetting settings to default on ESP32");
    
    // Xóa file settings để force về default
    if (LittleFS.exists("/settings.json")) {
      LittleFS.remove("/settings.json");
    }
    
    // Reset biến global về default
    conveyorName = "BT-001";
    displayBrightness = 35;
    sensorDelayMs = 0;
    bagDetectionDelay = 200;
    minBagInterval = 100;
    autoReset = true;  // Bật tự động chuyển đơn hàng
    
    // Reset network về default
    local_IP = IPAddress(192, 168, 41, 200);
    gateway = IPAddress(192, 168, 41, 1);
    subnet = IPAddress(255, 255, 255, 0);
    primaryDNS = IPAddress(8, 8, 8, 8);
    secondaryDNS = IPAddress(8, 8, 4, 4);
    
    Serial.println("Settings reset to default");
    server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Settings reset to default\",\"needRestart\":true}");
  });
  
  // API DEBUG SETTINGS - Để kiểm tra trạng thái file và biến
  server.on("/api/debug/settings", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    Serial.println("DEBUG: Settings debugging requested via API");
    debugSettingsFile();
    
    DynamicJsonDocument doc(2048);
    
    // Current variables in memory
    doc["memory"]["conveyorName"] = conveyorName;
    doc["memory"]["brightness"] = displayBrightness;
    doc["memory"]["sensorDelay"] = sensorDelayMs;
    doc["memory"]["bagDetectionDelay"] = bagDetectionDelay;
    doc["memory"]["minBagInterval"] = minBagInterval;
    doc["memory"]["autoReset"] = autoReset;
    
    // File status
    doc["files"]["settings_exists"] = LittleFS.exists("/settings.json");
    doc["files"]["products_exists"] = LittleFS.exists("/products.json");
    doc["files"]["orders_exists"] = LittleFS.exists("/orders.json");
    doc["files"]["bagtypes_exists"] = LittleFS.exists(BAGTYPES_FILE);
    doc["files"]["bagconfigs_exists"] = LittleFS.exists(BAGCONFIGS_FILE);
    
    // File content (settings.json)
    if (LittleFS.exists("/settings.json")) {
      File file = LittleFS.open("/settings.json", "r");
      if (file) {
        String content = file.readString();
        file.close();
        
        DynamicJsonDocument settingsDoc(1024);
        if (deserializeJson(settingsDoc, content) == DeserializationError::Ok) {
          doc["file_content"]["settings"] = settingsDoc;
        } else {
          doc["file_content"]["settings"] = "PARSE_ERROR";
        }
      }
    } else {
      doc["file_content"]["settings"] = "FILE_NOT_FOUND";
    }
    
    // System info
    doc["system"]["free_heap"] = ESP.getFreeHeap();
    doc["system"]["uptime_ms"] = millis();
    doc["system"]["littlefs_total"] = LittleFS.totalBytes();
    doc["system"]["littlefs_used"] = LittleFS.usedBytes();
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
    
    Serial.println("Debug info sent to web client");
  });
  
  // API FORCE REFRESH SETTINGS - Để reload settings từ file
  server.on("/api/settings/refresh", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    Serial.println("Force refreshing settings from file...");
    
    // Reload settings từ file
    loadSettingsFromFile();
    
    // Áp dụng ngay các thay đổi
    if (dma_display && displayBrightness >= 10 && displayBrightness <= 100) {
      dma_display->setBrightness8(map(displayBrightness, 0, 100, 0, 255));
      Serial.println("Display brightness re-applied: " + String(displayBrightness) + "%");
    }
    
    debounceDelay = sensorDelayMs;
    
    Serial.println("Settings refreshed from file:");
    Serial.println("  - conveyorName: " + conveyorName);
    Serial.println("  - brightness: " + String(displayBrightness) + "%");
    Serial.println("  - sensorDelay: " + String(sensorDelayMs) + "ms");
    
    server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Settings refreshed from file\"}");
  });
  
  // Catch-all handler cho bất kỳ domain nào khác
  server.onNotFound([](){
    if (currentNetworkMode == WIFI_AP_MODE) {
      // Redirect về IP của AP
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
      server.send(302, "text/plain", "Redirecting to Bag Counter Configuration");
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });

  // API cập nhật target khi thêm/sửa order trong existing batch
  server.on("/api/update-target", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      Serial.println("Received target update: " + body);
      
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, body);
      
      if (!error) {
        String batchName = doc["batchName"].as<String>();
        int newTotalTarget = doc["totalTarget"];
        
        // Kiểm tra batch name có khớp không
        if (batchName == currentBatchName) {
          // Cập nhật tổng target
          int oldTarget = batchTotalTarget;
          batchTotalTarget = newTotalTarget;
          
          Serial.println("Target updated from " + String(oldTarget) + " to " + String(newTotalTarget));
          
          // Cập nhật hiển thị LED matrix
          displayCurrentOrderInfo();
          
          // Lưu thông tin batch
          saveBatchInfoToFile();
          
          // Response thành công
          DynamicJsonDocument response(512);
          response["status"] = "OK";
          response["message"] = "Target updated successfully";
          response["batchName"] = batchName;
          response["oldTarget"] = oldTarget;
          response["newTarget"] = newTotalTarget;
          
          String responseStr;
          serializeJson(response, responseStr);
          server.send(200, "application/json", responseStr);
          
        } else {
          Serial.println("Batch name mismatch: expected " + currentBatchName + ", got " + batchName);
          server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Batch name mismatch\"}");
        }
        
      } else {
        Serial.println("JSON parse error: " + String(error.c_str()));
        server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"Invalid JSON\"}");
      }
    } else {
      server.send(400, "application/json", "{\"status\":\"Error\",\"message\":\"No data provided\"}");
    }
  });

  // API để lấy thông tin thiết bị
  server.on("/api/device_info", HTTP_GET, [](){
    DynamicJsonDocument doc(512);
    
    // Lấy MAC Address - Luôn dùng WiFi MAC của ESP32 cho nhất quán
    String macAddress = WiFi.macAddress();  // ESP32 WiFi MAC (unique per device)
    
    // Thông tin Ethernet MAC (W5500 shield MAC - cố định)
    String ethernetMAC = "";
    char ethMacStr[18];
    sprintf(ethMacStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ethernetMAC = String(ethMacStr);
    
    String realtimeHost = (currentNetworkMode == ETHERNET_MODE) ? local_IP.toString() :
                          (currentNetworkMode == WIFI_AP_MODE ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
    String realtimeEndpoint = "ws://" + realtimeHost + ":" + String(REALTIME_WS_PORT) + "/ws";
    
    doc["deviceMAC"] = macAddress;  // ESP32 WiFi MAC (unique)
    doc["ethernetMAC"] = ethernetMAC;  // W5500 Ethernet MAC (fixed)
    doc["realtimeEndpoint"] = realtimeEndpoint;
    doc["conveyorName"] = conveyorName;
    doc["firmwareVersion"] = "1.0.0";
    doc["networkMode"] = (currentNetworkMode == ETHERNET_MODE) ? "Ethernet" : 
                        (currentNetworkMode == WIFI_STA_MODE) ? "WiFi_STA" : "WiFi_AP";
    doc["ipAddress"] = (currentNetworkMode == ETHERNET_MODE) ? local_IP.toString() : WiFi.localIP().toString();
    doc["uptime"] = millis();
    doc["activeInterface"] = (currentNetworkMode == ETHERNET_MODE) ? "Ethernet (W5500)" : "WiFi";
    
    String response;
    serializeJson(doc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
    
    Serial.println("Device info sent:");
    Serial.println("   - ESP32 WiFi MAC: " + macAddress);
    Serial.println("   - W5500 Ethernet MAC: " + ethernetMAC);
    Serial.println("   - Realtime endpoint: " + realtimeEndpoint);
    Serial.println("   - Active Interface: " + doc["activeInterface"].as<String>());
  });

  // API để đo thời gian sensor đơn giản
  server.on("/api/sensor-timing/clear", HTTP_POST, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    lastMeasuredTime = 0;
    lastTimingSensorState = digitalRead(SENSOR_PIN);
    if (isSensorBlocked(lastTimingSensorState)) {
      sensorActiveStartTime = millis();
      isMeasuringSensor = true;
    } else {
      sensorActiveStartTime = 0;
      isMeasuringSensor = false;
    }
    
    Serial.println("📏 Sensor timing cleared");
    server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Sensor timing cleared\"}");
  });
  
  server.on("/api/sensor-timing", HTTP_GET, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    DynamicJsonDocument doc(384);
    doc["lastMeasuredTime"] = lastMeasuredTime;
    doc["isMeasuringSensor"] = isMeasuringSensor;
    int sensorReading = digitalRead(SENSOR_PIN);
    doc["sensorCurrentState"] = sensorRawStateName(sensorReading);
    doc["sensorBlocked"] = isSensorBlocked(sensorReading);
    doc["sensorActiveLevel"] = "LOW";
    if (isMeasuringSensor) {
      doc["currentMeasuringTime"] = millis() - sensorActiveStartTime;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });

  // Handle CORS preflight for update-target
  server.on("/api/update-target", HTTP_OPTIONS, [](){
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200);
  });

  // API cho anh Dũng lấy thông tin 7 trường dữ liệu chính
  server.on("/api/customer/info", HTTP_GET, [](){
    Serial.println("=== Customer Info API Called ===");
    Serial.println("Current values:");
    Serial.println("  orderCode: " + orderCode);
    Serial.println("  productCode: " + productCode);
    Serial.println("  customerName: " + customerName);
    Serial.println("  startTime: " + startTimeStr);
    Serial.println("  currentMode: " + currentMode);
    Serial.println("  location: " + location);
    Serial.println("  conveyorName: " + conveyorName);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    
    DynamicJsonDocument doc(1024);
    
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
    
    // 4. customerName - Tên khách hàng hiện tại
    doc["customerName"] = customerName;
    
    // 5. startTime - Thời gian bắt đầu đếm
    doc["startTime"] = startTimeStr;
    
    // 6. setMode - Chế độ hiển thị hiện tại
    doc["setMode"] = currentMode;
    
    // 7. location - Địa điểm đặt băng tải
    doc["location"] = location;

    doc["target"] = (int)targetCount;
    doc["count"] = (long)totalCount;
    doc["conveyor"] = conveyorName;
    doc["sensorTimeMs"] = (long)lastMeasuredTime;
    
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
    
    Serial.println("Customer info API response sent successfully");
    Serial.println("Response JSON: " + out);
    Serial.println("=== End Customer Info API ===");
  });

  server.begin();
  Serial.println("WebServer started");
  Serial.println("Access web interface at: http://192.168.4.1/");
  Serial.println("Test page at: http://192.168.4.1/test");
  Serial.println("Test Customer API at: http://192.168.4.1/test-customer-api");
}

