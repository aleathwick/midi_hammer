// https://cplusplus.com/doc/tutorial/classes/
// https://paulmurraycbr.github.io/ArduinoTheOOWay.html
// https://www.circuitbasics.com/programming-with-classes-and-objects-on-the-arduino/

#pragma once
#include "config.h"
#include <CircularBuffer.hpp>
#include <elapsedMillis.h>
#include "MidiSender.h"
#include "Statistical.h"
#include "SavGolayFilters.h"

enum PrintMode {
  PRINT_NONE,
  PRINT_NOTES,
  PRINT_BUFFER
};

enum class CalibMode {
  UP,
  DOWN
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


    bool enabled = true;

    // define the range of the sensors, with adcValKeyDown being the key fully depressed
  // this will work regardless of adcValKeyDown < adcValKeyUp or adcValKeyUp < adcValKeyDown
  protected:
    MidiSender* midiSender;
    int adcValKeyDown;
    int adcValKeyUp;
    float keyPosition;
    // key and hammer speeds are measured in adc bits per microsecond
    float keySpeed;
  private:
    int noteOnThreshold;
    // threshold for key to trigger noteoff
    int noteOffThreshold;
    // threshold for key to reset
    int keyResetThreshold;
    int sensorMin;
    int sensorMax;
    
    int rawADC;
    int lastKeyPosition;

    float hammerPosition;
    float hammerSpeed;
    float hammer_travel;
    float maxHammerSpeed_m_s;
    float gravity;
    // scale gravity applied to hammer, e.g. 0.1 will be 10% of 'normal' gravity
    float gravityScaler = 0.5;

    bool noteOn;
    bool keyArmed;
    float velocity;
    int velocityIndex;

    // used to put hammer speed on an appropriate scale for indexing into velocityMap
    float hammerSpeedScaler;

    // track number of simulation iterations
    int iteration = 0;

    bool bufferPrinted = false;
    float lastNoteOnHammerSpeed;
    int lastNoteOnVelocity = -1;
    int noteCount = 0;

    // whether or not to print note on/offs
    PrintMode printMode;
    
    // fn to scale the weights of a filter so they sum to 1
    void scaleFilterWeights(float* filter, size_t N);
    // fn to apply a filter to the most recent samples in a circular buffer
    template <typename T, size_t bufferLength>
    float KeyHammer::applyFilter(CircularBuffer<T, bufferLength>& buffer, const float* filter, size_t filterLength);

    // calibration related
    int c_sample_n = 100;
    float c_reservoir[100];
    int c_sample_t;
    bool calibrating = false;
    CalibMode c_mode;
    int c_start;
    int c_up_sample_med;
    float c_up_sample_std;
    int c_down_sample_med;
    float c_down_sample_std;

    bool updatedKeyDownThreshold = false;

    elapsedMillis c_elapsedMS;
    void stepCalibration();
    void calibrationSample();
    void updateADCParams();
    void updateKeySpeed();
    void updateHammer();
    void checkNoteOn();
    void checkNoteOff();
    void stepKey();
    void printBuffers();
    float convert_m_s2bits_us(float m_s);
    float convert_bits_us2m_s(float bits_us);
    
  protected:
    void updateElapsed();
    void updateKey();
    virtual void stepHammer();
  

  public:
    KeyHammer(int(*adcFnPtr)(void), MidiSender* midiSender,int pitch, int adcValKeyDown, int adcValKeyUp, float hammer_travel, float maxHammerSpeed_m_s);
    void step();
    // operation mode switches between operation as a hammer simulation key, a key, or a pedal
    int getAdcValue(void);
    // generateVelocityMap is used to fill values in for a blank velocity map.
    // In future, the plan is to keep a ref to a velocity map as a class member. generateVelocityMap
    // would then take in a parameter to determine the base of the logarithm used for stretching,
    // and probably also assign the new velocity map to the ref (or return, and use a setter fn).
    // A getter fn would also exist for obtaining a newly generated velocity map from a 
    // KeyHammer object and apply it to another.
    void generateVelocityMap();
    int controlNumber;
    void toggleCalibration();

    elapsedMicros elapsedUS;
    // for keeping track of time since last note on
    elapsedMicros noteOnElapsedUS;
    // for keeping track of time since noteOnThreshold passed by hammer
    elapsedMicros noteOnThresholdElapsedUS;
    // to check if we have generated a note on since the last time the hammer passed the noteOnThreshold
    bool noteOnThresholdPassed = false;

    // indicates whether or not the hammer is in contact with the key
    bool hammerKeyInteraction = false;
    // keep the average key speed since the beginning of the press or 'strike', updated iteratively
    // this is an alternative way of determining note on velocity that showed promise when analysing
    // data form key presses, but currently is not used, since sav golay with hammer speed works quite well anyway
    float meanStrikeKeySpeed = 0;
    // keep track of n samples contributing to the mean key strike speed, enabling iterative update
    int meanStrikeKeySpeedSamples = 0;

    float getKeyPosition() const { return keyPosition; }
    float getKeySpeed() const { return keySpeed; }
    float getHammerPosition() const { return hammerPosition; }
    float getHammerSpeed() const { return hammerSpeed; }
    int getRawADC() const { return rawADC; }
    int getElapsedUS() const { return elapsedUSBuffer.last(); }
    
    void setPrintMode(PrintMode mode) { printMode = mode; }
    void printKeyParams(); // includes calibration results

    // int getPitch() const { return pitch; }
    int pitch;

    // functions for enabling/disabling the key
    void enable() { enabled = true; }
    void disable() { enabled = false; }
    bool isEnabled() const { return enabled; }

    // setters/getters for adcValKeyUp and adcValKeyDown
    void setAdcValKeyUp(int value) { adcValKeyUp = value; updateADCParams(); }
    void setAdcValKeyDown(int value) { adcValKeyDown = value; updateADCParams(); }
    int getAdcValKeyUp() const { return adcValKeyUp; }
    int getAdcValKeyDown() const { return adcValKeyDown; }

};
