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

#include <Arduino.h>

#include <esp32-hal.h>
#include "esp32ModbusTCP.h"

esp32ModbusTCP::esp32ModbusTCP(uint8_t serverID, IPAddress addr, uint16_t port) :
  _client(),
  _lastMillis(0),
  _state(NOTCONNECTED),
  _serverID(serverID),
  _addr(addr),
  _port(port),
  _onDataHandler(nullptr),
  _onErrorHandler(nullptr),
  _queue() {
    _client.onConnect(_onConnected, this);
    _client.onDisconnect(_onDisconnected, this);
    _client.onError(_onError, this);
    _client.onTimeout(_onTimeout, this);
    _client.onPoll(_onPoll, this);
    _client.onData(_onData, this);
    _client.setNoDelay(true);
    _client.setAckTimeout(5000);
    _queue = xQueueCreate(MB_NUMBER_QUEUE_ITEMS, sizeof(esp32ModbusTCPInternals::ModbusRequest*));
  }

esp32ModbusTCP::~esp32ModbusTCP() {
  esp32ModbusTCPInternals::ModbusRequest* req = nullptr;
  while (xQueueReceive(_queue, &(req), (TickType_t)20)) {
    delete req;
  }
  vQueueDelete(_queue);
}

void esp32ModbusTCP::onData(esp32Modbus::MBTCPOnData handler) {
  _onDataHandler = handler;
}

void esp32ModbusTCP::onError(esp32Modbus::MBTCPOnError handler) {
  _onErrorHandler = handler;
}

uint16_t esp32ModbusTCP::readDiscreteInputs(uint16_t address, uint16_t numberInputs) {
  esp32ModbusTCPInternals::ModbusRequest* request =
    new esp32ModbusTCPInternals::ModbusRequest02(_serverID, address, numberInputs);
  return _addToQueue(request);
}

uint16_t esp32ModbusTCP::readHoldingRegisters(uint16_t address, uint16_t numberRegisters) {
  esp32ModbusTCPInternals::ModbusRequest* request =
    new esp32ModbusTCPInternals::ModbusRequest03(_serverID, address, numberRegisters);
  return _addToQueue(request);
}

uint16_t esp32ModbusTCP::readInputRegisters(uint16_t address, uint16_t numberRegisters) {
  esp32ModbusTCPInternals::ModbusRequest* request =
    new esp32ModbusTCPInternals::ModbusRequest04(_serverID, address, numberRegisters);
  return _addToQueue(request);
}

uint16_t esp32ModbusTCP::_addToQueue(esp32ModbusTCPInternals::ModbusRequest* request) {
  if (uxQueueSpacesAvailable(_queue) > 0) {
    if (xQueueSend(_queue, &request, (TickType_t) 10) == pdPASS) {
      _processQueue();
      return request->getId();
    }
  }
  delete request;
  return 0;
}

/************************ TCP Handling *******************************/

void esp32ModbusTCP::_connect() {
  if (_state > NOTCONNECTED) {
    log_i("already connected");
    return;
  }
  log_v("connecting");
  _client.connect(_addr, _port);
  _state = CONNECTING;
}
void esp32ModbusTCP::_disconnect(bool now) {
  log_v("disconnecting");
  _state = DISCONNECTING;
  _client.close(now);
}

void esp32ModbusTCP::_onConnected(void* mb, AsyncClient* client) {
  log_v("connected");
  esp32ModbusTCP* o = reinterpret_cast<esp32ModbusTCP*>(mb);
  o->_state = IDLE;
  o->_lastMillis = millis();
  o->_processQueue();
}

void esp32ModbusTCP::_onDisconnected(void* mb, AsyncClient* client) {
  log_v("disconnected");
  esp32ModbusTCP* o = reinterpret_cast<esp32ModbusTCP*>(mb);
  o->_state = NOTCONNECTED;
  o->_lastMillis = millis();
  o->_processQueue();
}

void esp32ModbusTCP::_onError(void* mb, AsyncClient* client, int8_t error) {
  esp32ModbusTCP* o = reinterpret_cast<esp32ModbusTCP*>(mb);
  if (o->_state == IDLE || o->_state == DISCONNECTING) {
    log_w("unexpected tcp error");
    return;
  }
  o->_tryError(esp32Modbus::COMM_ERROR);
}

void esp32ModbusTCP::_onTimeout(void* mb, AsyncClient* client, uint32_t time) {
  esp32ModbusTCP* o = reinterpret_cast<esp32ModbusTCP*>(mb);
  if (o->_state < WAITING) {
    log_w("unexpected tcp timeout");
    return;
  }
  o->_tryError(esp32Modbus::TIMEOUT);
}

void esp32ModbusTCP::_onData(void* mb, AsyncClient* client, void* data, size_t length) {
  /* It is assumed that a Modbus message completely fits in one TCP packet.
     So when soon _onData is called, the complete processing can be done.
     As we're communication in a sequential way, the message is corresponding to
     the request at the queue front. */
  log_v("data");
  esp32ModbusTCP* o = reinterpret_cast<esp32ModbusTCP*>(mb);
  esp32ModbusTCPInternals::ModbusRequest* req = nullptr;
  xQueuePeek(o->_queue, &req, (TickType_t)10);
  esp32ModbusTCPInternals::ModbusResponse resp(reinterpret_cast<uint8_t*>(data), length, req);
  if (resp.isComplete()) {
    if (resp.isSucces()) {  // all OK
      o->_tryData(&resp);
    } else {  // message not correct
      o->_tryError(resp.getError());
    }
  } else {  // message not complete
    o->_tryError(esp32Modbus::COMM_ERROR);
  }
}

void esp32ModbusTCP::_onPoll(void* mb, AsyncClient* client) {
  esp32ModbusTCP* o = static_cast<esp32ModbusTCP*>(mb);
  if (millis() - o->_lastMillis > MB_IDLE_DICONNECT_TIME) {
    log_v("idle time disconnecting");
    o->_disconnect();
  }
}

void esp32ModbusTCP::_processQueue() {
  if (_state == NOTCONNECTED && uxQueueMessagesWaiting(_queue) > 0) {
    _connect();
    return;
  }
  if (_state == CONNECTING ||
      _state == WAITING ||
      _state == DISCONNECTING ||
      uxQueueSpacesAvailable(_queue) == 0 ||
      !_client.canSend()) {
    return;
  }
  esp32ModbusTCPInternals::ModbusRequest* req = nullptr;
  if (xQueuePeek(_queue, &req, (TickType_t)20)) {
    _state = WAITING;
    log_v("send");
    _client.add(reinterpret_cast<char*>(req->getMessage()), req->getSize());
    _client.send();
    _lastMillis = millis();
  }
}

void esp32ModbusTCP::_tryError(esp32Modbus::Error error) {
  esp32ModbusTCPInternals::ModbusRequest* req = nullptr;
  if (xQueuePeek(_queue, &req, (TickType_t)10)) {
    if (_onErrorHandler) _onErrorHandler(req->getId(), error);
  }
  _next();
}

void esp32ModbusTCP::_tryData(esp32ModbusTCPInternals::ModbusResponse* response) {
  if (_onDataHandler) _onDataHandler(
      response->getId(),
      response->getSlaveAddress(),
      response->getFunctionCode(),
      response->getData(),
      response->getByteCount());
  _next();
}

void esp32ModbusTCP::_next() {
  esp32ModbusTCPInternals::ModbusRequest* req = nullptr;
  if (xQueueReceive(_queue, &req, (TickType_t)20)) {
    delete req;
  }
  _lastMillis = millis();
  _state = IDLE;
  _processQueue();
}
