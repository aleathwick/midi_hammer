// https://cplusplus.com/doc/tutorial/classes/
// https://paulmurraycbr.github.io/ArduinoTheOOWay.html
// https://www.circuitbasics.com/programming-with-classes-and-objects-on-the-arduino/

#pragma once
#include "config.h"
#include <CircularBuffer.hpp>
#include <elapsedMillis.h>
#include "MidiSender.h"

enum PrintMode {
  PRINT_NONE,
  PRINT_NOTES,
  PRINT_BUFFER
};

class KeyHammer
{
    // a pointer to a function that will return the position of the key
    // see here: https://forum.arduino.cc/t/function-as-a-parameter-in-class-object-function-pointer-in-library/461967/7
    int(*adcFnPtr)(void);
    // a circular buffer to store the last n adc values
    CircularBuffer<int, BUFFER_SIZE> adcBuffer;
    CircularBuffer<float, BUFFER_SIZE> hammerPositionBuffer;
    CircularBuffer<int, BUFFER_SIZE> elapsedUSBuffer;
    CircularBuffer<int, BUFFER_SIZE> iterationBuffer;

    // filters should be in dot product order, i.e. ordered like buffers, which is oldest to newest
    static const int posFilterLength = 18;
    float keyPosFilter[posFilterLength] = {
      -0.09356725, -0.07602339, -0.05847953, -0.04093567, -0.02339181,
      -0.00584795,  0.01169591,  0.02923977,  0.04678363,  0.06432749,
       0.08187135,  0.0994152 ,  0.11695906,  0.13450292,  0.15204678,
       0.16959064,  0.1871345 ,  0.20467836};
    static const int speedFilterLength = 18;
    float keySpeedFilter[speedFilterLength] = {
      -0.01754386, -0.01547988, -0.01341589, -0.01135191, -0.00928793,
      -0.00722394, -0.00515996, -0.00309598, -0.00103199,  0.00103199,
       0.00309598,  0.00515996,  0.00722394,  0.00928793,  0.01135191,
       0.01341589,  0.01547988,  0.01754386};

    MidiSender* midiSender;
    // define the range of the sensors, with sensorFullyOn being the key fully depressed
  // this will work regardless of sensorFullyOn < sensorFullyOff or sensorFullyOff < sensorFullyOn
    int sensorFullyOn;
    int sensorFullyOff;
    // threshold for hammer to activate note
    int noteOnThreshold;
    // threshold for key to trigger noteoff
    int noteOffThreshold;
    // threshold for key to reset
    int keyResetThreshold;
    int sensorMin;
    int sensorMax;

    int pitch;
    
    int rawADC;
    float keyPosition;
    int lastKeyPosition;
    // key and hammer speeds are measured in adc bits per microsecond
    float keySpeed;

    float hammerPosition;
    float hammerSpeed;
    float hammer_travel;
    float gravity;

    bool noteOn;
    bool keyArmed;
    float velocity;
    int velocityIndex;

    // this and over will result in velocity of 127
    // 0.06 about right for piano, foot drum is more like 0.04
    // measured in adc bits per microsecond
    int minPressUS;
    // used to put hammer speed on an appropriate scale for indexing into velocityMap
    float hammerSpeedScaler;

    // parameters for pedal mode
    int lastControlValue;
    int controlValue;
    // track number of simulation iterations
    int iteration = 0;

    bool bufferPrinted = false;
    float lastNoteOnHammerSpeed;
    int lastNoteOnVelocity = -1;
    int noteCount = 0;

    // whether or not to print note on/offs
    PrintMode printMode;
    
    // fn to scale the weights of a filter so they sum to 1
    template <size_t N>
    void scaleFilterWeights(float (&filter)[N]);
    // fn to apply a filter to the most recent samples in a circular buffer
    template <typename T, size_t bufferLength, size_t filterLength>
    float KeyHammer::applyFilter(CircularBuffer<T, bufferLength>& buffer, float (&filter)[filterLength]);

    void updateElapsed();
    void updateKey();
    void updateKeySpeed();
    void updateHammer();
    void checkNoteOn();
    void checkNoteOff();
    void test();
    void stepHammer();
    void stepKey();
    void stepPedal();
    void printBuffers();

  public:
    KeyHammer(int(*adcFnPtr)(void), MidiSender* midiSender,int pitch, char operationMode, int sensorFullyOn, int sensorFullyOff, float hammer_travel, int minPressUS);
    void step();
    // operation mode switches between operation as a hammer simulation key, a key, or a pedal
    char operationMode;
    int getAdcValue(void);
    // generateVelocityMap is used to fill values in for a blank velocity map.
    // In future, the plan is to keep a ref to a velocity map as a class member. generateVelocityMap
    // would then take in a parameter to determine the base of the logarithm used for stretching,
    // and probably also assign the new velocity map to the ref (or return, and use a setter fn).
    // A getter fn would also exist for obtaining a newly generated velocity map from a 
    // KeyHammer object and apply it to another.
    void generateVelocityMap();
    int controlNumber;
    void printState();

    elapsedMicros elapsedUS;
    // for keeping track of time since last note on
    elapsedMicros noteOnElapsedUS;
};
