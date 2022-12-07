// https://cplusplus.com/doc/tutorial/classes/
// https://paulmurraycbr.github.io/ArduinoTheOOWay.html
// https://www.circuitbasics.com/programming-with-classes-and-objects-on-the-arduino/

#pragma once
#include <elapsedMillis.h>
#include "MIDI.h"

class KeyHammer
{
    // a pointer to a function that will return the position of the key
    // see here: https://forum.arduino.cc/t/function-as-a-parameter-in-class-object-function-pointer-in-library/461967/7
    int(*adcFnPtr)(void);
    // define the range of the sensors, with sensorFullyOn being the key fully depressed
  // this will work regardless of sensorFullyOn < sensorFullyOff or sensorFullyOff < sensorFullyOn
    int sensorFullyOn;
    int sensorFullyOff;
    // threshold for hammer to activate note
    int noteOnThreshold;
    // threshold for key to trigger noteoff
    int noteOffThreshold;
    int sensorMin;
    int sensorMax;

    int pin;
    int pitch;
    
    int rawADC;
    int keyPosition;
    int lastKeyPosition;
    // key and hammer speeds are measured in adc bits per microsecond
    double keySpeed;

    double hammerPosition;
    double hammerSpeed;
    double hammer_travel;
    double gravity;

    bool noteOn;
    double velocity;
    int velocityIndex;

    // parameters for pedal mode
    int lastControlValue;
    int controlValue;

    char printMode;


    void update_key();
    void update_keyspeed();
    void update_hammer();
    void check_note_on();
    void check_note_off();
    void test();
    void step_hammer();
    void step_key();
    void step_pedal();

  public:
    KeyHammer(int(*adcFnPtr)(void), int pin, int pitch, char operationMode, int sensorFullyOn, int sensorFullyOff);
    void step();
    // operation mode switches between operation as a hammer simulation key, a key, or a pedal
    char operationMode;
    int getAdcValue(void);

    elapsedMicros elapsed;
};
