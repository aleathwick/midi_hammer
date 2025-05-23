#include "config.h"

// elapsedMillis: https://github.com/pfeerick/elapsedMillis
#include <elapsedMillis.h>
#include <SerialCommand.h>
// for log
#include <math.h>
#include <Bounce2.h>
#include "KeyHammer.h"
#include "Pedal.h"
#include "DualAdcManager.h"
#include <ParamHandler.h>

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
  const int addressPins0[] = {35, 34, 33};
  // second group of address pins, for the second set of muxes
  const int addressPins1[] = {1, 2, 3};
  const int enablePins[] = {39};
  int signalPins[] = {40, 15}; // A1
  int calibrationPin = 14;
  DualAdcManager dualAdcManager;
  
  #endif

int nEnable = sizeof(enablePins) / sizeof(enablePins[0]);

// can turn to int like so: int micros = elapsed[i][j];
// and reset to zero: elapsed[i][j] = 0;
elapsedMillis infoTimerMS;
elapsedMicros loopTimerUS;

// whether or not to print (at all, in any loop)
bool printInfo = false;
// whether or not to print in the current loop
bool printInfoTriggered = false;
// used to restrict printing so we don't overload serial
elapsedMillis printTimerMS = 0;
// used to restrict printing briefly after serial cmd info printed
// only applies to stream mode
elapsedMillis serialMsgTimerMS = 0;
void pausePrintStream() {
  serialMsgTimerMS = 0;
}

int serialMsgDelay = 1700;

elapsedMillis testAdcTimerMS;
int testFunction() {
  // test function for getting key position
  return (int)(sin(testAdcTimerMS / (float)300) * 512) + 512;
}

const int n_keys = 8;

// specify constant int value for min_key_press shared across all keys
const float maxHammerSpeed_m_s = 2.5;
const float hammer_travel = 7;
// default vals, but will set these during calibration / read in saved calibrated values from SD card
int adcValKeyDown = 560;
int adcValKeyUp = 450;
// initialise a ParamHandler object for loading parameters from SD card
ParamHandler ph;

#include "MidiSenderDummy.h"
MidiSenderDummy midiSenderDummy;
int settle_delay = 6;
// calling readDualGetAdcValue0(0, 1, 3, 3, settle_delay) will read adc values from pins
// 0 and 1, using adc0 and adc1, with multiplexers both sent to input 3, and will return
// the value from adc0 (reading occurs simultaneously)
// subsequently calling readDualGetAdcValue1(0, 1, 3, 3, settle_delay) will return the cached
// value from adc1, from the previous read without needing to set the muxes again
// the cached value is used since the configuration is the same
KeyHammer keys[] = {
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(0, 1, 3, 3, settle_delay); }, &midiSender, 49, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(0, 1, 3, 3, settle_delay); }, &midiSenderDummy, 61, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(0, 1, 2, 2, settle_delay); }, &midiSender, 50, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(0, 1, 2, 2, settle_delay); }, &midiSenderDummy, 62, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(0, 1, 1, 1, settle_delay); }, &midiSender, 51, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(0, 1, 1, 1, settle_delay); }, &midiSenderDummy, 63, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(0, 1, 0, 0, settle_delay); }, &midiSender, 52, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(0, 1, 0, 0, settle_delay); }, &midiSenderDummy, 64, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s}
};

int nPedals = 0;
Pedal pedals[] = {
  // { []() -> int { return dualAdcManager.readDualGetAdcValue0(0, 1, 4, 3, settle_delay); }, &midiSender, 64, 584, 534 },
};

int printkey = 0;
bool printAllKeys = false;

Bounce2::Button b_toggle_calibration = Bounce2::Button();

SerialCommand sCmd;

