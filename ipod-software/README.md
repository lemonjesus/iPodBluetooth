# The iPod Software

## About
This is the code that runs on the iPod that allows you to pick the Bluetooth device you want to connect to. It's based on the Rockbox bootloader. I've stripped away most of the Rockbox project files and only kept the files necessary to build this project.

The base implementation for the Nano 3rd Generation is by Castor - a developer from Madrid - who was working on porting Rockbox from a very long time ago.

## Developing

The main program that you see running on the iPod is `bootloader/ipods5l-devel.c`. This is probably where you'll want to add or change anything about the actual behavior of the software. I've also added some functions to the UART driver in `firmware/target/arm/s5l8702/ipodnano3g/serial-nano3g.c`. 

## Building and Using the iPod Software
### Prerequisites
 * `gcc` and `make` for your system (to build mks5lboot)
 * `gcc-arm-none-eabi`
 * `libusb`

### Building

```bash
cd build-ipodnano3g
./MAKEBOOTLOADER.sh
```

It should show you 

### Flash

```bash
# still in build-ipodnano3g with an iPod in DFU mode plugged in
./mks5lboot --bl-inst bootloader-ipodnano3g.ipod
```

Your iPod will beep a few times and then reboot into the software.
