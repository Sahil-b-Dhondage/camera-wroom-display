// performance_monitor.h
#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include "config.h"

class PerformanceMonitor {
private:
  uint32_t totalFramesStarted;
  uint32_t completeFramesReceived;
  uint32_t completeFramesRendered;
  uint32_t incompleteFramesDiscarded;
  uint32_t corruptFramesDiscarded;
  uint32_t memoryErrors;
  
  PerformanceMonitor() : totalFramesStarted(0), completeFramesReceived(0),
                        completeFramesRendered(0), incompleteFramesDiscarded(0),
                        corruptFramesDiscarded(0), memoryErrors(0) {}
  
public:
  static PerformanceMonitor& getInstance() {
    static PerformanceMonitor instance;
    return instance;
  }
  
  // Increment counters
  void incrementFramesStarted() { totalFramesStarted++; }
  void incrementCompleteFrames() { completeFramesReceived++; }
  void incrementRenderedFrames() { completeFramesRendered++; }
  void incrementIncompleteFrames() { incompleteFramesDiscarded++; }
  void incrementCorruptFrames() { corruptFramesDiscarded++; }
  void incrementMemoryErrors() { memoryErrors++; }
  
  // Getters
  uint32_t getFramesStarted() const { return totalFramesStarted; }
  uint32_t getCompleteFrames() const { return completeFramesReceived; }
  uint32_t getRenderedFrames() const { return completeFramesRendered; }
  uint32_t getIncompleteFrames() const { return incompleteFramesDiscarded; }
  uint32_t getCorruptFrames() const { return corruptFramesDiscarded; }
  uint32_t getMemoryErrors() const { return memoryErrors; }
  
  // Statistics
  float getCompletionRate() const;
  float getRenderRate() const;
  void printStatistics() const;
  void checkMemory();
};

#endif // PERFORMANCE_MONITOR_H

// performance_monitor.cpp
#include "performance_monitor.h"
#include "frame_processor.h"
#include "network_manager.h"

float PerformanceMonitor::getCompletionRate() const {
  return totalFramesStarted > 0 ? 
         (float)completeFramesReceived / totalFramesStarted * 100.0f : 0.0f;
}

float PerformanceMonitor::getRenderRate() const {
  return completeFramesReceived > 0 ? 
         (float)completeFramesRendered / completeFramesReceived * 100.0f : 0.0f;
}

void PerformanceMonitor::printStatistics() const {
  CompleteFrameState& currentFrame = FrameProcessor::getInstance().getCurrentFrame();
  uint32_t heapFree = ESP.getFreeHeap();
  
  Serial.printf("=== COMPLETE FRAME DISPLAY ===\n");
  Serial.printf("Started: %d, Complete: %d (%.1f%%)\n", 
               totalFramesStarted, completeFramesReceived, getCompletionRate());
  Serial.printf("Rendered: %d (%.1f%% of complete)\n", 
               completeFramesRendered, getRenderRate());
  Serial.printf("Discarded: Incomplete=%d, Corrupt=%d\n", 
               incompleteFramesDiscarded, corruptFramesDiscarded);
  Serial.printf("Current: ID=%d, Packets=%d/%d, Size=%d\n", 
               currentFrame.frameId, currentFrame.receivedPackets, 
               currentFrame.totalPackets, currentFrame.totalSize);
  Serial.printf("Memory: Free=%d KB, Errors=%d\n", heapFree/1024, memoryErrors);
  Serial.printf("Clients: %d\n", NetworkManager::getInstance().getConnectedClients());
  Serial.println("=============================");
}

void PerformanceMonitor::checkMemory() {
  if (ESP.getFreeHeap() < Config::MIN_HEAP_SIZE) {
    memoryErrors++;
  }
}
