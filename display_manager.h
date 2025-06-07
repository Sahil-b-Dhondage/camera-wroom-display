// display_manager.h
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "config.h"

class DisplayManager {
private:
  TFT_eSPI tft;
  uint16_t* displayBuffer;
  bool displayBufferEnabled;
  
  DisplayManager() : displayBuffer(nullptr), displayBufferEnabled(false) {}
  
public:
  static DisplayManager& getInstance() {
    static DisplayManager instance;
    return instance;
  }
  
  bool initialize();
  void cleanup();
  bool initializeDisplayBuffer();
  void showStartupMessage(const char* message);
  void clearScreen();
  TFT_eSPI& getTft() { return tft; }
  uint16_t* getDisplayBuffer() { return displayBuffer; }
  bool isDisplayBufferEnabled() const { return displayBufferEnabled; }
  bool renderFrame(uint8_t* frameData, uint32_t size);
  
  // High-speed rendering methods
  bool renderFrameHighSpeed(uint8_t* frameData, uint32_t size);
  void fastStripTransfer();
  
  ~DisplayManager() { cleanup(); }
};

// TJpg callback function
bool highSpeedTftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);

#endif // DISPLAY_MANAGER_H

// display_manager.cpp
#include "display_manager.h"

bool DisplayManager::initialize() {
  Serial.println("Initializing display...");
  
  tft.init();
  tft.setRotation(Config::DISPLAY_ROTATION);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  
  Serial.printf("Display initialized: %dx%d\n", tft.width(), tft.height());
  
  // Initialize TJpg_Decoder
  Serial.println("Initializing high-speed JPEG decoder...");
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(highSpeedTftOutput);
  Serial.println("High-speed JPEG decoder ready");
  
  // Try to initialize display buffer
  initializeDisplayBuffer();
  
  return true;
}

bool DisplayManager::initializeDisplayBuffer() {
  uint32_t availableHeap = ESP.getFreeHeap();
  displayBufferEnabled = (availableHeap > Config::DISPLAY_BUFFER_SIZE + 60000);
  
  if (displayBufferEnabled) {
    displayBuffer = (uint16_t*)heap_caps_malloc(Config::DISPLAY_BUFFER_SIZE, MALLOC_CAP_8BIT);
    if (displayBuffer) {
      memset(displayBuffer, 0, Config::DISPLAY_BUFFER_SIZE);
      Serial.printf("Display buffer allocated: %d KB\n", Config::DISPLAY_BUFFER_SIZE/1024);
    } else {
      displayBufferEnabled = false;
      Serial.println("Display buffer allocation failed");
    }
  }
  
  Serial.printf("Display buffer: %s\n", displayBufferEnabled ? "ENABLED" : "DISABLED");
  return true;
}

void DisplayManager::cleanup() {
  if (displayBuffer) {
    heap_caps_free(displayBuffer);
    displayBuffer = nullptr;
  }
}

void DisplayManager::showStartupMessage(const char* message) {
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(60, 130);
  tft.println("COMPLETE FRAME");
  tft.setCursor(80, 160);
  tft.println("DISPLAY READY");
}

void DisplayManager::clearScreen() {
  tft.fillScreen(TFT_BLACK);
}

bool DisplayManager::renderFrameHighSpeed(uint8_t* frameData, uint32_t size) {
  if (!frameData || size == 0) return false;
  
  uint32_t renderStart = micros();
  
  // Clear display buffer if available
  if (displayBuffer) {
    memset(displayBuffer, 0, Config::DISPLAY_BUFFER_SIZE);
  }
  
  // High-speed JPEG rendering
  TJpgDec.setJpgScale(1);
  bool success = TJpgDec.drawJpg(0, 0, frameData, size);
  
  if (success && displayBuffer) {
    fastStripTransfer();
  }
  
  uint32_t renderTime = micros() - renderStart;
  
  if (success) {
    Serial.printf("Frame rendered in %d µs\n", renderTime);
  }
  
  return success;
}

void DisplayManager::fastStripTransfer() {
  if (!displayBuffer) return;
  
  tft.startWrite();
  
  // Use smaller strips for higher refresh rate
  uint16_t stripHeight = (Config::TARGET_FPS > 45) ? Config::FAST_STRIP_HEIGHT : Config::STRIP_HEIGHT;
  
  for (uint16_t y = 0; y < Config::DISPLAY_HEIGHT; y += stripHeight) {
    uint16_t currentStripHeight = min((uint16_t)stripHeight, (uint16_t)(Config::DISPLAY_HEIGHT - y));
    
    tft.setAddrWindow(0, y, Config::DISPLAY_WIDTH, currentStripHeight);
    
    uint32_t pixelOffset = y * Config::DISPLAY_WIDTH;
    uint32_t pixelsToSend = currentStripHeight * Config::DISPLAY_WIDTH;
    
    tft.pushPixels(&displayBuffer[pixelOffset], pixelsToSend);
    
    // Minimal yielding for maximum speed
    if (y % (stripHeight * 4) == 0) {
      taskYIELD();
    }
  }
  
  tft.endWrite();
}

// TJpg callback function implementation
bool highSpeedTftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  DisplayManager& dm = DisplayManager::getInstance();
  uint16_t* displayBuffer = dm.getDisplayBuffer();
  
  if (!bitmap || y >= Config::DISPLAY_HEIGHT || x >= Config::DISPLAY_WIDTH) return 0;
  
  // Fast bounds checking
  if (x + w > Config::DISPLAY_WIDTH) w = Config::DISPLAY_WIDTH - x;
  if (y + h > Config::DISPLAY_HEIGHT) h = Config::DISPLAY_HEIGHT - y;
  
  if (w > 0 && h > 0) {
    if (displayBuffer) {
      // High-speed buffer copy with DMA-aligned access
      for (uint16_t row = 0; row < h; row++) {
        uint32_t dstOffset = (y + row) * Config::DISPLAY_WIDTH + x;
        uint32_t srcOffset = row * w;
        
        if (dstOffset + w <= Config::DISPLAY_WIDTH * Config::DISPLAY_HEIGHT) {
          // Fast memcpy for performance
          memcpy(&displayBuffer[dstOffset], &bitmap[srcOffset], w << 1);
        }
      }
    } else {
      // Direct high-speed rendering
      dm.getTft().pushImage(x, y, w, h, bitmap);
    }
  }
  
  // Reduced yield frequency for higher speed
  static uint16_t callCount = 0;
  if (++callCount % 32 == 0) {
    taskYIELD();
    callCount = 0;
  }
  
  return 1;
}
