#pragma once
#include "KeyHammer.h"

class Pedal : public KeyHammer {
private:
    int lastControlValue;
    int controlValue;
    
    
public:
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
    int controlNumber;

    Pedal(int(*adcFnPtr)(void), MidiSender* midiSender, int controlNumber = 64, 
          int adcValKeyDown = 430, int adcValKeyUp = 50)
        : KeyHammer(adcFnPtr, midiSender, 0, adcValKeyDown, adcValKeyUp, 0, 0),
          controlNumber(controlNumber), lastControlValue(0), controlValue(0) {
    }

    // Override the step method to implement pedal-specific behavior
    virtual void stepHammer() override;

private:
    // Pedal-specific processing of keyPosition
    void processPedalValue();
};
