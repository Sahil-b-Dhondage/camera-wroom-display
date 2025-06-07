// network_manager.h
#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "config.h"

class NetworkManager {
private:
  WiFiUDP udp;
  int connectedClients;
  
  NetworkManager() : connectedClients(0) {}
  
  static void wifiEventHandler(WiFiEvent_t event);
  
public:
  static NetworkManager& getInstance() {
    static NetworkManager instance;
    return instance;
  }
  
  bool initialize();
  WiFiUDP& getUDP() { return udp; }
  int getConnectedClients() const { return connectedClients; }
  void incrementClients() { connectedClients++; }
  void decrementClients() { 
    connectedClients--; 
    if (connectedClients < 0) connectedClients = 0;
  }
  
  // Packet processing
  bool hasPacket();
  int readPacket(uint8_t* buffer, int maxSize);
};

#endif // NETWORK_MANAGER_H

// network_manager.cpp
#include "network_manager.h"

bool NetworkManager::initialize() {
  Serial.println("Setting up WiFi Access Point...");
  
  // Setup WiFi event handler
  WiFi.onEvent(wifiEventHandler);
  WiFi.mode(WIFI_AP);
  
  if (!WiFi.softAPConfig(Config::LOCAL_IP, Config::GATEWAY, Config::SUBNET)) {
    Serial.println("FATAL: Failed to configure AP");
    return false;
  }
  
  if (!WiFi.softAP(Config::AP_SSID, Config::AP_PASSWORD, 1, 0, 4)) {
    Serial.println("FATAL: Failed to start AP");
    return false;
  }
  
  Serial.printf("WiFi AP: %s (%s)\n", Config::AP_SSID, WiFi.softAPIP().toString().c_str());
  
  // Start UDP server
  if (!udp.begin(Config::UDP_PORT)) {
    Serial.println("FATAL: Failed to start UDP server");
    return false;
  }
  
  Serial.printf("UDP server on port %d\n", Config::UDP_PORT);
  return true;
}

void NetworkManager::wifiEventHandler(WiFiEvent_t event) {
  NetworkManager& nm = NetworkManager::getInstance();
  
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      nm.incrementClients();
      Serial.printf("Client connected. Total: %d\n", nm.getConnectedClients());
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      nm.decrementClients();
      Serial.printf("Client disconnected. Total: %d\n", nm.getConnectedClients());
      break;
    default:
      break;
  }
}

bool NetworkManager::hasPacket() {
  return udp.parsePacket() > 0;
}

int NetworkManager::readPacket(uint8_t* buffer, int maxSize) {
  int packetSize = udp.parsePacket();
  if (packetSize > 0 && packetSize <= maxSize) {
    return udp.read(buffer, packetSize);
  }
  return 0;
}
