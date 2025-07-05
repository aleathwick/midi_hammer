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
  // pins for left muxes (each pcb has a left and right mux)
  const int addressPinsL[] = {35, 36, 37};
  // pins for right muxes
  const int addressPinsR[] = {32, 33, 34};
  const int enablePins[] = {}; // not relevant, since enable pins are tied to ground
                    //LU3 LU2 LU1 RU3 RU2 RU1  LM  RM  LD1 LD2 LD3 RD1 RD2 RD3
  const int signalPins[] = {23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 41, 40, 39, 38}; // 39 38 only on A1
  // indexes of specific signal pins in signalPins array
  // LU = Left Up, RD = Right Down, LM = Left Middle etc.
  const int SP_LU3 = 0;
  const int SP_LU2 = 1;
  const int SP_LU1 = 2;
  const int SP_RU3 = 3;
  const int SP_RU2 = 4;
  const int SP_RU1 = 5;
  const int SP_LM = 6;
  const int SP_RM = 7;
  const int SP_LD1 = 8;
  const int SP_LD2 = 9;
  const int SP_LD3 = 10;
  const int SP_RD1 = 11;
  const int SP_RD2 = 12;
  const int SP_RD3 = 13;
  // calibration pin, used for calibration button
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

// specify constant int value for min_key_press shared across all keys
const float maxHammerSpeed_m_s = 2.5;
const float hammer_travel = 7;
// default vals, but will set these during calibration / read in saved calibrated values from SD card
int adcValKeyDown = 560;
int adcValKeyUp = 450;
// initialise a ParamHandler object for loading parameters from SD card
ParamHandler ph;

const int shift = 0;
const int MIDI_A = 21 + shift;
const int MIDI_Bb = 22 + shift;
const int MIDI_B = 23 + shift;
const int MIDI_C = 24 + shift;
const int MIDI_Db = 25 + shift;
const int MIDI_D = 26 + shift;
const int MIDI_Eb = 27 + shift;
const int MIDI_E = 28 + shift;
const int MIDI_F = 29 + shift;
const int MIDI_Gb = 30 + shift;
const int MIDI_G = 31 + shift;
const int MIDI_Ab = 32 + shift;

