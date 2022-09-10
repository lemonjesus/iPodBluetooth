# The iPod Bluetooth Board

## About
Here are the Gerber files for the iPod Bluetooth board. This board is designed to be used with the iPod Nano 3G, but it should work with other iPods as well. The board is designed to be as small as possible, so it's not the easiest to solder.

## Bill of Materials (and what they're for)
I've linked example Mouser parts, but any equivalent part will do. The only thing that might be important to keep the same is the I2S level shifter because of its pinout. The passives can certainly be swapped out for whatever you want - although high quality parts are recommended. I've also included what each part is for.

 - `MD1` - [ESP32 WROOM Module](https://www.mouser.com/ProductDetail/356-ESP32WROOM-32D) - the microcontroller
 - `U1` - [3.3v Linear Regulator](https://www.mouser.com/ProductDetail/863-NCV8161BSN330T1G) - to power the ESP32 from the 3.7 volt battery input and for a reference voltage for the level shifter. Supports up to 5v input so you can also power the board over USB for debugging (although that might damage this component due to inadequate heat dissipation).
 - `U2` - [1.8v Linear Regulator](https://www.mouser.com/ProductDetail/863-NCV8164ASN180T1G) - generates the reference voltage for the level shifter
 - `U3` - [I2S Level Shifter](https://www.mouser.com/ProductDetail/595-TXU0104PWR) - shifts the I2S signals from the iPod (1.8v) to the ESP32 (3.3v)
 - `C1`, `C10`, `C11` - [0.1uF 0402 Capacitor](https://www.mouser.com/ProductDetail/81-GCM155R71C104KA5D) - various decoupling capacitors
 - `C2`, `C3`, `C4`, `C5`, `C6`, `C7`, `C8`, `C9` - [10uF 0402 Capacitor](https://www.mouser.com/ProductDetail/81-GRM155R60J106ME5D) - various decoupling capacitors
 - `R1` - [10k 0402 Resistor](https://www.mouser.com/ProductDetail/755-SFR01MZPF1002) - pullup resistor for the reset pin

## How to Purchase the Board
I used JLCPCB to fabricate my boards. Nothing special other than being a 4-layer 0.8mm board. You can specify color if you're feeling spicy. You also need a stencil of the top layer. It's not necessary for you to have a bottom layer stencil. There are two footprints on the bottom of the board for 0603 capacitors just in case I (or you) wanted to add extra capacitance to the power planes). I didn't end up using them, but they're there if you want to.

## How to Assemble (the easy way)
1. Get sand from your local beach or sand store.
1. Cook the sand so it's dry.
1. Use the SMT Stencil and smear solder paste on it.
1. Place the components carefully. It's somewhat forgiving, but still try and be accurate.
1. Carefully place the board on the pan full of sand.
1. Heat it until the solder paste melts.
1. Let it cool before removing it.

## How to Examine
Look it over with a magnifiying glass. You might get some solder bridges on the level shifter chip but some flux and a dab with the iron should get rid of those.

## How to Test
Simply uploading software to the board is enough to test it. But if you want to make sure everything works the way it should:

1. Put >3.3v on VCC and connect it to ground.
1. Is the 3.3v regulator generating out 3.3v?
1. Is the 1.8v regulator generating out 1.8v?
1. Is the ESP32's enable pin pulled high?
1. When you apply power, is there activity on the Tx pin?
