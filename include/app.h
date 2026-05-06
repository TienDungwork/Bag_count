#pragma once

#include <Arduino.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_task_wdt.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WebServer_ESP32_SC_W5500.hpp>
#include <AsyncWebServer_ESP32_SC_W5500.h>

#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
#endif

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include <FS.h>

class IRrecv;
struct decode_results;

#ifdef INT_GPIO
#undef INT_GPIO
#endif
#ifdef MISO_GPIO
#undef MISO_GPIO
#endif
#ifdef MOSI_GPIO
#undef MOSI_GPIO
#endif
#ifdef SCK_GPIO
#undef SCK_GPIO
#endif
#ifdef CS_GPIO
#undef CS_GPIO
#endif

#define INT_GPIO            45
#define MISO_GPIO           37
#define MOSI_GPIO           35
#define SCK_GPIO            36
#define CS_GPIO             48
#define SPI_CLOCK_MHZ       25

#define R1_PIN 10
#define G1_PIN 46
#define B1_PIN 3
#define R2_PIN 18
#define G2_PIN 17
#define B2_PIN 16
#define A_PIN 14
#define B_PIN 13
#define C_PIN 12
#define D_PIN 11
#define E_PIN -1
#define LAT_PIN 7
#define OE_PIN 21
#define CLK_PIN 15

#define SENSOR_PIN 40
#define TRIGGER_SENSOR_PIN 4
#define OUTPUT_TRIGGER_SENSOR_PIN 39
#define START_LED_PIN 38
#define DONE_LED_PIN 5
#define BUTTON_PIN3 2
#define BUTTON_PIN2 42
#define RECV_PIN 1

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 2

#define BAGTYPES_FILE "/bagtypes.json"
#define BAGCONFIGS_FILE "/bagconfigs.json"

#ifndef QR_READER_BAUD
#define QR_READER_BAUD 115200
#endif

#ifndef QR_READER_RX_PIN
#define QR_READER_RX_PIN 44
#endif

#ifndef QR_READER_TX_PIN
#define QR_READER_TX_PIN 43
#endif

static const int DEFAULT_SENSOR_DETECTED_LEVEL = LOW;
static const int DEFAULT_SENSOR_CLEAR_LEVEL = HIGH;
static const uint16_t REALTIME_WS_PORT = 81;
static const unsigned long MQTT_PUBLISH_INTERVAL = 500;
static const unsigned long HEARTBEAT_INTERVAL = 3000;
static const unsigned long COUNT_PUBLISH_THROTTLE = 100;
static const unsigned long COUNT_PUBLISH_INTERVAL = 100;

static const char* TOPIC_STATUS = "bagcounter/status";
static const char* TOPIC_COUNT = "bagcounter/count";
static const char* TOPIC_ALERTS = "bagcounter/alerts";
static const char* TOPIC_SENSOR = "bagcounter/sensor";
static const char* TOPIC_HEARTBEAT = "bagcounter/heartbeat";
static const char* TOPIC_IR_CMD = "bagcounter/ir_command";
static const char* TOPIC_CMD_START = "bagcounter/cmd/start";
static const char* TOPIC_CMD_PAUSE = "bagcounter/cmd/pause";
static const char* TOPIC_CMD_RESET = "bagcounter/cmd/reset";
static const char* TOPIC_CMD_BATCH = "bagcounter/cmd/batch_info";
static const char* TOPIC_CMD_TARGET = "bagcounter/cmd/target";
static const char* TOPIC_CONFIG = "bagcounter/config/update";

enum NetworkMode {
  ETHERNET_MODE,
  WIFI_STA_MODE,
  WIFI_AP_MODE
};

struct WeightDelayRule {
  float weight;
  int delay;
};

struct HistoryItem {
  String time;
  int count;
  String type;
};

struct BagConfig {
  String type;
  int target;
  int warn;
  String status;
};

inline const char* sensorRawStateName(int state) {
  return state == HIGH ? "HIGH" : "LOW";
}

