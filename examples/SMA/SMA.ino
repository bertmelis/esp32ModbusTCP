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


/*

The modbus server (= SMA Sunny Boy) is defined as
ModbusTCP sunnyboy(3, {192, 168, 123, 123}, 502);
where:
- 3 = device ID
- {192, 168, 123, 13} = device IP address
- 502 = port number

All defined registers are holding registers, 2 word size (4 bytes)

*/


#include <Arduino.h>
#include <WiFi.h>
#include <esp32ModbusTCP.h>

const char* ssid = "xxxx";
const char* pass = "xxxx";
bool WiFiConnected = false;

esp32ModbusTCP sunnyboy(3, {192, 168, 123, 123}, 502);
enum smaType {
  ENUM,   // enumeration
  UFIX0,  // unsigned, no decimals
  SFIX0,  // signed, no decimals
};
struct smaData {
  const char* name;
  uint16_t address;
  uint16_t length;
  smaType type;
  uint16_t packetId;
};
smaData smaRegisters[] = {
  "status", 30201, 2, ENUM, 0,
  "connectionstatus", 30217, 2, ENUM, 0,
  "totalpower", 30529, 2, UFIX0, 0,
  "currentpower", 30775, 2, SFIX0, 0,
  "currentdc1power", 30773, 2, SFIX0, 0,
  "currentdc2power", 30961, 2, SFIX0, 0
};
uint8_t numberSmaRegisters = sizeof(smaRegisters) / sizeof(smaRegisters[0]);
uint8_t currentSmaRegister = 0;


void setup() {
    Serial.begin(115200);
    WiFi.disconnect(true);  // delete old config

    sunnyboy.onData([](uint16_t packet, uint8_t slave, esp32Modbus::FunctionCode fc , uint8_t* data , uint16_t len) {
      for (uint8_t i = 0; i < numberSmaRegisters; ++i) {
        if (smaRegisters[i].packetId == packet) {
          smaRegisters[i].packetId = 0;
          switch (smaRegisters[i].type) {
          case ENUM:
          case UFIX0:
            {
            uint32_t value = 0;
            value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
            Serial.printf("%s: %u\n", smaRegisters[i].name, value);
            break;
            }
          case SFIX0:
            {
            int32_t value = 0;
            value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
            Serial.printf("%s: %i\n", smaRegisters[i].name, value);
            break;
            }
          }
          return;
        }
      }
    });
    sunnyboy.onError([](uint16_t packet, esp32Modbus::Error e) {
      Serial.printf("Error packet %u: %02x\n", packet, e);
    });

    delay(1000);

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      Serial.print("WiFi connected. IP: ");
      Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
      WiFiConnected = true;
    }, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
        Serial.print("WiFi lost connection. Reason: ");
        Serial.println(info.disconnected.reason);
        WiFi.disconnect();
        WiFiConnected = false;
    }, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);

    WiFi.begin(ssid, pass);

    Serial.println();
    Serial.println("Connecting to WiFi... ");
}

void loop() {
  static uint32_t lastMillis = 0;
  if ((millis() - lastMillis > 30000 && WiFiConnected)) {
    lastMillis = millis();
    Serial.print("reading registers\n");
    for (uint8_t i = 0; i < numberSmaRegisters; ++i) {
      uint16_t packetId = sunnyboy.readHoldingRegisters(smaRegisters[i].address, smaRegisters[i].length);
      if (packetId > 0) {
        smaRegisters[i].packetId = packetId;
      } else {
        Serial.print("reading error\n");
      }
    }
  }
}
