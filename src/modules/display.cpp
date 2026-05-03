#include "display.h"

struct VietnameseGlyph {
  char base;
  char topMark;
  char shapeMark;
  bool dotBelow;
  bool stroke;
};

static void drawAccentMark(int x, int y, char mark, uint16_t color) {
  if (!dma_display || mark == 0) return;

  switch (mark) {
    case '\'':
      dma_display->drawPixel(x + 3, y, color);
      dma_display->drawPixel(x + 2, y + 1, color);
      break;
    case '`':
      dma_display->drawPixel(x + 2, y, color);
      dma_display->drawPixel(x + 3, y + 1, color);
      break;
    case '?':
      dma_display->drawPixel(x + 2, y, color);
      dma_display->drawPixel(x + 3, y, color);
      dma_display->drawPixel(x + 3, y + 1, color);
      dma_display->drawPixel(x + 2, y + 2, color);
      break;
    case '~':
      dma_display->drawPixel(x + 1, y + 1, color);
      dma_display->drawPixel(x + 2, y, color);
      dma_display->drawPixel(x + 3, y + 1, color);
      dma_display->drawPixel(x + 4, y, color);
      break;
  }
}

static void drawShapeMark(int x, int y, char mark, uint16_t color) {
  if (!dma_display || mark == 0) return;

  switch (mark) {
    case '^':
      dma_display->drawPixel(x + 2, y + 1, color);
      dma_display->drawPixel(x + 3, y, color);
      dma_display->drawPixel(x + 4, y + 1, color);
      break;
    case '(':
      dma_display->drawPixel(x + 1, y, color);
      dma_display->drawPixel(x + 2, y + 1, color);
      dma_display->drawPixel(x + 3, y + 1, color);
      dma_display->drawPixel(x + 4, y, color);
      break;
    case '+':
      dma_display->drawPixel(x + 4, y + 1, color);
      dma_display->drawPixel(x + 5, y, color);
      break;
  }
}

