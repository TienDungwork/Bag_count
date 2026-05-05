#include "qr_reader.h"
#include "display.h"
#include "realtime.h"

static const unsigned long QR_DUPLICATE_WINDOW_MS = 1200;
static const unsigned long QR_IDLE_FRAME_TIMEOUT_MS = 150;
static const size_t QR_MAX_FRAME_LENGTH = 80;

static String normalizeQrProductCode(String value) {
  String out = "";
  value.trim();

  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c < 32 || c > 126) continue;
    if (c == ' ' || c == '\t') continue;
    out += c;
  }

  out.trim();
  out.toUpperCase();
  if (out.startsWith("SP-")) {
    out = out.substring(3);
  }
  return out;
}

String currentExpectedQrProductCode() {
  String expected = normalizeQrProductCode(productCode);
  if (expected.length() == 0 && bagType != "MA SP") {
    expected = normalizeQrProductCode(bagType);
  }
  return expected;
}

void clearQrProductMismatch(const String& reason) {
  if (!qrProductMismatchActive) return;

  qrProductMismatchActive = false;
  qrMismatchScannedCode = "";
  qrMismatchExpectedCode = "";
  digitalWrite(DONE_LED_PIN, LOW);
  updateDoneLED();
  publishAlert("QR_MATCH_RESTORED", reason.length() > 0 ? reason : "QR sản phẩm đã đúng mã hiện tại");
  publishStatusMQTT();
  needUpdate = true;
  Serial.println("QR mismatch alarm cleared: " + reason);
}

static void stopForQrProductMismatch(const String& scannedCode, const String& expectedCode) {
  qrProductMismatchActive = true;
  qrMismatchScannedCode = scannedCode;
  qrMismatchExpectedCode = expectedCode;

  isRunning = false;
  isTriggerEnabled = false;
  isCountingEnabled = false;
  isStartAuthorized = false;
  waitForSensorClearOnStart = false;
  isBagDetected = false;
  isWaitingForBagGroupEnd = false;
  sensorClearStartTime = 0;
  currentSystemStatus = "PRODUCT_MISMATCH";

  digitalWrite(START_LED_PIN, LOW);
  digitalWrite(DONE_LED_PIN, HIGH);
  startLedOn = false;
  doneLedOn = true;
  needUpdate = true;

  String message = "Sai mã QR sản phẩm: đang chạy " + expectedCode + ", đọc được " + scannedCode;
  publishAlert("PRODUCT_MISMATCH", message);
  publishStatusMQTT();
  publishSensorData();
  Serial.println("QR PRODUCT MISMATCH - conveyor stopped, alarm ON. Expected=" + expectedCode + ", scanned=" + scannedCode);
}

static void processQrFrame(String frame) {
  String scannedCode = normalizeQrProductCode(frame);
  if (scannedCode.length() == 0) return;

  unsigned long now = millis();
  if (scannedCode == qrLastScannedCode && now - qrLastScanTime < QR_DUPLICATE_WINDOW_MS) {
    return;
  }

  qrLastScannedCode = scannedCode;
  qrLastScanTime = now;

  String expectedCode = currentExpectedQrProductCode();
  if (expectedCode.length() == 0) {
    Serial.println("QR read ignored because current product code is empty: " + scannedCode);
    return;
  }

  Serial.println("QR read: scanned=" + scannedCode + ", expected=" + expectedCode);

  if (scannedCode == expectedCode) {
    clearQrProductMismatch("Đã đọc đúng mã QR: " + scannedCode);
    return;
  }

  stopForQrProductMismatch(scannedCode, expectedCode);
}

void setupQrReader() {
  qrRxBuffer.reserve(QR_MAX_FRAME_LENGTH);
  qrRxBuffer = "";
  qrLastScannedCode = "";
  qrMismatchScannedCode = "";
  qrMismatchExpectedCode = "";
  qrProductMismatchActive = false;
  Serial.println("QR reader initialized on RX GPIO" + String(QR_READER_RX_PIN) + ", " + String(QR_READER_BAUD) + " baud.");
}

void handleQrReader() {
  String expectedCode = currentExpectedQrProductCode();
  if (qrProductMismatchActive && qrMismatchExpectedCode.length() > 0 &&
      expectedCode.length() > 0 && expectedCode != qrMismatchExpectedCode) {
    clearQrProductMismatch("Đổi đơn sang mã sản phẩm mới: " + expectedCode);
  }

  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    qrLastByteTime = millis();

    if (c == '\r' || c == '\n') {
      if (qrRxBuffer.length() > 0) {
        processQrFrame(qrRxBuffer);
        qrRxBuffer = "";
      }
      continue;
    }

    if (c < 32 || c > 126) {
      continue;
    }

    if (qrRxBuffer.length() >= QR_MAX_FRAME_LENGTH) {
      Serial.println("QR frame too long, clearing buffer");
      qrRxBuffer = "";
      continue;
    }

    qrRxBuffer += c;
  }

  if (qrRxBuffer.length() > 0 && millis() - qrLastByteTime > QR_IDLE_FRAME_TIMEOUT_MS) {
    processQrFrame(qrRxBuffer);
    qrRxBuffer = "";
  }
}
