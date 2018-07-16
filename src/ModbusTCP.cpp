/* esp32Modbus

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

#include <esp32-hal.h>
#include "ModbusTCP.h"
#include "Helpers.h"

#include <Arduino.h>

ModbusTCP::ModbusTCP(uint8_t serverID, IPAddress addr, uint16_t port) :
  _client(),
  _lastMillis(0),
  _state(IDLE),
  _serverID(serverID),
  _addr(addr),
  _port(port),
  _TXBuff{0},
  _RXBuff{0} {
  _client.setRxTimeout(2000);
  _client.setAckTimeout(2000);
  _client.onConnect(_onConnect, this);
  _client.onDisconnect(_onDisconnect, this);
  _client.onAck(_onAck, this);
  _client.onError(_onError, this);
  _client.onData(_onData, this);
  _client.onPoll(_onPoll, this);
  _queue = xQueueCreate(NUMBER_QUEUE_ITEMS, sizeof(MB_ADU));
}

ModbusTCP::~ModbusTCP() {
  // nothing to be done (yet)?
}

uint16_t ModbusTCP::request(MBFunctionCode fc, uint16_t addr, uint16_t len, uint8_t* val) {
  uint16_t id = _getNextPacketId();
  MB_ADU* adu = new MB_ADU {
    id,
    fc,
    addr,
    len,
    val
  };
  if (xQueueSendToBack(_queue, adu, 10) == pdPASS) {
    _send();
    return id;
  }
  return 0;
}

void ModbusTCP::onAnswer(ModbusOnData handler) {
  _handler = handler;
}

void ModbusTCP::_onConnect(void* mb, AsyncClient* client) {
  ModbusTCP* instance = static_cast<ModbusTCP*>(mb);
  instance->_lastMillis = millis();
  instance->_send();
}

void ModbusTCP::_onDisconnect(void* mb, AsyncClient* client) {
  // ModbusTCP* instance = static_cast<ModbusTCP*>(mb);
  // do nothing
}

void ModbusTCP::_onAck(void* mb, AsyncClient* client, size_t len, uint32_t time) {
  ModbusTCP* instance = static_cast<ModbusTCP*>(mb);
  instance->_lastMillis = millis();
}

void ModbusTCP::_onError(void* mb, AsyncClient* client, int8_t error) {
  ModbusTCP* instance = static_cast<ModbusTCP*>(mb);
  MB_ADU mb_adu;
  xQueueReceive(instance->_queue, static_cast<void*>(&mb_adu), 50);
  instance->_handler(false, mb_adu);
  instance->_lastMillis = millis();
  instance->_state = IDLE;
}

void ModbusTCP::_onData(void* mb, AsyncClient* client, void* data, size_t len) {
  // It is assumed that a Modbus message completely fits in one TCP packet.
  // So when soon _onData is called, the complete processing can be done.
  // As we're communication in a sequential way, the message is corresponding to
  // the ADU at the queue front.
  ModbusTCP* instance = static_cast<ModbusTCP*>(mb);
  MB_ADU mb_adu;
  xQueueReceive(instance->_queue, static_cast<void*>(&mb_adu), 50);
  memcpy(instance->_RXBuff, data, len);
  for (uint8_t i = 0; i < len; ++i) Serial.printf("%02X ", instance->_RXBuff[i]);
  Serial.print("\n");
  mb_adu.value = &(instance->_RXBuff[9]);
  instance->_handler(true, mb_adu);
  instance->_lastMillis = millis();
  instance->_state = IDLE;
  instance->_send();  // check if there are other queued items
}

void ModbusTCP::_onTimeout(void* mb, AsyncClient* client, uint32_t time) {
  ModbusTCP* instance = static_cast<ModbusTCP*>(mb);
  instance->_onError(mb, client, -3);  // -3: timeout?
}

void ModbusTCP::_onPoll(void* mb, AsyncClient* client) {
  ModbusTCP* instance = static_cast<ModbusTCP*>(mb);
  if (millis() - instance->_lastMillis > IDLE_CONNECTION_TIMEOUT &&
    (uxQueueMessagesWaiting(instance->_queue) == 0)) {
    instance->_client.close();
  }
}

void ModbusTCP::_send() {
  if (!_client.connected()) {
    _client.connect(_addr, _port);
    return;
  }
  if (_state == IDLE && (uxQueueMessagesWaiting(_queue) != 0)) {
    MB_ADU mb_adu;
    xQueuePeek(_queue, &mb_adu, 50);
    memset(&_TXBuff[0], ModbusTCPInternals::high(mb_adu.id), 1);
    memset(&_TXBuff[1], ModbusTCPInternals::low(mb_adu.id), 1);
    memset(&_TXBuff[2], 0x00, 2);
    memset(&_TXBuff[4], 0x00, 1);
    memset(&_TXBuff[5], 0x06, 1);
    memset(&_TXBuff[6], _serverID, 1);
    memset(&_TXBuff[7], (uint8_t)mb_adu.functionCode, 1);
    memset(&_TXBuff[8], ModbusTCPInternals::high(mb_adu.address), 1);
    memset(&_TXBuff[9], ModbusTCPInternals::low(mb_adu.address), 1);
    memset(&_TXBuff[10], ModbusTCPInternals::high(mb_adu.length), 1);
    memset(&_TXBuff[11], ModbusTCPInternals::low(mb_adu.length), 1);
    _client.add(_TXBuff, 12);
    _client.send();
    _state = BUSY;
  }
}

uint16_t ModbusTCP::_getNextPacketId() {
  static uint16_t id = 0;
  return ++id;
}