static bool glyphColumns(char c, uint8_t out[5]) {
  switch (c) {
    case ' ': out[0] = 0x00; out[1] = 0x00; out[2] = 0x00; out[3] = 0x00; out[4] = 0x00; return true;
    case '-': out[0] = 0x08; out[1] = 0x08; out[2] = 0x08; out[3] = 0x08; out[4] = 0x08; return true;
    case '.': out[0] = 0x00; out[1] = 0x00; out[2] = 0x60; out[3] = 0x60; out[4] = 0x00; return true;
    case '/': out[0] = 0x20; out[1] = 0x10; out[2] = 0x08; out[3] = 0x04; out[4] = 0x02; return true;
    case ':': out[0] = 0x00; out[1] = 0x00; out[2] = 0x14; out[3] = 0x00; out[4] = 0x00; return true;
    case '0': out[0] = 0x3E; out[1] = 0x51; out[2] = 0x49; out[3] = 0x45; out[4] = 0x3E; return true;
    case '1': out[0] = 0x00; out[1] = 0x42; out[2] = 0x7F; out[3] = 0x40; out[4] = 0x00; return true;
    case '2': out[0] = 0x72; out[1] = 0x49; out[2] = 0x49; out[3] = 0x49; out[4] = 0x46; return true;
    case '3': out[0] = 0x21; out[1] = 0x41; out[2] = 0x49; out[3] = 0x4D; out[4] = 0x33; return true;
    case '4': out[0] = 0x18; out[1] = 0x14; out[2] = 0x12; out[3] = 0x7F; out[4] = 0x10; return true;
    case '5': out[0] = 0x27; out[1] = 0x45; out[2] = 0x45; out[3] = 0x45; out[4] = 0x39; return true;
    case '6': out[0] = 0x3C; out[1] = 0x4A; out[2] = 0x49; out[3] = 0x49; out[4] = 0x31; return true;
    case '7': out[0] = 0x41; out[1] = 0x21; out[2] = 0x11; out[3] = 0x09; out[4] = 0x07; return true;
    case '8': out[0] = 0x36; out[1] = 0x49; out[2] = 0x49; out[3] = 0x49; out[4] = 0x36; return true;
    case '9': out[0] = 0x46; out[1] = 0x49; out[2] = 0x49; out[3] = 0x29; out[4] = 0x1E; return true;
    case 'A': out[0] = 0x7C; out[1] = 0x12; out[2] = 0x11; out[3] = 0x12; out[4] = 0x7C; return true;
    case 'B': out[0] = 0x7F; out[1] = 0x49; out[2] = 0x49; out[3] = 0x49; out[4] = 0x36; return true;
    case 'C': out[0] = 0x3E; out[1] = 0x41; out[2] = 0x41; out[3] = 0x41; out[4] = 0x22; return true;
    case 'D': out[0] = 0x7F; out[1] = 0x41; out[2] = 0x41; out[3] = 0x41; out[4] = 0x3E; return true;
    case 'E': out[0] = 0x7F; out[1] = 0x49; out[2] = 0x49; out[3] = 0x49; out[4] = 0x41; return true;
    case 'F': out[0] = 0x7F; out[1] = 0x09; out[2] = 0x09; out[3] = 0x09; out[4] = 0x01; return true;
    case 'G': out[0] = 0x3E; out[1] = 0x41; out[2] = 0x41; out[3] = 0x51; out[4] = 0x73; return true;
    case 'H': out[0] = 0x7F; out[1] = 0x08; out[2] = 0x08; out[3] = 0x08; out[4] = 0x7F; return true;
    case 'I': out[0] = 0x00; out[1] = 0x41; out[2] = 0x7F; out[3] = 0x41; out[4] = 0x00; return true;
    case 'J': out[0] = 0x20; out[1] = 0x40; out[2] = 0x41; out[3] = 0x3F; out[4] = 0x01; return true;
    case 'K': out[0] = 0x7F; out[1] = 0x08; out[2] = 0x14; out[3] = 0x22; out[4] = 0x41; return true;
    case 'L': out[0] = 0x7F; out[1] = 0x40; out[2] = 0x40; out[3] = 0x40; out[4] = 0x40; return true;
    case 'M': out[0] = 0x7F; out[1] = 0x02; out[2] = 0x1C; out[3] = 0x02; out[4] = 0x7F; return true;
    case 'N': out[0] = 0x7F; out[1] = 0x04; out[2] = 0x08; out[3] = 0x10; out[4] = 0x7F; return true;
    case 'O': out[0] = 0x3E; out[1] = 0x41; out[2] = 0x41; out[3] = 0x41; out[4] = 0x3E; return true;
    case 'P': out[0] = 0x7F; out[1] = 0x09; out[2] = 0x09; out[3] = 0x09; out[4] = 0x06; return true;
    case 'Q': out[0] = 0x3E; out[1] = 0x41; out[2] = 0x51; out[3] = 0x21; out[4] = 0x5E; return true;
    case 'R': out[0] = 0x7F; out[1] = 0x09; out[2] = 0x19; out[3] = 0x29; out[4] = 0x46; return true;
    case 'S': out[0] = 0x26; out[1] = 0x49; out[2] = 0x49; out[3] = 0x49; out[4] = 0x32; return true;
    case 'T': out[0] = 0x03; out[1] = 0x01; out[2] = 0x7F; out[3] = 0x01; out[4] = 0x03; return true;
    case 'U': out[0] = 0x3F; out[1] = 0x40; out[2] = 0x40; out[3] = 0x40; out[4] = 0x3F; return true;
    case 'V': out[0] = 0x1F; out[1] = 0x20; out[2] = 0x40; out[3] = 0x20; out[4] = 0x1F; return true;
    case 'W': out[0] = 0x3F; out[1] = 0x40; out[2] = 0x38; out[3] = 0x40; out[4] = 0x3F; return true;
    case 'X': out[0] = 0x63; out[1] = 0x14; out[2] = 0x08; out[3] = 0x14; out[4] = 0x63; return true;
    case 'Y': out[0] = 0x03; out[1] = 0x04; out[2] = 0x78; out[3] = 0x04; out[4] = 0x03; return true;
    case 'Z': out[0] = 0x61; out[1] = 0x59; out[2] = 0x49; out[3] = 0x4D; out[4] = 0x43; return true;
  }
  return false;
}

