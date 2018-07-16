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
This example makes a request to my SMA solar inverter every 60 seconds
The request is 2 registers (each 16 bit wide) at address 30053
The returned device ID is 0x2468 = 9320

The modbus server (= SMA Sunny Boy) is defined as
ModbusTCP sunnyboy(3, {192, 168, 123, 123}, 502);
where:
- 3 = device ID
- {192, 168, 123, 13} = device IP address
- 502 = port number

The callback has a MB_ADU as argument. This is a structure containing
- the message ID
- the function code
- the address
- data length
- value
The bool argument indicates the modbus request was succesfull or not.
*/


#include <Arduino.h>
#include <WiFi.h>
#include <ModbusTCP.h>

const char* ssid = "xxxx";
const char* pass = "xxxx";
bool WiFiConnected = false;

ModbusTCP sunnyboy(3, {192, 168, 123, 123}, 502);

void setup() {
    Serial.begin(115200);

    WiFi.disconnect(true);  // delete old config
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(false);

    sunnyboy.onAnswer([](bool succes, MB_ADU mb_adu) {
      Serial.print("Received:\n");
      if (succes) {
        uint32_t deviceType = 0;
        uint8_t* value = mb_adu.value;
        deviceType = value[0];
        deviceType = deviceType << 8 | value[1];
        deviceType = deviceType << 8 | value[2];
        deviceType = deviceType << 8 | value[3];
        Serial.printf("Sunny Boy device type: %i\n", deviceType);
      } else {
        Serial.println("modbus error");
      }
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
  if ((millis() - lastMillis > 60000 &&
      WiFiConnected) ||
      lastMillis == 0) {
    lastMillis = millis();
    if (sunnyboy.request(READ_HOLD_REGISTER, 30053, 2)) {
      Serial.print("Requesting data\n");
    } else {
      Serial.print("Request not succeeded\n");
    }
  }
}
