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

#ifndef esp32ModbusTCP_h
#define esp32ModbusTCP_h

#include <functional>

#include <AsyncTCP.h>
#include <IPAddress.h>

#include <esp32-hal-log.h>  // for millis()
#include <freertos/queue.h>

#include "esp32ModbusTypeDefs.h"
#include "ModbusMessage.h"

#ifndef MB_NUMBER_QUEUE_ITEMS
#define MB_NUMBER_QUEUE_ITEMS 20  // size of queue (items)
#endif
#ifndef MB_IDLE_DICONNECT_TIME
#define MB_IDLE_DICONNECT_TIME 60000  // msecs before an idle conenction will be closed
#endif

class esp32ModbusTCP {
 public:
  esp32ModbusTCP(uint8_t serverID, IPAddress addr, uint16_t port = 502);
  ~esp32ModbusTCP();
  void onData(esp32Modbus::MBTCPOnData handler);
  void onError(esp32Modbus::MBTCPOnError handler);
  uint16_t readDiscreteInputs(uint16_t address, uint16_t numberInputs);
  uint16_t readHoldingRegisters(uint16_t address, uint16_t numberRegisters);
  uint16_t readInputRegisters(uint16_t address, uint16_t numberRegisters);

 private:
  uint16_t _addToQueue(esp32ModbusTCPInternals::ModbusRequest* request);

  AsyncClient _client;
  void _connect();
  void _disconnect(bool now = false);
  static void _onConnected(void* mb, AsyncClient* client);
  static void _onDisconnected(void* mb, AsyncClient* client);
  static void _onError(void* mb, AsyncClient* client, int8_t error);
  static void _onTimeout(void* mb, AsyncClient* client, uint32_t time);
  static void _onData(void* mb, AsyncClient* client, void* data, size_t length);
  static void _onPoll(void* mb, AsyncClient* client);
  void _processQueue();
  void _tryError(esp32Modbus::Error error);
  void _tryData(esp32ModbusTCPInternals::ModbusResponse* response);
  void _next();
  uint32_t _lastMillis;
  enum {
    NOTCONNECTED,
    CONNECTING,
    DISCONNECTING,
    IDLE,
    WAITING,
  } _state;
  const uint8_t _serverID;
  const IPAddress _addr;
  const uint16_t _port;
  esp32Modbus::MBTCPOnData _onDataHandler;
  esp32Modbus::MBTCPOnError _onErrorHandler;
  QueueHandle_t _queue;
};

#endif
