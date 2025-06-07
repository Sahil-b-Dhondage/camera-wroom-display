// frame_processor.h
#ifndef FRAME_PROCESSOR_H
#define FRAME_PROCESSOR_H

#include "config.h"

class FrameProcessor {
private:
  uint8_t* frameBuffer;
  uint8_t* assemblyBuffer;
  bool* packetReceived;
  CompleteFrameState currentFrame;
  
  SemaphoreHandle_t frameMutex;
  SemaphoreHandle_t displayMutex;
  
  FrameProcessor() : frameBuffer(nullptr), assemblyBuffer(nullptr), 
                    packetReceived(nullptr), frameMutex(nullptr), displayMutex(nullptr) {
    currentFrame.reset();
  }
  
public:
  static FrameProcessor& getInstance() {
    static FrameProcessor instance;
    return instance;
  }
  
  bool initialize();
  void cleanup();
  bool processPacket(uint8_t* packetData, int size);
  bool isFrameComplete() const { return currentFrame.isComplete; }
  bool isFrameValid() const { return currentFrame.isValid; }
  bool isFrameRendering() const { return currentFrame.isRendering; }
  uint8_t* getFrameBuffer() { return frameBuffer; }
  CompleteFrameState& getCurrentFrame() { return currentFrame; }
  
  // Frame processing methods
  bool assembleCompleteFrame();
  bool validateCompleteJPEG(uint8_t* buffer, uint32_t size);
  void handleFrameTimeout();
  void resetCurrentFrame();
  
  // Mutex management
  bool lockFrame(uint32_t timeoutMs = 10);
  void unlockFrame();
  bool lockDisplay(uint32_t timeoutMs = 15);
  void unlockDisplay();
  
  ~FrameProcessor() { cleanup(); }
};

#endif // FRAME_PROCESSOR_H

// frame_processor.cpp
#include "frame_processor.h"
#include "performance_monitor.h"

bool FrameProcessor::initialize() {
  Serial.println("Initializing frame processor...");
  
  // Calculate memory requirements
  uint32_t frameBufferSize = Config::MAX_FRAME_SIZE;
  uint32_t assemblyBufferSize = Config::MAX_FRAME_SIZE;
  uint32_t packetTrackingSize = Config::MAX_PACKETS;
  uint32_t totalNeeded = frameBufferSize + assemblyBufferSize + packetTrackingSize;
  
  uint32_t availableHeap = ESP.getFreeHeap();
  Serial.printf("Memory check: Need %d KB, Available %d KB\n", 
               totalNeeded/1024, availableHeap/1024);
  
  if (availableHeap < totalNeeded + 20000) {
    Serial.printf("Insufficient memory for frame processor!\n");
    return false;
  }
  
  // Allocate buffers
  frameBuffer = (uint8_t*)heap_caps_malloc(frameBufferSize, MALLOC_CAP_8BIT);
  assemblyBuffer = (uint8_t*)heap_caps_malloc(assemblyBufferSize, MALLOC_CAP_8BIT);
  packetReceived = (bool*)heap_caps_malloc(packetTrackingSize, MALLOC_CAP_8BIT);
  
  if (!frameBuffer || !assemblyBuffer || !packetReceived) {
    Serial.println("Failed to allocate frame processor buffers");
    cleanup();
    return false;
  }
  
  // Initialize buffers
  memset(frameBuffer, 0, frameBufferSize);
  memset(assemblyBuffer, 0, assemblyBufferSize);
  memset(packetReceived, false, packetTrackingSize);
  
  // Create synchronization objects
  frameMutex = xSemaphoreCreateMutex();
  displayMutex = xSemaphoreCreateMutex();
  
  if (frameMutex == NULL || displayMutex == NULL) {
    Serial.println("Failed to create frame processor mutexes!");
    cleanup();
    return false;
  }
  
  Serial.printf("Frame processor initialized: %d KB allocated\n", totalNeeded/1024);
  return true;
}

void FrameProcessor::cleanup() {
  if (frameBuffer) { heap_caps_free(frameBuffer); frameBuffer = nullptr; }
  if (assemblyBuffer) { heap_caps_free(assemblyBuffer); assemblyBuffer = nullptr; }
  if (packetReceived) { heap_caps_free(packetReceived); packetReceived = nullptr; }
  if (frameMutex) { vSemaphoreDelete(frameMutex); frameMutex = nullptr; }
  if (displayMutex) { vSemaphoreDelete(displayMutex); displayMutex = nullptr; }
}

