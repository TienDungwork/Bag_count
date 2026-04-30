#include "display.h"

//----------------------------------------Display Functions
void updateDisplay() {
  // Kiểm tra dma_display có khả dụng không
  if (!dma_display) {
    Serial.println("dma_display is null");
    return;
  }
  
  // Nếu chưa kết nối xong, hiển thị "Connecting"
  if (!systemConnected) {
    showConnectingDisplay();
    return;
  }
  
  if (isLimitReached && isBlinking && (blinkCount % 2 == 1)) {
    dma_display->fillScreen(myWHITE);
    return;
  }
  
  dma_display->clearScreen();
  
  //  LAYOUT LED THEO YÊU CẦU :
  // ┌─────────────────────┬──────────────┐
  // │ GAO THUONG (Size 2) │   "85" Size3 │  
  // │ XUAT: 100           │   (màu đỏ)   │
  // └─────────────────────┴──────────────┘
  
  // Chuyển đổi tên loại bao không dấu
  String displayType = bagType;
  //Serial.println("Displaying product: " + bagType + " -> " + displayType);
  displayType.replace("ạ", "a");
  displayType.replace("ă", "a"); 
  displayType.replace("â", "a");
  displayType.replace("á", "a");
  displayType.replace("à", "a");
  displayType.replace("ã", "a");
  displayType.replace("ả", "a");
  displayType.replace("ậ", "a");
  displayType.replace("ắ", "a");
  displayType.replace("ằ", "a");
  displayType.replace("ẵ", "a");
  displayType.replace("ẳ", "a");
  displayType.replace("ấ", "a");
  displayType.replace("ầ", "a");
  displayType.replace("ẫ", "a");
  displayType.replace("ẩ", "a");
  displayType.replace("ê", "e");
  displayType.replace("é", "e");
  displayType.replace("è", "e");
  displayType.replace("ẽ", "e");
  displayType.replace("ẻ", "e");
  displayType.replace("ế", "e");
  displayType.replace("ề", "e");
  displayType.replace("ễ", "e");
  displayType.replace("ể", "e");
  displayType.replace("ệ", "e");
  displayType.replace("í", "i");
  displayType.replace("ì", "i");
  displayType.replace("ĩ", "i");
  displayType.replace("ỉ", "i");
  displayType.replace("ị", "i");
  displayType.replace("ô", "o");
  displayType.replace("ơ", "o");
  displayType.replace("ó", "o");
  displayType.replace("ò", "o");
  displayType.replace("õ", "o");
  displayType.replace("ỏ", "o");
  displayType.replace("ọ", "o");
  displayType.replace("ố", "o");
  displayType.replace("ồ", "o");
  displayType.replace("ỗ", "o");
  displayType.replace("ổ", "o");
  displayType.replace("ộ", "o");
  displayType.replace("ớ", "o");
  displayType.replace("ờ", "o");
  displayType.replace("ỡ", "o");
  displayType.replace("ở", "o");
  displayType.replace("ợ", "o");
  displayType.replace("ư", "u");
  displayType.replace("ú", "u");
  displayType.replace("ù", "u");
  displayType.replace("ũ", "u");
  displayType.replace("ủ", "u");
  displayType.replace("ụ", "u");
  displayType.replace("ứ", "u");
  displayType.replace("ừ", "u");
  displayType.replace("ữ", "u");
  displayType.replace("ử", "u");
  displayType.replace("ự", "u");
  displayType.replace("ý", "y");
  displayType.replace("ỳ", "y");
  displayType.replace("ỹ", "y");
  displayType.replace("ỷ", "y");
  displayType.replace("ỵ", "y");
  displayType.replace("đ", "d");
  displayType.replace("Đ", "D");
  displayType.toUpperCase();
  
  // Rút gọn tên sản phẩm nếu quá dài (size 2)
  if (displayType.length() > 5) {
    displayType = displayType.substring(0, 4) + "..";
  }
  
  //  DÒNG 1: Mã sản phẩm bên trái (Size 2)
  dma_display->setTextSize(1.2);
  dma_display->setTextColor(myYELLOW);
    // Hiển thị mã sản phẩm, nếu dài thì xuống dòng(>10)
    String displayText = (productCode != "") ? productCode : (displayType != "") ? displayType : "HET DON";
    int maxCodeLen = 10;
    if (displayText.length() > maxCodeLen) {
      String line1 = displayText.substring(0, maxCodeLen);
      String line2 = displayText.substring(maxCodeLen);
      // Dòng trên
      dma_display->setCursor(1, 2);
      dma_display->print(line1);
      // Dòng dưới
      dma_display->setCursor(1, 10);
      dma_display->print(line2);
    } else {
      dma_display->setCursor(1, 2);
      dma_display->print(displayText);
    }

  // SỐ ĐẾM LỚN BÊN PHẢI DÒNG 1 (Size 3, màu đỏ)
  String countStr = String((int)totalCount);
  dma_display->setTextSize(4);
  dma_display->setTextColor(myGREEN);
  
  // Tính toán vị trí căn phải
  int16_t x1, y1;
  uint16_t w, h;
  dma_display->getTextBounds(countStr, 0, 0, &x1, &y1, &w, &h);
  
  // Đặt ở bên phải màn hình
  int totalWidth = PANEL_RES_X * PANEL_CHAIN;
  int x = totalWidth - w;  // 2 pixel margin từ bên phải
  int y = 2;  // Căn với dòng 1
  
  dma_display->setCursor(x, y);
  dma_display->print(countStr);
  
  // DÒNG 2: Hiển thị "XUẤT" hoặc "NHẬP" theo mode với số lượng đơn hàng hiện tại (Size 2)
  dma_display->setTextSize(2);
  dma_display->setTextColor(myCYAN);
  dma_display->setCursor(1, 18);  // Dòng 2 ở y=18
  
  // Hiển thị prefix theo mode và số lượng kế hoạch của đơn hàng hiện tại
  String line2;
  if (currentMode == "output") {
    line2 = "X:" + String(targetCount);  // XUẤT mode
  } else {
    line2 = "N:" + String(targetCount);  // NHẬP mode
  }
  dma_display->print(line2);
  
  needUpdate = false;
}

