#pragma once
#include "ESP32_NOW.h"
#include "WiFi.h"
#include <esp_mac.h>  // For the MAC2STR and MACSTR macros

//#define LOWPOWER    // for XIAO C3 it helps to reduce the transmit power because too much
#define HELLO_MSG "EPII"
#define OK_MSG    "OK"  

typedef enum {
  EPCLIENT,
  EPSERVER,
  EPBROADCAST
} ep_role_type;

typedef struct {
  bool ismaster;
  char str[32];
} __attribute__((packed)) esp_now_data_t;

class ESP_NOW_Peer_Class : public ESP_NOW_Peer {
public:
  bool ready = false;
  esp_now_data_t *msg;
  ESP_NOW_Peer_Class(const uint8_t *mac_addr,
                      ep_role_type prole,
                      ep_role_type nrole,
                      uint8_t channel,
                      wifi_interface_t iface,
                      const uint8_t *lmk);
  ~ESP_NOW_Peer_Class(void);
  bool add_peer(void);
  bool send_message(const uint8_t *data, size_t len);
  void onReceive(const uint8_t *data, size_t len, bool broadcast);
  void onSent(bool success);
private:
  const ep_role_type prole;
  const ep_role_type nrole;
};

class ESP_NOW_Network_Node : public ESP_NOW_Class {
private:
  inline static const ep_role_type DEF_ROLE = EPCLIENT;
  inline static const uint8_t DEF_CHANNEL = 4;
  esp_now_data_t new_msg;
  ESP_NOW_Peer_Class *broadcast_peer;

public:
  inline static const char* ESPNOW_EP_PMK = "pmk1234567890321";
  inline static const char* ESPNOW_EP_LMK = "lmk1234567890321";
  inline static const wifi_interface_t ESPNOW_WIFI_IFACE = WIFI_IF_STA;
  const uint8_t channel;
  const ep_role_type role;
  bool ready = false;

  ESP_NOW_Network_Node(const ep_role_type role = DEF_ROLE, const uint8_t channel = DEF_CHANNEL);
  ~ESP_NOW_Network_Node(void);
  void checkstate(void);
  void onNewRecv(void (*rc)(const uint8_t *addr, const uint8_t *data, int len), void *arg);
  void senddata(const char* data);
};
