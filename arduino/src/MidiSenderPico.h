#pragma once

#include "MidiSender.h"

class MidiSenderPico : public MidiSender {
public:
    void sendNoteOn(int pitch, int velocity, int channel) override;
    void sendNoteOff(int pitch, int velocity, int channel) override;
    void sendControlChange(int controlNumber, int controlValue, int channel) override;
    void initialize() override;
    void loopEnd() override;
};