static int scale15(int value) {
  return (value * 3) / 2;
}

static void drawScaledPixel15(int x, int y, int srcX, int srcY, uint16_t color) {
  int x0 = x + scale15(srcX);
  int x1 = x + scale15(srcX + 1);
  int y0 = y + scale15(srcY);
  int y1 = y + scale15(srcY + 1);
  dma_display->fillRect(x0, y0, std::max(1, x1 - x0), std::max(1, y1 - y0), color);
}

static void drawScaledAsciiChar15(int x, int y, char c, uint16_t color) {
  uint8_t columns[5];
  if (!glyphColumns(c, columns)) {
    dma_display->setTextSize(1);
    dma_display->setTextColor(color);
    dma_display->setCursor(x, y + 2);
    dma_display->print(c);
    return;
  }

  for (int col = 0; col < 5; col++) {
    uint8_t line = columns[col];
    for (int row = 0; row < 8; row++, line >>= 1) {
      if (line & 1) {
        drawScaledPixel15(x, y, col, row, color);
      }
    }
  }
}

static void drawAccentMark15(int x, int y, char mark, uint16_t color) {
  if (!dma_display || mark == 0) return;

  switch (mark) {
    case '\'':
      drawScaledPixel15(x, y, 3, 0, color);
      drawScaledPixel15(x, y, 2, 1, color);
      break;
    case '`':
      drawScaledPixel15(x, y, 2, 0, color);
      drawScaledPixel15(x, y, 3, 1, color);
      break;
    case '?':
      drawScaledPixel15(x, y, 2, 0, color);
      drawScaledPixel15(x, y, 3, 0, color);
      drawScaledPixel15(x, y, 3, 1, color);
      drawScaledPixel15(x, y, 2, 2, color);
      break;
    case '~':
      drawScaledPixel15(x, y, 1, 1, color);
      drawScaledPixel15(x, y, 2, 0, color);
      drawScaledPixel15(x, y, 3, 1, color);
      drawScaledPixel15(x, y, 4, 0, color);
      break;
  }
}

static void drawShapeMark15(int x, int y, char mark, uint16_t color) {
  if (!dma_display || mark == 0) return;

  switch (mark) {
    case '^':
      drawScaledPixel15(x, y, 2, 1, color);
      drawScaledPixel15(x, y, 3, 0, color);
      drawScaledPixel15(x, y, 4, 1, color);
      break;
    case '(':
      drawScaledPixel15(x, y, 1, 0, color);
      drawScaledPixel15(x, y, 2, 1, color);
      drawScaledPixel15(x, y, 3, 1, color);
      drawScaledPixel15(x, y, 4, 0, color);
      break;
    case '+':
      drawScaledPixel15(x, y, 4, 1, color);
      drawScaledPixel15(x, y, 5, 0, color);
      break;
  }
}

