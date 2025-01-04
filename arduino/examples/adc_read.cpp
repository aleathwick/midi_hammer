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
