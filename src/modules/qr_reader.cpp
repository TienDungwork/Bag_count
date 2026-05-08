#include "qr_reader.h"
#include "display.h"
#include "realtime.h"

#include "esp_err.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/usb_host.h"

static const unsigned long QR_DUPLICATE_WINDOW_MS = 1200;
static const unsigned long QR_IDLE_FRAME_TIMEOUT_MS = 150;
static const size_t QR_MAX_FRAME_LENGTH = 80;
static const size_t QR_USB_CHAR_QUEUE_LENGTH = 128;
static const uint8_t HID_CLASS_REQUEST_SET_PROTOCOL = 0x0B;
static const uint8_t HID_BOOT_PROTOCOL = 0;

static QueueHandle_t qrUsbCharQueue = nullptr;
static TaskHandle_t qrUsbHostTaskHandle = nullptr;
static usb_host_client_handle_t qrUsbClient = nullptr;
static usb_device_handle_t qrUsbDevice = nullptr;
static usb_transfer_t* qrUsbInTransfer = nullptr;
static bool qrUsbDeviceOpen = false;
static bool qrUsbInterfaceClaimed = false;
static bool qrUsbTransferInFlight = false;
static bool qrUsbKeyboardConnected = false;
static uint8_t qrUsbInterfaceNumber = 0;
static uint8_t qrUsbEndpointAddress = 0;
static uint16_t qrUsbEndpointMps = 8;
static uint8_t qrUsbPreviousKeys[6] = {0};

static void processQrFrame(String frame);
static void qrUsbClientEventCallback(const usb_host_client_event_msg_t* eventMessage, void* arg);
static void qrUsbInTransferCallback(usb_transfer_t* transfer);

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
  if (!isRunning) {
    currentSystemStatus = "PAUSE";
  }
  digitalWrite(DONE_LED_PIN, LOW);
  updateDoneLED();
  publishAlert("QR_MATCH_RESTORED", reason.length() > 0 ? reason : "QR sản phẩm đã đúng mã hiện tại");
  publishStatusMQTT(true);
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
  publishStatusMQTT(true);
  publishSensorData();
  Serial.println("QR PRODUCT MISMATCH - conveyor stopped, alarm ON. Expected=" + expectedCode + ", scanned=" + scannedCode);
}

static void processQrInputChar(char c) {
  qrLastByteTime = millis();
  qrTotalBytesReceived++;

  if (!qrHasSeenData) {
    qrHasSeenData = true;
    Serial.println("QR USB HID data detected on USB_N GPIO" + String(QR_READER_USB_N_PIN) +
                   "/USB_P GPIO" + String(QR_READER_USB_P_PIN) +
                   ". First char decimal=" + String((uint8_t)c));
  }

  if (c == '\r' || c == '\n') {
    if (qrRxBuffer.length() > 0) {
      processQrFrame(qrRxBuffer);
      qrRxBuffer = "";
    }
    return;
  }

  if (c < 32 || c > 126) {
    return;
  }

  if (qrRxBuffer.length() >= QR_MAX_FRAME_LENGTH) {
    Serial.println("QR frame too long, clearing buffer");
    qrRxBuffer = "";
    return;
  }

  qrRxBuffer += c;
}

static void enqueueQrUsbChar(char c) {
  if (qrUsbCharQueue == nullptr) return;
  xQueueSend(qrUsbCharQueue, &c, 0);
}

static bool hidReportHasKey(const uint8_t* keys, uint8_t key) {
  for (size_t i = 0; i < 6; i++) {
    if (keys[i] == key) return true;
  }
  return false;
}

static char mapHidKeyboardUsage(uint8_t usage, bool shifted) {
  if (usage >= 0x04 && usage <= 0x1D) {
    char c = char('a' + usage - 0x04);
    return shifted ? char(c - 'a' + 'A') : c;
  }

  if (usage >= 0x1E && usage <= 0x27) {
    static const char normalDigits[] = "1234567890";
    static const char shiftedDigits[] = "!@#$%^&*()";
    size_t index = usage - 0x1E;
    return shifted ? shiftedDigits[index] : normalDigits[index];
  }

  switch (usage) {
    case 0x28: return '\n';
    case 0x2B: return '\t';
    case 0x2C: return ' ';
    case 0x2D: return shifted ? '_' : '-';
    case 0x2E: return shifted ? '+' : '=';
    case 0x2F: return shifted ? '{' : '[';
    case 0x30: return shifted ? '}' : ']';
    case 0x31: return shifted ? '|' : '\\';
    case 0x33: return shifted ? ':' : ';';
    case 0x34: return shifted ? '"' : '\'';
    case 0x35: return shifted ? '~' : '`';
    case 0x36: return shifted ? '<' : ',';
    case 0x37: return shifted ? '>' : '.';
    case 0x38: return shifted ? '?' : '/';
    default: return 0;
  }
}

