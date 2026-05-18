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

static int scaleTextCoord(int value, float scale) {
  return (int)(value * scale + 0.5f);
}

static void drawScaledPixel(int x, int y, int srcX, int srcY, uint16_t color, float scale) {
  int x0 = x + scaleTextCoord(srcX, scale);
  int x1 = x + scaleTextCoord(srcX + 1, scale);
  int y0 = y + scaleTextCoord(srcY, scale);
  int y1 = y + scaleTextCoord(srcY + 1, scale);
  dma_display->fillRect(x0, y0, std::max(1, x1 - x0), std::max(1, y1 - y0), color);
}

static void drawScaledAsciiChar(int x, int y, char c, uint16_t color, float scale) {
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
        drawScaledPixel(x, y, col, row, color, scale);
      }
    }
  }
}

static void drawAccentMarkScaled(int x, int y, char mark, uint16_t color, float scale) {
  if (!dma_display || mark == 0) return;

  switch (mark) {
    case '\'':
      drawScaledPixel(x, y, 4, 1, color, scale);
      drawScaledPixel(x, y, 3, 2, color, scale);
      break;
    case '`':
      drawScaledPixel(x, y, 2, 0, color, scale);
      drawScaledPixel(x, y, 3, 1, color, scale);
      break;
    case '?':
      drawScaledPixel(x, y, 2, 0, color, scale);
      drawScaledPixel(x, y, 3, 0, color, scale);
      drawScaledPixel(x, y, 3, 1, color, scale);
      drawScaledPixel(x, y, 2, 2, color, scale);
      break;
    case '~':
      drawScaledPixel(x, y, 1, 1, color, scale);
      drawScaledPixel(x, y, 2, 0, color, scale);
      drawScaledPixel(x, y, 3, 1, color, scale);
      drawScaledPixel(x, y, 4, 0, color, scale);
      break;
  }
}

