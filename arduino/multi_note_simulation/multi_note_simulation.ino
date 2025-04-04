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
  const int addressPins[] = {18, 17, 16};
  const int enablePins[] = {20};
  int signalPins[] = {19, 27}; // A1 / GP27

#elif defined(TEENSY)
  const int addressPins[] = {35, 34, 33};
  const int enablePins[] = {39};
  int signalPins[] = {40, 15}; // A1
  ADC *adc = new ADC(); // adc object;

  #endif

int nEnable = sizeof(enablePins) / sizeof(enablePins[0]);

// function pointer type for ADC reading functions
typedef int (*ReadAdcFn)();

// function to update the states of the address and enable pins
void updateMuxAddress(int enableI, int address_0, int address_1, int address_2) {
  // update address pins
  digitalWrite(addressPins[0], address_0);
  digitalWrite(addressPins[1], address_1);
  digitalWrite(addressPins[2], address_2);
  
  // Update enable pins
  for (int i = 0; i < nEnable; i++) {
      digitalWrite(enablePins[i], i == enableI ? LOW : HIGH);
  }
}

// Function to read ADC value for a specific configuration
int readAdc(int enable_i, int address_0, int address_1, int address_2) {
  updateMuxAddress(enable_i, address_0, address_1, address_2);
  // sleep for a bit to allow the mux to settle
  // 1us is enough over 30cm long (26awg) cables and 5v powered 74hc4051, 3v powered 49e sensors
  // 2us needed for 3v powered hc4051
  // 5 or 6us needed for 3v powered 74hc4051 with 60cm long cables
  delayMicroseconds(1);
  // delayNanoseconds only works on teensy
  // delayNanoseconds(1000);
  return analogRead(signalPin);
}

// can turn to int like so: int micros = elapsed[i][j];
// and reset to zero: elapsed[i][j] = 0;
elapsedMillis infoTimerMS;
elapsedMicros loopTimerUS;


// whether or not to print in the current loop
bool printInfo = true;
// whether to use print statements for arduino serial plotter; if false, print text and disregard serial plotter formatting
const bool plotSerial = true;
// used to restrict printing to only a few iterations after certain events
elapsedMillis printTimerMS = 0;

elapsedMillis testAdcTimerMS;
int testFunction() {
  // test function for getting key position
  return (int)(sin(testAdcTimerMS / (float)300) * 512) + 512;
}

const int n_keys = 2;
KeyHammer keys[] = {
  { []() -> int { return readAdc(0, 0, 0, 0); }, &midiSender, 49, 'h', 1000, 50 , 0.6, 8500},
  { []() -> int { return readAdc(0, 0, 0, 1); }, &midiSender, 50, 'h', 1000, 50 , 0.6, 8500}
};

int printkey = 0;
void incrementPrintKey () {
  // Serial.print("Interrupt ");
  // Serial.print(y++);
  // Serial.println();
  delay(15);
  if (digitalRead(1) == 0) {
    printkey = (printkey + 1) % n_keys;
  }
}
void decrementPrintKey () {
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
  attachInterrupt(digitalPinToInterrupt(1), incrementPrintKey, CHANGE);
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), decrementPrintKey, CHANGE);

  midiSender.initialize();

  // ADC settings
  #ifdef TEENSY
    // default is averaging 4 samples, which takes 16-17us
    // takes around 7us for 1 sample
    analogReadAveraging(1);
    // analogReadResolution(12);
  #endif
}

// can do setup on the other core too
// void setup1() {

// }

void loop() {
  if (printTimerMS > 100) {
    printInfo = true;
  }

  if (keys[0].elapsedUS >= 1250) {
    for (int i = 0; i < n_keys; i++) {
      if (printInfo & i == printkey) {
        keys[i].printState();
        Serial.flush();
      }
      keys[i].step();

    }

    if (printInfo) {
      Serial.print('\n');
      printInfo = false;
      printTimerMS = 0;
    }
    
    loopTimerUS = 0;
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