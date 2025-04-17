#pragma once

#include "config.h"
#include <elapsedMillis.h>

#ifdef TEENSY
#include "ADC.h"
#endif

// note that on teensy 4.1 some ADC pins are connected to both ADC0 and ADC1, some to only one
// see here: https://forum.pjrc.com/index.php?threads/teensy-4-1-adc-channels.72373/post-322241
// ADC0: 24, 25
// ADC1: 26, 27, 38, 39
// Both: 14-23, 40 & 41

// Maximum number of signal pins supported
#define MAX_SIGNAL_PINS 16
#define N_ADDRESS_PINS 3

#if N_ADDRESS_PINS == 3
const int MUX_ADDRESSES[][N_ADDRESS_PINS] = {
    {0, 0, 0},
    {0, 0, 1},
    {0, 1, 0},
    {0, 1, 1},
    {1, 0, 0},
    {1, 0, 1},
    {1, 1, 0},
    {1, 1, 1}
};
#elif N_ADDRESS_PINS == 4
const int MUX_ADDRESSES[][N_ADDRESS_PINS] = {
    {0, 0, 0, 0},
    {0, 0, 0, 1},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 1, 0, 0},
    {0, 1, 0, 1},
    {0, 1, 1, 0},
    {0, 1, 1, 1},
    {1, 0, 0, 0},
    {1, 0, 0, 1},
    {1, 0, 1, 0},
    {1, 0, 1, 1},
    {1, 1, 0, 0},
    {1, 1, 0, 1},
    {1, 1, 1, 0},
    {1, 1, 1, 1}
};
#endif

// should really make this a singleton class
class DualAdcManager {
private:
    
    // Arrays of pins
    int _addressPins0[N_ADDRESS_PINS];
    int _addressPins1[N_ADDRESS_PINS];
    int _signalPins[MAX_SIGNAL_PINS];

    // indices of signal pins
    uint8_t _signalPinIndex0;
    uint8_t _signalPinIndex1;
    // Number of address pins
    uint8_t _numAddressPins;
    
    int _lastValue0;
    int _lastValue1;
    int _currentSignalPinIndex0;
    int _currentSignalPinIndex1;
    
    // Current address configuration
    int _currentMuxAddr0 = -1;
    int _currentMuxAddr1 = -1;
    
    // Flag to track if values need updating
    bool _adcNeedsUpdate;

    // time since last ADC read
    elapsedMicros _lastReadTimeUS;
    
    // duration for which ADC read is valid
    int _validReadDurationUS = 500;
    
    #ifdef TEENSY
    ADC* _adc;
    #endif
    
public:
    DualAdcManager();
    // Initialize the ADC manager with specific pins
    void begin(int addressPins0[], int addressPins1[], 
               int signalPins[], int numSignalPins);
    
    // Set mux configuration using an array of address values
    void setMuxConfig(int muxAddr0, int muxAddr1);

    // Set ADC pin configuration (which pins to read)
    void setAdcPinConfig(int signalPinIndex0, int signalPinIndex1);
    
    // Perform readings if needed
    void updateReadings(int settleDelayUS);
    
    // Get ADC value for specific channel and configuration
    // Reads values for two pins, returns the first pin
    int readDualGetAdcValue0(
        uint8_t adcPinIndex0,
        uint8_t adcPinIndex1,
        int muxAddr0,
        int muxAddr1,
        int settleDelayUS
    );
    int readDualGetAdcValue1(
        uint8_t adcPinIndex0,
        uint8_t adcPinIndex1,
        int muxAddr0,
        int muxAddr1,
        int settleDelayUS
    );

    int getAdcValue1() { return _lastValue0; }
    int getAdcValue2() { return _lastValue1; }
    

    // Function to create a reader for a single ADC channel
    int (*createAdcReader(uint8_t adcPinIndex0, uint8_t adcPinIndex1, int addressValues[], int settleDelayUS))();
};
