# midi_hammer
This repository contains firmware for my DIY digital piano project. Some videos of prototypes can be found here:  
- [Latest prototype (5 keys)](https://youtu.be/U8PAwi5l6Sw)
- [24 key prototype](https://youtu.be/tgWXtYCHDI4) - an older prototype that used more time-intensive construction techniques.

## Overview
Most of my prototypes so far use a simple lever action (like a see-saw - a weight on one end, finger on the other). Key positions are measured by hall effect sensors connected to a microcontroller (raspberry pico or teensy). To allow for half-presses etc., the microntroller firmware simulates a hammer, the position/speed of which is what generates note-ons sent over MIDI to a computer running virtual piano software like pianoteq.  

My original goal was to create a digital piano with narrower keys (i.e. smaller octave). Since experimenting with simple lever actions, which of course have very different properties to 'real' hammer actions (most importantly, different leverage and as a result lower inertia / dynamic touchweight), I've become interested in changing the other properties of the piano action to suit my liking, and now my goal is to create an instrument that differs from the piano in the following regards:  
- Narrower keys
- Less inertia
- Shallower key dip  

## Electronics
Originally this project used the raspberry pico, reading MCP3008 external ADCs. Since then I've transitioned to using the teensy 4.1, using the teensy's internal ADC with multiplexers, which is a lot faster (i.e. I can process all keys faster, working out to <200us for 88 keys).  

## Code
Arduino code is arranged as follows:
- `arduino/multi_note_simulation` - main firmware (currently for teensy, rpico support is broken right now), which contains a convenient serial command interface for controlling printing, calibration, and writing calibrated parameters to the SD card. Other funcationality is achieved utilising the below classes.  
- `arduino/src` contains various classes:
  - `DualAdcManager` - Abstracts the logic for automatically utilising the teensy's dual ADC's simultaneously whenever possible.
  - `KeyHammer` - Contains the logic for simulating a hammer action based on key positions. 
  - `MidiSender` - Abstract base class used by `MidiSenderPico` and `MidiSenderTeensy`, to provide a consistent interface to  MIDI communication.
  - `ParamHandler` - Handles storing parameters on an SD card, so that once keys are calibrated, the calibrated parameters can be re-used after power-cycling.
  - `Pedal` - subclass of `KeyHammer` for use with pedals. 

## Notes to self
### Arduino plotting
See here for a good guide: https://diyrobocars.com/2020/05/04/arduino-serial-plotter-the-missing-manual/  

### Arduino CLI
#### Installation
```sh
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
sudo mv bin/arduino-cli /usr/local/bin/arduino-cli
# alias for arduino-cli
echo "alias ac=arduino-cli" >> ~/.bashrc
# restart bash
source ~/.bashrc
# initialize config file
# for me the default location is: /home/ubuntu/.arduino15/arduino-cli.yaml
arduino-cli config init

```
#### Adding boards
Add board URLs by editing the config file:
```yaml
board_manager:
  additional_urls:
    - https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
    - https://www.pjrc.com/teensy/package_teensy_index.json
```
Or by running:
```sh
arduino-cli config add board_manager.additional_urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
arduino-cli config add board_manager.additional_urls https://www.pjrc.com/teensy/package_teensy_index.json
```

Then update the boards index and install cores:
```sh
# can search for available cores: arduino-cli core search teensy
arduino-cli core update-index
arduino-cli core install rp2040:rp2040
arduino-cli core install teensy:avr
```

#### Install required libraries
```sh
arduino-cli lib install "Adafruit MCP3008"
arduino-cli lib install "MCP3208"
# https://github.com/FortySevenEffects/arduino_midi_library
# by francois best, lathoub
# MIDI.h
arduino-cli lib install "MIDI Library"
# https://github.com/pfeerick/elapsedMillis/wiki
# by Paul Stoffregen
arduino-cli lib install "elapsedMillis"
# https://github.com/rlogiacco/CircularBuffer
arduino-cli lib install "CircularBuffer"
# https://github.com/akkoyun/Statistical
arduino-cli lib install "Statistical"
# https://github.com/thomasfredericks/Bounce2
arduino-cli lib install "Bounce2"
# https://github.com/kroimon/Arduino-SerialCommand/tree/master
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --git-url https://github.com/kroimon/Arduino-SerialCommand.git
# https://github.com/michalmonday/CSV-Parser-for-Arduino
arduino-cli lib install "CSV Parser"
```

#### Compilation
Can specify build properties, but it seems some of these can be appended to the end of the FQBN all of which can be viewed with:
```sh
arduino-cli compile -b rp2040:rp2040:rpipico --show-properties
```

Compiling for rpico:
```sh
# only this is needed for tinyusb with rpico:
# (alternatively, can specify rpipicow, rpipico2, or rpipico2w)
arduino-cli compile -b rp2040:rp2040:rpipico2:usbstack=tinyusb --library ../src
# originally I thought this would be needed, but options like usbstack and overclocking seem to be specified as part of the fqbn instead:
arduino-cli compile -b rp2040:rp2040:rpipico --build-property "menu.usbstack.tinyusb=\"Adafruit TinyUSB\"" --build-property "menu.usbstack.tinyusb.build.usbstack_flags=-DUSE_TINYUSB \"-I/home/ubuntu/.arduino15/packages/rp2040/hardware/rp2040/4.4.0/libraries/Adafruit_TinyUSB_Arduino/src/arduino\""
```

Compiling for teensy 4.1:
```sh
arduino-cli compile -b teensy:avr:teensy41:usb=serialmidi
```

Create aliases for the above commands:
```sh
echo alias compile_pico=\"arduino-cli compile -b rp2040:rp2040:rpipico:usbstack=tinyusb --library ../src\" >> ~/.bashrc
echo alias compile_picow=\'arduino-cli compile -b rp2040:rp2040:rpipicow:usbstack=tinyusb --library ../src\" >> ~/.bashrc
echo alias compile_pico2=\"arduino-cli compile -b rp2040:rp2040:rpipico2:usbstack=tinyusb --library ../src\" >> ~/.bashrc
echo alias compile_teensy=\"arduino-cli compile -b teensy:avr:teensy41:usb=serialmidi --library ../src\" >> ~/.bashrc
```

To view the FQBN used by the arduino IDE, set verbose to true and compile. This is where I found options specified in the FQBN like tinyusb in this: rp2040:rp2040:rpipico2:usbstack=tinyusb

#### Updating libraries
```sh
arduino-cli lib upgrade
arduino-cli core upgrade
```

### to do
- Arduino code:
  - simulation:
    - specify min key/hammer speed for midi velocity of 0 (currently only max speed for midi velocity of 127 is specified)
    - perhaps experiment with key speed method for velocity generation? Implemented but not used
  - add calibration of max key/hammer speed for midi velocity of 127 (might not per-key calibration when using PCBs)
  - interactive serial control
    - enable/disable keys
    - change simulation params for specific keys / all keys? 
    - change length of loop? i.e. minimum time for one iteration of all keys
  - pedal
    - add binary version of pedal

