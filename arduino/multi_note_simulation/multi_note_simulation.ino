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
#include <SerialCommand.h>
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
  int calibrationPin = 0;

#elif defined(TEENSY)
  const int addressPins[] = {35, 34, 33};
  const int enablePins[] = {39};
  int signalPins[] = {40, 15}; // A1
  int calibrationPin = 14;
  
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
bool printInfo = false;
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
bool printAllKeys = false;

Bounce2::Button b_toggle_calibration = Bounce2::Button();

SerialCommand sCmd;


void setup() {
  Serial.begin(57600);
  pinMode(LED_BUILTIN, OUTPUT);

  b_toggle_calibration.attach( calibrationPin, INPUT_PULLUP );
  b_toggle_calibration.interval(5);
  b_toggle_calibration.setPressedState(LOW); 

  sCmd.addCommand("c", toggleCalibration);
  sCmd.addCommand("pk", setPrintKey);
  sCmd.addCommand("pka", togglePrintAttributes);
  sCmd.setDefaultHandler(unrecognizedCmd);

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

// function for toggling calibration for all keys
void toggleCalibration () {
  for (int i = 0; i < n_keys; i++) {
    keys[i].toggleCalibration();
  }
}

//// sCmd commands

// function to set printkey, based on a key number
void setPrintKey (const char *command) {
  int key;
  char *arg;

  arg = sCmd.next();
  if (arg != NULL) {
    // check if the argument is 'a'
    if (strcmp(arg, "a") == 0) {
      printAllKeys = true;
      return;
    }

    // atoi vs atol:
    // atoi: convert string to int
    // atol: convert string to long
    key = atoi(arg);
    Serial.print("Second argument was: ");
    Serial.println(key);
    printkey = key % n_keys;
    printAllKeys = false;
  }
  else {
    Serial.println("No second argument, required by command: ");
    Serial.println(command);

  }


}

// key attributes for printing
enum KeyAttributes {
  RAW_ADC = 0,
  KEY_POSITION,
  KEY_SPEED,
  HAMMER_POSITION,
  HAMMER_SPEED,
  ELAPSED,
  NUM_ATTRIBUTES // Total number of attributes
};

// which attributes to print
bool attributeStates[NUM_ATTRIBUTES] = {true, true, true, true, true, true}; // All attributes enabled by default

// function to toggle printing of attributes
void togglePrintAttributes(const char *command) {
  char *arg = sCmd.next();
  if (arg != NULL) {
    if (strcmp(arg, "all") == 0) {
      for (int i = 0; i < NUM_ATTRIBUTES; i++) {
        attributeStates[i] = true;
      }
    } else if (strcmp(arg, "none") == 0) {
      for (int i = 0; i < NUM_ATTRIBUTES; i++) {
        attributeStates[i] = false;
      }
    } else if (strcmp(arg, "adc") == 0) {
        attributeStates[RAW_ADC] = !attributeStates[RAW_ADC];
    } else if (strcmp(arg, "kp") == 0) {
        attributeStates[KEY_POSITION] = !attributeStates[KEY_POSITION];
    } else if (strcmp(arg, "ks") == 0) {
        attributeStates[KEY_SPEED] = !attributeStates[KEY_SPEED];
    } else if (strcmp(arg, "hp") == 0) {
        attributeStates[HAMMER_POSITION] = !attributeStates[HAMMER_POSITION];
    } else if (strcmp(arg, "hs") == 0) {
        attributeStates[HAMMER_SPEED] = !attributeStates[HAMMER_SPEED];
    } else if (strcmp(arg, "el") == 0) {
        attributeStates[ELAPSED] = !attributeStates[ELAPSED];
    } else if (isdigit(arg[0])) {
      // Toggle a specific attribute by index
      int attrIndex = atoi(arg);
      if (attrIndex >= 0 && attrIndex < NUM_ATTRIBUTES) {
        attributeStates[attrIndex] = !attributeStates[attrIndex];
      } else {
        Serial.println("Invalid attribute index.");
      }
    } else {
      Serial.println("Invalid argument. Use 'all', 'none', or an attribute index / shorthand.");
    }
  } else {
    Serial.println("No argument provided. Use 'all', 'none', or an attribute index / shorthand.");
  }
}

// function to print the state of a key
void printKeyState(int i) {
  if (attributeStates[RAW_ADC]) {
    Serial.printf("rawADC_%d:%d,", keys[i].pitch, keys[i].getRawADC());
  }
  if (attributeStates[KEY_POSITION]) {
    Serial.printf("keyPos_%d:%f,", keys[i].pitch, keys[i].getKeyPosition());
  }
  if (attributeStates[KEY_SPEED]) {
    Serial.printf("keySpeed_%d:%f,", keys[i].pitch, keys[i].getKeySpeed());
  }
  if (attributeStates[HAMMER_POSITION]) {
    Serial.printf("hammerPos_%d:%f,", keys[i].pitch, keys[i].getHammerPosition());
  }
  if (attributeStates[HAMMER_SPEED]) {
    Serial.printf("hammerSpeed_%d:%f,", keys[i].pitch, keys[i].getHammerSpeed());
  }
  if (attributeStates[ELAPSED]) {
    Serial.printf("elapsedUS_%d:%d,", keys[i].pitch, keys[i].getElapsedUS());
  }
}

// function for unrecognized commands
void unrecognizedCmd (const char *command) {
  Serial.print("Command not recognized: ");
  Serial.println(command);
}



void loop() {
  if (printTimerMS > 100) {
    printInfo = true;
  }

  if (keys[0].elapsedUS >= 1250) {
    for (int i = 0; i < n_keys; i++) {
      if (printInfo & (i == printkey) || printAllKeys ) {
        printKeyState(i);
        Serial.flush();
      }
      keys[i].step();

    }

    if (printInfo) {
      Serial.print('\n');
      printInfo = false;
      printTimerMS = 0;
    }

    b_toggle_calibration.update();

  if ( b_toggle_calibration.pressed() ) {
    toggleCalibration();
  }
    
    loopTimerUS = 0;
    // do any loop end actions, such as reading any new MIDI messages
    midiSender.loopEnd();
  }
  // check if there are any new commands
  sCmd.readSerial();
}
  

// void loop1() {
//   // uint32_t rp2040.fifo.pop() will block if the FIFO is empty, there is also a bool rp2040.fifo.pop_nb version
//   Serial.printf("C1: Read value from FIFO: %d\n", rp2040.fifo.pop());
//   // int rp2040.fifo.available() - get number of values available in this core's FIFO

// Just need: index in velocity lookup table
// pitch to be sent
  
// }