inline const char* sensorLevelName(int level) {
  return level == HIGH ? "HIGH" : "LOW";
}

struct NetworkState {
  byte mac[6] = {};
  NetworkMode currentNetworkMode = ETHERNET_MODE;
  bool ethernetConnected = false;
  bool wifiConnected = false;
  String wifi_ssid = "";
  String wifi_password = "";
  bool wifi_use_static_ip = false;
  IPAddress wifi_static_ip;
  IPAddress wifi_gateway;
  IPAddress wifi_subnet;
  IPAddress wifi_dns1;
  IPAddress wifi_dns2;
  const char* ap_ssid = "BO DEM THONG MINH";
  const char* ap_password = "0989328858";
  IPAddress local_IP = IPAddress(192, 168, 41, 200);
  IPAddress gateway = IPAddress(192, 168, 41, 1);
  IPAddress subnet = IPAddress(255, 255, 255, 0);
  IPAddress primaryDNS = IPAddress(8, 8, 8, 8);
  IPAddress secondaryDNS = IPAddress(8, 8, 4, 4);
};

struct MqttState {
  String mqtt_server2 = "103.57.220.146";
  int mqtt_port2 = 1884;
  String mqtt2_username = "countingsystem";
  String mqtt2_password = "";
  unsigned long lastMqttPublish = 0;
  unsigned long lastHeartbeat = 0;
  unsigned long lastCountPublish = 0;
};

struct RuntimeServices {
  WebServer server;
  AsyncWebServer realtimeServer;
  AsyncWebSocket realtimeSocket;
  WiFiClient ethClient2;
  PubSubClient mqtt2;

  RuntimeServices()
    : server(80),
      realtimeServer(REALTIME_WS_PORT),
      realtimeSocket("/ws"),
      mqtt2(ethClient2) {}
};

struct SettingsState {
  int bagDetectionDelay;
  int minBagInterval;
  bool autoReset;
  String conveyorName;
  String location;
  int displayBrightness;
  int sensorDelayMs;
  int relayDelayAfterComplete;
  int bagTimeMultiplier;
  bool enableWeightBasedDelay = false;
  int countSensorActiveLevel = DEFAULT_SENSOR_DETECTED_LEVEL;
  int inputSensorActiveLevel = DEFAULT_SENSOR_DETECTED_LEVEL;
  int outputSensorActiveLevel = DEFAULT_SENSOR_DETECTED_LEVEL;
  std::vector<WeightDelayRule> weightDelayRules;
};

struct CounterState {
  unsigned long lastBagTime = 0;
  unsigned long bagStartTime = 0;
  bool isBagDetected = false;
  bool waitingForInterval = false;
  unsigned long sensorClearStartTime = 0;
  bool isWaitingForBagGroupEnd = false;
  unsigned long sensorActiveStartTime = 0;
  unsigned long lastMeasuredTime = 0;
  bool isMeasuringSensor = false;
  int lastTimingSensorState = DEFAULT_SENSOR_CLEAR_LEVEL;
  unsigned long orderCompleteTime = 0;
  bool isOrderComplete = false;
  bool isRelayDelayActive = false;
  unsigned long warningLedStartTime = 0;
  bool isWarningLedActive = false;
  bool hasReachedWarningThreshold = false;
  bool isManualRelayOn = false;
  unsigned long totalCount = 0;
  unsigned long lastUpdate = 0;
  bool isRunning = false;
  bool isLimitReached = false;
  bool finishedBlinking = false;
  int blinkCount = 0;
  bool isBlinking = false;
  unsigned long lastBlink = 0;
  bool needUpdate = true;
  String startTimeStr = "";
  bool timeWaitingForSync = false;
  String currentSystemStatus = "RESET";
  unsigned long lastDebounceTime = 0;
  unsigned long debounceDelay = 0;
  int lastSensorState = DEFAULT_SENSOR_CLEAR_LEVEL;
  int sensorState = DEFAULT_SENSOR_CLEAR_LEVEL;
  int lastTriggerState = HIGH;
  int triggerState;
  int lastOutputTriggerState = HIGH;
  int outputTriggerState = HIGH;
  unsigned long inputTriggerDebounceTime = 0;
  unsigned long outputTriggerDebounceTime = 0;
  bool inputTriggerBlockedState = false;
  bool outputTriggerBlockedState = false;
  bool isCountingEnabled = false;
  bool isTriggerEnabled = false;
  bool isCounting = false;
  bool isStartAuthorized = false;
  bool waitForSensorClearOnStart = false;
  bool startLedOn = false;
  bool doneLedOn = false;
};

