#include "counter.h"

unsigned long bagGroupGapToleranceMs() {
  const unsigned long minimumGapMs = 500;
  unsigned long debounceGapMs = (unsigned long)sensorDelayMs * 2;
  return std::max(minimumGapMs, debounceGapMs);
}

//----------------------------------------Dynamic Bag Detection Delay Calculation
int calculateDynamicBagDetectionDelay() {
  // BẮT BUỘC PHẢI CÓ WEIGHT-BASED RULES
  if (weightDelayRules.empty()) {
    Serial.println("ERROR: No weight delay rules configured!");
    return 2000;
  }
  
  // Tìm khối lượng của sản phẩm hiện tại
  float currentProductWeight = 0.0;
  bool foundProductWeight = false;
  
  Serial.println("=== Finding Product Weight ===");
  Serial.println("Current bagType: '" + bagType + "'");
  Serial.println("Current productCode: '" + productCode + "'");
  Serial.println("Total products in database: " + String(productsData.size()));
  
  // Kiểm tra xem có sản phẩm hiện tại không
  if (bagType.isEmpty() && productCode.isEmpty()) {
    Serial.println("ERROR: No product selected!");
    return 2000; // Emergency fallback
  }
  
  // Ưu tiên tuyệt đối weight gửi theo đơn hiện tại từ web
  if (currentOrderUnitWeight > 0.0f) {
    currentProductWeight = currentOrderUnitWeight;
    foundProductWeight = true;
    Serial.println("  ✓ Using unitWeight from current order: " + String(currentProductWeight, 3) + "kg");
  }

  // Chỉ fallback tra productsData khi payload đơn chưa có unitWeight
  for (size_t i = 0; i < productsData.size() && !foundProductWeight; i++) {
    JsonObject product = productsData[i];
    String productName = product["name"].as<String>();
    String productCodeCheck = product["code"].as<String>();
    
    Serial.println("  Checking product " + String(i+1) + ": name='" + productName + "', code='" + productCodeCheck + "'");
    
    // Match theo tên sản phẩm hoặc mã sản phẩm
    if ((!bagType.isEmpty() && productName == bagType) || 
        (!productCode.isEmpty() && productCodeCheck == productCode)) {
      
      Serial.println("  ✓ Product matched!");
      
      if (product.containsKey("unitWeight")) {
        currentProductWeight = product["unitWeight"].as<float>();
        foundProductWeight = true;
        Serial.println("  ✓ Found unitWeight: " + String(currentProductWeight, 3) + "kg");
        break;
      } else {
        Serial.println("  ✗ Product matched but NO unitWeight field!");
      }
    }
  }

  // Fallback khi productsData rỗng: lấy unitWeight từ ordersData
  if (!foundProductWeight) {
    for (size_t i = 0; i < ordersData.size() && !foundProductWeight; i++) {
      if (!ordersData[i].containsKey("orders")) continue;
      JsonArray orders = ordersData[i]["orders"];
      for (size_t j = 0; j < orders.size(); j++) {
        JsonObject order = orders[j];
        String orderProductName = order["productName"].as<String>();
        String orderProductCode = orderProductCodeFromJson(order);
        bool nameMatched = (!bagType.isEmpty() && orderProductName == bagType);
        bool codeMatched = (!productCode.isEmpty() && orderProductCode == productCode);
        if (!nameMatched && !codeMatched) continue;

        if (order.containsKey("product") && order["product"].is<JsonObject>() &&
            order["product"].containsKey("unitWeight")) {
          currentProductWeight = order["product"]["unitWeight"].as<float>();
          foundProductWeight = currentProductWeight > 0.0f;
          if (foundProductWeight) break;
        }
      }
    }
  }

  // Emergency fallback: nếu productsData rỗng/chưa sync, lấy unitWeight từ ordersData
  if (!foundProductWeight) {
    for (size_t i = 0; i < ordersData.size() && !foundProductWeight; i++) {
      if (!ordersData[i].containsKey("orders")) continue;
      JsonArray orders = ordersData[i]["orders"];
      for (size_t j = 0; j < orders.size(); j++) {
        JsonObject order = orders[j];
        String orderProductName = order["productName"].as<String>();
        String orderProductCode = orderProductCodeFromJson(order);

        bool nameMatched = (!bagType.isEmpty() && orderProductName == bagType);
        bool codeMatched = (!productCode.isEmpty() && orderProductCode == productCode);
        if (!nameMatched && !codeMatched) continue;

        if (order.containsKey("product") && order["product"].is<JsonObject>() &&
            order["product"].containsKey("unitWeight")) {
          currentProductWeight = order["product"]["unitWeight"].as<float>();
          foundProductWeight = currentProductWeight > 0.0f;
          if (foundProductWeight) {
            Serial.println("  ✓ Found unitWeight from ordersData fallback: " + String(currentProductWeight, 3) + "kg");
            break;
          }
        }
      }
    }
  }
  
  // BẮT BUỘC PHẢI CÓ KHỐI LƯỢNG
  if (!foundProductWeight) {
    if (millis() - lastWeightErrorLogMs > 3000) {
      lastWeightErrorLogMs = millis();
      Serial.println("ERROR: Product weight NOT FOUND!");
      Serial.println("   Product: '" + bagType + "' (code: '" + productCode + "')");
      Serial.println("   Hint: set_current_order must include unitWeight > 0.");
    }
    return 2000; // Emergency fallback
  }
  
  // Tìm rule phù hợp dựa trên khối lượng
  // Rules được sort theo khối lượng giảm dần (từ nặng đến nhẹ)
  Serial.println("=== Matching Weight with Rules ===");
  Serial.println("Product weight: " + String(currentProductWeight, 3) + "kg");
  Serial.println("Available rules: " + String(weightDelayRules.size()));
  
  int calculatedDelay = 0;
  bool ruleMatched = false;
  
  for (size_t i = 0; i < weightDelayRules.size(); i++) {
    const auto& rule = weightDelayRules[i];
    Serial.println("  Rule " + String(i+1) + ": " + String(rule.weight, 3) + "kg → " + String(rule.delay) + "ms");
    
    if (currentProductWeight >= rule.weight) {
      calculatedDelay = rule.delay;
      ruleMatched = true;
      Serial.println("  ✓ MATCHED! " + String(currentProductWeight, 3) + "kg >= " + String(rule.weight, 3) + "kg");
      Serial.println("  → Using delay: " + String(rule.delay) + "ms");
      break;
    } else {
      Serial.println("  ✗ No match: " + String(currentProductWeight, 3) + "kg < " + String(rule.weight, 3) + "kg");
    }
  }
  
  // Fallback: Nếu sản phẩm nhẹ hơn rule nhỏ nhất, dùng rule nhỏ nhất
  if (!ruleMatched) {
    auto minWeightRule = std::min_element(weightDelayRules.begin(), weightDelayRules.end(), 
                                          [](const WeightDelayRule& a, const WeightDelayRule& b) {
                                            return a.weight < b.weight;
                                          });
    calculatedDelay = minWeightRule->delay;
    Serial.println("⚠️ Product weight " + String(currentProductWeight, 3) + "kg is lighter than smallest rule (" + String(minWeightRule->weight, 3) + "kg)");
    Serial.println("  → Using smallest rule delay: " + String(calculatedDelay) + "ms");
  }
  
  // Áp dụng bagTimeMultiplier (dao động thời gian)
  int minDelay = calculatedDelay * (100 - bagTimeMultiplier) / 100;
  
  Serial.println("=== Final Calculation ===");
  Serial.println("Base delay: " + String(calculatedDelay) + "ms");
  Serial.println("Multiplier: " + String(bagTimeMultiplier) + "%");
  Serial.println("Min delay (after multiplier): " + String(minDelay) + "ms");
  Serial.println("Range: " + String(minDelay) + "ms - " + String(calculatedDelay) + "ms");
  Serial.println("================================");
  
  return minDelay;
}

