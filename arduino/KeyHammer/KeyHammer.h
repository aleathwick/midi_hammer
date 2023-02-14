// https://cplusplus.com/doc/tutorial/classes/
// https://paulmurraycbr.github.io/ArduinoTheOOWay.html
// https://www.circuitbasics.com/programming-with-classes-and-objects-on-the-arduino/

#pragma once
#include <elapsedMillis.h>
#include "MIDI.h"
#include <Adafruit_TinyUSB.h>
// the issue of defining the MIDI interface in a header file is raised here:
// https://github.com/FortySevenEffects/arduino_midi_library/issues/165
// Define the MIDI interface
extern MIDI_NAMESPACE::MidiInterface<midi::SerialMIDI<Adafruit_USBD_MIDI>> MIDI; 

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

    // this and over will result in velocity of 127
    // 0.06 about right for piano, foot drum is more like 0.04
    // measured in adc bits per microsecond
    int minPressUS;
    // used to put hammer speed on an appropriate scale for indexing into velocityMap
    double hammerSpeedScaler;

    // parameters for pedal mode
    int lastControlValue;
    int controlValue;

    // whether or not to print note on/offs
    bool printNotes;


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
    KeyHammer(int(*adcFnPtr)(void), int pitch, char operationMode, int sensorFullyOn, int sensorFullyOff, double hammer_travel, int minPressUS);
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

    elapsedMicros elapsed;
};
