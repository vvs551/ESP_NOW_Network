#include "esp_log.h"
#include "ESP_NOW_NETWORK.h"
#include <vector>
#include <iostream>
#include  <iomanip>
#ifdef USEEEPROM
#include "EEPROM.h"
#endif

static ESP_NOW_Peer_Class *ep_broadcast_peer;
static std::vector<ESP_NOW_Peer_Class *> ep_peers;
static void (*new_recv)(const uint8_t *addr, const uint8_t position, const uint8_t *data, int len) = NULL;
static void *new_arg = NULL;
#ifdef USEEEPROM
int find_peer_position(const std::vector<ESP_NOW_Peer_Class*>& peers, const uint8_t *newmac) {
    auto it = std::find_if(peers.begin(), peers.end(), [newmac](const ESP_NOW_Peer_Class* peer) {
        return std::equal(peer->mac, peer->mac + 6, newmac);
    });

    if (it != peers.end()) {
        return std::distance(peers.begin(), it);
    } else {
        return -1; // Not found
    }
}
static void store_peer(const uint8_t *macaddr){
  if (!EEPROM.begin(100)) {
    log_e("Failed to initialize EEPROM");
    return;
  }  
  int address = 0;
  uint8_t npeers = EEPROM.readByte(address);
  npeers %= 0xFF;                           // clear EEPROM
  EEPROM.writeByte(address, npeers+1);
  address = 2 + npeers * 6;
  for (int i = 0; i < 6; i++)
    EEPROM.writeByte(address + i, macaddr[i]);
  EEPROM.commit();
  log_d("EEPROM store Server MAC " MACSTR "", MAC2STR(macaddr));
}
static bool restore_peers(
  ep_role_type nrole,
  uint8_t channel,
  wifi_interface_t iface,
  const uint8_t *lmk){
  if (!EEPROM.begin(100)) {
    log_e("Failed to initialize EEPROM");
    return false;
  }    
  bool res = false;
  ep_role_type prole = static_cast<ep_role_type>(!(bool)nrole);
  int address = 0;
  uint8_t n_peers = EEPROM.readByte(address);
  if (n_peers == 0xFF)                        // EEPROM after the cleaning has FF          
    return false;
  address = 2;
  for (int i = 0; i < n_peers; i++){
    uint8_t *macaddr = new uint8_t[6];
    for (int i = 0; i < 6; i++)
      macaddr[i] = EEPROM.readByte(address + i);
    log_d("EEPROM restore MAC " MACSTR " at %d position", MAC2STR(macaddr), i);
    ESP_NOW_Peer_Class *new_peer = new ESP_NOW_Peer_Class(macaddr, prole, nrole, channel, iface, lmk);
    if (new_peer == nullptr || !new_peer->add_peer()) {
      log_e("Failed to create or register the new peer");
      delete new_peer;
      continue;
    }
    ep_peers.push_back(new_peer);
    res = true;
  }
  return res;  
}
static void delete_peers(){
  if (!EEPROM.begin(100)) {
    log_e("Failed to initialize EEPROM");
    return;
  }    
  EEPROM.writeByte(0,0);
  EEPROM.commit();
}
#endif
static void new_peer_added(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) {
  esp_now_data_t *msg = (esp_now_data_t *)data;
  ESP_NOW_Network_Node *node = (ESP_NOW_Network_Node *)arg;
  if ((strcmp(msg->str, HELLO_MSG) != 0) && (strcmp(msg->str, OK_MSG) != 0) && (memcmp(info->des_addr, WiFi.macAddress().c_str(), 6) != 0) ||
    (node->role == msg->ismaster))
    return;

  log_d("New peer added: " MACSTR ", my role: %s, message from: %s", MAC2STR(info->src_addr), node->role ? "server" : "client", msg->ismaster ? "master" : "client");

  int pos = find_peer_position(ep_peers, info->src_addr);
  if (pos > -1)                                                   // this mac addr already registred
    return;

  ESP_NOW_Peer_Class *new_peer = new ESP_NOW_Peer_Class(info->src_addr, static_cast<ep_role_type>(msg->ismaster), node->role, node->channel, node->ESPNOW_WIFI_IFACE, (const uint8_t *)node->ESPNOW_EP_LMK);
  if (new_peer == nullptr || !new_peer->add_peer()) {
    log_e("Failed to create or register the new peer");
    delete new_peer;
    return;
  }
  ep_peers.push_back(new_peer);
#ifdef USEEEPROM
    store_peer(info->src_addr);
#endif
  msg->ismaster = node->role == EPSERVER;
  strcpy(msg->str, OK_MSG);
  log_d("Total registred peers: %d", ep_peers.size());
  log_d("Send broadcast as %s", msg->ismaster ? "server" : "client" );
  while (!ep_broadcast_peer->send_message((const uint8_t *)msg, sizeof(esp_now_data_t)));
}

ESP_NOW_Peer_Class::ESP_NOW_Peer_Class(const uint8_t *mac_addr,
  ep_role_type prole,
  ep_role_type nrole,
  uint8_t channel,
  wifi_interface_t iface,
  const uint8_t *lmk) : ESP_NOW_Peer(mac_addr, channel, iface, lmk), nrole(nrole), prole(prole), mac(mac_addr) {}