struct DisplayState {
  MatrixPanel_I2S_DMA *dma_display = nullptr;
  uint16_t myBLACK;
  uint16_t myWHITE;
  uint16_t myRED;
  uint16_t myGREEN;
  uint16_t myBLUE;
  uint16_t myYELLOW;
  uint16_t myCYAN;
  bool systemConnected = false;
  bool showConnectingAnimation = true;
  bool showNetworkIp = false;
  unsigned long showNetworkIpUntil = 0;
  String networkIpText = "";
  unsigned long connectingAnimationTime = 0;
  int connectingDots = 0;
};

struct OrderState {
  std::vector<HistoryItem> history;
  String bagType = "MA SP";
  String productCode = "";
  String orderCode = "";
  String customerName = "";
  String vehicleNumber = "";
  int targetCount = 0;
  std::vector<String> bagTypes;
  String currentBatchName = "";
  String currentBatchId = "";
  String currentBatchDescription = "";
  int totalOrdersInBatch = 0;
  int batchTotalTarget = 0;
  String currentMode = "output";
  std::vector<BagConfig> bagConfigs;
};

struct StorageState {
  DynamicJsonDocument productsData;
  DynamicJsonDocument ordersData;
  bool dataLoaded = false;
  unsigned long lastWeightErrorLogMs = 0;
  float currentOrderUnitWeight = 0.0f;

  StorageState() : productsData(4096), ordersData(65536) {}
};

struct InputState {
  unsigned long lastIRCode = 0;
  unsigned long lastIRTime = 0;
  unsigned long debounceIRTime = 200;
  String lastIRCommand = "";
  unsigned long lastIRTimestamp = 0;
  bool hasNewIRCommand = false;
};

struct QrReaderState {
  String rxBuffer = "";
  String lastScannedCode = "";
  String mismatchScannedCode = "";
  String mismatchExpectedCode = "";
  unsigned long lastByteTime = 0;
  unsigned long lastScanTime = 0;
  unsigned long totalBytesReceived = 0;
  bool hasSeenData = false;
  bool productMismatchActive = false;
};

struct AppContext {
  NetworkState network;
  MqttState mqtt;
  RuntimeServices services;
  SettingsState settings;
  CounterState counter;
  DisplayState display;
  OrderState orders;
  StorageState storage;
  InputState input;
  QrReaderState qrReader;
};

extern AppContext app;
AppContext& appContext();

static auto& mac = app.network.mac;
static auto& currentNetworkMode = app.network.currentNetworkMode;
static auto& ethernetConnected = app.network.ethernetConnected;
static auto& wifiConnected = app.network.wifiConnected;
static auto& wifi_ssid = app.network.wifi_ssid;
static auto& wifi_password = app.network.wifi_password;
static auto& wifi_use_static_ip = app.network.wifi_use_static_ip;
static auto& wifi_static_ip = app.network.wifi_static_ip;
static auto& wifi_gateway = app.network.wifi_gateway;
static auto& wifi_subnet = app.network.wifi_subnet;
static auto& wifi_dns1 = app.network.wifi_dns1;
static auto& wifi_dns2 = app.network.wifi_dns2;
static auto& ap_ssid = app.network.ap_ssid;
static auto& ap_password = app.network.ap_password;
static auto& mqtt_server2 = app.mqtt.mqtt_server2;
static auto& mqtt_port2 = app.mqtt.mqtt_port2;
static auto& mqtt2_username = app.mqtt.mqtt2_username;
static auto& mqtt2_password = app.mqtt.mqtt2_password;
static auto& lastMqttPublish = app.mqtt.lastMqttPublish;
static auto& lastHeartbeat = app.mqtt.lastHeartbeat;
static auto& lastCountPublish = app.mqtt.lastCountPublish;
static auto& local_IP = app.network.local_IP;
static auto& gateway = app.network.gateway;
static auto& subnet = app.network.subnet;
static auto& primaryDNS = app.network.primaryDNS;
static auto& secondaryDNS = app.network.secondaryDNS;
static auto& server = app.services.server;
static auto& realtimeServer = app.services.realtimeServer;
static auto& realtimeSocket = app.services.realtimeSocket;
static auto& ethClient2 = app.services.ethClient2;
static auto& mqtt2 = app.services.mqtt2;

