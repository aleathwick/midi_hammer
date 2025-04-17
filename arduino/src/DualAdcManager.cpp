#include "DualAdcManager.h"
#include <Arduino.h>

// Constructor
DualAdcManager::DualAdcManager() {
    _adcNeedsUpdate = true;
    
    #ifdef TEENSY
    _adc = NULL;
    #endif
}

// Initialize the ADC manager with specific pins
void DualAdcManager::begin(int addressPins0[], int addressPins1[], 
                            int signalPins[], int numSignalPins) {
    // Limit to maximum supported pins
    numSignalPins = min(numSignalPins, MAX_SIGNAL_PINS);
    for(uint8_t i = 0; i < N_ADDRESS_PINS; i++) {
        _addressPins0[i] = addressPins0[i];
        _addressPins1[i] = addressPins1[i];
    }
    
    for(uint8_t i = 0; i < numSignalPins; i++) {
        _signalPins[i] = signalPins[i];
    }
    
    // Configure address pins as outputs
    for(uint8_t i = 0; i < N_ADDRESS_PINS; i++) {
        pinMode(_addressPins0[i], OUTPUT);
        pinMode(_addressPins1[i], OUTPUT);
        digitalWrite(_addressPins0[i], LOW);
        digitalWrite(_addressPins1[i], LOW);
    }
    
    // Configure signal pins as inputs
    for(uint8_t i = 0; i < numSignalPins; i++) {
        pinMode(_signalPins[i], INPUT);
    }
    
    #ifdef TEENSY
    _adc = new ADC();
    // analogReadResolution(12);
    // adc->adc1->setResolution(16); // set bits of resolution
    // Configure both ADC modules for fast operation
    // default is averaging 4 samples, which takes 16-17us (with other settings default)
    // takes around 7us for 1 sample
    // analogReadAveraging(1);
    _adc->adc0->setAveraging(1);
    _adc->adc1->setAveraging(1);
      // it can be any of the ADC_CONVERSION_SPEED enum: VERY_LOW_SPEED, LOW_SPEED,
    // MED_SPEED, HIGH_SPEED_16BITS, HIGH_SPEED or VERY_HIGH_SPEED see the
    // documentation for more information additionally the conversion speed can
    // also be ADACK_2_4, ADACK_4_0, ADACK_5_2 and ADACK_6_2, where the numbers
    // are the frequency of the ADC clock in MHz and are independent on the bus
    // speed.
    _adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_HIGH_SPEED);
    _adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_HIGH_SPEED);
    // it can be any of the ADC_MED_SPEED enum: VERY_LOW_SPEED, LOW_SPEED,
    // MED_SPEED, HIGH_SPEED or VERY_HIGH_SPEED
    _adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED);
    _adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::VERY_HIGH_SPEED);
    #endif
}

// Set mux configuration
void DualAdcManager::setMuxConfig(int muxAddr0, int muxAddr1) {
    if (muxAddr0 != _currentMuxAddr0) {
        _currentMuxAddr0 = muxAddr0;
        _adcNeedsUpdate = true; // Mark for update
        for(uint8_t i = 0; i < N_ADDRESS_PINS; i++) {
            digitalWrite(_addressPins0[i], MUX_ADDRESSES[muxAddr0][i]);
        }
    }
    if (muxAddr1 != _currentMuxAddr1) {
        _currentMuxAddr1 = muxAddr1;
        _adcNeedsUpdate = true; // Mark for update
        for(uint8_t i = 0; i < N_ADDRESS_PINS; i++) {
            digitalWrite(_addressPins1[i], MUX_ADDRESSES[muxAddr1][i]);
        }
    }
}

void DualAdcManager::setAdcPinConfig(int signalPinIndex0, int signalPinIndex1) {
    if (signalPinIndex0 != _currentSignalPinIndex0) {
        _currentSignalPinIndex0 = signalPinIndex0;
        _adcNeedsUpdate = true; // Mark for update
    }
    if (signalPinIndex1 != _currentSignalPinIndex1) {
        _currentSignalPinIndex1 = signalPinIndex1;
        _adcNeedsUpdate = true; // Mark for update
    }
}

// Perform readings if needed
void DualAdcManager::updateReadings(int settleDelayUS) {
    // Wait for mux to settle
    // 1us is enough over 30cm long (26awg) cables and 5v powered 74hc4051, 3v powered 49e sensors
    // 2us needed for 3v powered hc4051
    // 5 or 6us needed for 3v powered 74hc4051 with 60cm long cables
    delayMicroseconds(settleDelayUS);

    ADC::Sync_result result = _adc->analogSynchronizedRead(_signalPins[_currentSignalPinIndex0], _signalPins[_currentSignalPinIndex1]);
    _lastValue0 = (int)result.result_adc0;
    _lastValue1 = (int)result.result_adc1;
    // non-synchronized read:
    // _lastValue0 = _adc->adc0->analogRead(_signalPins[_currentSignalPinIndex0]);
    // _lastValue1 = _adc->adc1->analogRead(_signalPins[_currentSignalPinIndex1]);
    _adcNeedsUpdate = false;
    _lastReadTimeUS = 0; // Reset elapsed time
}

// Get ADC value for specific channel and configuration
int DualAdcManager::readDualGetAdcValue0(
            uint8_t adcPinIndex0,
            uint8_t adcPinIndex1,
            int muxAddr0,
            int muxAddr1,
            int settleDelayUS) {
    setMuxConfig(muxAddr0, muxAddr1);
    setAdcPinConfig(adcPinIndex0, adcPinIndex1);
    if (_adcNeedsUpdate) {
        updateReadings(settleDelayUS);
    }
    return _lastValue0;
}

// Get ADC value for specific channel and configuration
int DualAdcManager::readDualGetAdcValue1(
            uint8_t adcPinIndex0,
            uint8_t adcPinIndex1,
            int muxAddr0,
            int muxAddr1,
            int settleDelayUS) {
    setMuxConfig(muxAddr0, muxAddr1);
    setAdcPinConfig(adcPinIndex0, adcPinIndex1);
    if (_adcNeedsUpdate) {
        updateReadings(settleDelayUS);
    }
    return _lastValue1;
}

// Function to create a reader for a single ADC channel
// int (*DualAdcManager::createAdcReader(uint8_t adcPinIndex0,
//             uint8_t adcPinIndex1,
//             int muxAddr0,
//             int muxAddr1,
//             int settleDelayUS))() {
// implementation
// }