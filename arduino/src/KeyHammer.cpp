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
KeyHammer::KeyHammer (int(*adcFnPtr)(void), MidiSender* midiSender, int pitch, int adcValKeyDown=430, int adcValKeyUp=50, float hammer_travel=4.5, float maxHammerSpeed_m_s=2.5)
  : adcFnPtr(adcFnPtr), midiSender(midiSender), pitch(pitch), adcValKeyDown(adcValKeyDown), adcValKeyUp(adcValKeyUp), hammer_travel(hammer_travel), maxHammerSpeed_m_s(maxHammerSpeed_m_s) {
  
  updateADCParams();
  
  keyPosition = adcValKeyUp;
  lastKeyPosition = adcValKeyUp;
  rawADC = adcValKeyUp;
  keySpeed = 0.0;
  hammerPosition = adcValKeyUp;
  hammerSpeed = 0.0;

  noteOn = false;
  keyArmed = true;

  scaleFilterWeights(SavGolayFilters::posFilter, SavGolayFilters::posFilterLength);

  elapsedUS = 0;

  // initialize velocity map
  if (velocityMap[velocityMapLength-1] == 0) {
    generateVelocityMap();
  }

}

void KeyHammer::updateADCParams () {
  // assume ADC values increase as the key is pressed
  // i.e. adcValKeyDown > adcValKeyUp
  // if this is not the case, flip the values
  if (abs(adcValKeyDown) < abs(adcValKeyUp)) {
    adcValKeyDown = -abs(adcValKeyDown);
    adcValKeyUp = -abs(adcValKeyUp);
  } else {
    adcValKeyDown = abs(adcValKeyDown);
    adcValKeyUp = abs(adcValKeyUp);
  }
  
  // update thresholds
  noteOnThreshold = adcValKeyDown + 0.06 * (adcValKeyDown - adcValKeyUp);
  noteOffThreshold = adcValKeyDown - 0.5 * (adcValKeyDown - adcValKeyUp);
  keyResetThreshold = adcValKeyDown - 0.5 * (adcValKeyDown - adcValKeyUp);

  // gravity calculation
  // gravity in metres per microsecond^2
  float gravity_m = 9.81e-12 * gravityScaler;
  // gravity in mm per microsecond^2
  float gravity_mm = gravity_m * 1000;
  // gravity in adc bits per microsecond^2
  // hammer travel is in mm
  gravity = gravity_mm  / hammer_travel * (adcValKeyDown - adcValKeyUp);

  float maxHammerSpeed_bits_us = convert_m_s2bits_us(maxHammerSpeed_m_s);

  hammerSpeedScaler = velocityMapLength / maxHammerSpeed_bits_us;
}


void KeyHammer::toggleCalibration () {
  if (! calibrating) {
    c_mode = CalibMode::UP;
    calibrating = true;
    c_elapsedMS = 0;
    c_sample_t = 0;
  }
  else {
    calibrating = false;
    c_elapsedMS = 0;
    // add samples to stats object
    Array_Stats<float> downStats(c_reservoir, min(c_sample_t, c_sample_n));
    //  in later versions of Statistical, this is median()
    c_down_sample_med = downStats.Quartile(2);
    c_down_sample_std = downStats.Standard_Deviation();
    if ((abs(c_up_sample_med - c_down_sample_med) > (20 * c_up_sample_std)) && (c_sample_t >= c_sample_n)) {
      adcValKeyDown = c_down_sample_med;
      adcValKeyUp = c_up_sample_med;
      updatedKeyDownThreshold = true;
    } else {
      adcValKeyUp = c_up_sample_med;
      updatedKeyDownThreshold = false;
    }
  updateADCParams();
}
}

void KeyHammer::calibrationSample () {
  if (c_sample_t < c_sample_n) {
    c_reservoir[c_sample_t] = rawADC;
  } else {
    int m = random(0, c_sample_t + 1);
    if (m < c_sample_n) {
      c_reservoir[m] = rawADC;      
    }
  }
  c_sample_t += 1;
}

void KeyHammer::stepCalibration () {
  updateKey();
  updateElapsed();
  if (c_mode == CalibMode::UP) {
    calibrationSample();
    if (c_elapsedMS > 1000) {
      // use Array_Stats to calculate statistics
      Array_Stats<float> upStats(c_reservoir, min(c_sample_t, c_sample_n));
      //  in later versions of Statistical, this is median()
      c_up_sample_med = upStats.Quartile(2);
      c_up_sample_std = upStats.Standard_Deviation();
      c_mode = CalibMode::DOWN;
      c_sample_t = 0;
    }
  } else if (
            (rawADC > (c_up_sample_med + 15 * c_up_sample_std))
            or (rawADC < (c_up_sample_med - 15 * c_up_sample_std))
          ) {
    calibrationSample();
  }
  elapsedUS = 0;
}