bool FrameProcessor::processPacket(uint8_t* packetData, int size) {
  if (!packetData || size < 12) return false;
  
  // Fast packet header parsing
  uint32_t frame_id = *(uint32_t*)&packetData[0];
  uint16_t total_packets = *(uint16_t*)&packetData[4];
  uint16_t packet_idx = *(uint16_t*)&packetData[6];
  uint32_t packet_size = *(uint32_t*)&packetData[8];
  uint8_t* payload = &packetData[12];
  uint32_t actualDataSize = size - 12;
  
  // Quick validation
  if (packet_size != actualDataSize || packet_idx >= total_packets || 
      total_packets == 0 || total_packets > Config::MAX_PACKETS) {
    return false;
  }
  
  if (!lockFrame(5)) return false;
  
  bool success = false;
  
  // Handle first packet of new frame
  if (packet_idx == 0) {
    // Quick JPEG header validation
    if (actualDataSize < 2 || payload[0] != 0xFF || payload[1] != 0xD8) {
      unlockFrame();
      return false;
    }
    
    // Fast frame initialization
    currentFrame.frameId = frame_id;
    currentFrame.totalPackets = total_packets;
    currentFrame.receivedPackets = 1;
    currentFrame.totalSize = packet_size;
    currentFrame.startTime = millis();
    currentFrame.isComplete = false;
    currentFrame.isValid = false;
    currentFrame.isRendering = false;
    
    PerformanceMonitor::getInstance().incrementFramesStarted();
    
    // Fast packet tracking reset
    memset(packetReceived, false, Config::MAX_PACKETS);
    packetReceived[0] = true;
    
    // Fast buffer copy
    if (packet_size <= Config::MAX_FRAME_SIZE) {
      memcpy(assemblyBuffer, payload, packet_size);
      success = true;
      
      // Single packet frame check
      if (total_packets == 1) {
        currentFrame.isComplete = true;
      }
    }
    
  } else {
    // Fast continuation packet handling
    if (frame_id == currentFrame.frameId && currentFrame.receivedPackets > 0) {
      
      // Quick duplicate check
      if (!packetReceived[packet_idx]) {
        
        // Fast size validation
        if (currentFrame.totalSize + packet_size <= Config::MAX_FRAME_SIZE) {
          
          // High-speed packet copy
          memcpy(assemblyBuffer + currentFrame.totalSize, payload, packet_size);
          currentFrame.totalSize += packet_size;
          currentFrame.receivedPackets++;
          packetReceived[packet_idx] = true;
          success = true;
          
          // Frame completion check
          if (currentFrame.receivedPackets == currentFrame.totalPackets) {
            currentFrame.isComplete = true;
          }
        }
      }
    }
  }
  
  unlockFrame();
  return success;
}

bool FrameProcessor::assembleCompleteFrame() {
  if (!assemblyBuffer || !packetReceived) return false;
  
  // Verify ALL packets received
  for (uint16_t i = 0; i < currentFrame.totalPackets; i++) {
    if (!packetReceived[i]) {
      Serial.printf("Missing packet %d in frame %d\n", i, currentFrame.frameId);
      return false;
    }
  }
  
  // Validate complete JPEG
  if (!validateCompleteJPEG(assemblyBuffer, currentFrame.totalSize)) {
    Serial.printf("Invalid JPEG in frame %d\n", currentFrame.frameId);
    PerformanceMonitor::getInstance().incrementCorruptFrames();
    return false;
  }
  
  // Copy to final frame buffer
  memcpy(frameBuffer, assemblyBuffer, currentFrame.totalSize);
  currentFrame.isValid = true;
  PerformanceMonitor::getInstance().incrementCompleteFrames();
  
  Serial.printf("Frame %d assembled: %d packets, %d bytes\n", 
               currentFrame.frameId, currentFrame.totalPackets, currentFrame.totalSize);
  
  return true;
}

bool FrameProcessor::validateCompleteJPEG(uint8_t* buffer, uint32_t size) {
  if (!buffer || size < 20) return false;
  
  // Check JPEG start marker
  if (buffer[0] != 0xFF || buffer[1] != 0xD8) return false;
  
  // Check JPEG end marker - search last 20 bytes
  bool hasEndMarker = false;
  uint32_t searchStart = (size > 20) ? size - 20 : 0;
  for (uint32_t i = searchStart; i < size - 1; i++) {
    if (buffer[i] == 0xFF && buffer[i + 1] == 0xD9) {
      hasEndMarker = true;
      break;
    }
  }
  
  return hasEndMarker;
}

void FrameProcessor::handleFrameTimeout() {
  if (currentFrame.receivedPackets > 0 && 
      (millis() - currentFrame.startTime) > Config::FRAME_TIMEOUT) {
    
    if (lockFrame(2)) {
      currentFrame.receivedPackets = 0;
      currentFrame.isComplete = false;
      PerformanceMonitor::getInstance().incrementIncompleteFrames();
      unlockFrame();
    }
  }
}

void FrameProcessor::resetCurrentFrame() {
  currentFrame.isComplete = false;
  currentFrame.isValid = false;
  currentFrame.receivedPackets = 0;
}

bool FrameProcessor::lockFrame(uint32_t timeoutMs) {
  return xSemaphoreTake(frameMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void FrameProcessor::unlockFrame() {
  xSemaphoreGive(frameMutex);
}

bool FrameProcessor::lockDisplay(uint32_t timeoutMs) {
  return xSemaphoreTake(displayMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void FrameProcessor::unlockDisplay() {
  xSemaphoreGive(displayMutex);
}
