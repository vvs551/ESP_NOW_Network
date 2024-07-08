#include "ESP_NOW_NETWORK.h"

#include <iostream>
#include <time.h>

#define SERVER

typedef struct {
  uint16_t data0;
  uint16_t data1;
  uint16_t data2;
  uint16_t data3;
} dataType;

dataType toSend;
ESP_NOW_Network_Node *node;

void recv(const uint8_t *addr, const uint8_t *data, int len) {
  esp_now_data_t *msg = (esp_now_data_t *)data;
  if (len > 0) {
    dataType *recv = (dataType *)msg->str;
    log_i("Message: %d %d %d %d", recv->data0, recv->data1, recv->data2, recv->data3);
  }  
}

void setup() {
  Serial.begin(115200);
#ifdef SERVER
  node = new ESP_NOW_Network_Node(EPSERVER);
#else
  node = new ESP_NOW_Network_Node(EPCLIENT);
#endif
  node->onNewRecv(recv, NULL);
  srand((unsigned)time(NULL));
}

std::string dataToString(const int& value_0, const int& value_1, const int& value_2, const int& value_3) {
  std::ostringstream oss;
  oss << "{'DATA':[" << value_0 << "," << value_1 << "," << value_2 << "," << value_3 << "]}";
  return oss.str();
}

void sendData() {
  toSend.data0 = rand() / 1000000;
  toSend.data1 = rand() / 1000000;  
  toSend.data2 = rand() / 1000000;  
  toSend.data3 = rand() / 1000000;  
  const char *d = (const char *)(&toSend);
  node->senddata(d);
}

void loop() {
  if (!node->ready) {
    node->checkstate();
    delay(1000);
  } else {
#if !defined(SERVER)
    sendData();
    delay(10000);
#endif
  }
}