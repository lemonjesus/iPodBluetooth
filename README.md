# iPod Bluetooth Board, Firmware, and Software

## [Watch The Video First](https://youtu.be/AVvXbqsTUnk)
### [Featured on Hackaday!](https://hackaday.com/2022/09/14/esp32-adds-bluetooth-to-an-ipod-nano/)

I've been working on this project for a while because I thought it would be simple. Feature creep and lack of skill have lead me on a year long journey down a rabbit hole of things I didn't expect to learn about. This has been fun (by some definition) and I hope someone somewhere finds use for this.

## Aren't there already Bluetooth transmitters for the iPod?
Sort of. There aren't any fully digital solutions specifically for the iPod, and as far as I can tell none that are suited for use in an iPod Nano. I attempted to change that. I got close to an OK solution I think.

## Features
* **A completely digital audio pipeline!** The audio signal never goes through an ADC or DAC until it reaches your headphones!
* **The ability to pick what device to connect to from the iPod on boot!**
* **The ability to control the iPod from your Bluetooth speaker/headset** (play, pause, next, prev, etc.)
* Widely available components with an easy to assemble board (despite using 0402 passives)
* Uses the Arduino environment so it's easy(ish) to program and you don't need the ESP-IDF
* No Dongles! (just incredibly complicated soldering)

## Design Constraints 
_(or, justification as to why it's designed the way that it is)_

I set out to make something to fit into a 3rd Generation iPod Nano. I'm not sure if this is actually possible. I toyed with the idea of using a smaller battery and using the space left behind to house the board. But I think the power and size constraints already prevent this from working. The board design I ended up with in v0.4 (the version as of right now) is thicker than it needs to be. Once I get comfortable with using other chipsets, it will likely be less invasive.

I also attempted to design this board with highly available components with low entry-to-use. To that end, it is built around an ESP32 Module. This is likely not the most elegant way to do it, but they're readily available, have decent documentation, and I was able to program this with Arduino software which many people are already comfortable with. I hope this lowers the difficulty, because the soldering you have to do to make this work is kinda insane.

## How to Use
Each folder contains more detailed instructions and troubleshooting steps on how to work with that specific step, but it essentially goes like this:

### Step 0: Requirements
* An iPod you're willing to destroy (it might go wrong, fair warning)
* Really thin enamel wire (I use 30AWG)
* Lots of flux. I'm serious. And a god-like soldering skill.
* An FTDI cable or something that can do the same thing
* The parts listed in Step 1 (manufactured board, the parts, an SMT Stencil, solder paste)
* An iPod 30-pin male breakout board you can get from [here](https://elabbay.myshopify.com/products/apple-30m-bo-v1ac-apple-30-pin-male-plug-breakout-board-compact-type). This is optional, but it will allow you to reprogram the board while it's in the iPod. See Step 3.

### Step 1: Assemble (`board-designs`)
The first step is to manufactuer the board using the Gerber files in `board-designs`. Zip them up and upload them to your favorite board house. I've used JLCPCB for this, but I'm sure anyone will be happy to make these provided they can work with the small traces I've used throughout the board. I also use as thin of a board as I can without it being really expensive.

> **Note**
> Be sure to order an SMT stencil of the front side when you do this (unless you feel really, really confortable doing 0402 components by hand).

I've been ordering my parts from Mouser. The BOM is for one board (with some additional capacitors because they're really easy to lose). **You'll also need some solder paste if you're going to be soldering like me (the easy way).**

Next, you have to assemble the board. Watch [my video](https://youtu.be/AVvXbqsTUnk) for the easy way to do this. It involves sand and your stovetop. There are more details in the folder as well as a visual guide in the video linked above.

After you've done that, do a visual inspection. There might be some solder bridges, especially around the level shifter chip. A bit of flux and heat from your soldering iron will fix those.

See the detailed instructions in [`board-designs`](./board-designs/).

### Step 2: Install the Board (`installing`)
**This is by far the most complicated step of this process.** It's so complicated that I needed a friend with a microscope to do it. This is the barrier of entry to this project. If you can't do it and you don't have a friend who can do it, then you might be better off not doing this project. I'm sorry it's so hard, but this is the only way I could preserve an all-digital data path. One day I might make a version of this board that can take analog audio. If you want to do this, feel free to submit a PR.

There are detailed instructions on how to do this in the [`installing`](./installing) folder. I hope you have a lot of flux. And patience.

### Step 3: Program the Board (`board-firmware`)
Now that the board is installed, you can now program it through the 30-pin breakout board. You can, of course, program the board before it goes into the iPod and skip the 30-pin breakout board, but then it'll be exceptionally hard to reprogram it without disconnecting power.

This part requires Arduino and some custom modifications to some libraries. See the detailed instructions in [`board-firmware`](./board-firmware) on how to hook up the board to your FTDI and how to make it work.

### Step 4: Program the iPod (`ipod-software`)
Now you're in the home stretch. It's time to build the software for this. I only provide software for the Nano 3rd Generation, so if you want to make it work on other iPods (or with Rockbox), you're going to have to port the code yourself. I think the easiest one to port it to from here is the iPod Classic since they share the same architecture as the Nano 3G.

See the detailed instructions in [`ipod-software`](./ipod-software). I recommend you have a Linux VM handy because building Rockbox on Windows sounds like a nightmare.

## Caveats
I want to be very clear here: this is not intended as an easy, ready to go, or even 'good' solution. This is a novelty, a **proof of concept**.

* The power draw is enormous and the battery doesn't last very long. It's a small battery and a power hungry ESP32.
* I only really made this for the N3G. If you want it on another iPod, good luck. See the next section for how you might go about doing that.
* It's somewhat unstable. Sometimes I have to reset the ESP32 because it won't respond to the `reset` command. I'm not sure why this is.
* There's no mechanism to put the ESP32 to sleep, so it will drain the iPod battery dry.

## Portability
This project in its current state only really needs three things: power (3.3v to 5.5v), I2S data, and a serial connection to the iPod. 

The iPod software is only used for configuring the board on startup if you want to change what the iPod is connected to. You can hardcode the name of the thing you want to connect to and not even do this step.

If you want to port the iPod software to another iPod, it'll be somewhat easy so long as Rockbox already supports it. The software is based on the Rockbox bootloader only, so you don't need to port a lot.

## Future Work
* I'd like to use a bare ESP32 chip to reduce size, cost, and power draw. But I'm not familiar with how to do that.
  * To this end, I bought a nRF52840 dev board that I might start playing around with and see if it can do this with less power (and more features?)
* Make an analog version of this board that's easier to solder and the potential cost of quality.
* Make the code more resilient.
* Do something about the power draw. And the ability to turn it off (and, more importantly, back on).
