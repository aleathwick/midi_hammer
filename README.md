# midi_hammer
Raspberry pico firmware for measuring piano key positions, using those measurements to simulate hammer movements, and sending resultant velocity information over usb midi.  
Currently works for one key with a CNY70 sensor connected to analog 0 pin on a pico. The intention is to expand this to multiple picos with multiple keys per pico (maybe with an mcp3008 for each pico).
