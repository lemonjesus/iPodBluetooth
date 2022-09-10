# Board Firmware

## Prerequisites
This section assumes you can build for the ESP32 in Arduino. If you can't follow [this guide](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/). Be sure you use the newest version of the ESP32 board package.

This guide also assumes you're programming the board in-circuit. To do this, you'll need a 30-pin breakout board. The one I use I bought [here](https://elabbay.myshopify.com/products/apple-30m-bo-v1ac-apple-30-pin-male-plug-breakout-board-compact-type) and has worked very well for me.

## Install Libraries
This software uses modified versions of `arduino-audio-tools` and `ESP32-A2DP`. By the time this goes up, I probably won't have my code merged into the root fork, so you can use mine. Once cloned into your Arduino libraries folder, be sure you check out these specific SHAs:

 - [arduino-audio-tools](https://github.com/lemonjesus/arduino-audio-tools/tree/517695a2d953de52066b7be38fd35951edf9031d) @ SHA: `517695a2d953de52066b7be38fd35951edf9031d`
 - [ESP32-A2DP](https://github.com/lemonjesus/ESP32-A2DP/tree/08cdf1f96b26d54ff4a16171212da19edf79d683) @ SHA: `08cdf1f96b26d54ff4a16171212da19edf79d683`

## Connect the ESP32 to the computer
This assumes you've installed the board in the iPod and have soldered the relevant pins to the 30-pin connector. You can plug in your breakout board and wire it up to your FTDI connector as follows:

```
FTDI   30-pin
-----  -------
TX  -> TX (pin 12)
RX  -> RX (pin 13)
DTR -> Reserved (pin 14)
RTS -> Reserved (pin 17)
GND -> GND
```

Note that TX and RX are swapped. This relationship is correct between the ESP32 and the iPod, so we need to pretend to be the iPod and use the TX pin to send data to the ESP32 and the RX pin to receive data from the ESP32.

Be sure your FTDI is set to 3.3V. You can use the Arduino IDE to upload the code to the ESP32. You can also use the Arduino IDE to open the serial monitor and see the output of the ESP32.

## Program the Board
Make sure you have your Arduino IDE set up to program the ESP32. You also want to make sure the Core Logging Level is set to 'None' and Arduino Events happen on Core 1. You can then compile and program the board as you would normally.

## Testing
Immediately upon booting, this code will wait until it's put into setup mode. I made this easy to do over a serial console. With a baudrate of 9600, send `reset` over the wire to put it into setup mode. The ESP32 will reboot and start looking for devices. If it starts to log the Bluetooth devices it finds, then it should work as intended and you can proceed to the next step.

See the [ipod-software](../ipod-software/README.md) section for more information on the protocol.

## Additional Information
This code makes heavy use of FreeRTOS tasks and queues. Familiarize yourself with these concepts if you want to understand how this code works.
