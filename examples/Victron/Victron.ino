/* esp32Modbus

Copyright 2018 Bert Melis
Copyright 2020 Pablo Zerón

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

The modbus server (= Victron) can be defined as
ModbusTCP victron(239, {192, 168, 123, 123}, 502);
where:
- 239 = device ID
- {192, 168, 123, 13} = device IP address
- 502 = port number

or

ModbusTCP victron({192, 168, 123, 123}, 502);
where:
- {192, 168, 123, 13} = device IP address
- 502 = port number

and the query id's must be in registerData

*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp32ModbusTCP.h>

const char* ssid = "xxxx";
const char* pass = "xxxx";
bool WiFiConnected = false;

esp32ModbusTCP victron({192, 168, 123, 123}, 502);

enum valueType
{
    ENUM,     // enumeration
    U16FIX0,  // 16 bits unsigned, no decimals
    U16FIX1,  // 16 bits unsigned, 1 decimals (/10)
    U16FIX2,  // 16 bits unsigned, 2 decimals (/100)
    U16FIX3,  // 16 bits unsigned, 3 decimals (/1000)
    U16FIX10, // 16 bits unsigned, plus 10 (*10)
    U32FIX0,  // 32 bits unsigned, no decimals
    U32FIX1,  // 32 bits unsigned, 1 decimals
    U32FIX2,  // 32 bits unsigned, 2 decimals
    U32FIX3,  // 32 bits unsigned, 3 decimals
    U64FIX0,  // 64 bits unsigned, no decimals
    U64FIX1,  // 64 bits unsigned, 1 decimals
    U64FIX2,  // 64 bits unsigned, 2 decimals
    U64FIX3,  // 64 bits unsigned, 3 decimals
    S16FIX0,  // 16 bits signed, no decimals
    S16FIX1,  // 16 bits signed, 1 decimals (/10)
    S16FIX2,  // 16 bits signed, 2 decimals (/100)
    S16FIX3,  // 16 bits signed, 3 decimals (/1000)
    S16FIX10, // 16 bits signed, plus 10 (*10)
    S32FIX0,  // 32 bits signed, no decimals
    S32FIX1,  // 32 bits signed, 1 decimals
    S32FIX2,  // 32 bits signed, 2 decimals
    S32FIX3,  // 32 bits signed, 3 decimals
};

struct registerData
{
    const char* name;
    uint8_t serverID;
    uint16_t address;
    uint16_t length;
    valueType type;
};

registerData victronRegisters[] = {
    "Frecuency", 239, 9, 1, S16FIX2,
    "Grid", 239, 12, 1, S16FIX10,
    "DC voltage", 239, 26, 1, U16FIX2,
    "DC current", 239, 27, 1, S16FIX1,
    "Battery Temperature", 239, 61, 1, S16FIX1,
    "DC Power", 100, 842, 1, S16FIX0
};


uint8_t numberVictronRegisters = sizeof(victronRegisters) / sizeof(victronRegisters[0]);

float parseUnsigned16(uint8_t *data, int precision)
{
    uint16_t value = 0;
    value = (data[0] << 8) | (data[1]);

    switch (precision)
    {
        case 0: { return (float)value; break; }
        case 1: { return ((float)value / 10.0); break; }
        case 2: { return ((float)value / 100.0); break; }
        case 3: { return ((float)value / 1000.0); break; }
        case 10: { return ((float)value * 10.0); break; }
    }
    return 0;
}

float parseUnsigned32(uint8_t *data, int precision)
{
    uint32_t value = 0;
    value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);

    switch (precision)
    {
        case 0: { return (float)value; break; }
        case 1: { return ((float)value / 10.0); break; }
        case 2: { return ((float)value / 100.0); break; }
        case 3: { return ((float)value / 1000.0); break; }
    }
    return 0;
}

float parseUnsigned64(uint8_t *data, int precision)
{
    uint32_t high = 0;
    uint32_t low = 0;
    uint64_t value = 0;
    high = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
    low = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | (data[7]);
    value = (((uint64_t) high) << 32) | ((uint64_t) low);

    switch (precision)
    {
        case 0: { return (float)value; break; }
        case 1: { return ((float)value / 10.0); break; }
        case 2: { return ((float)value / 100.0); break; }
        case 3: { return ((float)value / 1000.0); break; }
    }
    return 0;
}

float parseSigned16(uint8_t *data, int precision)
{
    int16_t value = 0;
    value = (data[0] << 8) | (data[1]);

    switch (precision)
    {
        case 0: { return (float)value; break; }
        case 1: { return ((float)value / 10.0); break; }
        case 2: { return ((float)value / 100.0); break; }
        case 3: { return ((float)value / 1000.0); break; }
        case 10: { return ((float)value * 10.0); break; }
    }
    return 0;
}

float parseSigned32(uint8_t *data, int precision)
{
    int32_t value = 0;
    value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);

    switch (precision)
    {
        case 0: { return (float)value; break; }
        case 1: { return ((float)value / 10.0); break; }
        case 2: { return ((float)value / 100.0); break; }
        case 3: { return ((float)value / 1000.0); break; }
    }
    return 0;
}


void setup() {
    Serial.begin(115200);
    WiFi.disconnect(true);  // delete old config
    
    victron.onData([](uint16_t packet, uint8_t slave, esp32Modbus::FunctionCode fc , uint8_t* data , uint16_t len, void* arg) {
        registerData* a = reinterpret_cast<registerData*>(arg);
        switch (a->type)
        {
            case ENUM:
		        case U16FIX0:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned16(data, 0)); break; }
		        case U16FIX1:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned16(data, 1)); break; }
		        case U16FIX2:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned16(data, 2)); break; }
		        case U16FIX3:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned16(data, 3)); break; }
		        case U16FIX10: { Serial.printf("%s: %.2f\n", a->name, parseUnsigned16(data, 10)); break; }
		        case U32FIX0:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned32(data, 0)); break; }
		        case U32FIX1:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned32(data, 1)); break; }
		        case U32FIX2:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned32(data, 2)); break; }
		        case U32FIX3:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned32(data, 3)); break; }
		        case U64FIX0:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned64(data, 0)); break; }
		        case U64FIX1:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned64(data, 1)); break; }
		        case U64FIX2:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned64(data, 2)); break; }
		        case U64FIX3:  { Serial.printf("%s: %.2f\n", a->name, parseUnsigned64(data, 3)); break; }
		        case S16FIX0:  { Serial.printf("%s: %.2f\n", a->name, parseSigned16(data, 0)); break; }
		        case S16FIX1:  { Serial.printf("%s: %.2f\n", a->name, parseSigned16(data, 1)); break; }
		        case S16FIX2:  { Serial.printf("%s: %.2f\n", a->name, parseSigned16(data, 2)); break; }
		        case S16FIX3:  { Serial.printf("%s: %.2f\n", a->name, parseSigned16(data, 3)); break; }
		        case S16FIX10: { Serial.printf("%s: %.2f\n", a->name, parseSigned16(data, 10)); break; }
		        case S32FIX0:  { Serial.printf("%s: %.2f\n", a->name, parseSigned32(data, 0)); break; }
		        case S32FIX1:  { Serial.printf("%s: %.2f\n", a->name, parseSigned32(data, 1)); break; }
		        case S32FIX2:  { Serial.printf("%s: %.2f\n", a->name, parseSigned32(data, 2)); break; }
		        case S32FIX3:  { Serial.printf("%s: %.2f\n", a->name, parseSigned32(data, 3)); break; }
        }
        return;
    });
    victron.onError([](uint16_t packet, esp32Modbus::Error e, void* arg) {
      registerData* a = reinterpret_cast<registerData*>(arg);
      Serial.printf("Error packet in address %d (%u): %02x\n", a->address, packet, e);
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
        WiFi.begin(ssid, pass);
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
    Serial.print("reading registers:\n");
    for (uint8_t i = 0; i < numberVictronRegisters; ++i) {
	    if (victron.readHoldingRegisters(victronRegisters[i].serverID, victronRegisters[i].address, victronRegisters[i].length, &(victronRegisters[i])) > 0) {
	        Serial.printf("  requested %d\n", victronRegisters[i].address);
	    } else {
	        Serial.printf("  error requesting address %d\n", victronRegisters[i].address);
	    }
    }
  }
}
