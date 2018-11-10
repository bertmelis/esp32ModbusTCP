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
}

#include <IPAddress.h>
#include <AsyncTCP.h>

#define IDLE_CONNECTION_TIMEOUT 1000  // time before connection wil be closed (milliseconds)
#define NUMBER_QUEUE_ITEMS 20         // size of queue (items)
#define RX_BUFF_SIZE 300              // size of RX buffer (bytes)
#define TX_BUFF_SIZE 300              // size of TX buffer (bytes)
#define FLOOD_SERVER 0                // flood = don't wait for response before sending next request (currently not implemented)
#define RX_TIMEOUT 2000               // rx timeout for TCP (milliseconds)
#define ACK_TIMEOUT 2000              // ack timeout for TCP (milliseconds)

enum MBFunctionCode : uint8_t {
  READ_COIL = 0x01,
  READ_DISC_INPUT = 0x02,
  READ_HOLD_REGISTER = 0x03,
  READ_INPUT_REGISTER = 0x04,
  WRITE_COIL = 0x05,
  WRITE_HOLD_REGISTER = 0x06,
  WRITE_MULT_COILS = 0x0F,
  WRITE_MULT_REGISTERS = 0x10
};

struct MB_ADU {
  uint16_t id;
  MBFunctionCode functionCode;
  uint16_t address;
  uint16_t length;
  uint8_t* value;
};

typedef std::function<void(bool, MB_ADU)> ModbusOnData;

class esp32ModbusTCP {
 public:
  esp32ModbusTCP(uint8_t serverID, IPAddress addr, uint16_t port);
  ~esp32ModbusTCP();
  uint16_t request(MBFunctionCode fc, uint16_t addr, uint16_t len, uint8_t* val = nullptr);
  void onAnswer(ModbusOnData handler);

 private:
  // TCP
  AsyncClient _client;
  static void _onConnect(void* mb, AsyncClient* client);
  static void _onDisconnect(void* mb, AsyncClient* client);
  static void _onAck(void* mb, AsyncClient* client, size_t len, uint32_t time);
  static void _onError(void* mb, AsyncClient* client, int8_t error);
  static void _onData(void* mb, AsyncClient* client, void* data, size_t len);
  static void _onTimeout(void* mb, AsyncClient* client, uint32_t time);
  static void _onPoll(void* mb, AsyncClient* client);
  uint32_t _lastMillis;

  // Modbus
  enum STATE {
    IDLE,
    BUSY
  } _state;
  const uint8_t _serverID;
  const IPAddress _addr;
  const uint16_t _port;
  char _TXBuff[TX_BUFF_SIZE];
  uint8_t _RXBuff[RX_BUFF_SIZE];
  void _send();
  uint16_t _getNextPacketId();
  ModbusOnData _handler;
  QueueHandle_t _queue;
};
