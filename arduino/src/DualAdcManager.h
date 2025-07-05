#pragma once

#include "config.h"
#include <elapsedMillis.h>

#ifdef TEENSY
#include "ADC.h"
#endif

// note that on teensy 4.1 some ADC pins are connected to both ADC0 and ADC1, some to only one
// see here: https://forum.pjrc.com/index.php?threads/teensy-4-1-adc-channels.72373/post-322241
// ADC0: 24, 25 (A10, A11)
// ADC1: 26, 27, 38, 39 (A12, A13, A14, A15)
// Both: 14-23 (A0-9), 40 & 41 (A16 & A17)
// The L/R ADCs for each sensor board should be split over the two ADCs
// e.g. UP3_L and UP3_R should be on ADC0 and ADC1 respectively

// Maximum number of signal pins supported
#define MAX_SIGNAL_PINS 16
#define N_ADDRESS_PINS 3

#if N_ADDRESS_PINS == 3
const int MUX_ADDRESSES[][N_ADDRESS_PINS] = {
    {0, 0, 0},
    {1, 0, 0},
    {0, 1, 0},
    {1, 1, 0},
    {0, 0, 1},
    {1, 0, 1},
    {0, 1, 1},
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

/**
 * @brief Manages teensy dual ADC usage in conjunction with multiplexers
 * 
 * This class handles two sets of mux address pins, and simultaneous reading from two
 * ADC channels. The idea is to use two ADCs to read from two groups of muxes at the
 * same time.
 * The second value can be cached, and used if the configuration is the same when the
 * value for the second ADC is requested.
 */
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
    /**
     * @brief Initialize the ADC manager with address and signal pins
     * 
     * @param addressPins0 Array of pins connected to address lines of first multiplexer(s)
     * @param addressPins1 Array of pins connected to address lines of second multiplexer(s)
     * @param signalPins Array of pins connected to ADC inputs
     * @param numSignalPins Number of signal pins
     */
    void begin(int addressPins0[], int addressPins1[], 
               int signalPins[], int numSignalPins);
    
    
    /**
     * @brief Set the configuration (chosen address) of the multiplexers
     * 
     * @param muxAddr0 Input channel number of the first multiplexer(s)
     * @param muxAddr1 Input channel number of the second multiplexer(s)
     */
    void setMuxConfig(int muxAddr0, int muxAddr1);

    /**
     * @brief Choose (by index) the ADC pins to read
     * 
     * @param signalPinIndex0 Index of pin to read using ADC0
     * @param signalPinIndex1 Index of pin to read using ADC1
     *  */ 
    void setAdcPinConfig(int signalPinIndex0, int signalPinIndex1);
    
    /**
     * @brief Update ADC readings using both ADCs simulataneously
     * 
     * This function will always read from both ADCs, even if the config is the same.
     * readDualGetAdcValue0 and readDualGetAdcValue1 will only read from the ADC if the 
     * configuration has changed, or if the last read was more than _validReadDurationUS ago.
     * 
     * @param settleDelayUS Delay in microseconds to wait for the ADC to settle
     */
    void updateReadings(int settleDelayUS);
    
    /**
     * @brief Read from both ADCs (if update required) and get the ADC value for ADC0
     * 
     * If values have recently been read with the same configuration, the cached value will be
     * returned. This allows two ADC values to be read at the same time, and the second value
     * used on the next read.
     * 
     * @param adcPinIndex0 Index of pin to read using ADC0
     * @param adcPinIndex1 Index of pin to read using ADC1
     * @param muxAddr0 Input channel number of the first multiplexer(s)
     * @param muxAddr1 Input channel number of the second multiplexer(s)
     * @param settleDelayUS Delay in microseconds to wait for the ADC to settle
     */
    int readDualGetAdcValue0(
        uint8_t adcPinIndex0,
        uint8_t adcPinIndex1,
        int muxAddr0,
        int muxAddr1,
        int settleDelayUS
    );
   /**
     * @brief Read from both ADCs (if update required) and get the ADC value for ADC1
     * 
     * See readDualGetAdcValue0 for details. This function is the same as readDualGetAdcValue0,
     * but returns the value for ADC1.
     */
    int readDualGetAdcValue1(
        uint8_t adcPinIndex0,
        uint8_t adcPinIndex1,
        int muxAddr0,
        int muxAddr1,
        int settleDelayUS
    );

    // getter functions for retrieving the last (cached) ADC values
    int getAdcValue1() { return _lastValue0; }
    int getAdcValue2() { return _lastValue1; }
    

    // Function to create a reader for a single ADC channel
    // Not implemented yet
    int (*createAdcReader(uint8_t adcPinIndex0, uint8_t adcPinIndex1, int addressValues[], int settleDelayUS))();
};
