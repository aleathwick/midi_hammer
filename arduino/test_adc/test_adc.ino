#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_MCP3008.h>

Adafruit_MCP3008 adc1;
Adafruit_MCP3008 adc2;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
//  SPI.setCS(5);
//  SPI.setSCK(2);
//  SPI.setTX(3);
//  SPI.setRX(4);
  adc1.begin(17);
  adc2.begin(15);
}

void loop() {
  static uint32_t start_ms = 0;
  if ( millis() - start_ms > 266 )
  {
    start_ms += 266;
//    Serial.print(adc.readADC(0));
//    Serial.printf("ADC channel = %d, value = %d   ADC channel = %d, value = %d \n", 0, adc1.readADC(0), 1, adc2.readADC(1));
    Serial.printf("%d, %d, %d, %d, %d, %d \n", adc1.readADC(0), adc1.readADC(1), adc1.readADC(2), adc2.readADC(0), adc2.readADC(1), adc2.readADC(2));
  }
}
