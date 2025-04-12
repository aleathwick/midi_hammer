#include "Pedal.h"

void Pedal::stepHammer() {
    // Reuse parent's functionality to update key position
    updateKey();
    updateElapsed();
    // Process pedal-specific logic
    processPedalValue();
    
}

void Pedal::processPedalValue() {
    lastControlValue = controlValue;
    
    // Calculate control value based on key position
    controlValue = (int)((keyPosition - adcValKeyUp) / 
                   float(adcValKeyDown - adcValKeyUp) * 127);
    controlValue = constrain(controlValue, 0, 127);
    
    // Send MIDI control change message if value changed
    if (controlValue != lastControlValue) {
        midiSender->sendControlChange(controlNumber, controlValue, 2);
    }
}
