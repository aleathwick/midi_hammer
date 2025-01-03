#include "MidiSender.h"
// initialize midi
#ifdef PICO
    #include <Adafruit_TinyUSB.h>
    Adafruit_USBD_MIDI usb_midi;
    MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);
#endif
// see here for teensy midi example (and suggested use of 74hc405/1):
// https://www.pjrc.com/teensy/td_midi.html
// #elif defined(TEENSY)
// I don't THINK this is necessary for teensy
//     #include <usb_midi.h>
//     MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
// #endif

void MidiSender::sendNoteOn(int pitch, int velocity, int channel) {
    #ifdef PICO
        MIDI.sendNoteOn(pitch, velocity, channel);
    #elif defined(TEENSY)
        usbMIDI.sendNoteOn(pitch, velocity, channel);
    #endif
}

void MidiSender::sendNoteOff(int pitch, int velocity, int channel) {
    #ifdef PICO
        MIDI.sendNoteOff(pitch, velocity, channel);
    #elif defined(TEENSY)
        usbMIDI.sendNoteOff(pitch, velocity, channel);
    #endif
}

void MidiSender::initialize() {
    #ifdef PICO
        usb_midi.setStringDescriptor("Laser Piano");

        // Initialize MIDI, and listen to all MIDI channels
        // This will also call usb_midi's begin()
        MIDI.begin(MIDI_CHANNEL_OMNI);

        // wait until device mounted
        while (!USBDevice.mounted()) delay(1);
    #elif defined(TEENSY)
        // Set the USB MIDI product name for Teensy
        usbMIDI.setHandleName("Laser Piano");
    #endif