static auto& bagDetectionDelay = app.settings.bagDetectionDelay;
static auto& minBagInterval = app.settings.minBagInterval;
static auto& autoReset = app.settings.autoReset;
static auto& conveyorName = app.settings.conveyorName;
static auto& location = app.settings.location;
static auto& displayBrightness = app.settings.displayBrightness;
static auto& sensorDelayMs = app.settings.sensorDelayMs;
static auto& relayDelayAfterComplete = app.settings.relayDelayAfterComplete;
static auto& bagTimeMultiplier = app.settings.bagTimeMultiplier;
static auto& enableWeightBasedDelay = app.settings.enableWeightBasedDelay;
static auto& countSensorActiveLevel = app.settings.countSensorActiveLevel;
static auto& inputSensorActiveLevel = app.settings.inputSensorActiveLevel;
static auto& outputSensorActiveLevel = app.settings.outputSensorActiveLevel;
static auto& weightDelayRules = app.settings.weightDelayRules;
static auto& lastBagTime = app.counter.lastBagTime;
static auto& bagStartTime = app.counter.bagStartTime;
static auto& isBagDetected = app.counter.isBagDetected;
static auto& waitingForInterval = app.counter.waitingForInterval;
static auto& sensorClearStartTime = app.counter.sensorClearStartTime;
static auto& isWaitingForBagGroupEnd = app.counter.isWaitingForBagGroupEnd;
static auto& sensorActiveStartTime = app.counter.sensorActiveStartTime;
static auto& lastMeasuredTime = app.counter.lastMeasuredTime;
static auto& isMeasuringSensor = app.counter.isMeasuringSensor;
static auto& lastTimingSensorState = app.counter.lastTimingSensorState;
static auto& orderCompleteTime = app.counter.orderCompleteTime;
static auto& isOrderComplete = app.counter.isOrderComplete;
static auto& isRelayDelayActive = app.counter.isRelayDelayActive;
static auto& warningLedStartTime = app.counter.warningLedStartTime;
static auto& isWarningLedActive = app.counter.isWarningLedActive;
static auto& hasReachedWarningThreshold = app.counter.hasReachedWarningThreshold;
static auto& isManualRelayOn = app.counter.isManualRelayOn;
static auto& dma_display = app.display.dma_display;
static auto& myBLACK = app.display.myBLACK;
static auto& myWHITE = app.display.myWHITE;
static auto& myRED = app.display.myRED;
static auto& myGREEN = app.display.myGREEN;
static auto& myBLUE = app.display.myBLUE;
static auto& myYELLOW = app.display.myYELLOW;
static auto& myCYAN = app.display.myCYAN;
static auto& history = app.orders.history;
static auto& bagType = app.orders.bagType;
static auto& productCode = app.orders.productCode;
static auto& orderCode = app.orders.orderCode;
static auto& customerName = app.orders.customerName;
static auto& vehicleNumber = app.orders.vehicleNumber;
static auto& targetCount = app.orders.targetCount;
static auto& bagTypes = app.orders.bagTypes;
static auto& currentBatchName = app.orders.currentBatchName;
static auto& currentBatchId = app.orders.currentBatchId;
static auto& currentBatchDescription = app.orders.currentBatchDescription;
static auto& totalOrdersInBatch = app.orders.totalOrdersInBatch;
static auto& batchTotalTarget = app.orders.batchTotalTarget;
static auto& currentMode = app.orders.currentMode;
static auto& totalCount = app.counter.totalCount;
static auto& lastUpdate = app.counter.lastUpdate;
static auto& isRunning = app.counter.isRunning;
static auto& isLimitReached = app.counter.isLimitReached;
static auto& finishedBlinking = app.counter.finishedBlinking;
static auto& blinkCount = app.counter.blinkCount;
static auto& isBlinking = app.counter.isBlinking;
static auto& lastBlink = app.counter.lastBlink;
static auto& needUpdate = app.counter.needUpdate;
static auto& startTimeStr = app.counter.startTimeStr;
static auto& timeWaitingForSync = app.counter.timeWaitingForSync;
static auto& currentSystemStatus = app.counter.currentSystemStatus;
static auto& lastDebounceTime = app.counter.lastDebounceTime;
static auto& debounceDelay = app.counter.debounceDelay;
static auto& lastSensorState = app.counter.lastSensorState;
static auto& sensorState = app.counter.sensorState;
static auto& lastTriggerState = app.counter.lastTriggerState;
static auto& triggerState = app.counter.triggerState;
static auto& lastOutputTriggerState = app.counter.lastOutputTriggerState;
static auto& outputTriggerState = app.counter.outputTriggerState;
static auto& inputTriggerDebounceTime = app.counter.inputTriggerDebounceTime;
static auto& outputTriggerDebounceTime = app.counter.outputTriggerDebounceTime;
static auto& inputTriggerBlockedState = app.counter.inputTriggerBlockedState;
static auto& outputTriggerBlockedState = app.counter.outputTriggerBlockedState;
static auto& isCountingEnabled = app.counter.isCountingEnabled;
static auto& isTriggerEnabled = app.counter.isTriggerEnabled;
static auto& isCounting = app.counter.isCounting;
static auto& isStartAuthorized = app.counter.isStartAuthorized;
static auto& waitForSensorClearOnStart = app.counter.waitForSensorClearOnStart;
static auto& startLedOn = app.counter.startLedOn;
static auto& doneLedOn = app.counter.doneLedOn;
static auto& systemConnected = app.display.systemConnected;
static auto& showConnectingAnimation = app.display.showConnectingAnimation;
static auto& showNetworkIp = app.display.showNetworkIp;
static auto& showNetworkIpUntil = app.display.showNetworkIpUntil;
static auto& networkIpText = app.display.networkIpText;
static auto& connectingAnimationTime = app.display.connectingAnimationTime;
static auto& connectingDots = app.display.connectingDots;
static auto& productsData = app.storage.productsData;
static auto& ordersData = app.storage.ordersData;
static auto& dataLoaded = app.storage.dataLoaded;
static auto& lastWeightErrorLogMs = app.storage.lastWeightErrorLogMs;
static auto& currentOrderUnitWeight = app.storage.currentOrderUnitWeight;
extern IRrecv irrecv;
extern decode_results results;
static auto& lastIRCode = app.input.lastIRCode;
static auto& lastIRTime = app.input.lastIRTime;
static auto& debounceIRTime = app.input.debounceIRTime;
static auto& lastIRCommand = app.input.lastIRCommand;
static auto& lastIRTimestamp = app.input.lastIRTimestamp;
static auto& hasNewIRCommand = app.input.hasNewIRCommand;
static auto& bagConfigs = app.orders.bagConfigs;
static auto& qrRxBuffer = app.qrReader.rxBuffer;
static auto& qrLastScannedCode = app.qrReader.lastScannedCode;
static auto& qrMismatchScannedCode = app.qrReader.mismatchScannedCode;
static auto& qrMismatchExpectedCode = app.qrReader.mismatchExpectedCode;
static auto& qrLastByteTime = app.qrReader.lastByteTime;
static auto& qrLastScanTime = app.qrReader.lastScanTime;
static auto& qrTotalBytesReceived = app.qrReader.totalBytesReceived;
static auto& qrHasSeenData = app.qrReader.hasSeenData;
static auto& qrProductMismatchActive = app.qrReader.productMismatchActive;

