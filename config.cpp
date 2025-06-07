// config.cpp
#include "config.h"

namespace Config {
  // Network Configuration
  const char* AP_SSID = "WROOM_Display";
  const char* AP_PASSWORD = "12345678";
  const IPAddress LOCAL_IP(192, 168, 4, 1);
  const IPAddress GATEWAY(192, 168, 4, 1);
  const IPAddress SUBNET(255, 255, 255, 0);
  const int UDP_PORT = 4210;
  
  // Display Configuration
  const uint16_t DISPLAY_WIDTH = 480;
  const uint16_t DISPLAY_HEIGHT = 320;
  const uint8_t DISPLAY_ROTATION = 1;
  
  // High-Speed Configuration
  const uint32_t MAX_FRAME_SIZE = 35000;
  const uint32_t FRAME_TIMEOUT = 150;
  const uint32_t MIN_HEAP_SIZE = 15000;
  const uint32_t TARGET_FPS = 60;
  const uint32_t MIN_RENDER_INTERVAL = 16;   // 16ms = 60 FPS
  const uint32_t FAST_RENDER_INTERVAL = 8;   // 8ms = 125 FPS
  
  // Display Buffer Configuration
  const uint32_t DISPLAY_BUFFER_SIZE = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2; // 16-bit pixels
  const uint16_t STRIP_HEIGHT = 20;
  const uint16_t FAST_STRIP_HEIGHT = 10;
  const uint16_t MAX_PACKETS = 500;
}
