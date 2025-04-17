#ifndef PARAM_HANDLER_H
#define PARAM_HANDLER_H

#include <config.h>
#include <SD.h>
#include <SPI.h>
#include <CSV_Parser.h>

class ParamHandler {
private:
    // In-memory cache of parameters
    int* adcValKeyDown;
    int* adcValKeyUp;
    int numKeys;
    bool initialized;
    
    // null / not set value
    static const int ADC_VALUE_NOT_SET = -9999;

    // File paths
    const char* keyParamsFile = "/keyParams.csv";
    
    // SD card chip select pin
    #ifdef TEENSY
        const int chipSelect = BUILTIN_SDCARD;
    #else
        const int chipSelect = 10;
    #endif
    
public:
    ParamHandler();
    ~ParamHandler();
    
    // Initialize SD card and parameters
    bool initialize(int n_keys);
    
    // Key parameter getters
    bool existsAdcValKeyDown(int keyIndex);
    bool existsAdcValKeyUp(int keyIndex);
    int getAdcValKeyDown(int keyIndex);
    int getAdcValKeyUp(int keyIndex);
    
    // Key parameter setters
    void setAdcValKeyDown(int keyIndex, int value);
    void setAdcValKeyUp(int keyIndex, int value);
    
    // Write parameters to SD
    bool writeParams();
};

#endif // PARAM_HANDLER_H