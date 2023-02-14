// https://cplusplus.com/doc/tutorial/classes/
// https://paulmurraycbr.github.io/ArduinoTheOOWay.html
// https://www.circuitbasics.com/programming-with-classes-and-objects-on-the-arduino/
#include "KeyHammer.h"
#include <math.h>
// #include <Adafruit_MCP3008.h>
// #include <elapsedMillis.h>

// set up map of velocities, mapping from hammer speed to midi value
// hammer speed seems to range from ~0.005 to ~0.05 adc bits per microsecond
// ~5 to ~50 adc bits per millisecond, ~5000 to ~50,000 adc bits per second
const int velocityMapLength = 1024;
const double logBase = 5; // base used for log multiplier, with 1 setting the multiplier to always 1
int velocityMap[velocityMapLength];


// use a constructor initializer list for adc, otherwise the reference won't work
KeyHammer::KeyHammer (int(*adcFnPtr)(void), int pitch, char operationMode='h', int sensorFullyOn=430, int sensorFullyOff=50, double hammer_travel=4.5, int minPressUS=8500)
  : adcFnPtr(adcFnPtr), pitch(pitch), operationMode(operationMode), sensorFullyOn(sensorFullyOn), sensorFullyOff(sensorFullyOff), hammer_travel(hammer_travel), minPressUS(minPressUS) {

  sensorMax = max(sensorFullyOn, sensorFullyOff);
  sensorMin = min(sensorFullyOn, sensorFullyOff);

  noteOnThreshold = sensorFullyOn + 0.06 * (sensorFullyOn - sensorFullyOff);
  noteOffThreshold = sensorFullyOn - 0.5 * (sensorFullyOn - sensorFullyOff);

  // gravity for hammer, measured in adc bits per microsecond per microsecond
  // if the key press is ADC_range, where ADC_range is abs(sensorFullyOn - sensorFullyOff)
  // hammer travel in mm; used to calculate gravity in adc bits
  gravity = (sensorFullyOn - sensorFullyOff) / (hammer_travel * (double)9810000000);

  keyPosition = sensorFullyOff;
  lastKeyPosition = sensorFullyOff;
  rawADC = sensorFullyOff;
  keySpeed = 0.0;
  hammerPosition = sensorFullyOff;
  hammerSpeed = 0.0;
  // max hammer speed measured in adc bits per microseconds
  double maxHammerSpeed = (max(sensorFullyOn, sensorFullyOff) - min(sensorFullyOn, sensorFullyOff)) / (double)minPressUS;
  hammerSpeedScaler = velocityMapLength / maxHammerSpeed;

  noteOn = false;

  elapsed = 0;

  lastControlValue = 0;
  controlValue = 0;
  // default to 64 (sustain); manually change if necessary
  controlNumber = 64;

  printNotes=false;

  // initialize velocity map
  if (velocityMap[velocityMapLength-1] == 0) {
    generateVelocityMap();
  }

}

void KeyHammer::update_key () {
  lastKeyPosition = keyPosition;
  rawADC = getAdcValue();
  keyPosition = rawADC;
  // constrain key position to be within the range determined by sensor max and min
  keyPosition = min(keyPosition, sensorMax);
  keyPosition = max(keyPosition, sensorMin);

}

void KeyHammer::update_keyspeed () {
  keySpeed = (keyPosition - lastKeyPosition) / (double)elapsed;
}

void KeyHammer::update_hammer () {
  hammerSpeed = hammerSpeed - gravity * elapsed;
  hammerPosition = hammerPosition + hammerSpeed * elapsed;
  // check for interaction with key
  if ((hammerPosition > keyPosition) == (sensorFullyOff > sensorFullyOn)) {
          hammerPosition = keyPosition;
          // we could check to see if the hammer speed is greater then key speed, but probably not necessary
          // after all, the key as 'caught up' to the hammer
          // if ((hammerSpeed > keySpeed) == (sensorFullyOff > sensorFullyOn)) {
            // if (abs(hammerSpeed) < abs(keySpeed)) {
          hammerSpeed = keySpeed;
          // }
        // }
  }
}

