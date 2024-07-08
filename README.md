# ESP_NOW_Network

An easy-to-use library for creating ESP-NOW networks with ESP32 devices  for Arduino-ESP32 >= 3.02

## Installation

You can install this library through the Arduino Library Manager or by downloading it from GitHub and placing it in the `libraries` folder of your Arduino sketchbook.

## Usage

Include the library in your sketch and use the provided classes to set up an ESP-NOW network.

```cpp
#include "ESP_NOW_NETWORK.h"

ESP_NOW_Network_Node *node;

void setup() {
  Serial.begin(115200);
  node = new ESP_NOW_Network_Node(EPCLIENT);
  node->onNewRecv(recv, NULL);
}

void loop() {
  // Your code here
}

```
## Example

See the examples/BasicNetwork/BasicNetwork.ino file for a basic example of how to use this library.
