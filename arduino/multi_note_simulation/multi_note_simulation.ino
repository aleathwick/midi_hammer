/*
To do:
- Add midisender to KeyHammer
- Basic adc read with teensy, based on cpy thresholds
- Default midisender, and default arguments, modified by subclassing keyhammer? For ease of use
- Tidy up cpp/header files
- Bring in other changes from cpy
- Calibration code
- Benchmark
- SG filter

*/

#include "config.h"

// elapsedMillis: https://github.com/pfeerick/elapsedMillis
#include <elapsedMillis.h>
// CircularBuffer: https://github.com/rlogiacco/CircularBuffer
// for log
#include <math.h>
#include "KeyHammer.h"

// board specific imports and midi setup
#ifdef PICO
  // pico version uses Earle Philhower's Raspberry Pico Arduino core:
  // https://github.com/earlephilhower/arduino-pico
  // tinyUSB is used for USB MIDI, see MidiSenderPico.cpp
  #include "MidiSenderPico.h"
  MidiSenderPico midiSender;
#elif defined(TEENSY)
  #include "MidiSenderTeensy.h"
  MidiSenderTeensy midiSender;
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
  { []() -> int { return readAdc(0, 0, 0, 0); }, &midiSender, 49, 'h', 1000, 50 , 0.6, 8500},
  { []() -> int { return readAdc(0, 0, 0, 1); }, &midiSender, 50, 'h', 1000, 50 , 0.6, 8500}
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

  midiSender.initialize();
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
    // do any loop end actions, such as reading any new MIDI messages
    midiSender.loopEnd();
  }
}
  

// void loop1() {
//   // uint32_t rp2040.fifo.pop() will block if the FIFO is empty, there is also a bool rp2040.fifo.pop_nb version
//   Serial.printf("C1: Read value from FIFO: %d\n", rp2040.fifo.pop());
//   // int rp2040.fifo.available() - get number of values available in this core's FIFO

// Just need: index in velocity lookup table
// pitch to be sent
  
// }