int calculateBagCountFromDuration(unsigned long detectionDuration) {
  // Logic: Dựa trên thời gian phát hiện để xác định số bao
  // Thời gian xác nhận dao động (100-bagTimeMultiplier)% đến 100% của thời gian cài đặt
  // Ví dụ bagTimeMultiplier=25% → dao động 75%-100%
  // 1 bao: thời gian trong khoảng (75%-100%) * baseDelay * 1
  // 2 bao: thời gian trong khoảng (75%-100%) * baseDelay * 2  
  // 3 bao: thời gian trong khoảng (75%-100%) * baseDelay * 3
  // 5 bao: thời gian trong khoảng (75%-100%) * baseDelay * 5
  
  // LẤY BASE TIME TỪ WEIGHT-BASED RULES
  // Tính lại để lấy base delay (không áp dụng multiplier)
  int baseTime = 0;
  
  // Tìm khối lượng sản phẩm hiện tại và rule tương ứng
  float currentProductWeight = 0.0;
  bool foundWeight = false;

  if (currentOrderUnitWeight > 0.0f) {
    currentProductWeight = currentOrderUnitWeight;
    foundWeight = true;
  }
  
  for (size_t i = 0; i < productsData.size() && !foundWeight; i++) {
    JsonObject product = productsData[i];
    String productName = product["name"].as<String>();
    String productCodeCheck = product["code"].as<String>();
    
    if ((!bagType.isEmpty() && productName == bagType) || 
        (!productCode.isEmpty() && productCodeCheck == productCode)) {
      if (product.containsKey("unitWeight")) {
        currentProductWeight = product["unitWeight"].as<float>();
        foundWeight = true;
        break;
      }
    }
  }
  
  // Tìm rule phù hợp để lấy base delay
  if (foundWeight && !weightDelayRules.empty()) {
    for (const auto& rule : weightDelayRules) {
      if (currentProductWeight >= rule.weight) {
        baseTime = rule.delay;
        break;
      }
    }
    
    // Nếu không match rule nào, dùng rule nhỏ nhất
    if (baseTime == 0) {
      auto minRule = std::min_element(weightDelayRules.begin(), weightDelayRules.end(),
                                      [](const WeightDelayRule& a, const WeightDelayRule& b) {
                                        return a.weight < b.weight;
                                      });
      baseTime = minRule->delay;
    }
  } else {
    // Fallback nếu không có weight hoặc rules
    baseTime = 2000; // Emergency fallback
  }
  
  if (baseTime <= 0) {
    return 1;
  }

  int lowerPercent = 100 - bagTimeMultiplier; // Ví dụ: 100-25=75%
  
  // Ngưỡng tối thiểu để loại nhiễu rất ngắn.
  int min1Bag = (baseTime * 1 * lowerPercent) / 100;  // 75% thời gian 1 bao

  if (detectionDuration < min1Bag) {
    return 1; // Fallback: nếu thời gian quá ngắn, coi như 1 bao
  }

  // Làm tròn theo thời gian chuẩn mỗi bao để không có khoảng chết.
  // Ví dụ base 1900ms, duration 6000ms => 3.16 bao => làm tròn thành 3.
  int bagCount = (detectionDuration + (baseTime / 2)) / baseTime;
  return std::max(1, bagCount);
}