unsigned long mapIRButton(unsigned long code);
void handleIRCommand(int button);
void loadCurrentOrderForDisplay();
void handleWebCommand(int button);
void setupQrReader();
void handleQrReader();
void clearQrProductMismatch(const String& reason = "");
String currentExpectedQrProductCode();

void saveBagTypesToFile();
void loadBagTypesFromFile();
void saveBatchInfoToFile();
void loadBatchInfoFromFile();
void saveBagConfigsToFile();
void loadBagConfigsFromFile();
void copyJsonObject(JsonObject src, JsonObject dst);
String orderProductCodeFromJson(JsonObject order);
float resolveUnitWeightFromData(const String& wantedOrderCode, const String& wantedProductCode, const String& wantedProductName);
void saveSettingsToFile();
void loadSettingsFromFile();
void createDefaultSettingsFile();
void createDefaultDataFiles();
void debugSettingsFile();
void saveProductsToFile();
void loadProductsFromFile();
void saveOrdersToFile();
void loadOrdersFromFile();
void addNewProduct(String code, String name);
void deleteProduct(int productId);
void addNewOrder(String productCode, String customerName, int quantity, String notes);
void deleteOrder(int orderId);
void printDataStatus();

void loadWiFiConfig();
void saveWiFiConfig(String ssid, String password, bool useStaticIP = false,
                    String staticIP = "", String gateway = "", String subnet = "",
                    String dns1 = "", String dns2 = "");
