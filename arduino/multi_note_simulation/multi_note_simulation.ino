// choose the board type
// either PICO, TEENSY
// #define TEENSY
#define PICO

// works with raspberry pico, using Earle Philhower's Raspberry Pico Arduino core:
// https://github.com/earlephilhower/arduino-pico
// needs Adafruit TinyUSB for usb midi
#include <Arduino.h>
// MIDI Library: https://github.com/FortySevenEffects/arduino_midi_library
#include <MIDI.h>
// elapsedMillis: https://github.com/pfeerick/elapsedMillis
#include <elapsedMillis.h>
// CircularBuffer: https://github.com/rlogiacco/CircularBuffer
// for log
#include <math.h>
#include "KeyHammer.h"

// board specific imports and midi setup
#ifdef PICO
  #include <Adafruit_TinyUSB.h>

  // USB MIDI object
  Adafruit_USBD_MIDI usb_midi;

  // Create a new instance of the Arduino MIDI Library,
  // and attach usb_midi as the transport.
  MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);
#endif

//// c4051 setup
#ifdef PICO
  const int address_pins[] = {18, 17, 16};
  const int enable_pins[] = {20, 19};
  int signal_pin = 27; // A1 / GP27

#elif defined(TEENSY)
  const int address_pins[] = {33, 34, 35};
  const int enable_pins[] = {40, 39};
  int signal_pin = 15; // A1
#endif

int n_enable = sizeof(enable_pins) / sizeof(enable_pins[0]);

// function pointer type for ADC reading functions
typedef int (*ReadAdcFn)();

// function to update the states of the address and enable pins
void updateMuxAddress(int enable_i, int address_0, int address_1, int address_2) {
  // update address pins
  digitalWrite(address_pins[0], address_0);
  digitalWrite(address_pins[1], address_1);
  digitalWrite(address_pins[2], address_2);
  
  // Update enable pins
  for (int i = 0; i < n_enable; i++) {
      digitalWrite(enable_pins[i], i == enable_i ? LOW : HIGH);
  }
}

// Function to read ADC value for a specific configuration
int readAdc(int enable_i, int address_0, int address_1, int address_2) {
  updateMuxAddress(enable_i, address_0, address_1, address_2);
  return analogRead(signal_pin);
}

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

const int n_keys = 2;
KeyHammer keys[] = {
  { []() -> int { return readAdc(0, 0, 0, 0); }, 49, 'h', 1000, 50 , 0.6, 8500},
  { []() -> int { return readAdc(0, 0, 0, 1); }, 50, 'h', 1000, 50 , 0.6, 8500}
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
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(1, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(1), increment_printkey, CHANGE);
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), decrement_printkey, CHANGE);

  #ifdef PICO
    usb_midi.setStringDescriptor("Laser Piano");

    // Initialize MIDI, and listen to all MIDI channels
    // This will also call usb_midi's begin()
    MIDI.begin(MIDI_CHANNEL_OMNI);

    // wait until device mounted
    while (!USBDevice.mounted()) delay(1);
  #endif
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
    #ifdef PICO
      MIDI.read();
    #endif
  }
}
  

// void loop1() {
//   // uint32_t rp2040.fifo.pop() will block if the FIFO is empty, there is also a bool rp2040.fifo.pop_nb version
//   Serial.printf("C1: Read value from FIFO: %d\n", rp2040.fifo.pop());
//   // int rp2040.fifo.available() - get number of values available in this core's FIFO

// Just need: index in velocity lookup table
// pitch to be sent
  
// }