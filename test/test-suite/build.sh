#!/bin/bash

arduino-cli compile -e -b arduino:avr:mega \
  --libraries ~/Arduino/libraries \
  --build-property build.extra_flags="-DDEBUG" .

#arduino-cli upload -b arduino:avr:mega -p /dev/cu.usbmodem1101 .

# sleep 2  # to allow board reset

#arduino-cli monitor -p /dev/cu.usbmodem1101 -c baudrate=9600
