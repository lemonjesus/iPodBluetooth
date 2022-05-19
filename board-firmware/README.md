# Board Firmware

This section assumes you can build for the ESP32 in Arduino. If you can't follow [this guide](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/). Be sure you use the newest version of the ESP32 board package.

## Install Libraries
This software uses modified versions of `arduino-audio-tools` and `ESP32-A2DP`. By the time this goes up, I probably won't have my code merged into the root fork, so you can use mine: TODO: LINK TO THE REPOS.

## Connect the ESP32 to the computer
This assumes you've installed the board in the iPod and have soldered the relevant pins to the 30-pin connector. You can plug in your breakout board and wire it up to your FTDI connector as follows:

TODO: Add an image of the connection from FTDI to the 30pin breakout.

## Program the Board
Make sure you have your Arduino IDE set up to program the ESP32. You also want to make sure the Core Logging Level is set to 'None' and Arduino Events happen on Core 1. You can then compile and program the board as you would normally.

## Testing
TODO

Immediately upon booting, this code will wait until it's put into setup mode. I made this easy to do over a serial console. With a baudrate of 9600, send `reset` over the wire to put it into setup mode. The ESP32 will reboot and start looking for devices. If it starts to log the Bluetooth devices it finds, then it should work as intended and you can proceed to the next step.
