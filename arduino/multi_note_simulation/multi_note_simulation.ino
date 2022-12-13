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
// for log
#include <math.h>
#include "KeyHammer.h"



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




// can turn to int like so: int micros = elapsed[i][j];
// and reset to zero: elapsed[i][j] = 0;
elapsedMillis infoTimer;
elapsedMicros loopTimer;


// whether or not to print in the current loop
bool printInfo = true;
// whether to use print statements for arduino serial plotter; if false, print text and disregard serial plotter formatting
const bool plotSerial = true;
// used to restrict printing to only a few iterations after certain events
elapsedMillis printTimer = 0;

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

KeyHammer keys[] = { { []() -> int { return analogRead(28); }, 7, 36, 'p', 1010, 0 },
                    { []() -> int { return analogRead(27); }, 7, 36, 'p', 1010, 0 },
                    { []() -> int { return analogRead(26); }, 7, 36, 'p', 1010, 0 },
                  };

void change_mode () {
  // Serial.print("Interrupt ");
  // Serial.print(y++);
  // Serial.println();
  if (digitalRead(15) == 1) {
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

  pinMode(15, INPUT_PULLUP);
  // can then read this pin like so
  // int sensorVal = digitalRead(28);

  //interrupt for toggling mode 
  attachInterrupt(digitalPinToInterrupt(15), change_mode, CHANGE);

  // wait until device mounted
  while (!USBDevice.mounted()) delay(1);
}

// can do setup on the other core too
// void setup1() {

// }

void loop() {
  if (printTimer > 50) {
    printInfo = true;
  }

  if (keys[0].elapsed >= 2500) {
    for (int i = 0; i < n_keys; i++) {
      keys[i].step();

      if (printInfo) {
        keys[i].printState();
      }
    }

    if (printInfo) {
      Serial.print('\n');
      printInfo = false;
    }
    
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