// help string, printed with "help" command
const char* helpString = "Commands:\n"
                          "tc: toggle calibration of thresholds\n"
                          "ts: save updated thresholds to sd card\n"
                          "pp: print key parameters (including calibration results)\n"
                          "pm: change print mode (stream, buffers, notes, none)\n"
                          "pk: set print key (0-(nKeys-1), +, -)\n"
                          "pka: toggle print attributes (applicable to stream mode)\n"
                          "pf: set print frequency (ms)\n"
                          "h / help: show this message\n"
                          ;
                          
void printHelp() {
  Serial.print("\n");
  Serial.println(helpString);
  pausePrintStream();
}

void setup() {
  Serial.begin(57600);
  pinMode(LED_BUILTIN, OUTPUT);

  b_toggle_calibration.attach( calibrationPin, INPUT_PULLUP );
  b_toggle_calibration.interval(5);
  b_toggle_calibration.setPressedState(LOW); 

  sCmd.addCommand("help", printHelp);
  sCmd.addCommand("h", printHelp);
  sCmd.addCommand("tc", toggleCalibration);
  sCmd.addCommand("ts", saveKeyParams);
  sCmd.addCommand("pp", printKeyParams);
  sCmd.addCommand("pm", changePrintMode);
  sCmd.addCommand("pk", setPrintKey);
  sCmd.addCommand("pka", togglePrintAttributes);
  sCmd.addCommand("pf", setPrintFrequency);
  sCmd.setDefaultHandler(unrecognizedCmd);

  midiSender.initialize();
  ph.initialize(n_keys);

  // load key parameters from SD card
  for (int i = 0; i < n_keys; i++) {
    if (ph.existsAdcValKeyDown(i)) {
      // read the value from the SD card
      keys[i].setAdcValKeyDown(ph.getAdcValKeyDown(i));
      Serial.printf("Key %d: adcValKeyDown: %d\n", i, keys[i].getAdcValKeyDown());
    }
    if (ph.existsAdcValKeyUp(i)) {
      // read the value from the SD card
      keys[i].setAdcValKeyUp(ph.getAdcValKeyUp(i));
      Serial.printf("Key %d: adcValKeyUp: %d\n", i, keys[i].getAdcValKeyUp());
    }
  }

  for (int i = 0; i < nEnable; i++) {
    digitalWrite(enablePins[i], LOW);
  }
  int nAddressPins = sizeof(addressPins0) / sizeof(addressPins0[0]);
  dualAdcManager.begin(addressPins0, addressPins1, signalPins, nAddressPins);
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

// function for saving key parameters to SD card
void saveKeyParams () {
  // first, update the key parameters in the ParamHandler object
  for (int i = 0; i < n_keys; i++) {
    // read the value from the SD card
    ph.setAdcValKeyDown(i, keys[i].getAdcValKeyDown());
    ph.setAdcValKeyUp(i, keys[i].getAdcValKeyUp());
  }
  // write key parameters to SD card
  if (ph.writeParams()) {
    Serial.print("\n");
    Serial.println("Key parameters saved to SD card");
    pausePrintStream();
  } else {
    Serial.print("\n");
    Serial.println("Failed to save key parameters to SD card");
    pausePrintStream();
  }
}

//// sCmd commands

const char* printModeNames[] = {
  "none",
  "notes",
  "buffer"
};

PrintMode keyPrintMode;
// change print mode
void changePrintMode (const char *command) {
  
  char *arg = sCmd.next();
  if (arg != NULL) {
    if (strcmp(arg, "stream") == 0) {
      printInfo = true;
      keyPrintMode = PRINT_NONE;
      Serial.println("stream mode");
      pausePrintStream();
    } else if (strcmp(arg, "buffers") == 0) {
      printInfo = false;
      keyPrintMode = PRINT_BUFFER;
      Serial.println("buffer mode");
      pausePrintStream();
    } else if (strcmp(arg, "notes") == 0) {
      printInfo = false;
      keyPrintMode = PRINT_NOTES;
      Serial.println("notes mode");
      pausePrintStream();
    } else if (strcmp(arg, "none") == 0) {
      printInfo = false;
      keyPrintMode = PRINT_NONE;
      Serial.println("printing disabled");
    } else {
      Serial.print("\n");
      Serial.print("Second argument must be 'stream', 'buffers', 'notes', or 'none': ");
      Serial.println(arg);
      pausePrintStream();
    }
    updateKeyPrintModes();
  } 
  else {
    Serial.print("\n");
    Serial.println("Current print mode: ");
    Serial.println(printModeNames[keyPrintMode]);
    pausePrintStream();
  }
}

void updateKeyPrintModes () {
  for (int i = 0; i < n_keys; i++) {
    if ((i == printkey) || printAllKeys ) {
      keys[i].setPrintMode(keyPrintMode);
    } else {
      keys[i].setPrintMode(PRINT_NONE);
    }
  }
}

// function to print key settings/calibration results
void printKeyParams() {
  char *arg;
  int key;

  arg = sCmd.next();
  if (arg != NULL) {
    // check if the argument is 'a'
    if (strcmp(arg, "all") == 0) {
      for (int i = 0; i < n_keys; i++) {
        Serial.print("\n");
        Serial.printf("*** KEY %d ***\n", i);
        keys[i].printKeyParams();
      }
    } else if (isdigit(arg[0])) {
      // atoi vs atol:
      // atoi: convert string to int
      // atol: convert string to long
      key = atoi(arg);
      if (key < 0 || key >= n_keys) {
        Serial.print("\n");
        Serial.print("Key number out of range: ");
        Serial.println(arg);
        pausePrintStream();
        return;
      }
      Serial.print("\n");
      keys[key].printKeyParams();
    } else {
      Serial.print("\n");
      Serial.print("Second argument must be 'all' or a number: ");
      Serial.println(arg);
      pausePrintStream();
    }
  } else {
    Serial.print("\n");
    Serial.print("must provide a key index or 'all'\n");
    pausePrintStream();
  }
  
}

// function to set printkey, based on a key number
void setPrintKey (const char *command) {
  int key;
  char *arg;

  arg = sCmd.next();
  if (arg != NULL) {
    // check if the argument is 'a'
    if (strcmp(arg, "all") == 0) {
      printAllKeys = true;
    } else if (strcmp(arg, "-") == 0){
      printkey = (printkey - 1) % n_keys;
    } else if (strcmp(arg, "+") == 0){
      printkey = (printkey + 1) % n_keys;
    } else if (isdigit(arg[0])) {
      // atoi vs atol:
      // atoi: convert string to int
      // atol: convert string to long
      key = atoi(arg);
      printkey = key % n_keys;
      printAllKeys = false;
    } else {
      Serial.print("\n");
      Serial.print("Second argument must be 'all', '-', '+', or a number: ");
      Serial.println(arg);
      pausePrintStream();
    }
    updateKeyPrintModes();
  }
  else {
    Serial.print("\n");
    Serial.printf("printKey = %d (pitch %d), printAllKeys = %d, with printMode = %s\n", printkey, keys[printkey].pitch, printAllKeys, printModeNames[keyPrintMode]);
    pausePrintStream();

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

const char* keyAttributeAbbrev[NUM_ATTRIBUTES] = {
  "adc",  // RAW_ADC
  "kp",   // KEY_POSITION
  "ks",   // KEY_SPEED
  "hp",   // HAMMER_POSITION
  "hs",   // HAMMER_SPEED
  "el"    // ELAPSED
};

// full names for printing?
const char* keyAttributeNames[NUM_ATTRIBUTES] = {
  "rawADC",
  "keyPos",
  "keySpeed",
  "hammerPos",
  "hammerSpeed",
  "elapsedUS"
};

// which attributes to print
bool attributeStates[NUM_ATTRIBUTES] = {true, false, false,false,false,false}; // All attributes enabled by default

// choose which attributes of keys to print
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
    } else {
      // Check if arg matches any abbreviation
      bool found = false;
      for (int i = 0; i < NUM_ATTRIBUTES; i++) {
        if (strcmp(arg, keyAttributeAbbrev[i]) == 0) {
          attributeStates[i] = !attributeStates[i];
          found = true;
          break;
        }
      }
      
      if (!found && isdigit(arg[0])) {
        // Toggle a specific attribute by index
        int attrIndex = atoi(arg);
        if (attrIndex >= 0 && attrIndex < NUM_ATTRIBUTES) {
          attributeStates[attrIndex] = !attributeStates[attrIndex];
        } else {
          Serial.println("Invalid attribute index.");
          pausePrintStream();
        }
      } else if (!found) {
        Serial.print("\n");
        Serial.println("Invalid argument. Use 'all', 'none', or an attribute index / shorthand.");
        pausePrintStream();
      }
    }
  } else {
    Serial.print("\n");
    Serial.println("Current attribute states:");
    for (int i = 0; i < NUM_ATTRIBUTES; i++) {
      Serial.printf("%s (%s): %s\n", keyAttributeNames[i], keyAttributeAbbrev[i], attributeStates[i] ? "enabled" : "disabled");
    }
    pausePrintStream();
  }
}

