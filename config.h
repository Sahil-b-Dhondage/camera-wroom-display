// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <WiFi.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

namespace Config {
  // Network Configuration
  extern const char* AP_SSID;
  extern const char* AP_PASSWORD;
  extern const IPAddress LOCAL_IP;
  extern const IPAddress GATEWAY;
  extern const IPAddress SUBNET;
  extern const int UDP_PORT;
  
  // Display Configuration
  extern const uint16_t DISPLAY_WIDTH;
  extern const uint16_t DISPLAY_HEIGHT;
  extern const uint8_t DISPLAY_ROTATION;
  
  // High-Speed Configuration
  extern const uint32_t MAX_FRAME_SIZE;
  extern const uint32_t FRAME_TIMEOUT;
  extern const uint32_t MIN_HEAP_SIZE;
  extern const uint32_t TARGET_FPS;
  extern const uint32_t MIN_RENDER_INTERVAL;
  extern const uint32_t FAST_RENDER_INTERVAL;
  
  // Display Buffer Configuration
  extern const uint32_t DISPLAY_BUFFER_SIZE;
  extern const uint16_t STRIP_HEIGHT;
  extern const uint16_t FAST_STRIP_HEIGHT;
  extern const uint16_t MAX_PACKETS;
}

// Frame State Structure
struct CompleteFrameState {
  uint16_t frameId;
  uint16_t totalPackets;
  uint16_t receivedPackets;
  uint32_t totalSize;
  uint32_t startTime;
  bool isComplete;
  bool isValid;
  bool isRendering;
  
  void reset() {
    frameId = 0;
    totalPackets = 0;
    receivedPackets = 0;
    totalSize = 0;
    startTime = 0;
    isComplete = false;
    isValid = false;
    isRendering = false;
  }
};

#endif // CONFIG_H
