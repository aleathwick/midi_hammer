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

  scaleFilterWeights(keyPosFilter);

  elapsedUS = 0;

  lastControlValue = 0;
  controlValue = 0;
  // default to 64 (sustain); manually change if necessary
  controlNumber = 64;

  // initialize velocity map
  if (velocityMap[velocityMapLength-1] == 0) {
    generateVelocityMap();
  }

}

void KeyHammer::updateElapsed () {
  elapsedUSBuffer.push(elapsedUS);
}

void KeyHammer::updateKey () {
  lastKeyPosition = keyPosition;
  rawADC = getAdcValue();
  adcBuffer.push(rawADC);
  keyPosition = applyFilter(adcBuffer, keyPosFilter);
  // constrain key position to be within the range determined by sensor max and min
  keyPosition = min(keyPosition, sensorMax);
  keyPosition = max(keyPosition, sensorMin);

}

void KeyHammer::updateKeySpeed () {
  keySpeed = applyFilter(adcBuffer, keySpeedFilter) / (float)elapsedUSBuffer.last();

}

void KeyHammer::updateHammer () {
  // TODO: position should be updated using the mean of old and new speeds
  // see circuitpy code
  hammerSpeed = hammerSpeed - gravity * elapsedUSBuffer.last();
  hammerPosition = hammerPosition + hammerSpeed * elapsedUSBuffer.last();
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
  hammerPositionBuffer.push(hammerPosition);
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
    if (printMode == PRINT_NOTES){
      Serial.printf("\n note on: hammerSpeed %f, velocityIndex %d, velocity %d pitch %d \n", velocity, velocityIndex, velocityMap[velocityIndex], pitch);
    }
    // maybe print the buffer on note on?
    // could be useful for understanding adc/key/hammer behaviour
    bufferPrinted = false;
    noteOnElapsedUS = 0;
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
      if (printMode == PRINT_NOTES){
        Serial.printf("note off: noteOffThreshold %d, adcValue %d, velocity %d  pitch %d \n", noteOffThreshold, keyPosition, 64, pitch);
      }
      noteOn = false;
    }
  }
}

void KeyHammer::stepHammer () {
  updateKey();
  // call updateElapsed after updateKey because reading the ADC value is the slowest part of the loop
  updateElapsed();
  updateKeySpeed();
  if (iteration > BUFFER_SIZE) {
    updateHammer();
    // test();
    checkNoteOn();
    checkNoteOff();
    if ((printMode == PRINT_BUFFER) && (noteOnElapsedUS > 10000) && (!bufferPrinted)) {
      printBuffers();
      bufferPrinted = true;
    }
  }
  elapsedUS = 0;
}


void KeyHammer::stepPedal () {
  updateKey();
  updateElapsed();
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
  iteration++;
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

template <size_t N>
void KeyHammer::scaleFilterWeights(float (&filter)[N]) {
    float sum = 0;
    // size_t is an unsigned integer type, so use it for i also
    // (but if i never negative, it would be fine anyway)
    for (size_t i = 0; i < N; i++) {
        sum += filter[i];
    }
    for (size_t i = 0; i < N; i++) {
        filter[i] /= sum;
    }
}


template <typename T, size_t bufferLength, size_t filterLength>
float KeyHammer::applyFilter(CircularBuffer<T, bufferLength>& buffer, float (&filter)[filterLength]) {
  float filteredValue = 0;
  int bufferSize = buffer.size();
  int startIndex = max(0, bufferSize - filterLength);
  for (int i = 0; i < filterLength; i++) {
      int bufferIndex = startIndex + i;
      if (bufferIndex < bufferSize) {
          filteredValue += buffer[bufferIndex] * filter[i];
      }
  }
  return filteredValue;
}

void KeyHammer::printState () {
  if (operationMode=='p'){
    Serial.printf("key_%d:%f,", pitch, keyPosition);
    Serial.printf("rawADC_%d:%d,", pitch, rawADC);
    Serial.printf("elapsedUS_%d:%d,", pitch, elapsedUSBuffer.last());
    Serial.printf("controlValue_%d:%d,", pitch, controlValue);
  } else if (operationMode=='h'){
    // Serial.printf("%d %f %f ", keyPosition, keySpeed, hammerSpeed);
    Serial.printf("hammer_%d:%f,", pitch, hammerPosition);
    Serial.printf("rawADC_%d:%d,", pitch, rawADC);
    Serial.printf("hammerSpeed_%d:%f,", pitch, hammerSpeed);
    Serial.printf("elapsedUS_%d:%d,", pitch, elapsedUSBuffer.last());
  }

}

void KeyHammer::printBuffers () {
    for (int i = 0; i < adcBuffer.size(); ++i) {
      Serial.printf("rawADC_%d:%d,", pitch, adcBuffer[i]);
      Serial.printf("hammerPosition_%d:%f,", pitch, hammerPositionBuffer[i]);
      Serial.printf("elapsedUs_%d:%d,", pitch, elapsedUSBuffer[i]);
      Serial.printf("\n");
      Serial.flush();
    }
}
