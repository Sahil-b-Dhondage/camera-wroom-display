// main.cpp
#include "config.h"
#include "display_manager.h"
#include "network_manager.h"
#include "frame_processor.h"
#include "task_manager.h"
#include "performance_monitor.h"

void setup() {
  Serial.begin(115200);
  Serial.println("==========================================");
  Serial.println("COMPLETE FRAME DISPLAY SYSTEM");
  Serial.println("Memory-Optimized Anti-Partial Rendering");
  Serial.println("==========================================");
  
  // Initialize display manager
  if (!DisplayManager::getInstance().initialize()) {
    Serial.println("FATAL: Display initialization failed!");
    while(1) delay(1000);
  }
  
  // Initialize frame processor
  if (!FrameProcessor::getInstance().initialize()) {
    Serial.println("FATAL: Frame processor initialization failed!");
    while(1) delay(1000);
  }
  
  // Initialize network manager
  if (!NetworkManager::getInstance().initialize()) {
    Serial.println("FATAL: Network initialization failed!");
    while(1) delay(1000);
  }
  
  // Initialize and start task manager
  if (!TaskManager::getInstance().initialize()) {
    Serial.println("FATAL: Task manager initialization failed!");
    while(1) delay(1000);
  }
  
  // Show ready message
  DisplayManager::getInstance().showStartupMessage("HIGH-SPEED COMPLETE FRAME SYSTEM READY");
  delay(3000);
  DisplayManager::getInstance().clearScreen();
  
  Serial.println("==========================================");
  Serial.println("HIGH-SPEED COMPLETE FRAME SYSTEM READY");
  Serial.println("Features:");
  Serial.println("✓ 60+ FPS target with adaptive rate control");
  Serial.println("✓ High-speed packet processing");
  Serial.println("✓ Optimized rendering pipeline");
  Serial.println("✓ Complete frame validation");
  Serial.println("✓ NO screen clearing (smooth video)");
  Serial.println("✓ Multi-packet processing per cycle");
  Serial.println("✓ Adaptive strip rendering");
  Serial.printf("Target FPS: %d (adaptive up to 125 FPS)\n", Config::TARGET_FPS);
  Serial.printf("Min render interval: %d ms\n", Config::MIN_RENDER_INTERVAL);
  Serial.printf("Fast render interval: %d ms\n", Config::FAST_RENDER_INTERVAL);
  Serial.printf("Max frame size: %d KB\n", Config::MAX_FRAME_SIZE/1024);
  Serial.printf("Free memory: %d KB\n", ESP.getFreeHeap()/1024);
  Serial.println("HIGH-SPEED SMOOTH VIDEO READY!");
  Serial.println("==========================================");
  
  vTaskDelete(NULL);
}

void loop() {
  vTaskDelete(NULL);
}
