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
const float logBase = 5; // base used for log multiplier, with 1 setting the multiplier to always 1
int velocityMap[velocityMapLength];


// use a constructor initializer list for adc, otherwise the reference won't work
KeyHammer::KeyHammer (int(*adcFnPtr)(void), MidiSender* midiSender, int pitch, char operationMode='h', int sensorFullyOn=430, int sensorFullyOff=50, float hammer_travel=4.5, int minPressUS=8500)
  : adcFnPtr(adcFnPtr), midiSender(midiSender), pitch(pitch), operationMode(operationMode), sensorFullyOn(sensorFullyOn), sensorFullyOff(sensorFullyOff), hammer_travel(hammer_travel), minPressUS(minPressUS) {
  // TODO: there is a simpler way of doing this; see circuitpython code
  // instead of modifying if statements all throughout code, flip the sign on max/min vals and
  // on adc function
  sensorMax = max(sensorFullyOn, sensorFullyOff);
  sensorMin = min(sensorFullyOn, sensorFullyOff);

  noteOnThreshold = sensorFullyOn + 0.06 * (sensorFullyOn - sensorFullyOff);
  noteOffThreshold = sensorFullyOn - 0.5 * (sensorFullyOn - sensorFullyOff);
  keyResetThreshold = sensorFullyOn - 0.25 * (sensorFullyOn - sensorFullyOff);

  // gravity calculation
  // gravity in metres per microsecond^2
  float gravity_m = 9.81e-12;
  // gravity in mm per microsecond^2
  float gravity_mm = gravity_m * 1000;
  // gravity in adc bits per microsecond^2
  // hammer travel is in mm
  gravity = gravity_mm  / hammer_travel * (sensorFullyOn - sensorFullyOff);

  keyPosition = sensorFullyOff;
  lastKeyPosition = sensorFullyOff;
  rawADC = sensorFullyOff;
  keySpeed = 0.0;
  hammerPosition = sensorFullyOff;
  hammerSpeed = 0.0;
  // max hammer speed measured in adc bits per microseconds
  float maxHammerSpeed = (max(sensorFullyOn, sensorFullyOff) - min(sensorFullyOn, sensorFullyOff)) / (float)minPressUS;
  hammerSpeedScaler = velocityMapLength / maxHammerSpeed;

  noteOn = false;
  keyArmed = true;

  elapsedUS = 0;

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

void KeyHammer::updateKey () {
  lastKeyPosition = keyPosition;
  rawADC = getAdcValue();
  adcBuffer.push(rawADC);
  keyPosition = rawADC;
  // constrain key position to be within the range determined by sensor max and min
  keyPosition = min(keyPosition, sensorMax);
  keyPosition = max(keyPosition, sensorMin);

}

void KeyHammer::updateKeySpeed () {
  keySpeed = (keyPosition - lastKeyPosition) / (float)elapsedUS;
}

void KeyHammer::updateHammer () {
  // TODO: position should be updated using the mean of old and new speeds
  // see circuitpy code
  hammerSpeed = hammerSpeed - gravity * elapsedUS;
  hammerPosition = hammerPosition + hammerSpeed * elapsedUS;
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

void KeyHammer::checkNoteOn () {
  // check for note ons
  if (keyArmed && (hammerPosition < noteOnThreshold) == (sensorFullyOff > sensorFullyOn)) {
    // do something with hammer speed to get velocity
    velocity = hammerSpeed;
    velocityIndex = round(hammerSpeed * hammerSpeedScaler);
    velocityIndex = min(velocityIndex, velocityMapLength-1);
    // sometimes negative values for velocityIndex occur, probably due to a mismatch between thresholds and actual ADC range
    velocityIndex = max(velocityIndex, 0);
    midiSender->sendNoteOn(pitch, velocityMap[velocityIndex], 2);
    noteOn = true;
    keyArmed = false;
    if (printNotes){ //&& ((i == 0 && j == 0) || (i == 1 && j == 2))){
      Serial.printf("\n note on: hammerSpeed %f, velocityIndex %d, velocity %d pitch %d \n", velocity, velocityIndex, velocityMap[velocityIndex], pitch);
    }
    hammerPosition = noteOnThreshold;
    hammerSpeed = -hammerSpeed;
    }
}

void KeyHammer::checkNoteOff () {
  if (noteOn){
    if ((! keyArmed) && ((keyPosition > keyResetThreshold) == (sensorFullyOff > sensorFullyOn))) {
      keyArmed = true;
    }

    if ((keyPosition > noteOffThreshold) == (sensorFullyOff > sensorFullyOn)) {
      midiSender->sendNoteOff(pitch, 64, 2);
      if (printNotes){
        Serial.printf("note off: noteOffThreshold %d, adcValue %d, velocity %d  pitch %d \n", noteOffThreshold, keyPosition, 64, pitch);
      }
      noteOn = false;
    }
  }
}

void KeyHammer::stepHammer () {
  updateKey();
  updateKeySpeed();
  updateHammer();
  // test();
  checkNoteOn();
  checkNoteOff();
  elapsedUS = 0;
}


void KeyHammer::stepPedal () {
  updateKey();
  lastControlValue = controlValue;
  // this could be sped up by precomputing the possible values
  controlValue = (int)((keyPosition - sensorFullyOff) / float(sensorFullyOn - sensorFullyOff) * 127);
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
    midiSender->sendControlChange(controlNumber, controlValue, 2);
  }
  elapsedUS = 0;
}

void KeyHammer::step () {
  if (operationMode=='h')
  {
    stepHammer();
  } else if (operationMode=='p')
  {
    stepPedal();
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
  float logMultiplier;
  for (int i = 0; i < velocityMapLength; i++) {
    // logMultiplier will always be between 0 and 1
    logMultiplier = log(i / (float)velocityMapLength * (logBase - 1) + 1) / log(logBase);
    velocityMap[i] = round(127 * i / (float)velocityMapLength);
  }
}

void KeyHammer::printState () {
  if (operationMode=='p'){
    Serial.printf("key_%d:%f,", pitch, keyPosition);
    Serial.printf("rawADC_%d:%d,", pitch, rawADC);
    Serial.printf("elapsedUS_%d:%d,", pitch, (int)elapsedUS);
    Serial.printf("controlValue_%d:%d,", controlValue, (int)elapsedUS);
  } else if (operationMode=='h'){
    // Serial.printf("%d %f %f ", keyPosition, keySpeed, hammerSpeed);
    Serial.printf("hammer_%d:%f,", pitch, hammerPosition);
    Serial.printf("rawADC_%d:%d,", pitch, rawADC);
    Serial.printf("hammerSpeed_%d:%f,", pitch, hammerSpeed);
    // Serial.printf("elapsedUS_%d:%d,", pitch, (int)elapsedUS);
  }

}