void updateCount() {
  updateCount(1); // Default to 1 bag
}

void updateCount(int bagCount) {
  Serial.println("DEBUG updateCount: called with bagCount=" + String(bagCount) + ", isLimitReached=" + String(isLimitReached));
  
  if (!isLimitReached) {
    totalCount += bagCount;
    Serial.println("DEBUG updateCount: incremented totalCount to " + String(totalCount));
    
    // Cập nhật executeCount trong ordersData cho đơn hàng hiện tại
    // Tìm order hiện tại theo cả productName VÀ productCode VÀ status = "counting"
    for (size_t i = 0; i < ordersData.size(); i++) {
      JsonArray orders = ordersData[i]["orders"];
      
      for (size_t j = 0; j < orders.size(); j++) {
        JsonObject order = orders[j];
        String orderProductName = order["productName"].as<String>();
        String orderProductCode = orderProductCodeFromJson(order);
        String status = order["status"].as<String>();
        bool selected = order["selected"] | false;
        
        // Cập nhật executeCount CHỈ cho đơn hàng ĐANG counting
        if (orderProductName == bagType && orderProductCode == productCode && selected && status == "counting") {
          int currentExecuteCount = order["executeCount"] | 0;
          order["executeCount"] = currentExecuteCount + bagCount;
          Serial.println("Updated executeCount for counting order '" + bagType + "' (code: " + productCode + ") from " + String(currentExecuteCount) + " to " + String(currentExecuteCount + bagCount));
          break;
        }
      }
    }
    
    // Lưu ordersData sau khi cập nhật executeCount
    saveOrdersToFile();
    
    // MQTT: Publish count update ngay lập tức
    publishCountUpdate();
    // Kiểm tra ngưỡng cảnh báo và cập nhật đèn DONE
    for (auto& cfg : bagConfigs) {
      if (cfg.type == bagType) {
        int warningThreshold = cfg.target - cfg.warn;
        // DEBUG: print warning calculation details
        Serial.println("DEBUG warning check: bagType=" + String(bagType) + ", totalCount=" + String(totalCount) + ", cfg.target=" + String(cfg.target) + ", cfg.warn=" + String(cfg.warn) + ", threshold=" + String(warningThreshold) + ", targetCount=" + String(targetCount));
        if (totalCount >= warningThreshold && totalCount < targetCount) {
          Serial.println("Đạt ngưỡng cảnh báo!");
          
          // MQTT: Publish warning alert
          publishAlert("WARNING", "Đạt ngưỡng cảnh báo: " + String(totalCount) + "/" + String(targetCount));
          
          updateDoneLED();
        }
        break;
      }
    }
    
    // Kiểm tra đạt mục tiêu - CHỈ KHI CÓ TARGET HỢP LỆ VÀ ĐÃ THỰC SỰ ĐẾM
    if (totalCount >= targetCount && targetCount > 0 && totalCount > 0) {
      isLimitReached = true;
      finishedBlinking = false;
      blinkCount = 0;
      isBlinking = true;
      lastBlink = millis();
      
      // LƯU THÔNG TIN ĐƠN HÀNG HIỆN TẠI TRƯỚC KHI CHUYỂN SANG ĐƠN MỚI
      String completedCustomerName = customerName;
      String completedOrderCode = orderCode;
      String completedProductName = bagType;
      String completedProductCode = productCode;
      String completedVehicleNumber = vehicleNumber;
      int completedTargetCount = targetCount;
      
      // Lưu lịch sử với thêm thông tin loại - chỉ khi có thời gian thực
      String currentTime = (time(nullptr) > 24 * 3600) ? getTimeStr() : "Time not synced";
      history.push_back({currentTime, (int)totalCount, bagType});

      // Persist the new history entry to LittleFS (/history.json) immediately
      // so the ESP32 keeps a local record even if the web UI doesn't push history.
      {
        DynamicJsonDocument histDoc(4096);
        JsonArray histArr;

        // Load existing history.json if present
        if (LittleFS.exists("/history.json")) {
          File hf = LittleFS.open("/history.json", "r");
          if (hf) {
            String content = hf.readString();
            hf.close();
            DeserializationError err = deserializeJson(histDoc, content);
            if (!err && histDoc.is<JsonArray>()) {
              histArr = histDoc.as<JsonArray>();
            } else {
              // Start fresh array if parse fails
              histDoc.clear();
              histArr = histDoc.to<JsonArray>();
            }
          } else {
            histArr = histDoc.to<JsonArray>();
          }
        } else {
          histArr = histDoc.to<JsonArray>();
        }

        // Try to find the current order details from ordersData so we can
        // persist a richer history object (fields the web UI expects)
        // SỬ DỤNG THÔNG TIN ĐÃ LƯU CỦA ĐƠN HÀNG VỪA HOÀN THÀNH
        String historyCustomerName = completedCustomerName;
        String historyProductName = completedProductName;
        String historyOrderCode = completedOrderCode;
        String historyVehicleNumber = completedVehicleNumber;  // Sử dụng vehicleNumber đã lưu
        int historyPlannedQuantity = completedTargetCount;

        // Append new entry with fields the web expects
        JsonObject newEntry = histArr.createNestedObject();
        newEntry["timestamp"] = currentTime;
        newEntry["customerName"] = historyCustomerName;
        newEntry["productName"] = historyProductName;
        newEntry["orderCode"] = historyOrderCode;
        newEntry["vehicleNumber"] = historyVehicleNumber;
        newEntry["plannedQuantity"] = historyPlannedQuantity;
        newEntry["actualCount"] = completedTargetCount;  // Thực tế = mục tiêu khi hoàn thành
        newEntry["batchType"] = completedProductName;

        // Save back to file
        File wf = LittleFS.open("/history.json", "w");
        if (wf) {
          size_t written = serializeJson(histArr, wf);
          wf.close();
          Serial.println("History persisted on ESP32, bytes written: " + String(written));
        } else {
          Serial.println("ERROR: Cannot open /history.json for writing history");
        }
      }
      
      // Đánh dấu DONE cho loại hiện tại
      for (auto& c : bagConfigs) {
        if (c.type == bagType) {
          c.status = "DONE";
          break;
        }
      }
      saveBagConfigsToFile();
      
      // MQTT: Publish completion alert
      publishAlert("COMPLETED", "Hoàn thành đơn hàng: " + productCode + " - " + String(totalCount) + " bao");
      
      // DEBUG: Check auto reset status
      Serial.println("DEBUG Auto Reset: enabled=" + String(autoReset) + ", totalCount=" + String(totalCount) + ", targetCount=" + String(targetCount));
      
      // BẮT ĐẦU RELAY DELAY TIMER
      isOrderComplete = true;
      orderCompleteTime = millis();
      isRelayDelayActive = true;
      Serial.println("🔌 ORDER COMPLETED - Starting relay delay: " + String(relayDelayAfterComplete) + "ms");
      
      // MQTT: Publish final status
      publishStatusMQTT();
      
      // MQTT2: Publish order completion data
      publishMQTT2OrderComplete();
      
      // Auto Reset nếu được bật từ settings - CHỈ RESET ĐƠN HÀNG HIỆN TẠI
      // THÊM CHECK: Chỉ auto reset khi thực sự hoàn thành đơn hàng (target > 0 và đã đếm xong)
      if (autoReset && totalCount >= targetCount && targetCount > 0 && totalCount > 0) {
        Serial.println("Auto Reset enabled - resetting CURRENT ORDER only");
        
        // HIỂN THỊ SỐ ĐẾM CUỐI TRONG 3 GIÂY TRƯỚC KHI RESET
        Serial.println("Displaying final count " + String(totalCount) + " for 3 seconds before reset...");
        updateDisplay(); // Đảm bảo LED hiển thị số cuối
        delay(3000); // Hiển thị số đếm cuối trong 3 giây
        
        //  CHỈ RESET ĐƠN HÀNG HIỆN TẠI, GIỮ NGUYÊN DANH SÁCH
        String completedOrderType = bagType;  // Lưu tên đơn vừa hoàn thành
        String completedProductCode = productCode; // Lưu product code của đơn vừa hoàn thành
        int finalExecuteCount = totalCount; // LUU executeCount TRƯỚC KHI reset
        
        // Reset CHỈ số đếm, GIỮ NGUYÊN trạng thái running
        totalCount = 0;
        isLimitReached = false;
        // KHÔNG RESET: isRunning, isTriggerEnabled, isCountingEnabled
        // Giữ nguyên trạng thái đang chạy để tiếp tục đếm đơn tiếp theo
        
        Serial.println("Reset count to 0, keep running state for next order");
        
        // CLEAR RELAY DELAY STATE
        // isOrderComplete = false;
        // isRelayDelayActive = false;
        // orderCompleteTime = 0;
        // Serial.println("RELAY DELAY STATE CLEARED");
        
        // Mark order as completed in ordersData (bagConfigs will sync automatically)
        Serial.println("Order '" + completedOrderType + "' marked as COMPLETED");
        
        //  TỰ ĐỘNG CHUYỂN SANG ĐƠN HÀNG TIẾP THEO THEO ORDER NUMBER (hoặc thứ tự dòng nếu orderNumber = 0)
        bool foundNextOrder = false;
        Serial.println("Searching for next order...");
        
        int currentOrderNumber = 0;
        String currentProductCode = productCode;
        int completedOrderRow = -1;
        size_t completedBatchRow = 0;
        bool completedRowFound = false;
        
        Serial.println("Completed order info: productName=" + completedOrderType + ", productCode=" + currentProductCode);
        
        for (size_t i = 0; i < ordersData.size(); i++) {
          String batchId = ordersData[i]["id"].as<String>();
          if (batchId != currentBatchId) {
            continue;
          }
          
          JsonArray orders = ordersData[i]["orders"];
          for (size_t j = 0; j < orders.size(); j++) {
            JsonObject order = orders[j];
            String orderProductName = order["productName"].as<String>();
            String orderProductCode = orderProductCodeFromJson(order);
            bool selected = order["selected"] | false;
            
            if (orderProductName == completedOrderType && orderProductCode == currentProductCode && selected) {
              currentOrderNumber = order["orderNumber"] | 0;
              Serial.println("Completed order found: orderNumber=" + String(currentOrderNumber) + ", row=" + String(j) + ", product=" + orderProductName + ", code=" + orderProductCode);
              
              order["status"] = "completed";
              order["executeCount"] = finalExecuteCount;
              Serial.println("Set executeCount=" + String(finalExecuteCount) + " for completed order: " + orderProductName + " (code: " + orderProductCode + ")");
              
              completedOrderRow = (int)j;
              completedBatchRow = i;
              completedRowFound = true;
              break;
            }
          }
          if (completedRowFound) {
            break;
          }
        }
        
        Serial.println("Looking for next SELECTED order after orderNumber=" + String(currentOrderNumber) + " in batch=" + currentBatchId);
        
        int nextOrderNumber = -1;
        JsonObject nextOrder;
        
        for (size_t i = 0; i < ordersData.size(); i++) {
          String batchId = ordersData[i]["id"].as<String>();
          
          if (batchId != currentBatchId) {
            Serial.println("Skipping batch " + batchId + " (not current batch)");
            continue;
          }
          
          JsonArray orders = ordersData[i]["orders"];
          Serial.println("Searching in batch " + batchId + " with " + String(orders.size()) + " orders");
          
          for (size_t j = 0; j < orders.size(); j++) {
            JsonObject order = orders[j];
            int orderNumber = order["orderNumber"] | 0;
            bool selected = order["selected"] | false;
            String status = order["status"].as<String>();
            String productName = order["productName"].as<String>();
            
            Serial.println("  Order " + String(orderNumber) + ": " + productName + " (selected=" + String(selected) + ", status=" + status + ")");
            
            bool canBeNext = (status == "waiting" || status == "paused");
            if (selected && canBeNext && orderNumber > currentOrderNumber) {
              if (nextOrderNumber == -1 || orderNumber < nextOrderNumber) {
                nextOrderNumber = orderNumber;
                nextOrder = order;
                Serial.println("Found candidate next order: " + String(orderNumber) + " - " + productName);
              }
            }
          }
          
          // Fallback: orderNumber thiếu hoặc cả batch đều 0 — lấy đơn được chọn, chờ/pause, ngay sau dòng vừa hoàn thành
          if (nextOrderNumber == -1 && completedRowFound && completedOrderRow >= 0 && i == completedBatchRow) {
            for (size_t j = (size_t)completedOrderRow + 1; j < orders.size(); j++) {
              JsonObject order = orders[j];
              bool selected = order["selected"] | false;
              String status = order["status"].as<String>();
              String productName = order["productName"].as<String>();
              bool canBeNext = (status == "waiting" || status == "paused");
              if (selected && canBeNext) {
                nextOrder = order;
                nextOrderNumber = order["orderNumber"] | 0;
                if (nextOrderNumber <= 0) {
                  nextOrderNumber = (int)j + 1;
                }
                Serial.println("FALLBACK next order by table row " + String(j) + ": " + productName + " (effective order # " + String(nextOrderNumber) + ")");
                break;
              }
            }
          }
          
          break;
        }
        
        if (nextOrderNumber != -1) {
              String productName = nextOrder["productName"].as<String>();
              Serial.println("Found SELECTED next order: " + String(nextOrderNumber) + " - " + productName);
              
              String newProductCode = orderProductCodeFromJson(nextOrder);
              int quantity = nextOrder["quantity"] | 1;
              int warningQuantity = nextOrder["warningQuantity"].as<int>() | 5; // Mặc định 5 nếu không có
              
              Serial.println("Found next order with warningQuantity: " + String(warningQuantity));
              
              // Cập nhật hoặc tạo mới BagConfig với đúng warningQuantity
              bool foundBagConfig = false;
              for (auto& cfg : bagConfigs) {
                if (cfg.type == productName) {
                  cfg.target = quantity;
                  cfg.warn = warningQuantity;  // Cập nhật warning từ order data
                  cfg.status = "RUNNING";
                  foundBagConfig = true;
                  Serial.println("Updated existing bagConfig with warn: " + String(warningQuantity));
                  break;
                }
              }
              
              if (!foundBagConfig) {
                // Tạo mới bagConfig
                BagConfig newCfg;
                newCfg.type = productName;
                newCfg.target = quantity;
                newCfg.warn = warningQuantity;
                newCfg.status = "RUNNING";
                bagConfigs.push_back(newCfg);
                Serial.println("Created new bagConfig with warn: " + String(warningQuantity));
              }
              
              // Cập nhật trạng thái đơn mới thành counting
              nextOrder["status"] = "counting";
              
              // CẬP NHẬT BIẾN HIỂN THỊ
              bagType = productName;
              productCode = newProductCode;
              targetCount = quantity;
              totalCount = 0;  // Reset số đếm về 0
              isLimitReached = false;
              // Ép trạng thái chạy để đảm bảo đơn tiếp theo đếm thật sự
              isRunning = true;
              isTriggerEnabled = true;
              isCountingEnabled = true;
              isStartAuthorized = true;
              currentSystemStatus = "RUNNING";
              
              foundNextOrder = true;
              
              Serial.println("Auto switched to next order:");
              Serial.println("   OrderNumber: " + String(nextOrderNumber));
              Serial.println("   ProductName: " + productName);
              Serial.println("   ProductCode: " + newProductCode);
              Serial.println("   Target: " + String(quantity) + " bags");
              Serial.println("   Count reset to 0, continue running");
              
              // Lưu thay đổi vào file
              saveOrdersToFile();
        } else {
          // Không tìm thấy đơn tiếp theo được chọn
          foundNextOrder = false;
        }
        
        if (!foundNextOrder) {
      
          //DỪNG HOÀN TOÀN hệ thống
          isRunning = false;
          isTriggerEnabled = false;
          isCountingEnabled = false;
          isStartAuthorized = false;
          currentSystemStatus = "RESET";
          
          // Reset count về 0
          totalCount = 0;
          isLimitReached = true; // Đánh dấu đã hoàn thành
          
          // Thông báo hoàn thành batch
          publishAlert("BATCH_COMPLETED", "Hoàn thành tất cả đơn hàng trong batch hiện tại!");
          publishStatusMQTT();
          
          // Lưu ordersData để kích hoạt logic xóa completed orders
          saveOrdersToFile();
          
          Serial.println("Batch completed - System stopped. Please select new batch to continue.");
        } else {
          isOrderComplete = false;
          isRelayDelayActive = false;
          orderCompleteTime = 0;
          Serial.println("Found next order - CLEARED relay delay state, relay continues running");
          
          // RESET TRẠNG THÁI SENSOR ĐỂ TRÁNH ĐẾM NHẦM
          sensorActiveStartTime = 0;
          lastSensorState = sensorClearLevelForActive(countSensorActiveLevel);
          sensorState = sensorClearLevelForActive(countSensorActiveLevel);
          lastBagTime = 0;
          isBagDetected = false;
          waitingForInterval = false;
          bagStartTime = 0;
          lastDebounceTime = 0;
          waitForSensorClearOnStart = true;
          Serial.println("Sensor state cleared");
          
          // Đã tìm thấy đơn tiếp theo - gửi thông tin lên web
          loadCurrentOrderForDisplay();
          publishCountUpdate();
          publishStatusMQTT();
          publishBagConfigs();
          
          Serial.println("Sent new order info to web interface");
        }
        
        // Keep bagConfigs sync for legacy compatibility
        saveBagConfigsToFile();
        updateStartLED();
        updateDoneLED();
        needUpdate = true;
        
        Serial.println("Auto Reset completed - ready for next order");
        publishAlert("AUTO_RESET", "SP-" + completedProductCode + " (" + String(finalExecuteCount) + " bao) → " + 
                    (foundNextOrder ? "SP-" + productCode : "Hết đơn hàng"));
      } else {
        Serial.println("Auto Reset disabled - attempting manual order switch");
        
        // Manual order switching logic khi autoReset = false
        String completedOrderType = bagType;
        String completedProductCode = productCode; // Lưu product code của đơn vừa hoàn thành
        int finalExecuteCount = totalCount; // Lưu executeCount trước khi reset
        
        // Đánh dấu đơn hiện tại hoàn thành trong bagConfigs
        for (auto& cfg : bagConfigs) {
          if (cfg.type == completedOrderType) {
            cfg.status = "COMPLETED";
            Serial.println("Manual: Order '" + completedOrderType + "' marked as COMPLETED");
            break;
          }
        }
        
        // Đánh dấu đơn hiện tại hoàn thành trong ordersData (tương tự auto-reset)
        Serial.println("Manual: Marking completed order in ordersData - " + completedOrderType + " (code: " + completedProductCode + ")");
        for (size_t i = 0; i < ordersData.size(); i++) {
          String batchId = ordersData[i]["id"].as<String>();
          if (batchId != currentBatchId) continue;
          
          JsonArray orders = ordersData[i]["orders"];
          for (size_t j = 0; j < orders.size(); j++) {
            JsonObject order = orders[j];
            String orderProductName = order["productName"].as<String>();
            String orderProductCode = orderProductCodeFromJson(order);
            bool selected = order["selected"] | false;
            
            // Tìm đơn có cùng productName VÀ productCode với đơn vừa hoàn thành VÀ được chọn
            if (orderProductName == completedOrderType && orderProductCode == completedProductCode && selected) {
              Serial.println("Manual: Found completed order in ordersData, marking as completed");
              order["status"] = "completed";
              order["executeCount"] = finalExecuteCount;
              Serial.println("Manual: Set executeCount=" + String(finalExecuteCount) + " for completed order: " + orderProductName + " (code: " + orderProductCode + ")");
              break;
            }
          }
          break;
        }
        
        // Tìm đơn hàng tiếp theo
        bool foundNextOrder = false;
        Serial.println("Manual search for next order...");
        
        for (auto& cfg : bagConfigs) {
          Serial.println("   Order: " + cfg.type + " | Status: " + cfg.status + " | Target: " + String(cfg.target));
        }
        
        for (auto& cfg : bagConfigs) {
          if (cfg.status == "WAIT" || cfg.status == "SELECTED" || (cfg.status == "RUNNING" && cfg.type != completedOrderType)) {
            bagType = cfg.type;
            targetCount = cfg.target;
            cfg.status = "COUNTING";
            totalCount = 0;
            
            // CẬP NHẬT PRODUCT CODE từ bagType (cho manual mode)
            int underscorePos = bagType.lastIndexOf('_');
            if (underscorePos > 0) {
              String productName = bagType.substring(0, underscorePos);
              productCode = bagType.substring(underscorePos + 1);
              Serial.println("Manual - Extracted: productName='" + productName + "', productCode='" + productCode + "'");
            } else {
              productCode = "1"; // Fallback
            }
            
            // GIỮ NGUYÊN trạng thái running
            isLimitReached = false;
            waitForSensorClearOnStart = true;
            
            foundNextOrder = true;
            
            Serial.println("Manual switched to next order: " + bagType);
            Serial.println("   ProductCode: " + productCode);
            Serial.println("   Count reset to 0, continue running");
            loadCurrentOrderForDisplay();
            publishCountUpdate();
            publishBagConfigs();
            break;
          }
        }
        
        if (!foundNextOrder) {
          Serial.println("No orders available for manual switch");
        }
        
        // Lưu thay đổi ordersData sau manual completion
        saveOrdersToFile();
      }
      
      // Realtime status broadcast để web đồng bộ ngay sau khi chuyển đơn
      DynamicJsonDocument doc(256);
      doc["count"] = totalCount;
      doc["time"] = currentTime;
      doc["type"] = bagType;
      String msg;
      serializeJson(doc, msg);
      broadcastRealtimeMessage("bagcounter/status", msg);
    }
  }
}

