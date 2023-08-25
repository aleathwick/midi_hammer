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
Adafruit_MCP3008 adcs[3];
int adcCount = 3;
int adcSelects[] = { 26, 22, 17 };

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

elapsedMillis testAdcTimer;
int testFunction() {
  // test function for getting key position
  return (int)(sin(testAdcTimer / (float)300) * 512) + 512;
}


// const int n_keys = 24;

// KeyHammer keys[] = { { []() -> int { return adcs[0].readADC(7); }, 48, 'h',390,50, 0.6, 8500},
//                     { []() -> int { return adcs[0].readADC(6); }, 49, 'h', 320, 50 , 0.6, 8500},//allegro324
//                     { []() -> int { return adcs[0].readADC(5); }, 50, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[0].readADC(4); }, 51, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[0].readADC(3); }, 52, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[0].readADC(2); }, 53, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[0].readADC(1); }, 54, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[0].readADC(0); }, 55, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[1].readADC(7); }, 56, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[1].readADC(6); }, 57, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[1].readADC(5); }, 58, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[1].readADC(4); }, 59, 'h', 320, 50 , 0.6, 8500},//allegro324
//                     { []() -> int { return adcs[1].readADC(3); }, 60, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[1].readADC(2); }, 61, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[1].readADC(1); }, 62, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[1].readADC(0); }, 63, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[2].readADC(7); }, 64, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[2].readADC(6); }, 65, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[2].readADC(5); }, 66, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[2].readADC(4); }, 67, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[2].readADC(3); }, 68, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[2].readADC(2); }, 69, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[2].readADC(1); }, 70, 'h', 390, 50 , 0.6, 8500},
//                     { []() -> int { return adcs[2].readADC(0); }, 71, 'h', 390, 50 , 0.6, 8500}
//                   };

const int n_keys = 7;

KeyHammer keys[] = { { []() -> int { return adcs[2].readADC(0); }, 48, 'h',1000,50, 0.6, 8500},
                    { []() -> int { return adcs[2].readADC(1); }, 49, 'h', 1000, 50 , 0.6, 8500},
                    { []() -> int { return adcs[2].readADC(2); }, 50, 'h', 1000, 50 , 0.6, 8500},
                    { []() -> int { return adcs[2].readADC(3); }, 51, 'h', 1000, 50 , 0.6, 8500},
                    { []() -> int { return adcs[2].readADC(4); }, 52, 'h', 1000, 50 , 0.6, 8500},
                    { []() -> int { return adcs[2].readADC(5); }, 53, 'h', 1000, 50 , 0.6, 8500},
                    { []() -> int { return adcs[2].readADC(6); }, 54, 'h', 1000, 50 , 0.6, 8500}
                  };

int printkey = 0;
void increment_printkey () {
  // Serial.print("Interrupt ");
  // Serial.print(y++);
  // Serial.println();
  delay(15);
  if (digitalRead(1) == 0) {
    printkey = (printkey + 1) % n_keys;
  }
}
void decrement_printkey () {
  // Serial.print("Interrupt ");
  // Serial.print(y++);
  // Serial.println();
  delay(15);
  if (digitalRead(2) == 0) {
    printkey = (printkey - 1) % n_keys;
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
  pinMode(1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(1), increment_printkey, CHANGE);
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), decrement_printkey, CHANGE);

  usb_midi.setStringDescriptor("Laser Piano");

  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);

  // wait until device mounted
  while (!USBDevice.mounted()) delay(1);
}

// can do setup on the other core too
// void setup1() {

// }

void loop() {
  if (printTimer > 100) {
    printInfo = true;
  }

  if (keys[0].elapsed >= 1250) {
    for (int i = 0; i < n_keys; i++) {
      if (printInfo & i == printkey) {
        keys[i].printState();
      }
      keys[i].step();

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