#ifndef ESP_NOW_NETWORK_H
#define ESP_NOW_NETWORK_H
#include "ESP32_NOW.h"
#include "WiFi.h"
#include <esp_mac.h>            // For the MAC2STR and MACSTR macros

#define USEEEPROM
#define HELLO_MSG "EPII"        // Any hello message for handshaking
#define OK_MSG    "OK"          // Any Ok message for handshaking 

typedef enum {
  EPCLIENT,
  EPSERVER,
  EPBROADCAST
} ep_role_type;                 // Roles of the node

typedef struct {                // message pocket type
  bool ismaster;
  char str[32];
} __attribute__((packed)) esp_now_data_t;

//
// Peer class - used by node for store peers data
//
class ESP_NOW_Peer_Class : public ESP_NOW_Peer {
public:
  bool ready = false;
  const uint8_t * mac;
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
//  ==========
//  Node class 
//  ==========
class ESP_NOW_Network_Node : public ESP_NOW_Class {
private:
  inline static const ep_role_type DEF_ROLE = EPCLIENT;
  inline static const uint8_t DEF_CHANNEL = 4;
  esp_now_data_t new_msg;
  ESP_NOW_Peer_Class *broadcast_peer;
  const bool LOWTXPOWER;													// it helps to reduce the transmit power because the power may be is too much
public:
  inline static const char* ESPNOW_EP_PMK = "pmk1234567890321";
  inline static const char* ESPNOW_EP_LMK = "lmk1234567890321";
  inline static const wifi_interface_t ESPNOW_WIFI_IFACE = WIFI_IF_STA;
  const uint8_t channel;
  const ep_role_type role;
  bool ready = false;
  ESP_NOW_Network_Node(const ep_role_type role = DEF_ROLE, const uint8_t channel = DEF_CHANNEL, bool lowpowermode = false); // lowpower mode helps to reduce the transmit power
  ~ESP_NOW_Network_Node(void);
  void checkstate(void);
  void onNewRecv(void (*rc)(const uint8_t *addr, const uint8_t position, const uint8_t *data, int len), void *arg);
  void senddata(const char* data, int size);
  void clearAllPeers(void);
};
#endif //ESP_NOW_NETWORK_H