void updateDoneLED() {
  // Đèn DONE (GPIO 5) với logic Active HIGH - BẬT khi đạt ngưỡng cảnh báo hoặc hoàn thành
  doneLedOn = false; // Mặc định TẮT
  
  for (auto& cfg : bagConfigs) {
    if (cfg.type == bagType) {
      // Kiểm tra ngưỡng cảnh báo được set từ web
      int warningThreshold = cfg.target - cfg.warn;  // Số bao còn lại để cảnh báo
      
      // Kiểm tra ngưỡng cảnh báo
      if (totalCount >= warningThreshold && totalCount < cfg.target) {
        // Đạt ngưỡng cảnh báo
        if (!hasReachedWarningThreshold) {
          // Lần đầu đạt ngưỡng cảnh báo
          hasReachedWarningThreshold = true;
          isWarningLedActive = true;
          warningLedStartTime = millis();
          doneLedOn = true;  // BẬT LED cảnh báo
          Serial.println("WARNING THRESHOLD REACHED! LED ON for 5 seconds");
          Serial.println("Count: " + String(totalCount) + "/" + String(cfg.target) + 
                        ", Warning at: " + String(warningThreshold));
        } else if (isWarningLedActive) {
          // Đang trong thời gian cảnh báo 10 giây
          if (millis() - warningLedStartTime < 10000) {
            doneLedOn = true;  // Giữ LED BẬT trong 10 giây
          } else {
            // Hết 5 giây, tắt LED cảnh báo
            isWarningLedActive = false;
            doneLedOn = false;
            Serial.println("⏰ Warning LED timeout - LED OFF");
          }
        }
      } else if (totalCount >= cfg.target) {
        // Hoàn thành đơn hàng - BẬT LED liên tục
        doneLedOn = true;
        isWarningLedActive = false;  // Reset warning state
        hasReachedWarningThreshold = false;
        Serial.println("ORDER COMPLETED - LED ON");
      } else {
        // Chưa đạt ngưỡng cảnh báo - TẮT LED
        doneLedOn = false;
        isWarningLedActive = false;
        hasReachedWarningThreshold = false;
      }
      
      digitalWrite(DONE_LED_PIN, doneLedOn ? HIGH : LOW);  // Active HIGH logic
      
      // Debug info
      static bool lastDoneState = false;
      static unsigned long lastDebugTime = 0;
      if (doneLedOn != lastDoneState || (millis() - lastDebugTime > 10000)) {
        lastDoneState = doneLedOn;
        lastDebugTime = millis();
        
        String reason = "";
        if (totalCount >= cfg.target) {
          reason = "COMPLETED";
        } else if (isWarningLedActive) {
          reason = "WARNING (5s timer)";
        } else if (hasReachedWarningThreshold) {
          reason = "WARNING (timeout)";
        } else {
          reason = "NORMAL";
        }
        
        Serial.println("DONE LED: " + String(doneLedOn ? "ON" : "OFF") + 
                      " - Count: " + String(totalCount) + 
                      "/" + String(cfg.target) + 
                      ", Warning at: " + String(warningThreshold) +
                      " (" + reason + ")");
      }
      break;
    }
  }
}