static VietnameseGlyph glyphFromUtf8(const String& text, int& index) {
  String ch = text.substring(index, index + 1);
  unsigned char first = text[index];
  int charLen = 1;
  if ((first & 0xF0) == 0xE0) charLen = 3;
  else if ((first & 0xE0) == 0xC0) charLen = 2;

  if (charLen > 1 && index + charLen <= (int)text.length()) {
    ch = text.substring(index, index + charLen);
  }
  index += charLen;

  if (ch == "Đ" || ch == "đ") return {'D', 0, 0, false, true};
  if (ch == "Á" || ch == "á") return {'A', '\'', 0, false, false};
  if (ch == "À" || ch == "à") return {'A', '`', 0, false, false};
  if (ch == "Ả" || ch == "ả") return {'A', '?', 0, false, false};
  if (ch == "Ã" || ch == "ã") return {'A', '~', 0, false, false};
  if (ch == "Ạ" || ch == "ạ") return {'A', 0, 0, true, false};
  if (ch == "Â" || ch == "â") return {'A', 0, '^', false, false};
  if (ch == "Ấ" || ch == "ấ") return {'A', '\'', '^', false, false};
  if (ch == "Ầ" || ch == "ầ") return {'A', '`', '^', false, false};
  if (ch == "Ẩ" || ch == "ẩ") return {'A', '?', '^', false, false};
  if (ch == "Ẫ" || ch == "ẫ") return {'A', '~', '^', false, false};
  if (ch == "Ậ" || ch == "ậ") return {'A', 0, '^', true, false};
  if (ch == "Ă" || ch == "ă") return {'A', 0, '(', false, false};
  if (ch == "Ắ" || ch == "ắ") return {'A', '\'', '(', false, false};
  if (ch == "Ằ" || ch == "ằ") return {'A', '`', '(', false, false};
  if (ch == "Ẳ" || ch == "ẳ") return {'A', '?', '(', false, false};
  if (ch == "Ẵ" || ch == "ẵ") return {'A', '~', '(', false, false};
  if (ch == "Ặ" || ch == "ặ") return {'A', 0, '(', true, false};
  if (ch == "É" || ch == "é") return {'E', '\'', 0, false, false};
  if (ch == "È" || ch == "è") return {'E', '`', 0, false, false};
  if (ch == "Ẻ" || ch == "ẻ") return {'E', '?', 0, false, false};
  if (ch == "Ẽ" || ch == "ẽ") return {'E', '~', 0, false, false};
  if (ch == "Ẹ" || ch == "ẹ") return {'E', 0, 0, true, false};
  if (ch == "Ê" || ch == "ê") return {'E', 0, '^', false, false};
  if (ch == "Ế" || ch == "ế") return {'E', '\'', '^', false, false};
  if (ch == "Ề" || ch == "ề") return {'E', '`', '^', false, false};
  if (ch == "Ể" || ch == "ể") return {'E', '?', '^', false, false};
  if (ch == "Ễ" || ch == "ễ") return {'E', '~', '^', false, false};
  if (ch == "Ệ" || ch == "ệ") return {'E', 0, '^', true, false};
  if (ch == "Í" || ch == "í") return {'I', '\'', 0, false, false};
  if (ch == "Ì" || ch == "ì") return {'I', '`', 0, false, false};
  if (ch == "Ỉ" || ch == "ỉ") return {'I', '?', 0, false, false};
  if (ch == "Ĩ" || ch == "ĩ") return {'I', '~', 0, false, false};
  if (ch == "Ị" || ch == "ị") return {'I', 0, 0, true, false};
  if (ch == "Ó" || ch == "ó") return {'O', '\'', 0, false, false};
  if (ch == "Ò" || ch == "ò") return {'O', '`', 0, false, false};
  if (ch == "Ỏ" || ch == "ỏ") return {'O', '?', 0, false, false};
  if (ch == "Õ" || ch == "õ") return {'O', '~', 0, false, false};
  if (ch == "Ọ" || ch == "ọ") return {'O', 0, 0, true, false};
  if (ch == "Ô" || ch == "ô") return {'O', 0, '^', false, false};
  if (ch == "Ố" || ch == "ố") return {'O', '\'', '^', false, false};
  if (ch == "Ồ" || ch == "ồ") return {'O', '`', '^', false, false};
  if (ch == "Ổ" || ch == "ổ") return {'O', '?', '^', false, false};
  if (ch == "Ỗ" || ch == "ỗ") return {'O', '~', '^', false, false};
  if (ch == "Ộ" || ch == "ộ") return {'O', 0, '^', true, false};
  if (ch == "Ơ" || ch == "ơ") return {'O', 0, '+', false, false};
  if (ch == "Ớ" || ch == "ớ") return {'O', '\'', '+', false, false};
  if (ch == "Ờ" || ch == "ờ") return {'O', '`', '+', false, false};
  if (ch == "Ở" || ch == "ở") return {'O', '?', '+', false, false};
  if (ch == "Ỡ" || ch == "ỡ") return {'O', '~', '+', false, false};
  if (ch == "Ợ" || ch == "ợ") return {'O', 0, '+', true, false};
  if (ch == "Ú" || ch == "ú") return {'U', '\'', 0, false, false};
  if (ch == "Ù" || ch == "ù") return {'U', '`', 0, false, false};
  if (ch == "Ủ" || ch == "ủ") return {'U', '?', 0, false, false};
  if (ch == "Ũ" || ch == "ũ") return {'U', '~', 0, false, false};
  if (ch == "Ụ" || ch == "ụ") return {'U', 0, 0, true, false};
  if (ch == "Ư" || ch == "ư") return {'U', 0, '+', false, false};
  if (ch == "Ứ" || ch == "ứ") return {'U', '\'', '+', false, false};
  if (ch == "Ừ" || ch == "ừ") return {'U', '`', '+', false, false};
  if (ch == "Ử" || ch == "ử") return {'U', '?', '+', false, false};
  if (ch == "Ữ" || ch == "ữ") return {'U', '~', '+', false, false};
  if (ch == "Ự" || ch == "ự") return {'U', 0, '+', true, false};
  if (ch == "Ý" || ch == "ý") return {'Y', '\'', 0, false, false};
  if (ch == "Ỳ" || ch == "ỳ") return {'Y', '`', 0, false, false};
  if (ch == "Ỷ" || ch == "ỷ") return {'Y', '?', 0, false, false};
  if (ch == "Ỹ" || ch == "ỹ") return {'Y', '~', 0, false, false};
  if (ch == "Ỵ" || ch == "ỵ") return {'Y', 0, 0, true, false};

  char base = ch[0];
  if (base >= 'a' && base <= 'z') base -= 32;
  return {base, 0, 0, false, false};
}

