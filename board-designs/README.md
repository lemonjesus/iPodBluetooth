# The iPod Bluetooth Board

## About
TODO

## How to Purchase
TODO. Use JLCPCB

## How to Assemble (the easy way)
TODO
1. Get Sand
1. Cook sand so it's dry
1. Use the SMT Stencil and smear solder paste on it
1. Place the components carefully. It's forgiving, but still try and be accurate.
1. Carefully place the board on the pan full of sand
1. Heat it until the solder paste melts
1. Let it cool before removing it

## How to Examine
TODO

Look it over with a magnifiying glass. You might get some solder bridges on the level shifter chip but some flux and a dab with the iron should get rid of those.

## How to Test
Simply uploading software to the board is enough to test it. But if you want to make sure everything works the way it should:

1. Put >3.3v on VCC and connect it to ground.
1. Is the 3.3v regulator pumping out 3.3v?
1. Is the 1.8v regulator pumping out 1.8v?
1. Is the ESP32's enable pin pulled high?
1. When you apply power, is there activity on the Tx pin?