static void handleHidKeyboardReport(const uint8_t* data, int length) {
  if (length < 8) return;

  int offset = 0;
  if (length >= 9 && data[0] != 0 && data[1] <= 0x22 && data[2] == 0) {
    offset = 1;
  }

  uint8_t modifiers = data[offset];
  const uint8_t* keys = data + offset + 2;
  bool shifted = (modifiers & 0x22) != 0;

  for (size_t i = 0; i < 6; i++) {
    uint8_t usage = keys[i];
    if (usage == 0 || hidReportHasKey(qrUsbPreviousKeys, usage)) {
      continue;
    }

    char c = mapHidKeyboardUsage(usage, shifted);
    if (c != 0) {
      enqueueQrUsbChar(c);
    }
  }

  memcpy(qrUsbPreviousKeys, keys, sizeof(qrUsbPreviousKeys));
}

static void submitQrUsbInTransfer() {
  if (qrUsbInTransfer == nullptr || qrUsbDevice == nullptr || qrUsbTransferInFlight) return;

  qrUsbInTransfer->device_handle = qrUsbDevice;
  qrUsbInTransfer->bEndpointAddress = qrUsbEndpointAddress;
  qrUsbInTransfer->callback = qrUsbInTransferCallback;
  qrUsbInTransfer->context = nullptr;
  qrUsbInTransfer->num_bytes = usb_round_up_to_mps(qrUsbEndpointMps, qrUsbEndpointMps);
  qrUsbInTransfer->flags = 0;

  esp_err_t err = usb_host_transfer_submit(qrUsbInTransfer);
  if (err == ESP_OK) {
    qrUsbTransferInFlight = true;
  } else {
    Serial.println("QR USB HID transfer submit failed: " + String(esp_err_to_name(err)));
  }
}

static void qrUsbInTransferCallback(usb_transfer_t* transfer) {
  qrUsbTransferInFlight = false;

  if (transfer->status == USB_TRANSFER_STATUS_COMPLETED && transfer->actual_num_bytes > 0) {
    handleHidKeyboardReport(transfer->data_buffer, transfer->actual_num_bytes);
  } else if (transfer->status == USB_TRANSFER_STATUS_NO_DEVICE) {
    qrUsbKeyboardConnected = false;
    if (qrUsbDevice == nullptr && qrUsbInTransfer == transfer) {
      usb_host_transfer_free(qrUsbInTransfer);
      qrUsbInTransfer = nullptr;
    }
    return;
  }

  if (qrUsbKeyboardConnected) {
    submitQrUsbInTransfer();
  }
}

static const usb_ep_desc_t* findHidKeyboardEndpoint(const usb_config_desc_t* configDesc,
                                                    const usb_intf_desc_t** selectedInterface) {
  const usb_standard_desc_t* descriptor = (const usb_standard_desc_t*)configDesc;
  const usb_intf_desc_t* currentInterface = nullptr;
  const usb_intf_desc_t* bestInterface = nullptr;
  const usb_ep_desc_t* bestEndpoint = nullptr;
  int bestScore = -1;
  int offset = 0;

  while ((descriptor = usb_parse_next_descriptor(descriptor, configDesc->wTotalLength, &offset)) != nullptr) {
    if (descriptor->bDescriptorType == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
      currentInterface = (const usb_intf_desc_t*)descriptor;
      continue;
    }

    if (descriptor->bDescriptorType != USB_B_DESCRIPTOR_TYPE_ENDPOINT || currentInterface == nullptr) {
      continue;
    }

    if (currentInterface->bInterfaceClass != USB_CLASS_HID) {
      continue;
    }

    const usb_ep_desc_t* endpoint = (const usb_ep_desc_t*)descriptor;
    bool isInterruptEndpoint = (endpoint->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) == USB_BM_ATTRIBUTES_XFER_INT;
    bool isInputEndpoint = (endpoint->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) != 0;
    if (!isInterruptEndpoint || !isInputEndpoint) {
      continue;
    }

    int score = currentInterface->bInterfaceProtocol == 1 ? 10 : 5;
    if (score > bestScore) {
      bestScore = score;
      bestInterface = currentInterface;
      bestEndpoint = endpoint;
    }
  }

  *selectedInterface = bestInterface;
  return bestEndpoint;
}

static void cleanupQrUsbDevice() {
  qrUsbKeyboardConnected = false;

  if (qrUsbInterfaceClaimed && qrUsbDevice != nullptr) {
    usb_host_interface_release(qrUsbClient, qrUsbDevice, qrUsbInterfaceNumber);
    qrUsbInterfaceClaimed = false;
  }

  if (qrUsbDeviceOpen && qrUsbDevice != nullptr) {
    usb_host_device_close(qrUsbClient, qrUsbDevice);
    qrUsbDeviceOpen = false;
  }

  if (!qrUsbTransferInFlight && qrUsbInTransfer != nullptr) {
    usb_host_transfer_free(qrUsbInTransfer);
    qrUsbInTransfer = nullptr;
  }

  qrUsbDevice = nullptr;
  memset(qrUsbPreviousKeys, 0, sizeof(qrUsbPreviousKeys));
}