static void drawShapeMarkScaled(int x, int y, char mark, uint16_t color, float scale) {
  if (!dma_display || mark == 0) return;

  switch (mark) {
    case '^':
      drawScaledPixel(x, y, 1, 2, color, scale);
      drawScaledPixel(x, y, 2, 1, color, scale);
      drawScaledPixel(x, y, 3, 1, color, scale);
      drawScaledPixel(x, y, 4, 2, color, scale);
      break;
    case '(':
      drawScaledPixel(x, y, 1, 0, color, scale);
      drawScaledPixel(x, y, 2, 1, color, scale);
      drawScaledPixel(x, y, 3, 1, color, scale);
      drawScaledPixel(x, y, 4, 0, color, scale);
      break;
    case '+':
      drawScaledPixel(x, y, 4, 1, color, scale);
      drawScaledPixel(x, y, 5, 0, color, scale);
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
  bool scaledText = textSize > 1.0f && textSize < 2.0f;
  int intTextSize = (int)textSize;
  int charWidth = scaledText ? scaleTextCoord(6, textSize) : 6 * intTextSize;
  int topY = std::max(0, y - (scaledText ? scaleTextCoord(4, textSize) : 4));
  int shapeY = std::max(0, y - (scaledText ? scaleTextCoord(2, textSize) : 2));
  int dotY = y + (scaledText ? scaleTextCoord(8, textSize) : 8 * intTextSize);

  dma_display->setTextSize(intTextSize);
  dma_display->setTextColor(color);

  while (i < (int)text.length()) {
    VietnameseGlyph glyph = glyphFromUtf8(text, i);
    if (scaledText) {
      drawScaledAsciiChar(cursorX, y, glyph.base, color, textSize);
    } else {
      dma_display->setCursor(cursorX, y);
      dma_display->print(glyph.base);
    }

    if (glyph.stroke) {
      if (scaledText) {
        dma_display->drawLine(cursorX + scaleTextCoord(1, textSize),
                              y + scaleTextCoord(4, textSize),
                              cursorX + scaleTextCoord(5, textSize),
                              y + scaleTextCoord(2, textSize),
                              color);
      } else {
        dma_display->drawLine(cursorX + 1, y + 4, cursorX + 5, y + 2, color);
      }
    }
    if (scaledText) {
      drawShapeMarkScaled(cursorX, shapeY, glyph.shapeMark, color, textSize);
      drawAccentMarkScaled(cursorX, topY, glyph.topMark, color, textSize);
    } else {
      drawShapeMark(cursorX, shapeY, glyph.shapeMark, color);
      drawAccentMark(cursorX, topY, glyph.topMark, color);
    }
    if (glyph.dotBelow) {
      if (scaledText) {
        drawScaledPixel(cursorX, dotY, 3, 0, color, textSize);
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

static uint32_t nextUtf8Codepoint(const String& text, int& byteIndex) {
  unsigned char first = text[byteIndex];
  uint32_t codepoint = first;
  int charLen = 1;

  if ((first & 0xF0) == 0xE0) {
    charLen = 3;
    codepoint = first & 0x0F;
  } else if ((first & 0xE0) == 0xC0) {
    charLen = 2;
    codepoint = first & 0x1F;
  } else if ((first & 0xF8) == 0xF0) {
    charLen = 4;
    codepoint = first & 0x07;
  }

  if (byteIndex + charLen > (int)text.length()) {
    byteIndex++;
    return first;
  }

  for (int i = 1; i < charLen; i++) {
    unsigned char next = text[byteIndex + i];
    if ((next & 0xC0) != 0x80) {
      byteIndex++;
      return first;
    }
    codepoint = (codepoint << 6) | (next & 0x3F);
  }

  byteIndex += charLen;
  return codepoint;
}

static char asciiBaseForVietnameseCodepoint(uint32_t codepoint) {
  switch (codepoint) {
    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C4:
    case 0x00C5: case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3:
    case 0x00E4: case 0x00E5: case 0x0102: case 0x0103:
    case 0x1EA0: case 0x1EA1: case 0x1EA2: case 0x1EA3: case 0x1EA4:
    case 0x1EA5: case 0x1EA6: case 0x1EA7: case 0x1EA8: case 0x1EA9:
    case 0x1EAA: case 0x1EAB: case 0x1EAC: case 0x1EAD: case 0x1EAE:
    case 0x1EAF: case 0x1EB0: case 0x1EB1: case 0x1EB2: case 0x1EB3:
    case 0x1EB4: case 0x1EB5: case 0x1EB6: case 0x1EB7:
      return 'A';
    case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
    case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
    case 0x1EB8: case 0x1EB9: case 0x1EBA: case 0x1EBB: case 0x1EBC:
    case 0x1EBD: case 0x1EBE: case 0x1EBF: case 0x1EC0: case 0x1EC1:
    case 0x1EC2: case 0x1EC3: case 0x1EC4: case 0x1EC5: case 0x1EC6:
    case 0x1EC7:
      return 'E';
    case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
    case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
    case 0x0128: case 0x0129: case 0x1EC8: case 0x1EC9:
    case 0x1ECA: case 0x1ECB:
      return 'I';
    case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5: case 0x00D6:
    case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5: case 0x00F6:
    case 0x01A0: case 0x01A1: case 0x1ECC: case 0x1ECD: case 0x1ECE:
    case 0x1ECF: case 0x1ED0: case 0x1ED1: case 0x1ED2: case 0x1ED3:
    case 0x1ED4: case 0x1ED5: case 0x1ED6: case 0x1ED7: case 0x1ED8:
    case 0x1ED9: case 0x1EDA: case 0x1EDB: case 0x1EDC: case 0x1EDD:
    case 0x1EDE: case 0x1EDF: case 0x1EE0: case 0x1EE1: case 0x1EE2:
    case 0x1EE3:
      return 'O';
    case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
    case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
    case 0x0168: case 0x0169: case 0x01AF: case 0x01B0:
    case 0x1EE4: case 0x1EE5: case 0x1EE6: case 0x1EE7: case 0x1EE8:
    case 0x1EE9: case 0x1EEA: case 0x1EEB: case 0x1EEC: case 0x1EED:
    case 0x1EEE: case 0x1EEF: case 0x1EF0: case 0x1EF1:
      return 'U';
    case 0x00DD: case 0x00FD: case 0x00FF:
    case 0x1EF2: case 0x1EF3: case 0x1EF4: case 0x1EF5:
    case 0x1EF6: case 0x1EF7: case 0x1EF8: case 0x1EF9:
      return 'Y';
    case 0x0110: case 0x0111:
      return 'D';
  }

  return 0;
}

static String ledAsciiText(const String& text) {
  String out = "";
  int byteIndex = 0;

  while (byteIndex < (int)text.length()) {
    uint32_t codepoint = nextUtf8Codepoint(text, byteIndex);

    if (codepoint >= 0x0300 && codepoint <= 0x036F) {
      continue;
    }

    char ascii = asciiBaseForVietnameseCodepoint(codepoint);
    if (ascii != 0) {
      out += ascii;
    } else if (codepoint < 128) {
      char c = (char)codepoint;
      if (c >= 'a' && c <= 'z') c -= 32;
      out += c;
    }
  }

  return out;
}

static const int LED_PRODUCT_VISIBLE_CHARS = 5;
static const int LED_PRODUCT_SCROLL_GAP_CHARS = 3;
static const unsigned long LED_PRODUCT_SCROLL_STEP_MS = 350;
static const unsigned long LED_PRODUCT_HOLD_AFTER_CYCLE_MS = 5000;
static const unsigned long LED_TARGET_TOGGLE_MS = 3000;

static bool ledProductShowingName = false;
static bool ledProductWaitingAfterCycle = false;
static unsigned long ledProductCycleFinishedMs = 0;
static String ledProductLastCode = "";
static String ledProductLastName = "";

static bool canAlternateLedProductText() {
  return hasDisplayValue(productCode) && hasDisplayValue(bagType) && bagType != "MA SP";
}

static void resetLedProductTextCycleIfNeeded() {
  String currentCode = hasDisplayValue(productCode) ? productCode : "";
  String currentName = (hasDisplayValue(bagType) && bagType != "MA SP") ? bagType : "";

  if (currentCode == ledProductLastCode && currentName == ledProductLastName) {
    return;
  }

  ledProductLastCode = currentCode;
  ledProductLastName = currentName;
  ledProductShowingName = false;
  ledProductWaitingAfterCycle = false;
  ledProductCycleFinishedMs = 0;
}

static void markLedProductCycleComplete() {
  if (!canAlternateLedProductText() || ledProductWaitingAfterCycle) {
    return;
  }

  ledProductWaitingAfterCycle = true;
  ledProductCycleFinishedMs = millis();
}

static String currentLedProductText(bool& noOrder, bool advanceCycle = false) {
  String displayText;
  noOrder = false;
  resetLedProductTextCycleIfNeeded();

  if (advanceCycle && ledProductWaitingAfterCycle &&
      millis() - ledProductCycleFinishedMs >= LED_PRODUCT_HOLD_AFTER_CYCLE_MS) {
    ledProductShowingName = !ledProductShowingName;
    ledProductWaitingAfterCycle = false;
  }

  if (hasDisplayValue(productCode)) {
    bool hasName = hasDisplayValue(bagType) && bagType != "MA SP";
    displayText = (hasName && ledProductShowingName) ? bagType : productCode;
  } else if (hasDisplayValue(bagType) && bagType != "MA SP") {
    displayText = bagType;
  } else {
    displayText = "CHƯA CÓ ĐƠN";
    noOrder = true;
  }

  return noOrder ? displayText : ledAsciiText(displayText);
}

static String scrollingWindowText(const String& text, bool& cycleComplete, bool pauseScroll = false) {
  static String lastText = "";
  static int scrollOffset = 0;
  static unsigned long lastStepMs = 0;
  static bool shortTextCycleReported = false;

  cycleComplete = false;

  int charCount = utf8CharCount(text);
  if (charCount <= LED_PRODUCT_VISIBLE_CHARS) {
    if (text != lastText) {
      lastText = text;
      shortTextCycleReported = false;
    }
    if (!shortTextCycleReported) {
      cycleComplete = true;
      shortTextCycleReported = true;
    }
    scrollOffset = 0;
    lastStepMs = millis();
    return text;
  }

  if (text != lastText) {
    lastText = text;
    scrollOffset = 0;
    lastStepMs = millis();
    shortTextCycleReported = false;
  }

  unsigned long now = millis();
  if (!pauseScroll && now - lastStepMs >= LED_PRODUCT_SCROLL_STEP_MS) {
    lastStepMs = now;
    int cycleChars = charCount + LED_PRODUCT_SCROLL_GAP_CHARS;
    scrollOffset = (scrollOffset + 1) % cycleChars;
    cycleComplete = scrollOffset == 0;
  }

  String padded = text;
  for (int i = 0; i < LED_PRODUCT_SCROLL_GAP_CHARS; i++) {
    padded += " ";
  }

  int cycleChars = charCount + LED_PRODUCT_SCROLL_GAP_CHARS;
  String out = "";
  for (int i = 0; i < LED_PRODUCT_VISIBLE_CHARS; i++) {
    int index = (scrollOffset + i) % cycleChars;
    out += utf8SliceChars(padded, index, 1);
  }
  return out;
}

static uint8_t sevenSegmentMask(char digit) {
  switch (digit) {
    case '0': return 0b0111111;
    case '1': return 0b0000110;
    case '2': return 0b1011011;
    case '3': return 0b1001111;
    case '4': return 0b1100110;
    case '5': return 0b1101101;
    case '6': return 0b1111101;
    case '7': return 0b0000111;
    case '8': return 0b1111111;
    case '9': return 0b1101111;
  }
  return 0;
}

static void drawSevenSegmentDigit(int x, int y, int w, int h, int t, char digit, uint16_t color) {
  if (!dma_display) return;

  if (digit == '1') {
    dma_display->fillRect(x + w - t, y, t, h, color);
    return;
  }

  if (digit == '4') {
    int midY = y + h / 2 - t / 2;
    dma_display->fillRect(x, y, t, midY + t - y, color);
    dma_display->fillRect(x + w - t, y, t, h, color);
    dma_display->fillRect(x + t, midY, w - 2 * t, t, color);
    return;
  }

  uint8_t mask = sevenSegmentMask(digit);
  int midY = y + h / 2 - t / 2;
  int upperH = midY - (y + t);
  int lowerY = midY + t;
  int lowerH = y + h - t - lowerY;

  if (mask & 0b0000001) dma_display->fillRect(x + t, y, w - 2 * t, t, color);                 // A
  if (mask & 0b0000010) dma_display->fillRect(x + w - t, y + t, t, upperH, color);             // B
  if (mask & 0b0000100) dma_display->fillRect(x + w - t, lowerY, t, lowerH, color);            // C
  if (mask & 0b0001000) dma_display->fillRect(x + t, y + h - t, w - 2 * t, t, color);          // D
  if (mask & 0b0010000) dma_display->fillRect(x, lowerY, t, lowerH, color);                    // E
  if (mask & 0b0100000) dma_display->fillRect(x, y + t, t, upperH, color);                     // F
  if (mask & 0b1000000) dma_display->fillRect(x + t, midY, w - 2 * t, t, color);               // G
}

static void drawSevenSegmentNumberRight(int rightX, int y, const String& number, uint16_t color) {
  if (!dma_display) return;

  int digits = std::max(1, (int)number.length());
  int digitW = 15;
  int digitH = 28;
  int thickness = 3;
  int gap = 2;

  if (digits >= 5) {
    digitW = 12;
    digitH = 25;
    thickness = 2;
    gap = 1;
    y += 2;
  }

  int totalW = digits * digitW + (digits - 1) * gap;
  int x = std::max(0, rightX - totalW);

  for (int i = 0; i < digits; i++) {
    char c = number[i];
    if (c >= '0' && c <= '9') {
      drawSevenSegmentDigit(x + i * (digitW + gap), y, digitW, digitH, thickness, c, color);
    }
  }
}

bool displayNeedsFastRefresh() {
  if (!systemConnected || showNetworkIp || isLimitReached) return false;
  if (ledProductWaitingAfterCycle) return false;

  bool noOrder = false;
  String displayText = currentLedProductText(noOrder);
  return !noOrder && utf8CharCount(displayText) > LED_PRODUCT_VISIBLE_CHARS;
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
  
  bool noOrder = false;
  String displayText = currentLedProductText(noOrder, true);

  if (noOrder) {
    drawVietnameseText(1, 5, displayText, myYELLOW, 1.5f);
  } else {
    bool productCycleComplete = false;
    drawVietnameseText(1, 0, scrollingWindowText(displayText, productCycleComplete, ledProductWaitingAfterCycle), myYELLOW, 2.0f);
    if (productCycleComplete) {
      markLedProductCycleComplete();
    }
  }

  // SỐ ĐẾM LỚN BÊN PHẢI: kiểu LED 7 đoạn, đủ chỗ cho 4 chữ số.
  String countStr = String((int)totalCount);
  int totalWidth = PANEL_RES_X * PANEL_CHAIN;
  drawSevenSegmentNumberRight(totalWidth - 1, 2, countStr, myGREEN);
  
  // DÒNG 2: Luân phiên hiển thị mode rồi target để tránh chèn sang số đếm.
  String modeLabel;
  if (currentMode == "output") {
    modeLabel = "XUAT";
  } else {
    modeLabel = "NHAP";
  }
  bool showTarget = ((millis() / LED_TARGET_TOGGLE_MS) % 2) == 1;
  drawVietnameseText(1, 18, showTarget ? String(targetCount) : modeLabel, myCYAN, 2.0f);
  
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