//----------------------------------------Display Current Order Info Function
void displayCurrentOrderInfo() {
  // Gọi updateDisplay để cập nhật thông tin hiện tại
  needUpdate = true;
  updateDisplay();
}

//----------------------------------------Connecting Display Function
void showConnectingDisplay() {
  if (!dma_display) {
    Serial.println("dma_display is null!");
    return;
  }
  
  if (!showConnectingAnimation) {
    Serial.println("showConnectingAnimation is false");
    return;
  }
  
  if (systemConnected) {
    Serial.println("systemConnected is true");
    return;
  }
  
  // Animation mỗi 1000ms
  if (connectingAnimationTime == 0 || millis() - connectingAnimationTime > 1000) {
    connectingAnimationTime = millis();
    connectingDots = (connectingDots + 1) % 4; // 0-3 dots
    
    dma_display->clearScreen();
    
    // Hiển thị "chữ khi đang kết nối"
    dma_display->setTextSize(1.2);
    dma_display->setTextColor(myYELLOW);
    dma_display->setCursor(8, 10);
    dma_display->print("BO DEM THONG MINH");
    
    // Hiển thị dots
    dma_display->setTextSize(1);
    dma_display->setTextColor(myCYAN);
    dma_display->setCursor(8, 20);
    
    // Hiển thị dots
    for (int i = 0; i < connectingDots; i++) {
      dma_display->print(".");
    }
    
    Serial.println("Connecting display successfully with " + String(connectingDots) + " dots");
  }
}

//----------------------------------------Set System Connected
void setSystemConnected() {
  if (!systemConnected) {
    systemConnected = true;
    showConnectingAnimation = false;
    needUpdate = true;  // Trigger display update to normal layout
    
    Serial.println("System fully connected");
    
    // Cập nhật display ngay
    updateDisplay();
  }
}