void drawVietnameseText(int x, int y, const String& text, uint16_t color, float textSize) {
  if (!dma_display) return;

  int cursorX = x;
  int i = 0;
  bool scale15Text = textSize > 1.4f && textSize < 1.6f;
  int intTextSize = (int)textSize;
  int charWidth = scale15Text ? 9 : 6 * intTextSize;
  int topY = std::max(0, y - (scale15Text ? 6 : 4));
  int shapeY = std::max(0, y - (scale15Text ? 3 : 2));
  int dotY = y + (scale15Text ? 12 : 8 * intTextSize);

  dma_display->setTextSize(intTextSize);
  dma_display->setTextColor(color);

  while (i < (int)text.length()) {
    VietnameseGlyph glyph = glyphFromUtf8(text, i);
    if (scale15Text) {
      drawScaledAsciiChar15(cursorX, y, glyph.base, color);
    } else {
      dma_display->setCursor(cursorX, y);
      dma_display->print(glyph.base);
    }

    if (glyph.stroke) {
      if (scale15Text) {
        dma_display->drawLine(cursorX + 2, y + 6, cursorX + 8, y + 3, color);
      } else {
        dma_display->drawLine(cursorX + 1, y + 4, cursorX + 5, y + 2, color);
      }
    }
    if (scale15Text) {
      drawShapeMark15(cursorX, shapeY, glyph.shapeMark, color);
      drawAccentMark15(cursorX, topY, glyph.topMark, color);
    } else {
      drawShapeMark(cursorX, shapeY, glyph.shapeMark, color);
      drawAccentMark(cursorX, topY, glyph.topMark, color);
    }
    if (glyph.dotBelow) {
      if (scale15Text) {
        drawScaledPixel15(cursorX, dotY, 3, 0, color);
      } else {
        dma_display->drawPixel(cursorX + 3, dotY, color);
      }
    }

    cursorX += charWidth;
  }
}

static bool hasDisplayValue(const String& value) {
  String check = value;
  check.trim();
  check.toLowerCase();
  return check.length() > 0 && check != "null" && check != "undefined";
}

