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
  return (int)(sin(testAdcTimer / (double)300) * 512) + 512;
}


const int n_keys = 3;

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

  keys[1].controlNumber = 11; // expression
  keys[2].controlNumber = 67; // una corda

  pinMode(LED_BUILTIN, OUTPUT);

  usb_midi.setStringDescriptor("Pedal Unit");

  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);

  pinMode(15, INPUT_PULLUP);
  // can then read this pin like so
  // int sensorVal = digitalRead(28);

  //interrupt for toggling mode 
  //attachInterrupt(digitalPinToInterrupt(15), change_mode, CHANGE);

  // wait until device mounted
  while (!USBDevice.mounted()) delay(1);
}

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