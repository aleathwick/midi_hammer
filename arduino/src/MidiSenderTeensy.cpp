#include "config.h"
#ifdef TEENSY
#include "MidiSenderTeensy.h"
// seems like including usb_midi.h is necessary if not in the main .ino file?
// Otherwise it doesn't know what usbMIDI is
#include <usb_midi.h>

// see here for teensy midi example (and suggested use of 74hc405/1):
// https://www.pjrc.com/teensy/td_midi.html

void MidiSenderTeensy::sendNoteOn(int pitch, int velocity, int channel) {
    usbMIDI.sendNoteOn(pitch, velocity, channel);
}

void MidiSenderTeensy::sendNoteOff(int pitch, int velocity, int channel) {
    usbMIDI.sendNoteOff(pitch, velocity, channel);
}

//?
void MidiSenderTeensy::sendControlChange(int controlNumber, int controlValue, int channel) {
    usbMIDI.sendControlChange(controlNumber, controlValue, channel);
}

void MidiSenderTeensy::initialize() {
    // no need to do anything here
}

void MidiSenderTeensy::loopEnd() {
    while (usbMIDI.read()) {
    }
}
#endif
