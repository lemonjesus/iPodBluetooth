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

That would look something like:

```
sudo apt install build-essential gcc-arm-none-eabi libusb-1.0-0-dev
```

### Building
```bash
cd build-ipodnano3g
./MAKEBOOTLOADER.sh
```

At the end, if it all worked, you should see a directory listing with two files: the installer and uninstaller files. If one is missing, the code failed to compile and an error was printed to the console further up.

### Flash
```bash
# still in build-ipodnano3g with an iPod in DFU mode plugged in
./mks5lboot --bl-inst bootloader-ipodnano3g.ipod
```

Your iPod will beep a few times and then reboot into the software. If you hold the Play button down, the software will not enable serial communication so you can program the ESP32 without the iPod holding the serial lines high. 

## Protocol
On boot, the iPod will begin by sending `reset` commands to the Bluetooth board until it gets a response. This is to ensure that the Bluetooth board is in a known state.

After that, the Bluetooth board will search for devices and report them in plain text to the iPod. The iPod will then display the devices on the screen and allow you to select one. Once you select a device, the iPod will send the name of the device to the Bluetooth board. The Bluetooth board will then attempt to connect to the device. If it succeeds, it will send `connected` to the iPod. The iPod will then boot into the iPod OS.

Example communication (-> is iPod to Bluetooth, <- is Bluetooth to iPod):

```
-> reset
-> reset
<- starting
<- device: Device Name 1
<- device: Device Name 2
<- device: Device Name 3
-> Device Name 2
<- connecting
<- connecting
<- connecting
<- connected
```

At this point, the Bluetooth board uses serial to interact with the iPod over the accessory protocol. Theoretically, the ESP32 should always respond to `reset`, but sometimes it doesn't. I'm not sure why.

## Troubleshooting

### The iPod doesn't turn on, it just clicks over and over again!
This is the result of a weak battery. You can keep it plugged in for a while to see if it charges. After a while, the clicks will start to have pitch to them and eventually turn into beeps and eventually the iPod will start. If this doesn't happen and it clicks forever, then the iPod battery is totally lifeless and you'll need to jump start it with 3.7v on the battery terminals. Be careful, however. This is a LiPo battery and they can be nasty.

### When trying to flash the iPod, it clicks (or beeps shortly) but then just reboots into the iPod OS!
This might also be the result of a bad battery. Follow the jumpstarting instructions above. If that doesn't work, then you might have a bad iPod. You can try to flash it again, but if it doesn't work, then you'll need to get a new iPod.
