#pragma once

#include "config.h"

class MidiSender {
public:
    virtual void sendNoteOn(int pitch, int velocity, int channel) = 0;
    virtual void sendNoteOff(int pitch, int velocity, int channel) = 0;
    virtual void sendControlChange(int controlNumber, int controlValue, int channel) = 0;
    virtual void initialize() = 0;
    virtual void loopEnd() = 0;
    virtual ~MidiSender() = default;
};

