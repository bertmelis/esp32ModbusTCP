/* esp32ModbusTCP

Copyright 2018 Bert Melis

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <functional>

extern "C" {
#include <FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
}

#include <WiFiClient.h>
//  #include <IPAddress.h>  // already included in WiFiClient.h

#include "esp32ModbusTypeDefs.h"
using namespace esp32Modbus;

#define IDLE_CONNECTION_TIMEOUT 10000  // time before connection wil be closed (milliseconds)
#define NUMBER_QUEUE_ITEMS 20         // size of queue (items)
#define RX_TIMEOUT 10                  // rx timeout for TCP (seconds)
#define ACK_TIMEOUT 5000              // ack timeout for TCP (milliseconds)

class esp32ModbusTCP {
 public:
  esp32ModbusTCP(uint8_t serverID, IPAddress addr, uint16_t port = 502);
  ~esp32ModbusTCP();
  void begin();
  void onData(MBOnData handler);
  void onError(MBOnError handler);
  uint16_t readHoldingRegister(uint16_t address, uint16_t byteCount);

 private:
  // TCP
  WiFiClient _client;
  static void _processQueue(esp32ModbusTCP* instance);
  const uint8_t _serverID;
  const IPAddress _addr;
  const uint16_t _port;
  MBOnData _onDataHandler;
  MBOnError _onErrorHandler;
  QueueHandle_t _queue;
  TaskHandle_t _task;
};