void KeyHammer::updateElapsed () {
  elapsedUSBuffer.push(elapsedUS);
}

void KeyHammer::updateKey () {
  lastKeyPosition = keyPosition;
  rawADC = getAdcValue();
  adcBuffer.push(rawADC);
  keyPosition = applyFilter(adcBuffer, SavGolayFilters::posFilter, SavGolayFilters::posFilterLength);

}

void KeyHammer::updateKeySpeed () {
  keySpeed = applyFilter(adcBuffer, SavGolayFilters::speedFilter, SavGolayFilters::speedFilterLength) / (float)elapsedUSBuffer.last();
  // track the mean key speed since last indication of the start of a key press or 'strike'
  // that indication could be keySpeed > 0, along with one of...
  // - hammer key interaction
  // - hammer being less than the note on threshold
  // at this point, hammerKeyInteraction won't have been updated since the last iteration
  // if ((keySpeed < 0) && (hammerKeyInteraction)) {
  if ((keySpeed <= 0) && (!noteOnThresholdPassed)) {
    // reset meanStrikeKeySpeed
    meanStrikeKeySpeed = 0;
    meanStrikeKeySpeedSamples = 0;
  } else {
    // update meanStrikeKeySpeed
    meanStrikeKeySpeedSamples += 1;
    meanStrikeKeySpeed = keySpeed / meanStrikeKeySpeedSamples + meanStrikeKeySpeed * (meanStrikeKeySpeedSamples - 1) / meanStrikeKeySpeedSamples;
  }

}

void KeyHammer::updateHammer () {
  // TODO: position should be updated using the mean of old and new speeds
  // see circuitpy code
  hammerSpeed = hammerSpeed - gravity * elapsedUSBuffer.last();
  hammerPosition = hammerPosition + hammerSpeed * elapsedUSBuffer.last();
  // check for interaction with key
  if (hammerPosition < keyPosition) {
          hammerPosition = keyPosition;
          // we could check to see if the hammer speed is greater then key speed, but probably not necessary
          // after all, the key as 'caught up' to the hammer
          // if ((hammerSpeed > keySpeed) == (adcValKeyUp > adcValKeyDown)) {
            // if (abs(hammerSpeed) < abs(keySpeed)) {
          hammerSpeed = keySpeed;
          hammerKeyInteraction = true;
          // }
        // }
  } else {
    hammerKeyInteraction = false;
  }
  hammerPositionBuffer.push(hammerPosition);
}

void KeyHammer::checkNoteOn () {
  // check for note ons
  // do something with noteOnThresholdElapsedUS... set to 0 when hammer passes threshold for the first time noteOnThresholdPassed

  if (keyArmed && (hammerPosition > noteOnThreshold)) {
    // if this is the first time the hammer has passed the noteOnThreshold, start the clock
    if (! noteOnThresholdPassed) {
      noteOnThresholdElapsedUS = 0;
      noteOnThresholdPassed = true;
    }
    // we generate a note on after the hammer has passed the noteOnThreshold if
    // - more than 10ms have elapsed, or
    // - the key is no longer moving down
    if ((noteOnThresholdElapsedUS > 10000) || (keySpeed <= 0)) {
      // do something with hammer speed to get velocity
      velocity = hammerSpeed;
      // velocity = meanStrikeKeySpeed;
      velocityIndex = round(velocity * hammerSpeedScaler);
      velocityIndex = min(velocityIndex, velocityMapLength-1);
      // sometimes negative values for velocityIndex occur, probably due to a mismatch between thresholds and actual ADC range
      velocityIndex = max(velocityIndex, 0);
      midiSender->sendNoteOn(pitch, velocityMap[velocityIndex], 2);
      // useful when testing
      // midiSender->sendNoteOn(50 + noteCount % 12, 64, 2);
      noteOn = true;
      noteOnThresholdPassed = false;
      keyArmed = false;
      if (printMode == PRINT_NOTES){
        Serial.printf("\n ON-%d: hammerSpeed_bits_us %f, hammerSpeed_m_s %f, meanStrikeKeySpeed_m_s %f, meanKeySamples %d, velocity %d \n", pitch, velocity, convert_bits_us2m_s(velocity), convert_bits_us2m_s(meanStrikeKeySpeed), meanStrikeKeySpeedSamples, velocityMap[velocityIndex]);
      }
      // maybe print the buffer on note on?
      // could be useful for understanding adc/key/hammer behaviour
      bufferPrinted = false;
      noteOnElapsedUS = 0;
      lastNoteOnHammerSpeed = hammerSpeed;
      lastNoteOnVelocity = velocityMap[velocityIndex];
      noteCount++;
      hammerPosition = noteOnThreshold;
      hammerSpeed = -hammerSpeed;
    }
  }
}

