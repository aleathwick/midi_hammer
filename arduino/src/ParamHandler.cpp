#include "ParamHandler.h"

ParamHandler::ParamHandler() {
    adcValKeyDown = nullptr;
    adcValKeyUp = nullptr;
    numKeys = 0;
    initialized = false;
}

ParamHandler::~ParamHandler() {
    if (adcValKeyDown != nullptr) {
        delete[] adcValKeyDown;
    }
    if (adcValKeyUp != nullptr) {
        delete[] adcValKeyUp;
    }
}

bool ParamHandler::initialize(int n_keys) {
    // Clean up any existing data
    if (adcValKeyDown != nullptr) {
        delete[] adcValKeyDown;
    }
    if (adcValKeyUp != nullptr) {
        delete[] adcValKeyUp;
    }
    
    // Allocate memory for new parameters
    numKeys = n_keys;
    adcValKeyDown = new int[numKeys];
    adcValKeyUp = new int[numKeys];
    
    // Initialize with defaults or ADC_VALUE_NOT_SET to indicate not set
    for (int i = 0; i < numKeys; i++) {
        adcValKeyDown[i] = ADC_VALUE_NOT_SET;
        adcValKeyUp[i] = ADC_VALUE_NOT_SET;
    }
    
    // Initialize SD card if not already done
    if (!SD.begin(chipSelect)) {
        Serial.println("SD card initialization failed");
        return false;
    }
    
    // Check if parameter file exists and read it
    if (SD.exists(keyParamsFile)) {
        // Create CSV_Parser with two columns of integers, with header
        CSV_Parser cp("ddd", true, ',');
        
        if (cp.readSDfile(keyParamsFile)) {
            int16_t* keyIndices = (int16_t*)cp["key_index"];
            int16_t* csvKeyDownValues = (int16_t*)cp["key_down_adc"];
            int16_t* csvKeyUpValues = (int16_t*)cp["key_up_adc"];
            
            if (keyIndices && csvKeyDownValues && csvKeyUpValues) {
                int rowCount = cp.getRowsCount();
                
                for (int row = 0; row < rowCount; row++) {
                    int keyIndex = keyIndices[row];
                    Serial.printf("Key %d: KeyDown: %d, KeyUp: %d\n", keyIndex, csvKeyDownValues[row], csvKeyUpValues[row]);
                    
                    // Only store values if index is within bounds
                    if (keyIndex >= 0 && keyIndex < numKeys) {
                        adcValKeyDown[keyIndex] = csvKeyDownValues[row];
                        adcValKeyUp[keyIndex] = csvKeyUpValues[row];
                    }
                }
            } else {
                Serial.println("Error parsing parameter file structure");
            }
        } else {
            Serial.println("Could not read parameter file");
        }
    }
    
    initialized = true;
    return true;
}

bool ParamHandler::existsAdcValKeyDown(int keyIndex) {
    if (!initialized || keyIndex < 0 || keyIndex >= numKeys) {
        return false;
    }
    return adcValKeyDown[keyIndex] != ADC_VALUE_NOT_SET;
}

bool ParamHandler::existsAdcValKeyUp(int keyIndex) {
    if (!initialized || keyIndex < 0 || keyIndex >= numKeys) {
        return false;
    }
    return adcValKeyUp[keyIndex] != ADC_VALUE_NOT_SET;
}

int ParamHandler::getAdcValKeyDown(int keyIndex) {
    if (!initialized || keyIndex < 0 || keyIndex >= numKeys) {
        return ADC_VALUE_NOT_SET;
    }
    return adcValKeyDown[keyIndex];
}

int ParamHandler::getAdcValKeyUp(int keyIndex) {
    if (!initialized || keyIndex < 0 || keyIndex >= numKeys) {
        return ADC_VALUE_NOT_SET;
    }
    return adcValKeyUp[keyIndex];
}

void ParamHandler::setAdcValKeyDown(int keyIndex, int value) {
    if (!initialized || keyIndex < 0 || keyIndex >= numKeys) {
        return;
    }
    adcValKeyDown[keyIndex] = value;
}

void ParamHandler::setAdcValKeyUp(int keyIndex, int value) {
    if (!initialized || keyIndex < 0 || keyIndex >= numKeys) {
        return;
    }
    adcValKeyUp[keyIndex] = value;
}

bool ParamHandler::writeParams() {
    if (!initialized) {
        return false;
    }
    
    // Remove existing file
    if (SD.exists(keyParamsFile)) {
        SD.remove(keyParamsFile);
    }
    
    // Create new file
    File dataFile = SD.open(keyParamsFile, FILE_WRITE);
    if (!dataFile) {
        Serial.println("Could not create parameter file");
        return false;
    }
    
    // Write header
    dataFile.println("key_index,key_down_adc,key_up_adc");
    
    // Write params
    for (int i = 0; i < numKeys; i++) {
        if (adcValKeyDown[i] != ADC_VALUE_NOT_SET || adcValKeyUp[i] != ADC_VALUE_NOT_SET) {
            dataFile.print(i);
            dataFile.print(",");
            dataFile.print(adcValKeyDown[i]);
            dataFile.print(",");
            dataFile.println(adcValKeyUp[i]);
        }
    }
    
    dataFile.close();
    return true;
}