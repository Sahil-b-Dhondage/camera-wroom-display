#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiUdp.h>

#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"

// WiFi settings - Connect to WROOM's Access Point
const char* ssid = "WROOM_Display";
const char* password = "12345678";

// UDP settings - Send to WROOM's IP
const char *udpAddress = "192.168.4.1";  // WROOM's AP IP
const int udpPort = 4210;
const int maxPacketSize = 1400;

// LED for status indication
#define LED_PIN 33
#define LED_ON LOW
#define LED_OFF HIGH

// Camera settings
#define FRAME_SIZE FRAMESIZE_QVGA  // 320x240
#define JPEG_QUALITY 15            // Good balance of quality/size

// Frame rate control
unsigned long previousFrameTime = 0;
const int frameInterval = 200;     // 5 fps (200ms between frames)

// UDP instance
WiFiUDP udp;

// Frame counter and statistics
uint32_t frameCount = 0;
uint32_t packetCount = 0;
uint32_t successfulFrames = 0;
uint32_t failedFrames = 0;
unsigned long lastStatsTime = 0;
bool isConnectedToWROOM = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32-CAM Client Mode (Simple) ===");
  
  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ON); // LED on during setup
  
  // Print chip info
  Serial.printf("ESP32 Chip model: %s\n", ESP.getChipModel());
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("PSRAM size: %d bytes\n", ESP.getPsramSize());
  
  // Initialize camera
  Serial.println("Initializing camera...");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAME_SIZE;
  config.jpeg_quality = JPEG_QUALITY;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    while(1) {
      digitalWrite(LED_PIN, LED_ON);
      delay(100);
      digitalWrite(LED_PIN, LED_OFF);
      delay(100);
    }
  }
  Serial.println("✓ Camera initialized successfully");
  
  // Test camera
  Serial.println("Testing camera capture...");
  camera_fb_t *test_fb = esp_camera_fb_get();
  if (test_fb) {
    Serial.printf("✓ Test capture: %dx%d, %d bytes\n", 
                 test_fb->width, test_fb->height, test_fb->len);
    esp_camera_fb_return(test_fb);
  } else {
    Serial.println("✗ Test capture failed!");
    while(1) {
      digitalWrite(LED_PIN, LED_ON);
      delay(200);
      digitalWrite(LED_PIN, LED_OFF);
      delay(200);
    }
  }
  
  // Connect to WROOM's Access Point
  Serial.println("Connecting to WROOM's WiFi AP...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Wait for connection with detailed feedback
  int timeout = 30;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    timeout--;
    Serial.printf("WiFi status: %d, attempt %d/30\n", WiFi.status(), 31-timeout);
    
    // Blink LED during connection
    digitalWrite(LED_PIN, (timeout % 2) ? LED_ON : LED_OFF);
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ WiFi connection to WROOM failed!");
    Serial.printf("Final WiFi status: %d\n", WiFi.status());
    Serial.println("Make sure WROOM's AP is running first!");
    while(1) {
      digitalWrite(LED_PIN, LED_ON);
      delay(500);
      digitalWrite(LED_PIN, LED_OFF);
      delay(500);
    }
  }
  
  // WiFi connected - print detailed info
  isConnectedToWROOM = true;
  Serial.println("✓ WiFi connected to WROOM AP!");
  Serial.printf("✓ CAM IP address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("✓ Gateway (WROOM): %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("✓ Signal strength: %d dBm\n", WiFi.RSSI());
  Serial.printf("✓ MAC address: %s\n", WiFi.macAddress().c_str());
  
  // Test network connectivity
  Serial.println("Testing network connectivity...");
  Serial.printf("✓ Connected to WROOM gateway: %s\n", WiFi.gatewayIP().toString().c_str());
  
  // Start UDP
  udp.begin(udpPort);
  Serial.printf("✓ UDP started on port %d\n", udpPort);
  
  // Test UDP connectivity
  Serial.printf("Testing UDP connectivity to WROOM at %s:%d\n", udpAddress, udpPort);
  testUDPConnection();
  
  // Setup complete
  Serial.println("=== CAM Client Setup Complete ===");
  Serial.printf("Streaming to WROOM at: %s:%d\n", udpAddress, udpPort);
  Serial.printf("Frame size: QVGA (320x240)\n");
  Serial.printf("JPEG quality: %d\n", JPEG_QUALITY);
  Serial.printf("Frame interval: %d ms (5 FPS)\n", frameInterval);
  Serial.println("Starting video streaming to WROOM...");
  
  digitalWrite(LED_PIN, LED_OFF);
  lastStatsTime = millis();
}

void testUDPConnection() {
  // Send a test packet to WROOM
  const char* testMessage = "ESP32-CAM-HELLO-WROOM";
  
  udp.beginPacket(udpAddress, udpPort);
  udp.write((const uint8_t*)testMessage, strlen(testMessage));
  bool success = udp.endPacket();
  
  if (success) {
    Serial.println("✓ Test UDP packet sent to WROOM successfully");
  } else {
    Serial.println("✗ Test UDP packet to WROOM failed");
  }
  
  delay(100);
}

