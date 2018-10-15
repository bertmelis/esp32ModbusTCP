# esp32ModbusTCP

[![Build Status](https://travis-ci.com/bertmelis/esp32ModbusTCP.svg?branch=master)](https://travis-ci.com/bertmelis/esp32ModbusTCP)

> WORK IN PROGRESS

This is a async Modbus client (master) for ESP32.
It uses [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) for TCP handling.

Currently only functioncode 03 (read holding registers) and TCP connections are implemented.

To do:

- implement other function codes

For modbus-RTU, check out [esp32ModbusRTU](https://github.com/bertmelis/esp32ModbusRTU)