// mapping of pitch numbers to mux addresses, with A=0
const int pC2muxAddr[] = {
  6, 4, 7, 3, 1, 0, // A to Eb indexes on left mux
  4, 6, 7, 1, 3, 0 // Eb to Ab indexes on right mux
}; // 7 resolves to 6, 3 and 1 to 0

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
  // A / pc 0
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD3, SP_LU2, pC2muxAddr[0], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_A, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD3, SP_LU2, pC2muxAddr[0], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_A+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD1, SP_LM, pC2muxAddr[0], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_A+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD1, SP_LM, pC2muxAddr[0], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_A+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU1, SP_LU2, pC2muxAddr[0], pC2muxAddr[6], settle_delay); }, &midiSender, MIDI_A+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LU1, SP_LU2, pC2muxAddr[0], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_A+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU3, SP_LU2, pC2muxAddr[0], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_A+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  
  // Eb / pc 6
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD3, SP_RU2, pC2muxAddr[1], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_Eb, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD3, SP_RU2, pC2muxAddr[1], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_Eb+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD1, SP_RM, pC2muxAddr[1], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_Eb+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD1, SP_RM, pC2muxAddr[1], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_Eb+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU1, SP_RU2, pC2muxAddr[1], pC2muxAddr[6], settle_delay); }, &midiSender, MIDI_Eb+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RU1, SP_RU2, pC2muxAddr[1], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_Eb+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU3, SP_RU2, pC2muxAddr[1], pC2muxAddr[6], settle_delay); }, &midiSenderDummy, MIDI_Eb+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  
  // Bb / pc 1
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD3, SP_LU2, pC2muxAddr[1], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_Bb, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD3, SP_LU2, pC2muxAddr[1], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_Bb+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD1, SP_LM, pC2muxAddr[1], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_Bb+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD1, SP_LM, pC2muxAddr[1], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_Bb+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU1, SP_LU2, pC2muxAddr[1], pC2muxAddr[7], settle_delay); }, &midiSender, MIDI_Bb+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LU1, SP_LU2, pC2muxAddr[1], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_Bb+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU3, SP_LU2, pC2muxAddr[1], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_Bb+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
    
  // E / pc 7
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD3, SP_RU2, pC2muxAddr[2], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_E, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD3, SP_RU2, pC2muxAddr[2], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_E+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD1, SP_RM, pC2muxAddr[2], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_E+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD1, SP_RM, pC2muxAddr[2], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_E+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU1, SP_RU2, pC2muxAddr[2], pC2muxAddr[7], settle_delay); }, &midiSender, MIDI_E+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RU1, SP_RU2, pC2muxAddr[2], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_E+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU3, SP_RU2, pC2muxAddr[2], pC2muxAddr[7], settle_delay); }, &midiSenderDummy, MIDI_E+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  
  // B / pc 2 
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD3, SP_LU2, pC2muxAddr[2], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_B, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD3, SP_LU2, pC2muxAddr[2], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_B+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD1, SP_LM, pC2muxAddr[2], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_B+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD1, SP_LM, pC2muxAddr[2], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_B+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU1, SP_LU2, pC2muxAddr[2], pC2muxAddr[8], settle_delay); }, &midiSender, MIDI_B+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LU1, SP_LU2, pC2muxAddr[2], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_B+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU3, SP_LU2, pC2muxAddr[2], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_B+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  
  // F / pc 8
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD3, SP_RU2, pC2muxAddr[3], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_F, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD3, SP_RU2, pC2muxAddr[3], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_F+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD1, SP_RM, pC2muxAddr[3], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_F+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD1, SP_RM, pC2muxAddr[3], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_F+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU1, SP_RU2, pC2muxAddr[3], pC2muxAddr[8], settle_delay); }, &midiSender, MIDI_F+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RU1, SP_RU2, pC2muxAddr[3], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_F+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU3, SP_RU2, pC2muxAddr[3], pC2muxAddr[8], settle_delay); }, &midiSenderDummy, MIDI_F+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},

  // C / pc 3
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD3, SP_LU2, pC2muxAddr[3], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_C, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD3, SP_LU2, pC2muxAddr[3], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_C+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD1, SP_LM, pC2muxAddr[3], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_C+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD1, SP_LM, pC2muxAddr[3], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_C+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU1, SP_LU2, pC2muxAddr[3], pC2muxAddr[9], settle_delay); }, &midiSender, MIDI_C+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LU1, SP_LU2, pC2muxAddr[3], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_C+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU3, SP_LU2, pC2muxAddr[3], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_C+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  
  // Gb / pc 9
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD3, SP_RU2, pC2muxAddr[4], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_Gb, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD3, SP_RU2, pC2muxAddr[4], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_Gb+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD1, SP_RM, pC2muxAddr[4], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_Gb+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD1, SP_RM, pC2muxAddr[4], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_Gb+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU1, SP_RU2, pC2muxAddr[4], pC2muxAddr[9], settle_delay); }, &midiSender, MIDI_Gb+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RU1, SP_RU2, pC2muxAddr[4], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_Gb+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU3, SP_RU2, pC2muxAddr[4], pC2muxAddr[9], settle_delay); }, &midiSenderDummy, MIDI_Gb+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  
  // Db / pc 4
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD3, SP_LU2, pC2muxAddr[4], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_Db, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD3, SP_LU2, pC2muxAddr[4], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_Db+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD1, SP_LM, pC2muxAddr[4], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_Db+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD1, SP_LM, pC2muxAddr[4], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_Db+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU1, SP_LU2, pC2muxAddr[4], pC2muxAddr[10], settle_delay); }, &midiSender, MIDI_Db+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LU1, SP_LU2, pC2muxAddr[4], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_Db+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU3, SP_LU2, pC2muxAddr[4], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_Db+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  
  // G / pc 10
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD3, SP_RU2, pC2muxAddr[5], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_G, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD3, SP_RU2, pC2muxAddr[5], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_G+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD1, SP_RM, pC2muxAddr[5], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_G+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD1, SP_RM, pC2muxAddr[5], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_G+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU1, SP_RU2, pC2muxAddr[5], pC2muxAddr[10], settle_delay); }, &midiSender, MIDI_G+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RU1, SP_RU2, pC2muxAddr[5], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_G+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU3, SP_RU2, pC2muxAddr[5], pC2muxAddr[10], settle_delay); }, &midiSenderDummy, MIDI_G+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  
  // D / pc 5
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD3, SP_LU2, pC2muxAddr[5], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_D, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD3, SP_LU2, pC2muxAddr[5], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_D+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LD1, SP_LM, pC2muxAddr[5], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_D+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LD1, SP_LM, pC2muxAddr[5], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_D+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU1, SP_LU2, pC2muxAddr[5], pC2muxAddr[11], settle_delay); }, &midiSender, MIDI_D+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_LU1, SP_LU2, pC2muxAddr[5], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_D+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_LU3, SP_LU2, pC2muxAddr[5], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_D+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  
  // Ab / pc 5
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD3, SP_RU2, pC2muxAddr[0], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_Ab, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD3, SP_RU2, pC2muxAddr[0], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_Ab+12, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RD1, SP_RM, pC2muxAddr[0], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_Ab+24, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RD1, SP_RM, pC2muxAddr[0], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_Ab+36, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU1, SP_RU2, pC2muxAddr[0], pC2muxAddr[11], settle_delay); }, &midiSender, MIDI_Ab+48, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue1(SP_RU1, SP_RU2, pC2muxAddr[0], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_Ab+60, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  { []() -> int { return dualAdcManager.readDualGetAdcValue0(SP_RU3, SP_RU2, pC2muxAddr[0], pC2muxAddr[11], settle_delay); }, &midiSenderDummy, MIDI_Ab+72, adcValKeyDown, adcValKeyUp , hammer_travel, maxHammerSpeed_m_s},
  
};

Pedal pedals[] = {
  // { []() -> int { return dualAdcManager.readDualGetAdcValue0(0, 1, 4, 3, settle_delay); }, &midiSender, 64, 584, 534 },
};

const int n_keys = sizeof(keys) / sizeof(keys[0]);
const int nPedals = sizeof(pedals) / sizeof(pedals[0]);

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
  // currently the number of address pins is controlled in DualADCManager.h
  // but could make this more flexible
  int nAddressPins = sizeof(addressPinsL) / sizeof(addressPinsL[0]);
  int nSignalPins = sizeof(signalPins) / sizeof(signalPins[0]);
  dualAdcManager.begin(addressPinsL, addressPinsR, signalPins, nSignalPins);
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