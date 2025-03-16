# midi_hammer
Raspberry pico firmware for measuring piano key positions, using those measurements to simulate hammer movements, and sending resultant velocity information over usb midi.  
Videos of prototypes can be found here:  
- [Latest prototype (5 keys)](https://youtu.be/U8PAwi5l6Sw)  
- [24 key prototype](https://youtu.be/tgWXtYCHDI4)

## Arduino
Uses [Earle Philhower's Raspberry Pico Arduino core](https://github.com/earlephilhower/arduino-pico), using Adafruit TinyUSB for usb midi.  
There is code for:
- Reading sensors from multiple mcp3008 or mcp3208
- Sending notes over usb midi  

The KeyHammer class encapsulates the logic required to run simulation. It can run in two modes: hammer or pedal. In hammer mode, a hammer is simulated in response to ADC values which are assumed to come from a key (or a foot pedal, for triggering drums). In pedal mode, values are read from ADC and directly turned into midi values, useful for controlling sustain pedal or other parameters.  
The latest firmware for powering a digital piano sits in `arduino/multi_note_simulation`.  
Pedal firmware sits in `arduino/pedal_unit`.  

## Circuit Python
This was the initial prototype. There is working code for reading two sensors via mcp3008, and sending resultant velocities over usb midi.  


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
    - wait until key goes below certain speed before note on
    - don't simulate hammer when key is not armed, and when armed again reset to key pos/speed
    - refactor: simplify handling of sensorFullyOn < sensorFullyOff (flip signs, as in circuitpy code)
    - specify min / max key speed in terms of mm/s key travel
    - fix for behaviour of gravity (speed update should be based on mean of last and current speeds)
  - filtering
    - share filter values between keys
    - choose filter based on total elapsed time target? E.g. 3000us.
  - interactive serial control
  - calibration
  - store params on SD card
  - Simultaneous dual ADC read (teensy)
  - pedal
    - sub class for pedal mode, rather than one class with different modes?
    - add binary version of pedal

