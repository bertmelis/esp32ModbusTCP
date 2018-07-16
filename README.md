# esp32Modbus

[![Build Status](https://travis-ci.com/bertmelis/esp32Modbus.svg?branch=master)](https://travis-ci.com/bertmelis/esp32Modbus)

> WORK IN PROGRESS

This is a async Modbus client (master) for ESP32.
It uses [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) for TCP handling.

Currently only functioncode 03 (read holding registers) and TCP connections are implemented.

To do:
- implement other function codes
- implement esp32-RS485 for modbus-rtu
