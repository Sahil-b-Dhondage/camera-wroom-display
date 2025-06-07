// task_manager.h
#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "config.h"

class TaskManager {
private:
  TaskHandle_t udpTaskHandle;
  TaskHandle_t displayTaskHandle;
  TaskHandle_t monitorTaskHandle;
  
  TaskManager() : udpTaskHandle(nullptr), displayTaskHandle(nullptr), 
                 monitorTaskHandle(nullptr) {}
  
  // Static task functions
  static void highSpeedUdpTask(void *pvParameters);
  static void highSpeedDisplayTask(void *pvParameters);
  static void monitorTask(void *pvParameters);
  
public:
  static TaskManager& getInstance() {
    static TaskManager instance;
    return instance;
  }
  
  bool initialize();
  void cleanup();
  
  ~TaskManager() { cleanup(); }
};

#endif // TASK_MANAGER_H

// task_manager.cpp
#include "task_manager.h"
#include "network_manager.h"
#include "frame_processor.h"
#include "display_manager.h"
#include "performance_monitor.h"

bool TaskManager::initialize() {
  Serial.println("Creating high-speed tasks...");
  
  BaseType_t result1 = xTaskCreatePinnedToCore(
    highSpeedUdpTask, "High-Speed UDP", 3072, NULL, 7, &udpTaskHandle, 0);
  
  BaseType_t result2 = xTaskCreatePinnedToCore(
    highSpeedDisplayTask, "High-Speed Display", 4096, NULL, 6, &displayTaskHandle, 1);
  
  BaseType_t result3 = xTaskCreatePinnedToCore(
    monitorTask, "Monitor", 2048, NULL, 1, &monitorTaskHandle, 0);
  
  if (result1 != pdPASS || result2 != pdPASS || result3 != pdPASS) {
    Serial.println("FATAL: Failed to create tasks");
    return false;
  }
  
  Serial.println("Tasks created successfully");
  return true;
}

void TaskManager::cleanup() {
  if (udpTaskHandle) { vTaskDelete(udpTaskHandle); udpTaskHandle = nullptr; }
  if (displayTaskHandle) { vTaskDelete(displayTaskHandle); displayTaskHandle = nullptr; }
  if (monitorTaskHandle) { vTaskDelete(monitorTaskHandle); monitorTaskHandle = nullptr; }
}

void TaskManager::highSpeedUdpTask(void *pvParameters) {
  const TickType_t xDelay = pdMS_TO_TICKS(1);
  uint8_t packetBuffer[1500];
  
  NetworkManager& nm = NetworkManager::getInstance();
  FrameProcessor& fp = FrameProcessor::getInstance();
  
  while(1) {
    // Process multiple packets per cycle for higher throughput
    for (int i = 0; i < 3; i++) {
      int bytesRead = nm.readPacket(packetBuffer, sizeof(packetBuffer));
      if (bytesRead > 0) {
        fp.processPacket(packetBuffer, bytesRead);
      } else {
        break; // No more packets, exit loop
      }
    }
    
    fp.handleFrameTimeout();
    vTaskDelay(xDelay);
  }
}

void TaskManager::highSpeedDisplayTask(void *pvParameters) {
  const TickType_t xDelay = pdMS_TO_TICKS(8);
  uint32_t lastRenderTime = 0;
  uint32_t frameCount = 0;
  uint32_t adaptiveInterval = Config::MIN_RENDER_INTERVAL;
  
  FrameProcessor& fp = FrameProcessor::getInstance();
  DisplayManager& dm = DisplayManager::getInstance();
  PerformanceMonitor& pm = PerformanceMonitor::getInstance();
  
  while(1) {
    uint32_t currentTime = millis();
    
    // Adaptive frame rate control
    if (frameCount % 30 == 0 && frameCount > 0) {
      float avgFPS = 30000.0f / (currentTime - lastRenderTime + 1);
      if (avgFPS > Config::TARGET_FPS * 1.1f) {
        adaptiveInterval = min((uint32_t)(adaptiveInterval + 1), (uint32_t)Config::MIN_RENDER_INTERVAL);
      } else if (avgFPS < Config::TARGET_FPS * 0.9f) {
        adaptiveInterval = max((uint32_t)(adaptiveInterval - 1), (uint32_t)Config::FAST_RENDER_INTERVAL);
      }
    }
    
    // Check for complete frame ready to render
    if (fp.isFrameComplete() && !fp.isFrameRendering() && 
        (currentTime - lastRenderTime) >= adaptiveInterval) {
      
      if (fp.lockDisplay(15)) {
        
        // Fast frame assembly
        if (fp.assembleCompleteFrame()) {
          // High-speed frame rendering
          CompleteFrameState& currentFrame = fp.getCurrentFrame();
          if (dm.renderFrameHighSpeed(fp.getFrameBuffer(), currentFrame.totalSize)) {
            lastRenderTime = currentTime;
            frameCount++;
            pm.incrementRenderedFrames();
          }
        }
        
        // Quick frame state reset
        fp.resetCurrentFrame();
        
        fp.unlockDisplay();
      }
    }
    
    // Quick memory check
    pm.checkMemory();
    
    vTaskDelay(xDelay);
  }
}

void TaskManager::monitorTask(void *pvParameters) {
  const TickType_t xDelay = pdMS_TO_TICKS(3000);
  PerformanceMonitor& pm = PerformanceMonitor::getInstance();
  
  while(1) {
    pm.printStatistics();
    vTaskDelay(xDelay);
  }
}
