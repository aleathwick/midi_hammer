// examples of different methods of ADC reading
#include <Adafruit_MCP3008.h>
#include <MCP3208.h>


//// MCP3008
Adafruit_MCP3008 adcs[3];
int adcCount = 3;
int adcSelects[] = { 26, 22, 17 };

// Then need the following code in setup():
// can set SPI default pins
 SPI.setCS(5);
 SPI.setSCK(2);
 SPI.setTX(3);
 SPI.setRX(4);
for (int i = 0; i < adcCount; i++) {
  adcs[i].begin(adcSelects[i]);
}

// Then the ADC fn looks like:
[]() -> int { return adcs[0].readADC(7); };

//// Built in ADC
[]() -> int { return analogRead(signal_pin); };


//// built in ADC with c4051 etc.

// function to update the states of the address and enable pins
void updateMuxAddress(int address_0, int address_1, int address_2) {
  // update address pins
  digitalWrite(addressPins[0], address_0);
  digitalWrite(addressPins[1], address_1);
  digitalWrite(addressPins[2], address_2);
}

// Function to read ADC value for a specific configuration
int readAdc(int signalPin, int address_0, int address_1, int address_2, int delay) {
  updateMuxAddress(address_0, address_1, address_2);
  // sleep for a bit to allow the mux to settle
  delayMicroseconds(delay);
  // delayNanoseconds only works on teensy
  // delayNanoseconds(1000);
  return analogRead(signalPin);
}

// then the ADC function looks like:
[]() -> int { return readAdc(signalPins[0], 0, 1, 1, settle_delay); }
