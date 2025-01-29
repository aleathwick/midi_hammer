#include "config.h"
#ifdef PICO
#include "MidiSenderPico.h"
// initialize midi
// tinyUSB library is used for USB MIDI
#include <Adafruit_TinyUSB.h>
// https://github.com/FortySevenEffects/arduino_midi_library
#include <MIDI.h>
Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

void MidiSenderPico::sendNoteOn(int pitch, int velocity, int channel) {
    MIDI.sendNoteOn(pitch, velocity, channel);
}

void MidiSenderPico::sendNoteOff(int pitch, int velocity, int channel) {
    MIDI.sendNoteOff(pitch, velocity, channel);
}

void MidiSenderPico::sendControlChange(int controlNumber, int controlValue, int channel) {
    MIDI.sendControlChange(controlNumber, controlValue, channel);
}

void MidiSenderPico::initialize() {
    usb_midi.setStringDescriptor("Laser Piano");

    // Initialize MIDI, and listen to all MIDI channels
    // This will also call usb_midi's begin()
    MIDI.begin(MIDI_CHANNEL_OMNI);

    // wait until device mounted
    while (!USBDevice.mounted()) delay(1);
}

void MidiSenderPico::loopEnd() {
    MIDI.read();
}
#endif
