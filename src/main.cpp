#include <IRremote.h>
#include "app.h"
#include "input_commands.h"
#include "storage.h"
#include "network.h"
#include "mqtt2.h"
#include "realtime.h"
#include "web_server.h"
#include "display.h"
#include "counter.h"
#include "qr_reader.h"

IRrecv irrecv(RECV_PIN);
decode_results results;

//----------------------------------------SETUP & LOOP
void setup() {
  // Tắt brownout detector để tránh reset do điện áp thấp
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Disable watchdog timer to prevent resets during heavy processing
  esp_task_wdt_init(30, false); // 30 second timeout, no panic
  
  Serial.begin(QR_READER_BAUD, SERIAL_8N1, QR_READER_RX_PIN, QR_READER_TX_PIN);
  Serial.println("Booting ESP32 Bag Counter System...");
  setupQrReader();
  
  // Print free heap at startup
  Serial.println("Free heap at startup: " + String(ESP.getFreeHeap()) + " bytes");
  
  // BƯỚC 1: Khởi tạo LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS failed!");
    while (1);
  }
  Serial.println("LittleFS initialized");
  
  // BƯỚC 2: Tạo các file mặc định LẦN ĐẦU (nếu chưa có)
  Serial.println("Checking and creating default files...");
  createDefaultDataFiles();  // Tạo products, orders, history nếu chưa có
  
  // BƯỚC 3: Load cấu hình từ file (với đảm bảo file tồn tại)
  Serial.println("Loading configurations from files...");
  loadBagTypesFromFile();
  loadBagConfigsFromFile();
  loadBatchInfoFromFile(); // Thêm load batch info
  loadSettingsFromFile();  // Load settings (file đã được tạo nếu chưa có)
  loadProductsFromFile();  // Load products data
  loadOrdersFromFile();    // Load orders data
  
  // BƯỚC QUAN TRỌNG: Debug và Force Refresh Settings
  Serial.println("POST-LOAD DEBUG:");
  debugSettingsFile();
  
  // FORCE REFRESH để đảm bảo settings được áp dụng đúng
  Serial.println("Force refreshing settings...");
  // NOTE: dma_display chưa được khởi tạo ở đây, sẽ xử lý brightness sau
  
  // Đồng bộ debounce delay với sensorDelayMs  
  debounceDelay = sensorDelayMs;
  
  Serial.println("All configurations loaded and verified:");
  Serial.println("  - location: " + location);
  Serial.println("  - conveyorName: " + conveyorName);
  Serial.println("  - brightness: " + String(displayBrightness) + "%");
  Serial.println("  - sensorDelay: " + String(sensorDelayMs) + "ms");
  Serial.println("  - bagDetectionDelay: " + String(bagDetectionDelay) + "ms");
  Serial.println("  - minBagInterval: " + String(minBagInterval) + "ms");
  Serial.println("  - autoReset: " + String(autoReset ? "true" : "false"));
  Serial.println("  - products: " + String(productsData.size()) + " items");
  Serial.println("  - orders: " + String(ordersData.size()) + " items");
  Serial.println("  - relayDelayAfterComplete: " + String(relayDelayAfterComplete) + "ms");
  Serial.println("  - enableWeightBasedDelay: " + String(enableWeightBasedDelay ? "true" : "false"));
  Serial.println("  - weightDelayRules: " + String(weightDelayRules.size()) + " rules");
  for (size_t i = 0; i < weightDelayRules.size(); i++) {
    Serial.println("    Rule " + String(i+1) + ": " + String(weightDelayRules[i].weight) + "kg → " + String(weightDelayRules[i].delay) + "ms");
  }
  // BƯỚC 3: Khởi tạo hardware
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(TRIGGER_SENSOR_PIN, INPUT_PULLUP);
  pinMode(OUTPUT_TRIGGER_SENSOR_PIN, INPUT_PULLUP);
  pinMode(START_LED_PIN, OUTPUT);
  pinMode(DONE_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN3, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  lastTimingSensorState = digitalRead(SENSOR_PIN);
  sensorState = lastTimingSensorState;
  lastSensorState = lastTimingSensorState;
  triggerState = digitalRead(TRIGGER_SENSOR_PIN);
  lastTriggerState = triggerState;
  outputTriggerState = digitalRead(OUTPUT_TRIGGER_SENSOR_PIN);
  lastOutputTriggerState = outputTriggerState;
  bool initialInputTriggerBlocked = isInputTriggerBlocked(triggerState);
  bool initialOutputTriggerBlocked = isOutputTriggerBlocked(outputTriggerState);
  inputTriggerBlockedState = false;
  outputTriggerBlockedState = false;
  Serial.println("Trigger sensors initialized:");
  Serial.println("  - GPIO4 input trigger: " + String(sensorRawStateName(triggerState)) +
                 ", active=" + String(sensorLevelName(inputSensorActiveLevel)) +
                 ", blocked=" + String(initialInputTriggerBlocked ? "true" : "false"));
  Serial.println("  - GPIO39 output trigger: " + String(sensorRawStateName(outputTriggerState)) +
                 ", active=" + String(sensorLevelName(outputSensorActiveLevel)) +
                 ", blocked=" + String(initialOutputTriggerBlocked ? "true" : "false"));
  if (isSensorBlocked(lastTimingSensorState)) {
    sensorActiveStartTime = millis();
    isMeasuringSensor = true;
  }
  
  // Khởi tạo IR Remote
  irrecv.enableIRIn();
  Serial.println("IR Remote initialized");
  
  // Tắt LED ban đầu
  digitalWrite(START_LED_PIN, LOW);  // Đèn START tắt (HIGH)
  digitalWrite(DONE_LED_PIN, LOW);   // Đèn DONE tắt (HIGH)
  
  // BƯỚC 4: Khởi tạo các biến trạng thái
  isRunning = false;
  isTriggerEnabled = false;
  isCountingEnabled = false;
  isLimitReached = false;
  totalCount = 0;
  
  // BƯỚC 5: Khởi tạo LED Matrix display TRƯỚC network
  Serial.println("Initializing LED Matrix display...");
  HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, _pins);
  
  // Cấu hình để tránh xung đột với WiFi - tối ưu hóa timing
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_8M;   // Giảm thêm tốc độ để tránh xung đột với WiFi
  mxconfig.latch_blanking = 6;                 // Tăng latch blanking để ổn định hơn
  mxconfig.clkphase = false;                   // Clock phase
  mxconfig.double_buff = false;                // DISABLE double buffering để fix LED không sáng
  // Giảm tốc độ I2S xuống 8MHz để tránh xung đột với WiFi 2.4GHz
  
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  
  // Kiểm tra khởi tạo thành công
  if (!dma_display) {
    Serial.println("FAILED to create LED Matrix display!");
    return;
  }
  
  dma_display->begin();
  Serial.println("LED Matrix display object created successfully");
  
  // CRITICAL: Force enable output và kiểm tra OE pin
  Serial.println("Checking OE_PIN (Output Enable)...");
  pinMode(OE_PIN, OUTPUT);
  digitalWrite(OE_PIN, LOW);  // FORCE LOW để enable output
  Serial.println("OE_PIN forced to LOW (enabled)");
  
  // Delay để đảm bảo hardware stable
  delay(100);
  
  displayBrightness = 100;
  dma_display->setBrightness8(255);
  Serial.println("Display brightness fixed at 100%");
  
  // Khởi tạo colors cho display
  myBLACK = dma_display->color565(0, 0, 0);
  myWHITE = dma_display->color565(255, 255, 255);
  myRED = dma_display->color565(255, 0, 0);
  myGREEN = dma_display->color565(0, 255, 0);
  myBLUE = dma_display->color565(0, 0, 255);
  myYELLOW = dma_display->color565(255, 255, 0);
  myCYAN = dma_display->color565(0, 255, 255);
  
  // Test LED matrix cơ bản
  Serial.println("Testing LED Matrix...");
  dma_display->clearScreen();
  
  // Test với fill màu sáng
  dma_display->fillScreen(myWHITE);
  delay(1000);
  dma_display->fillScreen(myRED);
  delay(1000);
  dma_display->clearScreen();
  
  // Test text - "0989328858" căn giữa
  String phoneText = "0989328858";
  dma_display->setTextSize(1.6);
  dma_display->setTextColor(myGREEN);
  
  // Tính toán vị trí căn giữa cho phone number (text size 2)
  int16_t x1, y1;
  uint16_t w, h;
  dma_display->getTextBounds(phoneText, 0, 0, &x1, &y1, &w, &h);
  int totalWidth = PANEL_RES_X * PANEL_CHAIN;
  int phoneX = (totalWidth - w) / 2;
  
  dma_display->setCursor(phoneX, 0);
  dma_display->print(phoneText);
  delay(2000);
  Serial.println("LED Matrix test completed");
  
  // Hiển thị "Connecting" NGAY LẬP TỨC và LIÊN TỤC
  showConnectingAnimation = true;
  systemConnected = false;
  connectingAnimationTime = 0; // Bắt buộc update ngay lần đầu
  connectingDots = 0;
  
  // Hiển thị Connecting ngay lập tức (không chờ timer)
  dma_display->clearScreen();
  
  // "BỘ ĐẾM THÔNG MINH" căn giữa
  drawVietnameseText(10, 10, "BỘ ĐẾM THÔNG MINH", myYELLOW, 1);
  
  // Dấu chấm căn giữa (text size 2)
  dma_display->setTextSize(1.6);
  dma_display->setTextColor(myCYAN);
  dma_display->getTextBounds(".", 0, 0, &x1, &y1, &w, &h);
  int dotX = (totalWidth - w) / 2;
  dma_display->setCursor(dotX, 20);
  dma_display->print(".");
  
  Serial.println("Displaying 'Connecting' while initializing services...");
  delay(1000); // Cho user thấy "Connecting" trước khi bắt đầu init services
  
  // BƯỚC 6: Khởi tạo network và services
  Serial.println("Starting network setup...");
  ESP32_W5500_onEvent();
  
  setupNetwork();
  Serial.println("Network initialized");
  delay(1000); // Chờ network ổn định
  
  Serial.println("Starting realtime WebSocket setup...");
  setupRealtimeTransport();
  Serial.println("Realtime WebSocket initialized");
  delay(1000);
  
  // Chỉ setup Time sync khi có kết nối Internet
  if (currentNetworkMode != WIFI_AP_MODE) {
    Serial.println("Starting Time sync setup...");
    setupTime();
    Serial.println("Time sync initialized");
    delay(1000); // Chờ Time sync ổn định
  } else {
    Serial.println("Skipping Time sync (AP mode - no Internet connection)");
  }
  
  Serial.println("Starting Web server setup...");
  setupWebServer();
  Serial.println("Web server initialized");
  delay(1000); // Chờ Web server ổn định
  
  // KẾT THÚC NETWORK SETUP - CHO PHÉP LED MATRIX HOẠT ĐỘNG LẠI
  Serial.println("All network services ready - LED Matrix can resume");
  
  // Tất cả services đã sẵn sàng - hiển thị "Connecting" ít hơn để giảm flicker
  Serial.println("All services ready! Showing connecting animation for 3 more seconds...");
  unsigned long finishTime = millis() + 3000;
  while (millis() < finishTime) {
    showConnectingDisplay();
    delay(200);
    Serial.print(".");
  }
  Serial.println(); // New line after dots
  
  setSystemConnected();
  
  // FORCE UPDATE LED MATRIX NGAY SAU KHI SETUP XONG
  Serial.println("Force updating LED display after setup...");
  needUpdate = true;
  updateDisplay();
  
  Serial.println("Setup completed successfully!");
  Serial.println("System ready - Web interface available");
  
  // Hiển thị IP đúng theo network mode
  String ipAddress = "Unknown";
  if (currentNetworkMode == ETHERNET_MODE) {
    ipAddress = ETH.localIP().toString();
  } else if (currentNetworkMode == WIFI_STA_MODE) {
    ipAddress = WiFi.localIP().toString();
  } else if (currentNetworkMode == WIFI_AP_MODE) {
    ipAddress = WiFi.softAPIP().toString();
  }
  
  Serial.println("IP: " + ipAddress);
  Serial.println("Network Mode: " + String(currentNetworkMode == ETHERNET_MODE ? "Ethernet" : 
                                            currentNetworkMode == WIFI_STA_MODE ? "WiFi STA" : "WiFi AP"));
  
  // Hiển thị trạng thái dữ liệu
  printDataStatus();
  
  // Debug settings file 
  debugSettingsFile();
}