// print the state of a key (limited to the attributes that are enabled)
void printKeyState(int i) {
  for (int attr = 0; attr < NUM_ATTRIBUTES; attr++) {
    if (attributeStates[attr]) {
      switch (attr) {
        case RAW_ADC:
          Serial.printf("%s_%d-%d:%d,", keyAttributeAbbrev[attr], i, keys[i].pitch, keys[i].getRawADC());
          break;
        case KEY_POSITION:
          Serial.printf("%s_%d-%d:%f,", keyAttributeAbbrev[attr], i, keys[i].pitch, keys[i].getKeyPosition());
          break;
        case KEY_SPEED:
          Serial.printf("%s_%d-%d:%f,", keyAttributeAbbrev[attr], i, keys[i].pitch, keys[i].getKeySpeed());
          break;
        case HAMMER_POSITION:
          Serial.printf("%s_%d-%d:%f,", keyAttributeAbbrev[attr], i, keys[i].pitch, keys[i].getHammerPosition());
          break;
        case HAMMER_SPEED:
          Serial.printf("%s_%d-%d:%f,", keyAttributeAbbrev[attr], i, keys[i].pitch, keys[i].getHammerSpeed());
          break;
        case ELAPSED:
          Serial.printf("%s_%d-%d:%d,", keyAttributeAbbrev[attr], i, keys[i].pitch, keys[i].getElapsedUS());
          break;
      }
    }
  }
}