void loop() {
  // Print stats every 10 seconds
  if (millis() - lastStatsTime > 10000) {
    printDetailedStats();
    lastStatsTime = millis();
  }
  
  // Check WiFi connection to WROOM
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi disconnected from WROOM, reconnecting...");
    isConnectedToWROOM = false;
    digitalWrite(LED_PIN, LED_ON);
    WiFi.reconnect();
    
    int reconnectTimeout = 10;
    while (WiFi.status() != WL_CONNECTED && reconnectTimeout > 0) {
      delay(500);
      reconnectTimeout--;
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✓ WiFi reconnected to WROOM!");
      Serial.printf("New IP: %s\n", WiFi.localIP().toString().c_str());
      isConnectedToWROOM = true;
    } else {
      Serial.println("\n✗ WiFi reconnection to WROOM failed!");
    }
    digitalWrite(LED_PIN, LED_OFF);
  }
  
  // Only stream if connected to WROOM
  if (!isConnectedToWROOM) {
    delay(1000);
    return;
  }
  
  // Frame rate control
  unsigned long currentTime = millis();
  if (currentTime - previousFrameTime < frameInterval) {
    return;
  }
  previousFrameTime = currentTime;
  
  // Capture frame
  digitalWrite(LED_PIN, LED_ON);
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("✗ Camera capture failed");
    failedFrames++;
    digitalWrite(LED_PIN, LED_OFF);
    delay(100);
    return;
  }
  
  // Validate frame
  if (fb->len == 0) {
    Serial.println("✗ Empty frame captured");
    esp_camera_fb_return(fb);
    failedFrames++;
    digitalWrite(LED_PIN, LED_OFF);
    return;
  }
  
  // Check JPEG header
  if (fb->len < 2 || fb->buf[0] != 0xFF || fb->buf[1] != 0xD8) {
    Serial.println("✗ Invalid JPEG header");
    esp_camera_fb_return(fb);
    failedFrames++;
    digitalWrite(LED_PIN, LED_OFF);
    return;
  }
  
  // Increment frame counter
  frameCount++;
  
  // Send frame to WROOM
  bool frameSuccess = sendFrameToWROOM(fb);
  
  if (frameSuccess) {
    successfulFrames++;
  } else {
    failedFrames++;
  }
  
  // Return the frame buffer
  esp_camera_fb_return(fb);
  
  // Turn off LED
  digitalWrite(LED_PIN, LED_OFF);
  
  // Print frame info every 20 frames
  if (frameCount % 20 == 0) {
    Serial.printf("Frame %u: %dx%d, %u bytes, %s\n", 
                 frameCount, fb->width, fb->height, fb->len, 
                 frameSuccess ? "✓ sent to WROOM" : "✗ failed");
  }
}

bool sendFrameToWROOM(camera_fb_t *fb) {
  // Calculate number of packets needed
  size_t totalBytes = fb->len;
  uint16_t totalPackets = (totalBytes + maxPacketSize - 12 - 1) / (maxPacketSize - 12);
  
  // Prepare header with frame info
  uint8_t headerData[8];
  memcpy(headerData, &frameCount, 4);      // Frame ID
  memcpy(headerData + 4, &totalPackets, 2); // Total packets
  
  // Log frame info for first few frames
  if (frameCount <= 5) {
    Serial.printf("📦 Sending frame %u to WROOM: %u bytes in %u packets\n", 
                 frameCount, totalBytes, totalPackets);
  }
  
  bool allPacketsSuccess = true;
  
  // Send each packet to WROOM
  for (uint16_t packetIndex = 0; packetIndex < totalPackets; packetIndex++) {
    // Calculate chunk size for this packet
    size_t offset = packetIndex * (maxPacketSize - 12);
    size_t packetDataSize = min(maxPacketSize - 12, (int)(totalBytes - offset));
    
    // Create packet: [frameCount(4)][totalPackets(2)][packetIndex(2)][packetSize(4)][data]
    uint8_t packetBuffer[12 + packetDataSize];
    
    // Copy header
    memcpy(packetBuffer, headerData, 6);
    
    // Add packet index
    memcpy(packetBuffer + 6, &packetIndex, 2);
    
    // Add packet data size
    memcpy(packetBuffer + 8, &packetDataSize, 4);
    
    // Copy image data for this packet
    memcpy(packetBuffer + 12, fb->buf + offset, packetDataSize);
    
    // Send UDP packet to WROOM
    udp.beginPacket(udpAddress, udpPort);
    udp.write(packetBuffer, sizeof(packetBuffer));
    bool success = udp.endPacket();
    
    if (success) {
      packetCount++;
    } else {
      if (frameCount <= 5) {
        Serial.printf("✗ Packet %u/%u to WROOM failed\n", packetIndex + 1, totalPackets);
      }
      allPacketsSuccess = false;
    }
    
    // Small delay to prevent UDP buffer overflow
    delay(1);
  }
  
  return allPacketsSuccess;
}

void printDetailedStats() {
  unsigned long uptime = millis() / 1000;
  float actualFps = (float)frameCount / (uptime > 0 ? uptime : 1);
  float successRate = frameCount > 0 ? (float)successfulFrames / frameCount * 100.0f : 0;
  
  Serial.println("=== CAM CLIENT STATISTICS ===");
  Serial.printf("Uptime: %lu seconds\n", uptime);
  Serial.printf("Connection to WROOM: %s\n", isConnectedToWROOM ? "✓ Connected" : "✗ Disconnected");
  Serial.printf("Frames: %u total, %u successful, %u failed\n", 
               frameCount, successfulFrames, failedFrames);
  Serial.printf("Success rate: %.1f%%\n", successRate);
  Serial.printf("Packets sent: %u\n", packetCount);
  Serial.printf("Actual FPS: %.2f\n", actualFps);
  Serial.printf("WiFi signal: %d dBm\n", WiFi.RSSI());
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Target WROOM: %s:%d\n", udpAddress, udpPort);
  Serial.println("============================");
}