void KeyHammer::check_note_on () {
  // check for note ons
  if ((hammerPosition < noteOnThreshold) == (sensorFullyOff > sensorFullyOn)) {
    // do something with hammer speed to get velocity
    velocity = hammerSpeed;
    velocityIndex = round(hammerSpeed * hammerSpeedScaler);
    velocityIndex = min(velocityIndex, velocityMapLength-1);
    MIDI.sendNoteOn(pitch, velocityMap[velocityIndex], 2);
    noteOn = true;
    if (printNotes){ //&& ((i == 0 && j == 0) || (i == 1 && j == 2))){
      Serial.printf("\n note on: hammerSpeed %f, velocityIndex %d, velocity %d pitch %d \n", velocity, velocityIndex, velocityMap[velocityIndex], pitch);
    }
    hammerPosition = noteOnThreshold;
    hammerSpeed = -hammerSpeed;
    }
}

void KeyHammer::check_note_off () {
  if (noteOn){
    if ((keyPosition > noteOffThreshold) == (sensorFullyOff > sensorFullyOn)) {
      MIDI.sendNoteOff(pitch, 64, 2);
      if (printNotes){
        Serial.printf("note off: noteOffThreshold %d, adcValue %d, velocity %d  pitch %d \n", noteOffThreshold, keyPosition, 64, pitch);
      }
      noteOn = false;
    }
  }
}

void KeyHammer::step_hammer () {
  update_key();
  update_keyspeed();
  update_hammer();
  // test();
  check_note_on();
  check_note_off();
  elapsed = 0;
}


void KeyHammer::step_pedal () {
  update_key();
  lastControlValue = controlValue;
  // this could be sped up by precomputing the possible values
  controlValue = (int)((keyPosition - sensorFullyOff) / double(sensorFullyOn - sensorFullyOff) * 127);
  if (controlValue != lastControlValue) {
    // control numbers:
    // 1 = Modulation wheel
    // 2 = Breath Control
    // 7 = Volume
    // 10 = Pan
    // 11 = Expression
    // 64 = Sustain Pedal (on/off)
    // 65 = Portamento (on/off)
    // 67 = Soft Pedal
    // 71 = Resonance (filter)
    // 74 = Frequency Cutoff (filter)
    MIDI.sendControlChange(	controlNumber, controlValue, 2);
  }
  elapsed = 0;
}

void KeyHammer::step () {
  if (operationMode=='h')
  {
    step_hammer();
  } else if (operationMode=='p')
  {
    step_pedal();
  }
}

void KeyHammer::test () {
  Serial.printf("value:%d\n", getAdcValue());
}

int KeyHammer::getAdcValue () {
  return adcFnPtr();
}

void KeyHammer::generateVelocityMap () {
  // generate values for velocity map
  double logMultiplier;
  for (int i = 0; i < velocityMapLength; i++) {
    // logMultiplier will always be between 0 and 1
    logMultiplier = log(i / (double)velocityMapLength * (logBase - 1) + 1) / log(logBase);
    velocityMap[i] = round(127 * i / (double)velocityMapLength);
  }
}

void KeyHammer::printState () {
  if (operationMode=='p'){
    Serial.printf("key_%d:%f,", pitch, keyPosition);
    Serial.printf("rawADC_%d:%d,", pitch, rawADC);
    Serial.printf("elapsed_%d:%d,", pitch, (int)elapsed);
    Serial.printf("controlValue_%d:%d,", controlValue, (int)elapsed);
  } else if (operationMode=='h'){
    // Serial.printf("%d %f %f ", keyPosition, keySpeed, hammerSpeed);
    Serial.printf("hammer_%d:%f,", pitch, hammerPosition);
    Serial.printf("rawADC_%d:%d,", pitch, rawADC);
    Serial.printf("elapsed_%d:%d,", pitch, (int)elapsed);
  }

}