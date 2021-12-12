# midi_hammer
Raspberry pico firmware for measuring piano key positions, using those measurements to simulate hammer movements, and sending resultant velocity information over usb midi.  
### Circuit Python
Working code for reading two sensors via mcp3008, and sending resultant velocities over usb midi.  
### Arduino
Uses [Earle Philhower's Raspberry Pico Arduino core](https://github.com/earlephilhower/arduino-pico), using Adafruit TinyUSB for usb midi.  
There is code for:
- Reading sensors from multiple mcp3008s
- Sending notes over usb midi  