int printFreqMS = 200; // print frequency in ms

void setPrintFrequency() {
  char *arg = sCmd.next();
  if (arg != NULL) {
    int freq = atoi(arg);
    if (freq > 0) {
      printFreqMS = freq;
    } else {
      Serial.print("\n");
      Serial.println("Invalid frequency. Must be a positive integer.");
      pausePrintStream();
    }
  } else {
    Serial.print("\n");
    Serial.println("current print frequency (MS): ");
    Serial.println(printFreqMS);
    pausePrintStream();
  }
  
}

// function for unrecognized commands
void unrecognizedCmd (const char *command) {
  Serial.print("\n");
  Serial.print("Command not recognized: ");
  Serial.println(command);
  Serial.print("Type 'help' for a list of commands.\n");
  pausePrintStream();
}



void loop() {
  if ((printInfo) & (printTimerMS > printFreqMS) & (serialMsgTimerMS > serialMsgDelay)) {
    printInfoTriggered = true;
  }

  if (keys[0].elapsedUS >= 250) {
    for (int i = 0; i < n_keys; i++) {
      if (printInfoTriggered & ((i == printkey) || printAllKeys )) {
        printKeyState(i);
        Serial.flush();
      }
      keys[i].step();
    }
    for (int i = 0; i < nPedals; i++) {
      pedals[i].step();
    }

    if (printInfoTriggered) {
      Serial.print('\n');
      printInfoTriggered = false;
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