// works with raspberry pico, using Earle Philhower's Raspberry Pico Arduino core:
// https://github.com/earlephilhower/arduino-pico
// needs Adafruit TinyUSB for usb midi
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_MCP3008.h>
// mcp3208 library: 
#include <MCP3208.h>
// MIDI Library: https://github.com/FortySevenEffects/arduino_midi_library
#include <MIDI.h>
// elapsedMillis: https://github.com/pfeerick/elapsedMillis
#include <elapsedMillis.h>
// CircularBuffer: https://github.com/rlogiacco/CircularBuffer
#include <CircularBuffer.h>
// for log
#include <math.h>
// #include <gram_savitzky_golay.h>

// to do:
// try filtering 
// circular buffer - store previous values



// ADCs
// MCP3208 adcs[2];
Adafruit_MCP3008 adcs[2];
int adcCount = 2;
int adcSelects[] = { 22, 17 };

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);





//// circular buffer
// const int buffer_len = 25;
// CircularBuffer<int, buffer_len> rawAdcBuffers[adcCount][8];
// CircularBuffer<double, buffer_len> keySpeedBuffers[adcCount][8];

//// savitzky golay
// // Window size is 2*m+1
// const size_t m = 12;
// // Polynomial Order
// const size_t n = 1;
// // Initial Point Smoothing (ie evaluate polynomial at first point in the window)
// // Points are defined in range [-m;m]
// const size_t t = m;
// // Derivation order? 0: no derivation, 1: first derivative, 2: second derivative...
// const int d = 0;

// Real-time filter (filtering at latest data point)
// gram_sg::SavitzkyGolayFilter filter(m, t, n, d);


// can turn to int like so: int micros = elapsed[i][j];
// and reset to zero: elapsed[i][j] = 0;
elapsedMillis infoTimer;
elapsedMicros loopTimer;


// whether or not to print in the current loop
bool printInfo = true;
// whether to use print statements for arduino serial plotter; if false, print text and disregard serial plotter formatting
const bool plotSerial = true;
// used to restrict printing to only a few iterations after certain events
int printTime = 0;

//////////////////////////////////////////////////////////////
// https://cplusplus.com/doc/tutorial/classes/
// https://paulmurraycbr.github.io/ArduinoTheOOWay.html
// https://www.circuitbasics.com/programming-with-classes-and-objects-on-the-arduino/
// #include <Adafruit_MCP3008.h>
// #include <elapsedMillis.h>

elapsedMillis testAdcTimer;
int testFunction() {
  // test function for getting key position
  return (int)(sin(testAdcTimer / (double)300) * 512) + 512;
}


const int n_keys = 1;
// KeyHammer keys[] = { { adcs[0], 4, 60 },
//                        { adcs[0], 3, 61 },
//                        { adcs[0], 2, 62 },
//                        { adcs[0], 1, 63 },
//                        { adcs[0], 0, 64 },
//                        { adcs[1], 7, 65 },
//                        { adcs[1], 6, 66 },
//                        { adcs[1], 5, 67 },
//                        { adcs[1], 4, 68 },
//                        { adcs[1], 3, 69 },
//                        { adcs[1], 2, 70 },
//                        { adcs[1], 1, 71 },
//                        { adcs[1], 0, 72 }};

KeyHammer keys[] = { { testFunction, 7, 36, 'p', 1010, 0 }};

void change_mode () {
  // Serial.print("Interrupt ");
  // Serial.print(y++);
  // Serial.println();
  if (digitalRead(28) == 1) {
    keys[0].operationMode = 'h';
  } else {
    keys[0].operationMode = 'p';
  }
}


void setup() {
  Serial.begin(57600);
  // begin ADC
  // can set SPI default pins
  //  SPI.setCS(5);
  //  SPI.setSCK(2);
  //  SPI.setTX(3);
  //  SPI.setRX(4);
  for (int i = 0; i < adcCount; i++) {
    adcs[i].begin(adcSelects[i]);
  }

  pinMode(LED_BUILTIN, OUTPUT);

  usb_midi.setStringDescriptor("Laser Piano");

  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);

  pinMode(28, INPUT_PULLUP);
  // can then read this pin like so
  // int sensorVal = digitalRead(28);

  //interrupt for toggling mode 
  attachInterrupt(digitalPinToInterrupt(28), change_mode, CHANGE);

  // wait until device mounted
  while (!USBDevice.mounted()) delay(1);
}

// can do setup on the other core too
// void setup1() {

// }

void loop() {
  
  if (keys[0].elapsed >= 2500) {
    for (int i = 0; i < n_keys; i++) {
      keys[i].step();
    }
    Serial.print('\n');
    loopTimer = 0;
    // read any new MIDI messages
    MIDI.read();
  }
}
  

// void loop1() {
//   // uint32_t rp2040.fifo.pop() will block if the FIFO is empty, there is also a bool rp2040.fifo.pop_nb version
//   Serial.printf("C1: Read value from FIFO: %d\n", rp2040.fifo.pop());
//   // int rp2040.fifo.available() - get number of values available in this core's FIFO

// Just need: index in velocity lookup table
// pitch to be sent
  
// }