void loop() {
  handleQrReader();

  // Ensure warning LED timeout is evaluated continuously so the LED will
  // turn off after the configured 5 second window even when no new
  // bag count events occur.
  updateDoneLED();
  // Update LED display LUÔN LUÔN nếu cần thiết
  if (needUpdate || (millis() - lastUpdate > 1000)) { // Update mỗi 1 giây thay vì 2 giây
    updateDisplay();
    updateStartLED();  // Luôn cập nhật đèn START
    lastUpdate = millis();
    needUpdate = false; // Reset flag
  }
  
  // Cập nhật animation "Connecting" nếu chưa kết nối xong
  if (!systemConnected && dma_display) {
    showConnectingDisplay();
  }

  // FORCE SET needUpdate = true periodically để đảm bảo LED luôn update
  static unsigned long lastForceUpdate = 0;
  if (millis() - lastForceUpdate > 5000) { // Force update mỗi 5 giây
    needUpdate = true;
    lastForceUpdate = millis();
  }
  
  // RELAY DELAY TIMER - Kiểm tra và tắt relay sau khi hoàn thành đơn hàng
  if (isRelayDelayActive && isOrderComplete) {
    if (millis() - orderCompleteTime >= relayDelayAfterComplete) {
      // Hết thời gian delay, tắt relay
      isRelayDelayActive = false;
      isOrderComplete = false;
      Serial.println("RELAY DELAY FINISHED - Relay will be turned OFF by updateStartLED()");
      // Relay sẽ được tắt tự động trong updateStartLED() khi isRelayDelayActive = false
    }
  }
  
  // Xử lý IR Remote
  if (irrecv.decode(&results)) {
    unsigned long now = millis();
    unsigned long code = results.value;

    // Bỏ qua mã lặp 0xFFFFFFFF
    if (code == 0xFFFFFFFF) {
      irrecv.resume();
    } else {
      // Chống nhấn lặp quá nhanh
      if (now - lastIRTime > debounceIRTime || code != lastIRCode) {
        lastIRTime = now;
        lastIRCode = code;

        unsigned long btn = mapIRButton(code);
        if (btn > 0) {
          Serial.print("IR Remote - Nhan nut: ");
          Serial.println(btn);
          Serial.print("IR Remote - Ma hex: 0x");
          Serial.println(code, HEX);
          Serial.print("Trang thai truoc khi xu ly - isRunning: ");
          Serial.print(isRunning);
          Serial.print(", isTriggerEnabled: ");
          Serial.print(isTriggerEnabled);
          Serial.print(", isCountingEnabled: ");
          Serial.println(isCountingEnabled);
          handleIRCommand(btn);
          Serial.print("Trang thai sau khi xu ly - isRunning: ");
          Serial.print(isRunning);
          Serial.print(", isTriggerEnabled: ");
          Serial.print(isTriggerEnabled);
          Serial.print(", isCountingEnabled: ");
          Serial.println(isCountingEnabled);
        } else {
          Serial.print("IR Remote - Ma khong xac dinh: 0x");
          Serial.println(code, HEX);
        }
      }
    }
    irrecv.resume(); // Chuẩn bị nhận tiếp
  }

  // Đo thời gian sensor độc lập với trạng thái START/PAUSE để màn hình cài đặt
  // luôn nhận được thời gian bao đi qua cảm biến.
  updateSensorTimingMeasurement();

  // Chỉ xử lý cảm biến khởi động khi được kích hoạt.
  // GPIO4 là input trigger, GPIO39 là output trigger.
  if (isTriggerEnabled) {
    int inputTriggerReading = digitalRead(TRIGGER_SENSOR_PIN);
    int outputTriggerReading = digitalRead(OUTPUT_TRIGGER_SENSOR_PIN);
    
    if (inputTriggerReading != lastTriggerState) {
      inputTriggerDebounceTime = millis();
    }
    
    if ((millis() - inputTriggerDebounceTime) > debounceDelay) {
      bool inputBlocked = isInputTriggerBlocked(inputTriggerReading);
      if (inputTriggerReading != triggerState || inputBlocked != inputTriggerBlockedState) {
        triggerState = inputTriggerReading;
        inputTriggerBlockedState = inputBlocked;

        if (inputBlocked) {  // Khi phát hiện vật thể ở chiều nhập
          if (currentMode != "input") {
            currentMode = "input";
            needUpdate = true;
            Serial.println("TRIGGER SENSOR GPIO4: Mode -> input");
          } else {
            Serial.println("TRIGGER SENSOR GPIO4: Mode input");
          }
          isCountingEnabled = true;  // Kích hoạt cảm biến đếm
          Serial.println("TRIGGER SENSOR GPIO4: Phat hien vat the -> Kich hoat dem! Active=" + String(sensorLevelName(inputSensorActiveLevel)));
          Serial.print("isCountingEnabled = ");
          Serial.println(isCountingEnabled);
          publishStatusMQTT();
          publishSensorData();
        } else {
          Serial.println("TRIGGER SENSOR GPIO4: Khong con vat the");
        }
      }
    }
    lastTriggerState = inputTriggerReading;

    if (outputTriggerReading != lastOutputTriggerState) {
      outputTriggerDebounceTime = millis();
    }

    if ((millis() - outputTriggerDebounceTime) > debounceDelay) {
      bool outputBlocked = isOutputTriggerBlocked(outputTriggerReading);
      if (outputTriggerReading != outputTriggerState || outputBlocked != outputTriggerBlockedState) {
        outputTriggerState = outputTriggerReading;
        outputTriggerBlockedState = outputBlocked;

        if (outputBlocked) {  // Khi phát hiện vật thể ở chiều xuất
          if (currentMode != "output") {
            currentMode = "output";
            needUpdate = true;
            Serial.println("TRIGGER SENSOR GPIO39: Mode -> output");
          } else {
            Serial.println("TRIGGER SENSOR GPIO39: Mode output");
          }
          isCountingEnabled = true;  // Kích hoạt cảm biến đếm
          Serial.println("TRIGGER SENSOR GPIO39: Phat hien vat the -> Kich hoat dem! Active=" + String(sensorLevelName(outputSensorActiveLevel)));
          Serial.print("isCountingEnabled = ");
          Serial.println(isCountingEnabled);
          publishStatusMQTT();
          publishSensorData();
        } else {
          Serial.println("TRIGGER SENSOR GPIO39: Khong con vat the");
        }
      }
    }
    lastOutputTriggerState = outputTriggerReading;
  }

  // Chỉ đếm khi được kích hoạt - SỬ DỤNG SETTINGS ĐỒNG BỘ
  if (isCountingEnabled && isRunning && isStartAuthorized && !isLimitReached) {
    int reading = digitalRead(SENSOR_PIN);

    // Sau khi START/chuyển đơn, chờ sensor hết bị che rồi mới cho đếm
    if (waitForSensorClearOnStart) {
      if (!isSensorBlocked(reading)) {
        waitForSensorClearOnStart = false;
        isBagDetected = false;
        bagStartTime = 0;
        lastDebounceTime = millis();
        Serial.println("Sensor clear detected after START, begin counting");
      } else {
        if (isBagDetected) {
          isBagDetected = false;
        }
        lastSensorState = reading;
        sensorState = reading;
        lastDebounceTime = millis();
        reading = lastSensorState;
      }
    }
    
    // Sử dụng sensorDelayMs từ settings thay vì debounceDelay cố định
    if (reading != lastSensorState) {
      lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > sensorDelayMs) {
      if (reading != sensorState) {
        sensorState = reading;
        
        // IN RA TRẠNG THÁI SENSOR KHI CÓ THAY ĐỔI
        Serial.print("SENSOR THAY ĐỔI: ");
        Serial.println(isSensorBlocked(sensorState) ? String(sensorLevelName(countSensorActiveLevel)) + " (có vật thể)" : String(sensorRawStateName(sensorState)) + " (không có vật thể)");

        if (isSensorBlocked(sensorState)) {  // LOW khi phát hiện bao
          unsigned long currentTime = millis();
          
          // Kiểm tra khoảng cách tối thiểu giữa 2 bao (minBagInterval từ settings)
          if (currentTime - lastBagTime >= minBagInterval) {
            
            if (!isBagDetected) {
              // Bắt đầu phát hiện bao mới
              isBagDetected = true;
              bagStartTime = currentTime;
              sensorClearStartTime = 0;
              isWaitingForBagGroupEnd = false;
              int dynamicDelay = calculateDynamicBagDetectionDelay();
              Serial.print("BẮT ĐẦU phát hiện bao - thời gian xác nhận: ");
              Serial.print(dynamicDelay);
              Serial.println("ms");
            } else if (isWaitingForBagGroupEnd) {
              isWaitingForBagGroupEnd = false;
              sensorClearStartTime = 0;
              Serial.println("Sensor che lại trong khoảng gom nhóm - nối tiếp cùng nhóm bao");
            }
            
          } else {
            Serial.print("Chờ khoảng cách tối thiểu (");
            Serial.print(minBagInterval);
            Serial.print("ms), còn lại: ");
            Serial.print(minBagInterval - (currentTime - lastBagTime));
            Serial.println("ms");
          }
          
        } else {
          // Sensor đã nhả về CLEAR: chờ thêm một khoảng ngắn để gom các bao sát nhau.
          if (isBagDetected && !isWaitingForBagGroupEnd) {
            sensorClearStartTime = millis();
            isWaitingForBagGroupEnd = true;
            Serial.print("Sensor nhả - chờ gom nhóm ");
            Serial.print(bagGroupGapToleranceMs());
            Serial.println("ms trước khi chốt số bao");
          }
        }
      }

      if (isBagDetected && isWaitingForBagGroupEnd && !isSensorBlocked(sensorState) &&
          (millis() - sensorClearStartTime >= bagGroupGapToleranceMs())) {
        unsigned long detectionDuration = sensorClearStartTime - bagStartTime;

        // Kiểm tra thời gian xác nhận đủ lâu (dynamic bagDetectionDelay từ weight-based settings)
        int dynamicDelay = calculateDynamicBagDetectionDelay();
        if (detectionDuration >= dynamicDelay) {
          // XÁC NHẬN BAO HỢP LỆ - ĐẾM!
          int bagCount = calculateBagCountFromDuration(detectionDuration);
          Serial.print("XÁC NHẬN NHÓM BAO! Thời gian phát hiện: ");
          Serial.print(detectionDuration);
          Serial.print("ms >= ");
          Serial.print(dynamicDelay);
          Serial.print("ms. Phát hiện ");
          Serial.print(bagCount);
          Serial.print(" bao. Count: ");
          Serial.print(totalCount);
          Serial.print(" -> ");
          Serial.println(totalCount + bagCount);

          updateCount(bagCount);
          needUpdate = true;
          lastBagTime = millis();

          // MQTT: Publish sensor data khi đếm thành công
          publishSensorData();

        } else {
          Serial.print("BAO KHÔNG HỢP LỆ - thời gian quá ngắn: ");
          Serial.print(detectionDuration);
          Serial.print("ms < ");
          Serial.print(dynamicDelay);
          Serial.println("ms");
        }

        isBagDetected = false;
        isWaitingForBagGroupEnd = false;
        sensorClearStartTime = 0;
      }

      // Chỉ xác nhận đếm sau khi sensor đã nhả về HIGH và hết cửa sổ gom nhóm.
      // Điều này tránh đếm ảo khi sensor giữ LOW, đồng thời không mất các bao sát nhau có khe hở ngắn.
    }
    lastSensorState = reading;
    
  } else if (isCountingEnabled && !isRunning) {
    // Đã kích hoạt counting nhưng hệ thống đang pause
    int reading = digitalRead(SENSOR_PIN);
    if (isSensorBlocked(reading)) {
      Serial.println("Phát hiện bao nhưng hệ thống đang PAUSE");
    }
  }

  if (isLimitReached && !finishedBlinking) {
    if (isBlinking && millis() - lastBlink >= 250) {
      blinkCount++;
      lastBlink = millis();
      if (blinkCount >= 10) {
        isBlinking = false;
        finishedBlinking = true;
      }
      needUpdate = true;
    }
  }

  // Kiểm tra và cập nhật thời gian bắt đầu nếu đang chờ đồng bộ
  if (timeWaitingForSync && time(nullptr) > 24 * 3600) {
    startTimeStr = getTimeStr();
    timeWaitingForSync = false;
    Serial.print("Time sync completed - Start time updated to: ");
    Serial.println(startTimeStr);
  }
  
  realtimeSocket.cleanupClients();

  // Realtime WebSocket publish hoạt động cho cả Ethernet, WiFi STA và AP mode
  if (millis() - lastMqttPublish > MQTT_PUBLISH_INTERVAL) {
    publishStatusMQTT();

    if (isCountingEnabled || isTriggerEnabled) {
      publishSensorData();
    }

    lastMqttPublish = millis();
  }

  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    publishHeartbeat();
    lastHeartbeat = millis();
  }
  
  // Handle MQTT2 Client (Server anh Dũng) - CHỈ KHI CÓ INTERNET VÀ KEYLOGIN
  if (currentNetworkMode != WIFI_AP_MODE && mqtt2_password.length() > 0) {
    if (!mqtt2.connected()) {
      static unsigned long lastMqtt2ReconnectAttempt = 0;
      if (millis() - lastMqtt2ReconnectAttempt > 10000) { // 10s interval
        lastMqtt2ReconnectAttempt = millis();
        Serial.println("Attempting MQTT2 reconnection...");
        setupMQTT2();
      }
    } else {
      // MQTT2 connected - handle messages
      mqtt2.loop();
    }
  }
  
  // Web server handling
  server.handleClient();
  
  // Feed watchdog to prevent timeout
  yield();
  
  // Monitor memory periodically
  static unsigned long lastMemoryCheck = 0;
  if (millis() - lastMemoryCheck > 30000) { // Check every 30 seconds
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10000) {
      Serial.println("WARNING: Low memory detected - " + String(freeHeap) + " bytes free");
    }
    lastMemoryCheck = millis();
  }
  
  // Cập nhật hệ thống với delay tối ưu
  delay(50);  // Giảm từ 100ms xuống 50ms để cải thiện responsiveness
}
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