static void sendHidBootProtocolRequest() {
  usb_transfer_t* controlTransfer = nullptr;
  esp_err_t err = usb_host_transfer_alloc(USB_SETUP_PACKET_SIZE, 0, &controlTransfer);
  if (err != ESP_OK) {
    Serial.println("QR USB HID SET_PROTOCOL alloc failed: " + String(esp_err_to_name(err)));
    return;
  }

  usb_setup_packet_t* setupPacket = (usb_setup_packet_t*)controlTransfer->data_buffer;
  setupPacket->bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT |
                               USB_BM_REQUEST_TYPE_TYPE_CLASS |
                               USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
  setupPacket->bRequest = HID_CLASS_REQUEST_SET_PROTOCOL;
  setupPacket->wValue = HID_BOOT_PROTOCOL;
  setupPacket->wIndex = qrUsbInterfaceNumber;
  setupPacket->wLength = 0;
  controlTransfer->num_bytes = USB_SETUP_PACKET_SIZE;
  controlTransfer->device_handle = qrUsbDevice;
  controlTransfer->bEndpointAddress = 0;
  controlTransfer->callback = [](usb_transfer_t* transfer) {
    usb_host_transfer_free(transfer);
  };

  err = usb_host_transfer_submit_control(qrUsbClient, controlTransfer);
  if (err != ESP_OK) {
    Serial.println("QR USB HID SET_PROTOCOL submit failed: " + String(esp_err_to_name(err)));
    usb_host_transfer_free(controlTransfer);
  }
}

static void openQrUsbDevice(uint8_t address) {
  if (qrUsbDeviceOpen) {
    Serial.println("QR USB device ignored: one scanner is already open");
    return;
  }

  esp_err_t err = usb_host_device_open(qrUsbClient, address, &qrUsbDevice);
  if (err != ESP_OK) {
    Serial.println("QR USB device open failed: " + String(esp_err_to_name(err)));
    qrUsbDevice = nullptr;
    return;
  }
  qrUsbDeviceOpen = true;

  const usb_device_desc_t* deviceDesc = nullptr;
  const usb_config_desc_t* configDesc = nullptr;
  usb_host_get_device_descriptor(qrUsbDevice, &deviceDesc);
  err = usb_host_get_active_config_descriptor(qrUsbDevice, &configDesc);
  if (err != ESP_OK || configDesc == nullptr) {
    Serial.println("QR USB config descriptor read failed: " + String(esp_err_to_name(err)));
    cleanupQrUsbDevice();
    return;
  }

  const usb_intf_desc_t* hidInterface = nullptr;
  const usb_ep_desc_t* hidEndpoint = findHidKeyboardEndpoint(configDesc, &hidInterface);
  if (hidInterface == nullptr || hidEndpoint == nullptr) {
    Serial.println("QR USB device connected but no HID keyboard interrupt IN endpoint found");
    cleanupQrUsbDevice();
    return;
  }

  qrUsbInterfaceNumber = hidInterface->bInterfaceNumber;
  qrUsbEndpointAddress = hidEndpoint->bEndpointAddress;
  qrUsbEndpointMps = hidEndpoint->wMaxPacketSize > 0 ? hidEndpoint->wMaxPacketSize : 8;

  err = usb_host_interface_claim(qrUsbClient, qrUsbDevice, qrUsbInterfaceNumber, hidInterface->bAlternateSetting);
  if (err != ESP_OK) {
    Serial.println("QR USB HID interface claim failed: " + String(esp_err_to_name(err)));
    cleanupQrUsbDevice();
    return;
  }
  qrUsbInterfaceClaimed = true;

  size_t transferSize = usb_round_up_to_mps(max((int)qrUsbEndpointMps, 8), qrUsbEndpointMps);
  err = usb_host_transfer_alloc(transferSize, 0, &qrUsbInTransfer);
  if (err != ESP_OK) {
    Serial.println("QR USB HID transfer alloc failed: " + String(esp_err_to_name(err)));
    cleanupQrUsbDevice();
    return;
  }

  qrUsbKeyboardConnected = true;
  if (deviceDesc != nullptr) {
    Serial.println("QR USB HID scanner connected: VID=0x" + String(deviceDesc->idVendor, HEX) +
                   ", PID=0x" + String(deviceDesc->idProduct, HEX) +
                   ", interface=" + String(qrUsbInterfaceNumber) +
                   ", endpoint=0x" + String(qrUsbEndpointAddress, HEX) +
                   ", mps=" + String(qrUsbEndpointMps));
  } else {
    Serial.println("QR USB HID scanner connected");
  }

  sendHidBootProtocolRequest();
  submitQrUsbInTransfer();
}

