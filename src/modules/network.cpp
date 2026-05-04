#include "network.h"

//----------------------------------------Network Setup Functions
void loadWiFiConfig() {
  // Set default values first
  wifi_static_ip = IPAddress(192, 168, 1, 201);  // Default static IP
  wifi_gateway = IPAddress(192, 168, 1, 1);      // Default gateway
  wifi_subnet = IPAddress(255, 255, 255, 0);     // Default subnet
  wifi_dns1 = IPAddress(8, 8, 8, 8);             // Google DNS
  wifi_dns2 = IPAddress(8, 8, 4, 4);             // Google DNS backup
  
  if (LittleFS.exists("/wifi_config.json")) {
    File file = LittleFS.open("/wifi_config.json", "r");
    if (file) {
      String content = file.readString();
      file.close();
      
      DynamicJsonDocument doc(1024);
      if (deserializeJson(doc, content) == DeserializationError::Ok) {
        wifi_ssid = doc["ssid"].as<String>();
        wifi_password = doc["password"].as<String>();
        wifi_use_static_ip = doc["use_static_ip"] | false;
        
        if (wifi_use_static_ip) {
          String ip_str = doc["static_ip"].as<String>();
          String gateway_str = doc["gateway"].as<String>();
          String subnet_str = doc["subnet"].as<String>();
          String dns1_str = doc["dns1"].as<String>();
          String dns2_str = doc["dns2"].as<String>();

          if (ip_str.length() > 0) wifi_static_ip.fromString(ip_str);
          if (gateway_str.length() > 0) wifi_gateway.fromString(gateway_str);
          if (subnet_str.length() > 0) wifi_subnet.fromString(subnet_str);
          if (dns1_str.length() > 0) wifi_dns1.fromString(dns1_str);
          if (dns2_str.length() > 0) wifi_dns2.fromString(dns2_str);
        }
        
        Serial.println("WiFi config loaded: " + wifi_ssid);
        if (wifi_use_static_ip) {
          Serial.println("Static IP: " + wifi_static_ip.toString());
          Serial.println("Gateway: " + wifi_gateway.toString());
          Serial.println("Subnet: " + wifi_subnet.toString());
        }
      }
    }
  } else {
    Serial.println("No WiFi config found, using defaults");
  }
}

void saveWiFiConfig(String ssid, String password, bool useStaticIP,
                   String staticIP, String gateway, String subnet,
                   String dns1, String dns2) {
  DynamicJsonDocument doc(1024);
  doc["ssid"] = ssid;
  doc["password"] = password;
  doc["use_static_ip"] = useStaticIP;
  
  if (useStaticIP) {
    doc["static_ip"] = staticIP;
    doc["gateway"] = gateway;
    doc["subnet"] = subnet;
    doc["dns1"] = dns1;
    doc["dns2"] = dns2;
  }
  
  File file = LittleFS.open("/wifi_config.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    Serial.println("WiFi config saved");
  }
}

// Generate unique Ethernet MAC address based on ESP32 WiFi MAC
void generateEthernetMAC() {
  // Get WiFi MAC address
  uint8_t wifiMac[6];
  WiFi.macAddress(wifiMac);
  
  // Generate Ethernet MAC by modifying the WiFi MAC
  // Keep first 3 bytes (OUI) and modify last 3 bytes
  mac[0] = wifiMac[0];
  mac[1] = wifiMac[1]; 
  mac[2] = wifiMac[2];
  mac[3] = wifiMac[3] ^ 0x02; // XOR to make it different
  mac[4] = wifiMac[4] ^ 0x01;
  mac[5] = wifiMac[5] ^ 0x03;
  
  // Make sure it's a locally administered MAC (bit 1 of first byte = 1)
  mac[0] |= 0x02;
  
  Serial.print("Generated Ethernet MAC: ");
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n", 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool setupEthernet() {
  Serial.println("Trying Ethernet connection...");
  
  try {
    // Generate unique MAC address for Ethernet
    generateEthernetMAC();
    
    // called before ETH.begin()
    ESP32_W5500_onEvent();
    
    // Initialize W5500 with static IP
    ETH.begin(MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac);
    ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
    
    // Wait for connection with timeout
    unsigned long startTime = millis();
    while (!ESP32_W5500_isConnected() && millis() - startTime < 10000) {
      delay(100);
    }
    
    if (ESP32_W5500_isConnected()) {
      ethernetConnected = true;
      currentNetworkMode = ETHERNET_MODE;
      Serial.println("Ethernet connected!");
      Serial.print("IP: ");
      Serial.println(ETH.localIP());
      return true;
    }
  } catch (...) {
    Serial.println("Ethernet failed");
  }
  
  ethernetConnected = false;
  return false;
}

