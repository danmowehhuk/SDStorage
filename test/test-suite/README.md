# Physical Device Test Suite

These test are intended to be run on a real Arduino with an SD card attached to the
SPI bus. The SD chip select pin is expected on pin 12, but you can change this in the
`test-suite.ino` file.

The tests in this suite create, delete and update files and directories under the root
directory `/TESTROOT`, so ensure that directory does not already exist on the SD card.