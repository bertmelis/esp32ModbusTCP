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

#include <esp32-hal.h>
#include <esp_log.h>
#include "esp32ModbusTCP.h"
#include "ModbusMessage.h"

esp32ModbusTCP::esp32ModbusTCP(uint8_t serverID, IPAddress addr, uint16_t port) :
  _client(),
  _serverID(serverID),
  _addr(addr),
  _port(port),
  _onDataHandler(nullptr),
  _onErrorHandler(nullptr),
  _queue(nullptr),
  _task(nullptr) {
  _queue = xQueueCreate(NUMBER_QUEUE_ITEMS, sizeof(ModbusRequest*));
  if (_queue == NULL) {
    abort();
  }
}

esp32ModbusTCP::~esp32ModbusTCP() {
  // TODO(bertmelis): check destructor workings
  vTaskDelete(_task);
  vQueueDelete(_queue);
}

void esp32ModbusTCP::begin() {
  xTaskCreate((TaskFunction_t)&_processQueue, "esp32ModbusTCP", 4096, this, 5, &_task);
}

void esp32ModbusTCP::onData(MBOnData handler) {
  _onDataHandler = handler;
}

void esp32ModbusTCP::onError(MBOnError handler) {
  _onErrorHandler = handler;
}

uint16_t esp32ModbusTCP::readHoldingRegister(uint16_t address, uint16_t byteCount) {
  ModbusRequest* request = new ModbusRequest03(_serverID, address, byteCount);
  if (xQueueSendToBack(_queue, reinterpret_cast<void*>(&request), 100) == pdPASS) {
    return request->getId();
  }
  delete request;
  return 0;
}

void esp32ModbusTCP::_processQueue(esp32ModbusTCP* instance) {
  while (true) {
    ModbusRequest* request;
    if (xQueueReceive(instance->_queue, &request, portMAX_DELAY) == pdTRUE) {
      ESP_LOGD("request received id:%u\n", request->getId());
      uint8_t rxBuffer[256] = {0};
      size_t index = 0;
      uint8_t state = 0;
      uint32_t lastMillis = millis();
      bool busy = true;
      while (busy) {
        if (millis() - lastMillis > 10 * 1000) {
          ESP_LOGD("timeout\n");
          if (instance->_onErrorHandler) instance->_onErrorHandler(TIMEOUT);
          state = 0;
          busy = false;
          delete request;  // created in main task
          break;
        } else {
          switch (state) {
          case 0:  // idle
            if (!instance->_client.connected()) {
              ESP_LOGD("connecting\n");
              instance->_client.connect(instance->_addr, instance->_port);
              state = 1;
            } else {
              state = 2;
            }
            break;
          case 1:  // connecting
            if (!instance->_client.connected()) {
              delay(1);
              break;
            }
            ESP_LOGD("connected\n");
            state = 2;
            break;
          case 2:  // connected
            ESP_LOGD("0x");
            {
            uint8_t* msg = request->getMessage();
            for (uint8_t i = 0; i < 8; ++i) {
              ESP_LOGD("%02x", *(msg++));
            }
            ESP_LOGD("\n");
            }
            instance->_client.write(request->getMessage(), request->getSize());
            instance->_client.flush();
            state = 3;
            break;
          case 3:  // waiting for answer
            while (instance->_client.available()) {
              rxBuffer[index++] = instance->_client.read();
              if (index == request->responseLength()) {
                state = 4;
              }
            }
          case 4:  // data received
            ModbusResponse response(rxBuffer, index, request);
            if (response.isComplete()) {
              ESP_LOGD("msg complete\n");
              if (response.isSucces()) {
                ESP_LOGD("msg succes\n");
                // onDataHandler
                if (instance->_onDataHandler) instance->_onDataHandler(response.getPacketId(),
                                                                       response.getSlaveAddress(),
                                                                       response.getFunctionCode(),
                                                                       response.getData(),
                                                                       response.getByteCount());
                state = 0;
                busy = false;
                delete request;  // created in main task
              } else {
                ESP_LOGD("msg fail\n");
                if (instance->_onErrorHandler) instance->_onErrorHandler(response.getError());
                state = 0;
                busy = false;
                delete request;  // created in main task
              }
            } else {
              delay(1);
            }
          }
        }
      }
    }
  }
}