void generateEthernetMAC();
bool setupEthernet();
bool setupWiFiSTA();
void setupWiFiAP();
void setupNetwork();
void setupRealtimeTransport();

void setupMQTT2();
void onMqttMessage2(char* topic, byte* payload, unsigned int length);
void publishMQTT2OrderComplete();
void setupTime();
String getTimeStr();
void broadcastRealtimeMessage(const char* topic, const String& payloadJson);
void sendRealtimeSnapshot(AsyncWebSocketClient *client);
void onRealtimeSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void setupRealtimeServer();

void handleRealtimeMessage(const String& topicStr, const String& message);
void publishStatusMQTT();
void publishCountUpdate();
void publishAlert(String alertType, String message);
void publishSensorData();
void updateSensorTimingMeasurement();
void publishHeartbeat();
void publishBagConfigs();

void setupWebServer();

void updateDisplay();
void displayCurrentOrderInfo();
void showConnectingDisplay();
void showNetworkIpDisplay();
void drawVietnameseText(int x, int y, const String& text, uint16_t color, float textSize = 1);
void setSystemConnected();

inline int sensorClearLevelForActive(int activeLevel) {
  return activeLevel == HIGH ? LOW : HIGH;
}

inline bool isCountSensorBlocked(int state) {
  return state == countSensorActiveLevel;
}

inline bool isSensorBlocked(int state) {
  return isCountSensorBlocked(state);
}

inline bool isInputSensorBlocked(int state) {
  return state == inputSensorActiveLevel;
}

inline bool isOutputSensorBlocked(int state) {
  return state == outputSensorActiveLevel;
}

inline bool isTriggerSensorBlocked(int state) {
  return currentMode == "input" ? isInputSensorBlocked(state) : isOutputSensorBlocked(state);
}

inline bool isInputTriggerBlocked(int state) {
  return isInputSensorBlocked(state);
}

inline bool isOutputTriggerBlocked(int state) {
  return isOutputSensorBlocked(state);
}

int calculateDynamicBagDetectionDelay();
int calculateBagCountFromDuration(unsigned long detectionDuration);
unsigned long bagGroupGapToleranceMs();
void updateCount();
void updateCount(int bagCount);
void updateDoneLED();
void updateStartLED();