static String utf8SliceChars(const String& text, int startChar, int maxChars) {
  String out = "";
  int byteIndex = 0;
  int charIndex = 0;
  int copied = 0;

  while (byteIndex < (int)text.length() && copied < maxChars) {
    unsigned char first = text[byteIndex];
    int charLen = 1;
    if ((first & 0xF0) == 0xE0) charLen = 3;
    else if ((first & 0xE0) == 0xC0) charLen = 2;

    if (charIndex >= startChar && byteIndex + charLen <= (int)text.length()) {
      out += text.substring(byteIndex, byteIndex + charLen);
      copied++;
    }

    byteIndex += charLen;
    charIndex++;
  }

  return out;
}

static int utf8CharCount(const String& text) {
  int byteIndex = 0;
  int count = 0;
  while (byteIndex < (int)text.length()) {
    unsigned char first = text[byteIndex];
    if ((first & 0xF0) == 0xE0) byteIndex += 3;
    else if ((first & 0xE0) == 0xC0) byteIndex += 2;
    else byteIndex += 1;
    count++;
  }
  return count;
}

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

  if (showNetworkIp && millis() < showNetworkIpUntil) {
    showNetworkIpDisplay();
    return;
  }

  if (showNetworkIp && millis() >= showNetworkIpUntil) {
    showNetworkIp = false;
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
  
  String displayText;
  bool noOrder = false;
  if (hasDisplayValue(productCode)) {
    displayText = productCode;
  } else if (hasDisplayValue(bagType) && bagType != "MA SP") {
    displayText = bagType;
  } else {
    displayText = "CHƯA CÓ ĐƠN";
    noOrder = true;
  }

  if (noOrder) {
    drawVietnameseText(1, 5, displayText, myYELLOW, 1.5f);
  } else {
    int maxCodeLen = 7;
    if (utf8CharCount(displayText) > maxCodeLen) {
      String line1 = utf8SliceChars(displayText, 0, maxCodeLen);
      String line2 = utf8SliceChars(displayText, maxCodeLen, maxCodeLen);
      drawVietnameseText(1, 5, line1, myYELLOW, 1.5f);
      drawVietnameseText(1, 16, line2, myYELLOW, 1.5f);
    } else {
      drawVietnameseText(1, 5, displayText, myYELLOW, 1.5f);
    }
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
  
  // DÒNG 2: Hiển thị "XUẤT" hoặc "NHẬP" theo mode với số lượng đơn hàng hiện tại
  String modeLabel;
  if (currentMode == "output") {
    modeLabel = "XUẤT:";
  } else {
    modeLabel = "NHẬP:";
  }
  drawVietnameseText(1, 21, modeLabel, myCYAN, 1.5f);
  drawVietnameseText(50, 21, String(targetCount), myCYAN, 1.5f);
  
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
    
    // Hiển thị chữ khi đang kết nối
    drawVietnameseText(10, 10, "BỘ ĐẾM THÔNG MINH", myYELLOW, 1);
    
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

//----------------------------------------Network IP Display Function
void showNetworkIpDisplay() {
  if (!dma_display) {
    Serial.println("dma_display is null!");
    return;
  }

  dma_display->clearScreen();
  drawVietnameseText(1, 6, "IP THIẾT BỊ", myYELLOW, 1);

  dma_display->setTextColor(myCYAN);
  dma_display->setCursor(1, 17);
  dma_display->print(networkIpText.length() > 0 ? networkIpText : "UNKNOWN");
}

//----------------------------------------Set System Connected
void setSystemConnected() {
  if (!systemConnected) {
    systemConnected = true;
    showConnectingAnimation = false;
    showNetworkIp = true;
    showNetworkIpUntil = millis() + 5000;
    if (currentNetworkMode == ETHERNET_MODE) {
      networkIpText = ETH.localIP().toString();
    } else if (currentNetworkMode == WIFI_STA_MODE) {
      networkIpText = WiFi.localIP().toString();
    } else {
      networkIpText = WiFi.softAPIP().toString();
    }
    needUpdate = true;  // Trigger display update to normal layout
    
    Serial.println("System fully connected. Showing IP on LED: " + networkIpText);
    
    // Cập nhật display ngay
    updateDisplay();
  }
}
