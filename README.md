# esp32ModbusTCP

[![Build Status](https://travis-ci.com/bertmelis/esp32ModbusTCP.svg?branch=master)](https://travis-ci.com/bertmelis/esp32ModbusTCP)

> WORK IN PROGRESS

This is a async Modbus client (master) for ESP32.  
~~It uses [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) for TCP handling.~~  
While the API is async, this lib uses the sync WiFi client as backend.  All the blocking calls are moved to a seperate task.

## Status

- The lib is working. Use SyncTCP branch
- Currently only functioncode 03 (read holding registers) is implemented.

## To do

- implement other function codes
- try to implement AsyncTCP

For modbus-RTU, check out [esp32ModbusRTU](https://github.com/bertmelis/esp32ModbusRTU)
