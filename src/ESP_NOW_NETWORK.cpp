#include "esp_log.h"
#include "ESP_NOW_NETWORK.h"
#include <vector>

static ESP_NOW_Peer_Class *ep_broadcast_peer;
static std::vector<ESP_NOW_Peer_Class *> ep_peers;
static void (*new_recv)(const uint8_t *addr, const uint8_t *data, int len) = NULL;
static void *new_arg = NULL;

static void new_peer_added(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) {
  esp_now_data_t *msg = (esp_now_data_t *)data;
  if ((strcmp(msg->str, HELLO_MSG) != 0) && (strcmp(msg->str, OK_MSG) != 0))
    return;

  ESP_NOW_Network_Node *node = (ESP_NOW_Network_Node *)arg;
  log_i("New peer added: " MACSTR ", my role: %s, message from: %s", MAC2STR(info->src_addr), node->role ? "server" : "client", msg->ismaster ? "master" : "client");

  ESP_NOW_Peer_Class *new_peer = new ESP_NOW_Peer_Class(info->src_addr, static_cast<ep_role_type>(msg->ismaster), node->role, node->channel, node->ESPNOW_WIFI_IFACE, (const uint8_t *)node->ESPNOW_EP_LMK);
  if (new_peer == nullptr || !new_peer->add_peer()) {
    log_e("Failed to create or register the new peer");
    delete new_peer;
    return;
  }
  ep_peers.push_back(new_peer);
  node->ready = true;
  log_i("New peer added: " MACSTR, MAC2STR(info->src_addr));
  msg->ismaster = node->role == EPSERVER;
  strcpy(msg->str, OK_MSG);
  log_i("Peers: %d new msg ismaster? %d", ep_peers.size(), msg->ismaster);
  while (!ep_broadcast_peer->send_message((const uint8_t *)msg, sizeof(esp_now_data_t)));
}

ESP_NOW_Peer_Class::ESP_NOW_Peer_Class(const uint8_t *mac_addr,
  ep_role_type prole,
  ep_role_type nrole,
  uint8_t channel,
  wifi_interface_t iface,
  const uint8_t *lmk) : ESP_NOW_Peer(mac_addr, channel, iface, lmk), nrole(nrole), prole(prole) {}

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
  log_i("Received a message from " MACSTR " (%s) (%s) length %d", MAC2STR(addr()), broadcast ? "broadcast" : "unicast", prole ? "server" : "client", len);
  if (broadcast) {
    if (strcmp(msg->str, HELLO_MSG) == 0) {
      msg->ismaster = nrole == EPSERVER;
      strcpy(msg->str, OK_MSG);
      while (!ep_broadcast_peer->send_message((const uint8_t *)msg, sizeof(esp_now_data_t)));
      log_i("Broadcast message to " MACSTR " %s as %s", MAC2STR(addr()), msg->str, msg->ismaster ? "ismaster" : "-");
    }
  } else {
    new_recv(addr(), data, len);
  }
}

void ESP_NOW_Peer_Class::onSent(bool success) {
  bool broadcast = memcmp(addr(), ESP_NOW.BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0;
  std::string R = "broadcast";
  if (prole == EPSERVER)
    R = "server";
  if (prole == EPCLIENT)
    R = "client";
  if (broadcast) {
    log_i("Broadcast %s message reported as sent %s", R.c_str(), success ? "successfully" : "unsuccessfully");
  } else {
    log_i("Unicast %s message reported as sent %s to peer " MACSTR, success ? "successfully" : "unsuccessfully", R.c_str(), MAC2STR(addr()));
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
  onNewPeer(new_peer_added, this);
  memset(&new_msg, 0, sizeof(new_msg));
  strcpy(new_msg.str, HELLO_MSG);
  new_msg.ismaster = role == EPSERVER;
  log_i("Setup complete. Broadcasting... %s ismaster: %s", new_msg.str, new_msg.ismaster ? "yes" : "no");
  while (!ep_broadcast_peer->send_message((const uint8_t *)&new_msg, sizeof(esp_now_data_t)));
}

ESP_NOW_Network_Node::~ESP_NOW_Network_Node() {}

void ESP_NOW_Network_Node::checkstate() {
  new_msg.ismaster = role == EPSERVER;
  strcpy(new_msg.str, HELLO_MSG);
  if (!ep_broadcast_peer->send_message((const uint8_t *)&new_msg, sizeof(esp_now_data_t)))
    log_e("Failed to broadcast message");
  else 
    log_i("Sent broadcast %s", new_msg.str);
}

void ESP_NOW_Network_Node::senddata(const char* data) {
  if (role == EPCLIENT) {
    for (auto& peer : ep_peers) {
      new_msg.ismaster = false;
      strcpy(new_msg.str, data);
      if (!peer->send_message((const uint8_t *)&new_msg, sizeof(esp_now_data_t)))
        log_e("Failed to send message");
    }
  }
}

void ESP_NOW_Network_Node::onNewRecv(void (*rc)(const uint8_t *addr, const uint8_t *data, int len), void *arg) {
  new_recv = rc;
  new_arg = arg;
}