static void qrUsbClientEventCallback(const usb_host_client_event_msg_t* eventMessage, void* arg) {
  (void)arg;

  switch (eventMessage->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      openQrUsbDevice(eventMessage->new_dev.address);
      break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      Serial.println("QR USB scanner disconnected");
      cleanupQrUsbDevice();
      break;
    default:
      break;
  }
}

static void qrUsbHostTask(void* parameter) {
  (void)parameter;

  usb_host_config_t hostConfig = {};
  hostConfig.skip_phy_setup = false;
  hostConfig.intr_flags = ESP_INTR_FLAG_LEVEL1;

  esp_err_t err = usb_host_install(&hostConfig);
  if (err != ESP_OK) {
    Serial.println("QR USB host install failed: " + String(esp_err_to_name(err)));
    vTaskDelete(nullptr);
    return;
  }

  usb_host_client_config_t clientConfig = {};
  clientConfig.is_synchronous = false;
  clientConfig.max_num_event_msg = 5;
  clientConfig.async.client_event_callback = qrUsbClientEventCallback;
  clientConfig.async.callback_arg = nullptr;

  err = usb_host_client_register(&clientConfig, &qrUsbClient);
  if (err != ESP_OK) {
    Serial.println("QR USB host client register failed: " + String(esp_err_to_name(err)));
    usb_host_uninstall();
    vTaskDelete(nullptr);
    return;
  }

  Serial.println("QR reader USB host initialized on native USB_N GPIO" + String(QR_READER_USB_N_PIN) +
                 "/USB_P GPIO" + String(QR_READER_USB_P_PIN) +
                 ". Configure scanner as USB HID keyboard.");

  while (true) {
    uint32_t eventFlags = 0;
    usb_host_lib_handle_events(pdMS_TO_TICKS(10), &eventFlags);
    if (qrUsbClient != nullptr) {
      usb_host_client_handle_events(qrUsbClient, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

static void processQrFrame(String frame) {
  Serial.println("QR raw frame: '" + frame + "'");

  String scannedCode = normalizeQrProductCode(frame);
  if (scannedCode.length() == 0) {
    Serial.println("QR frame ignored after normalize: empty");
    return;
  }

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
    Serial.println("QR MATCH OK: " + scannedCode);
    clearQrProductMismatch("Đã đọc đúng mã QR: " + scannedCode);
    return;
  }

  stopForQrProductMismatch(scannedCode, expectedCode);
}

void setupQrReader() {
  if (qrUsbCharQueue == nullptr) {
    qrUsbCharQueue = xQueueCreate(QR_USB_CHAR_QUEUE_LENGTH, sizeof(char));
  }

  qrRxBuffer.reserve(QR_MAX_FRAME_LENGTH);
  qrRxBuffer = "";
  qrLastScannedCode = "";
  qrMismatchScannedCode = "";
  qrMismatchExpectedCode = "";
  qrTotalBytesReceived = 0;
  qrHasSeenData = false;
  qrProductMismatchActive = false;
  memset(qrUsbPreviousKeys, 0, sizeof(qrUsbPreviousKeys));

  if (qrUsbCharQueue == nullptr) {
    Serial.println("QR USB char queue allocation failed");
    return;
  }

  if (qrUsbHostTaskHandle == nullptr) {
    BaseType_t created = xTaskCreatePinnedToCore(
      qrUsbHostTask,
      "qr_usb_host",
      4096,
      nullptr,
      5,
      &qrUsbHostTaskHandle,
      0
    );
    if (created != pdPASS) {
      qrUsbHostTaskHandle = nullptr;
      Serial.println("QR USB host task creation failed");
      return;
    }
  }

  Serial.println("QR reader waiting for USB HID keyboard data.");
}

void handleQrReader() {
  String expectedCode = currentExpectedQrProductCode();
  if (qrProductMismatchActive && qrMismatchExpectedCode.length() > 0 &&
      expectedCode.length() > 0 && expectedCode != qrMismatchExpectedCode) {
    clearQrProductMismatch("Đổi đơn sang mã sản phẩm mới: " + expectedCode);
  }

  char c = 0;
  while (qrUsbCharQueue != nullptr && xQueueReceive(qrUsbCharQueue, &c, 0) == pdTRUE) {
    processQrInputChar(c);
  }

  if (qrRxBuffer.length() > 0 && millis() - qrLastByteTime > QR_IDLE_FRAME_TIMEOUT_MS) {
    processQrFrame(qrRxBuffer);
    qrRxBuffer = "";
  }
}
