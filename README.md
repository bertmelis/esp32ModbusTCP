# ARCHIVED

This repo is archived no longer maintained.

For a complete Modbus library for ESP32, I invite you to https://github.com/eModbus/eModbus

# esp32ModbusTCP

This is a async Modbus client (master) for ESP32.

- Modbus Client aka Master for ESP32
- built for the [Arduino framework for ESP32](https://github.com/espressif/arduino-esp32)
- completely delay()-free
- async operation, based on [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
- optional debug info
- similar API as my [esp32ModbusRTU](https://github.com/bertmelis/esp32ModbusRTU) implementation

## Developement status

I have this library in use myself with quite some uptime (only using FC3 -read holding registers- though).

Things to do, ranked:

- limit reconnecting rate in case of server failure
- unit testing for ModbusMessage
- handle messages split over multiple TCP packets
- implement missing function codes (no priority, pull requests happily accepted)
- multiple connections (not recommended by Modbus spec, lowest priority)

## Installation

The easiest, and my preferred one, method to install this library is to use Platformio.
[https://platformio.org/lib/show/5902/esp32ModbusTCP/installation](https://platformio.org/lib/show/5902/esp32ModbusTCP/installation)

Alternatively, you can use Arduino IDE for developement.
[https://www.arduino.cc/en/guide/libraries](https://www.arduino.cc/en/guide/libraries)

There are two dependencies:

- Arduino framework for ESP32 ([https://github.com/espressif/arduino-esp32](https://github.com/espressif/arduino-esp32))
- AsyncTCP ([https://github.com/me-no-dev/AsyncTCP](https://github.com/me-no-dev/AsyncTCP))

## Usage

The API is quite lightweight. It takes minimal 3 steps to get going.

First create the ModbusTCP object. The constructor takes three arguments: server ID, IP address and port

```C++
esp32ModbusTCP myModbusServer(3, {192, 168, 1, 2}, 502);
```

Next add a onData callback. This can be any function object. Here it uses a simple function pointer.

```C++

void handleData(uint16_t packet, uint8_t slave, MBFunctionCode fc , uint8_t* data , uint16_t len) {
  Serial.printf("received data: id %u, slave %u, fc %u\ndata: 0x", packet, slave, fc);
  for (uint8_t i = 0; i < len; ++i) {
    Serial.printf("%02x", data[i]);
  }
  Serial.print("\n");
}

// in setup()
myModbusServer.onData(handleData);
```

Optionally you can attach an onError callback. Again, any function object is possible.

```C++
// in setup()
myModbusServer.onError([](uint16_t packet, MBError e) {
  Serial.printf("Error packet %u: %02x\n", packet, e);
});
```

Now ModbusTCP is setup, you can start reading or writing. The arguments depend on the function used. Obviously, address and length are always required. The length is specified in words, not in bytes!

```C++
uint16_t packetId = myModbusServer.readHoldingRegisters(30201, 2);  // address + length
```

All communication objects return the packet ID in case of succes or zero in case of failure. You can consider this packet ID as unique per ModbusTCP object.
The requests are places in a queue. The function returns immediately and doesn't wait for the server to respond.
Mind there will only be made one connection to the server, so the requests are handled one by one.

## Configuration

A connection to the server is only initiated upon the first request. After 60 seconds of idle time (no TCP traffic), the connection will be closed. The first request takes a bit longer to setup the connection. De fefault idle time is 60 seconds, but you can change this in the header file or by setting a compiler flag 

```C++
#define MB_IDLE_DICONNECT_TIME 60000  // in milliseconds
```

The request queue holds maximum 20 items. So a 21st request will fail until the queue has an empty spot. You can change the queue size in the header file or by using a compiler flag:

```C++
#define MB_NUMBER_QUEUE_ITEMS 20
```

## Implementing new function codes

This library uses classes called `ModbusMessage`, which is the base type. A subtype called `ModbusRequest` is the base to implement new function codes.
To create a new function code, you first have to create the new type in the header file `ModbusMessage.h`:

```C++
// ModbusMessage.h

// replace xx by the new function code
class ModbusRequestxx : public ModbusRequest {
 public:
  explicit ModbusRequestxx(uint8_t slaveAddress, uint16_t address, uint16_t numberCoils);
  size_t responseLength();

};
```

```C++
// ModbusMessage.cpp

// replace xx by the new function code
ModbusRequestxx::ModbusRequestxx(uint8_t slaveAddress, uint16_t address) :  // add extra arguments as nessecary
  ModbusRequest(<TOTAL LENGTH IN BYTES>) {  // contstruct a ModbusRequest with a certain nuber of bytes
  _slaveAddress = slaveAddress;
  _functionCode = READ_INPUT_REGISTER;
  _address = address;
  _byteCount = <INSERT CALCULATION >;  // specify the paykload length in bytes
  add(high(_packetId));
  add(low(_packetId));
  // from here, build rest of message
  // include 
}

size_t ModbusRequest04::responseLength() {
  // return the total (= including all Modbus protocol bytes) number of bytes a response should have. Rely on _byteCount for calculation
}
```

Don't forget the code to include the payload.

To make the newly created ModbusRequest useful, include it in the ModbusTCP interface. Check line 52+ from `espModbusTCP.h` and line 67+ from `espModbusTCP.cpp`

## Issues

Please file a Github issue ~~if~~ when you find a bug. You can also use the issue tracker for feature requests.