bool setupWiFiSTA() {
  if (wifi_ssid.length() == 0) {
    Serial.println("No WiFi credentials found");
    return false;
  }
  
  Serial.println("Trying WiFi connection to: " + wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);  // Tự động kết nối lại
  WiFi.persistent(true);        // Lưu cấu hình WiFi
  
  // Configure static IP if enabled
  if (wifi_use_static_ip) {
    Serial.println("Using static IP: " + wifi_static_ip.toString());
    if (!WiFi.config(wifi_static_ip, wifi_gateway, wifi_subnet, wifi_dns1, wifi_dns2)) {
      Serial.println("Failed to configure static IP");
      return false;
    }
  }
  
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  
  Serial.print("Connecting");
  unsigned long startTime = millis();
  int dotCount = 0;
  
  // Tối ưu thời gian chờ 
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(300);
    Serial.print(".");
    dotCount++;
    
    // Hiển thị tiến trình mỗi 10 dots
    if (dotCount % 10 == 0) {
      Serial.print(" (" + String((millis() - startTime)/1000) + "s)");
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    currentNetworkMode = WIFI_STA_MODE;
    Serial.println();
    Serial.println(" WiFi connected successfully!");
    Serial.println(" Network Information:");
    Serial.println("   IP Address: " + WiFi.localIP().toString());
    Serial.println("   Gateway: " + WiFi.gatewayIP().toString());
    Serial.println("   Subnet: " + WiFi.subnetMask().toString());
    Serial.println("   DNS: " + WiFi.dnsIP().toString());
    Serial.println("   Signal: " + String(WiFi.RSSI()) + " dBm");
    
    // Hiển thị URL truy cập web interface
    Serial.println(" Web Interface URLs:");
    Serial.println("   Main: http://" + WiFi.localIP().toString() + "/");
    Serial.println("   Test: http://" + WiFi.localIP().toString() + "/test");
    
    return true;
  }
  
  Serial.println();
  Serial.println(" WiFi connection failed after 10 seconds");
  wifiConnected = false;
  return false;
}

void setupWiFiAP() {
  Serial.println("Starting WiFi Access Point mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress apIP = WiFi.softAPIP();
  currentNetworkMode = WIFI_AP_MODE;
  
  Serial.println("Access Point started!");
  Serial.print("AP SSID: ");
  Serial.println(ap_ssid);
  Serial.print("AP Password: ");
  Serial.println(ap_password);
  Serial.print("AP IP: ");
  Serial.println(apIP);
}

void setupNetwork() {
  loadWiFiConfig();
  
  // Try Ethernet first
  if (setupEthernet()) {
    Serial.println("Using Ethernet connection");
    return;
  }
  
  // Try WiFi STA if Ethernet fails
  if (setupWiFiSTA()) {
    Serial.println("Using WiFi STA connection");
    return;
  }
  
  // Fallback AP mode
  setupWiFiAP();
  Serial.println("Using WiFi AP mode for configuration");
}

void setupRealtimeTransport() {
  setupRealtimeServer();

  DynamicJsonDocument readyDoc(256);
  readyDoc["source"] = "SYSTEM";
  readyDoc["action"] = "REALTIME_READY";
  readyDoc["status"] = "ESP32_ONLINE";
  readyDoc["timestamp"] = millis();
  String readyMessage;
  serializeJson(readyDoc, readyMessage);
  broadcastRealtimeMessage(TOPIC_IR_CMD, readyMessage);

  // CHỈ setup MQTT2 khi có Internet connection và KeyLogin được cấu hình
  if (currentNetworkMode != WIFI_AP_MODE && mqtt2_password.length() > 0) {
    Serial.println("Setting up MQTT2");
    setupMQTT2();
  } else {
    if (currentNetworkMode == WIFI_AP_MODE) {
      Serial.println("Skipping MQTT2 setup (AP modet)");
    } else if (mqtt2_password.length() == 0) {
      Serial.println("Skipping MQTT2 setup (not KeyLogin)");
    }
  }
}