ESP_NOW_Peer_Class::~ESP_NOW_Peer_Class() {}

bool ESP_NOW_Peer_Class::add_peer() {
  if (!add()) {
    log_e("Failed to register the broadcast peer");
    return false;
  }
  return true;
}

bool ESP_NOW_Peer_Class::send_message(const uint8_t *data, size_t len) {
  if (data == NULL || len == 0) {
    log_e("Data to be sent is NULL or has a length of 0");
    return false;
  }
  return send(data, len);
}

void ESP_NOW_Peer_Class::onReceive(const uint8_t *data, size_t len, bool broadcast) {
  msg = (esp_now_data_t *)data;
  log_d("Received a message from " MACSTR " (%s) (%s) length %d", MAC2STR(addr()), broadcast ? "broadcast" : "unicast", prole ? "server" : "client", len);
  if (broadcast) {
    if (strcmp(msg->str, HELLO_MSG) == 0) {
      msg->ismaster = nrole == EPSERVER;
      strcpy(msg->str, OK_MSG);
      while (!ep_broadcast_peer->send_message((const uint8_t *)msg, sizeof(esp_now_data_t)));
      log_d("Broadcast message to " MACSTR " %s as %s", MAC2STR(addr()), msg->str, msg->ismaster ? "server" : "client");
    }
  } else {
    const uint8_t *macaddr = addr();
    int pos = find_peer_position(ep_peers, macaddr);
    if (pos > -1)
       new_recv(macaddr, pos, data, len);     
  }
}

void ESP_NOW_Peer_Class::onSent(bool success) {
  bool broadcast = memcmp(addr(), ESP_NOW.BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0;
  if (broadcast) {
    log_d("Broadcast from %s message reported as sent %s", (nrole == EPSERVER) ? "server" : "client", success ? "successfully" : "unsuccessfully");
  } else {
    log_d("Unicast message reported as %s sent to %s  " MACSTR, success ? "successfully" : "unsuccessfully", (prole == EPSERVER) ? "server" : "client", MAC2STR(addr()));
  }
}

ESP_NOW_Network_Node::ESP_NOW_Network_Node(const ep_role_type role, const uint8_t channel): ESP_NOW_Class(), role(role), channel(channel) {
  WiFi.mode(WIFI_STA);
#ifdef  LOWPOWER
  WiFi.setTxPower(WIFI_POWER_7dBm);
#endif
  WiFi.setChannel(channel);
  while (!WiFi.STA.started()) {
    delay(100);
  }

  log_i("Mode: STA");
  log_i("MAC Address: %s", WiFi.macAddress().c_str());
  log_i("Channel: %d", channel);
  log_i("Role: %s", role ? "Server" : "Client");

  if (!begin((const uint8_t *)ESPNOW_EP_PMK)) {
    log_e("Failed to initialize ESP-NOW");
  }
  ep_broadcast_peer = new ESP_NOW_Peer_Class(ESP_NOW.BROADCAST_ADDR, role, role, channel, ESPNOW_WIFI_IFACE, NULL);
  if (!ep_broadcast_peer->add_peer()) {
    log_e("Failed to initialize ESP-NOW or register the peer");
  }
#ifdef USEEEPROM
    ready = restore_peers(role, channel, ESPNOW_WIFI_IFACE, (const uint8_t *)ESPNOW_EP_LMK);
#endif
  onNewPeer(new_peer_added, this);
  memset(&new_msg, 0, sizeof(new_msg));
  strcpy(new_msg.str, HELLO_MSG);
  new_msg.ismaster = role == EPSERVER;
  log_d("Setup complete. Broadcasting... %s ismaster: %s", new_msg.str, new_msg.ismaster ? "yes" : "no");
  while (!ep_broadcast_peer->send_message((const uint8_t *)&new_msg, sizeof(esp_now_data_t)));
}

ESP_NOW_Network_Node::~ESP_NOW_Network_Node() {}

void ESP_NOW_Network_Node::checkstate() {
  new_msg.ismaster = role == EPSERVER;
  strcpy(new_msg.str, HELLO_MSG);
  if (!ep_broadcast_peer->send_message((const uint8_t *)&new_msg, sizeof(esp_now_data_t)))
    log_e("Failed to broadcast message");
  else 
    log_d("Sent broadcast %s", new_msg.str);
}

void ESP_NOW_Network_Node::senddata(const char* data, int size) {
  for (auto& peer : ep_peers) {
    new_msg.ismaster = (bool)role;
    memcpy(new_msg.str, data, size);
    new_msg.str[size] = '\0';         // ??
    if (!peer->send_message((const uint8_t *)&new_msg, sizeof(esp_now_data_t)))
      log_e("Failed to send message");
  }
}
void ESP_NOW_Network_Node::clearAllPeers(){
  ep_peers.clear();
#ifdef USEEEPROM
  delete_peers();
#endif
}
void ESP_NOW_Network_Node::onNewRecv(void (*rc)(const uint8_t *addr, const uint8_t position, const uint8_t *data, int len), void *arg) {
  new_recv = rc;
  new_arg = arg;
}