void updateStartLED() {
  DynamicJsonDocument doc(2048);
  String msg;
  String action = "";
  // Xử lý nút bấm BUTTON_PIN3
  static bool lastButtonState = HIGH;
  static unsigned long lastButtonTime = 0;
  static unsigned long buttonDebounceTime = 200; // 200ms debounce
  
  bool currentButtonState = digitalRead(BUTTON_PIN3);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    if (millis() - lastButtonTime > buttonDebounceTime) {
      isRunning = true;
      currentSystemStatus = "RUNNING";
      action = "START";
      Serial.println("BUTTON_PIN3 press");
      
      needUpdate = true;
      lastButtonTime = millis();
    }
  }
  
  lastButtonState = currentButtonState;

  // Xử lý nút bấm BUTTON_PIN2
  static bool lastButton2State = HIGH;
  static unsigned long lastButton2Time = 0;
  static unsigned long button2DebounceTime = 200; // 200ms debounce
  
  bool currentButton2State = digitalRead(BUTTON_PIN2);
  if (currentButton2State == LOW && lastButton2State == HIGH) {
    if (millis() - lastButton2Time > button2DebounceTime) {
      // CHỈ TẮT - không toggle
      isRunning = false;
      isTriggerEnabled = false;
      isCountingEnabled = false;
      isStartAuthorized = false;
      currentSystemStatus = "PAUSE";
      action = "PAUSE";
      Serial.println("BUTTON_PIN2 press");
      
      needUpdate = true;
      lastButton2Time = millis();
    }
  }
  
  lastButton2State = currentButton2State;

  // Đèn START (GPIO 38 - relay) logic cập nhật:
  // - Sáng (HIGH) khi: isRunning = true HOẶC đang trong thời gian relay delay
  // - Tắt (LOW) khi: isRunning = false VÀ không trong thời gian relay delay
  
  if (isRunning || isRelayDelayActive) {
    startLedOn = true;  // Sáng (HIGH) - relay hoạt động
  } else {
    startLedOn = false; // Tắt (LOW) - relay ngưng
  }
  
  if (!isManualRelayOn) {
    digitalWrite(START_LED_PIN, startLedOn ? HIGH : LOW);
  }
  
  // Debug relay state với thông tin chi tiết
  static bool lastRelayState = false;
  if (startLedOn != lastRelayState) {
    lastRelayState = startLedOn;
    String reason = "";
    if (isRunning) reason = "System Running";
    else if (isRelayDelayActive) reason = "Relay Delay Active (" + String(relayDelayAfterComplete) + "ms)";
    else reason = "System Stopped - NO ORDERS LEFT";
    
    Serial.println("RELAY STATE CHANGED: " + String(startLedOn ? "ON (HIGH)" : "OFF (LOW)") + " - Reason: " + reason);
  }
}
