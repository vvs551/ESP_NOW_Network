#include "ESP_NOW_NETWORK.h"

#include <iostream>
#include <time.h>

//#define SERVER        // node is a SERVER. If the CLIENT - comment this line

// ANY USER TYPE of the data from clients and from server
typedef struct {
  bool          data0;  // 1 byte
  bool          data1;  // 1 byte
  uint16_t      data2;  // 2 bytes
  unsigned long data3;  // 4 bytes
} serverDataType;

typedef struct {
  uint16_t data0;
  uint16_t data1;
  uint16_t data2;
  uint16_t data3;
} clientDataType;

clientDataType clientMessage;
serverDataType serverMessage;

ESP_NOW_Network_Node *node;

void recvData(const uint8_t *addr, const uint8_t position, const uint8_t *data, int len){
  if (((int)data[2]==0x50)&&((int)data[3]==0x49)&&((int)data[3]==0x49))
    return;
  esp_now_data_t *msg = (esp_now_data_t *)data;
  if (len > 0){
#ifdef SERVER
    clientDataType *d = (clientDataType *)msg->str;
#else
    serverDataType *d = (serverDataType *)msg->str;
#endif
    log_i("  From %d Received: %d %d %d %d", position, d->data0, d->data1, d->data2, d->data3);
  }  
}

void setup() {
  Serial.begin(115200);
  //while(!Serial);
#ifdef SERVER
  node = new ESP_NOW_Network_Node(EPSERVER);
#else
  node = new ESP_NOW_Network_Node(EPCLIENT);
#endif
  node->onNewRecv(recvData, NULL);

  srand( (unsigned)time(NULL) );

  //node->clearAllPeers();
}
#ifdef SERVER
void sendData(){
  serverMessage.data0 = true;
  serverMessage.data1 = false;  
  serverMessage.data2 = rand() / 1000000;  
  serverMessage.data3 = millis();  
  const char *d = (const char *)(&serverMessage);
  node->senddata(d, sizeof(serverMessage));
  log_i("  Sent: %d %d %d %d", serverMessage.data0, serverMessage.data1, serverMessage.data2, serverMessage.data3);
}
#else
void sendData(){
  clientMessage.data0 = rand() / 1000000;
  clientMessage.data1 = rand() / 1000000;  
  clientMessage.data2 = rand() / 1000000;  
  clientMessage.data3 = rand() / 1000000;  
  const char *d = (const char *)(&clientMessage);
  node->senddata(d, sizeof(clientMessage));
  log_i("  Sent: %d %d %d %d", clientMessage.data0, clientMessage.data1, clientMessage.data2, clientMessage.data3);
}
#endif

void loop() {
  if (!node->ready){
    node->checkstate();
    delay(1000);
  }
  else {
    sendData();
    delay(10000);
  }
}

