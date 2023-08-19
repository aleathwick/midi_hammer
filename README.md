# midi_hammer
Raspberry pico firmware for measuring piano key positions, using those measurements to simulate hammer movements, and sending resultant velocity information over usb midi.  
See a demo here:  
https://youtu.be/tgWXtYCHDI4

### Arduino
Uses [Earle Philhower's Raspberry Pico Arduino core](https://github.com/earlephilhower/arduino-pico), using Adafruit TinyUSB for usb midi.  
There is code for:
- Reading sensors from multiple mcp3008 or mcp3208
- Sending notes over usb midi  

The KeyHammer class encapsulates the logic required to run simulation. It can run in two modes: hammer or pedal. In hammer mode, a hammer is simulated in response to ADC values which are assumed to come from a key (or a foot pedal, for triggering drums). In pedal mode, values are read from ADC and directly turned into midi values, useful for controlling sustain pedal or other parameters.  
The latest firmware for powering a digital piano sits in `arduino/multi_note_simulation`.  
Pedal firmware sits in `arduino/pedal_unit`.  

### Circuit Python
This was the initial prototype. There is working code for reading two sensors via mcp3008, and sending resultant velocities over usb midi.  


## Notes to self
### Arduino plotting
See here for a good guide: https://diyrobocars.com/2020/05/04/arduino-serial-plotter-the-missing-manual/  

### to do
- Add initial elapsed time for understanding how much spare time each loop there is.
- Compare pico with ESP32 for speed.
- Vectorize code for speed.
- Test simple averaging of multiple adc speeds to reduce hall sensor noise.
- Calibration code
- Add features to arduino code
    - fix for behaviour of gravity (speed update should be based on mean of last and current speeds)
    - simplify handling of sensorFullyOn < sensorFullyOff (flip signs, as in circuitpy code)
    - sub class for pedal mode, rather than one class with different modes?
    - add binary version of pedal