void KeyHammer::checkNoteOff () {
  if (noteOn){
    if ((! keyArmed) && (keyPosition < keyResetThreshold)) {
      keyArmed = true;
      // reset hammer position / speed when key is re-armed
      hammerPosition = keyPosition;
      hammerSpeed = keySpeed;
    }

    if (keyPosition < noteOffThreshold) {
      midiSender->sendNoteOff(pitch, 64, 2);
      if (printMode == PRINT_NOTES){
        Serial.printf("OFF-%d: adcValue %d, keySpeed_bits_us %f, keySpeed_m_s \n", pitch, keySpeed, convert_bits_us2m_s(keySpeed));
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
    if (keyArmed) {
      updateHammer();
      checkNoteOn();
    }
    checkNoteOff();
    if ((printMode == PRINT_BUFFER) && (noteOnElapsedUS > 10000) && (!bufferPrinted)) {
      printBuffers();
      bufferPrinted = true;
    }
  }
  elapsedUS = 0;
}


void KeyHammer::step () {
  if (enabled) {
    iterationBuffer.push(iteration);
    if (calibrating) {
      stepCalibration();
    } else
    {
      stepHammer();
    }
    iteration++;
  }
}

int KeyHammer::getAdcValue () {
  if (adcValKeyUp < 0) {
    return -adcFnPtr();
  }
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

void KeyHammer::scaleFilterWeights(float* filter, size_t N) {
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


template <typename T, size_t bufferLength>
float KeyHammer::applyFilter(CircularBuffer<T, bufferLength>& buffer, const float* filter, size_t filterLength) {
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


void KeyHammer::printBuffers () {
  int delayUS = 15;
    for (int i = 0; i < (int)adcBuffer.size(); ++i) {
      Serial.printf("pitch:%d,", pitch);
      delayMicroseconds(delayUS);
      Serial.flush();
      Serial.printf("noteCount:%d,", noteCount);
      delayMicroseconds(delayUS);
      Serial.flush();
      Serial.printf("noteOnHammerSpeed:%f,", lastNoteOnHammerSpeed);
      delayMicroseconds(delayUS);
      Serial.flush();
      Serial.printf("noteOnVelocity:%d,", lastNoteOnVelocity);
      delayMicroseconds(delayUS);
      Serial.flush();
      Serial.printf("rawADC:%d,", adcBuffer[i]);
      delayMicroseconds(delayUS);
      Serial.flush();
      Serial.printf("hammerPosition:%f,", hammerPositionBuffer[i]);
      delayMicroseconds(delayUS);
      Serial.flush();
      Serial.printf("elapsedUs:%d,", elapsedUSBuffer[i]);
      delayMicroseconds(delayUS);
      Serial.flush();
      Serial.printf("iteration:%d,", iterationBuffer[i]);
      delayMicroseconds(delayUS);
      Serial.flush();
      Serial.printf("\n");
      delayMicroseconds(delayUS);
      Serial.flush();
    }
}

void KeyHammer::printKeyParams() {
  Serial.println("-- SETTINGS --");
  Serial.printf("pitch: %d\n", pitch);
  Serial.printf("adcValKeyDown: %d\n", adcValKeyDown);
  Serial.printf("adcValKeyUp: %d\n", adcValKeyUp);
  Serial.printf("noteOnThreshold: %d\n", noteOnThreshold);
  Serial.printf("noteOffThreshold: %d\n", noteOffThreshold);
  Serial.printf("keyResetThreshold: %d\n", keyResetThreshold);
  // results from calibration
  Serial.println("-- CALIBRATION --");
  Serial.printf("c_up_sample_med: %d \n", c_up_sample_med);
  Serial.printf("c_up_sample_std: %f \n", c_up_sample_std);
  Serial.printf("c_down_sample_med: %d \n", c_down_sample_med);
  Serial.printf("c_down_sample_std: %f \n", c_down_sample_std);
  Serial.printf("samples collected: %d \n", c_sample_t);
  Serial.printf("samples used: %d \n", c_sample_n);
  //updatedKeyDownThreshold
  Serial.printf("updatedKeyDownThreshold: %d \n", updatedKeyDownThreshold);
  Serial.flush();
}

float KeyHammer::convert_m_s2bits_us(float m_s) {
  // convert from m/s to bits/us
  float mm_s = m_s * 1000;
  return m_s * (adcValKeyDown - adcValKeyUp) * 1000 / 1e6  / hammer_travel;
}
//maxHammerSpeed_m_s / 1000 / 1e6;
float KeyHammer::convert_bits_us2m_s(float bits_us) {
  // convert from bits/us to m/s
  return bits_us * hammer_travel / (adcValKeyDown - adcValKeyUp) / 1000 * 1e6;
}
