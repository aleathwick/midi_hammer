// works with raspberry pico, using Earle Philhower's Raspberry Pico Arduino core:
// https://github.com/earlephilhower/arduino-pico
// needs Adafruit TinyUSB for usb midi
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <Adafruit_MCP3008.h>

// ADCs
Adafruit_MCP3008 adcs[2];

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

int adcCount = 2; // n adcs
int adcBits = 1024;
int adcSelects[] = {17, 15}; // pins for selecting adcs
int adcSensorCounts[] = {2,2}; // sensors per adc
// last dimension must be of size max(adcSensorCounts)
int adcPins[][2] = {{0,1},
                  {0,1}};
int adcNotes[][2] = {{64,65},
                  {66,67}};

void setup()
{
  Serial.begin(115200);
  // begin ADC
  // can set SPI default pins
  //  SPI.setCS(5);
  //  SPI.setSCK(2);
  //  SPI.setTX(3);
  //  SPI.setRX(4);
  for (int i = 0; i < adcCount; i++)
  {
    adcs[i].begin(adcSelects[i]);
  }

  pinMode(LED_BUILTIN, OUTPUT);
  
  usb_midi.setStringDescriptor("Laser Piano");

  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);

  

  // wait until device mounted
  while( !USBDevice.mounted() ) delay(1);
}

void loop()
{
  static uint32_t start_ms = 0;
  if ( millis() - start_ms > 266 )
  {
    for (int i = 0; i < adcCount; i++)
    {    
      for (int j = 0; j < adcSensorCounts[i]; j++)
      {
        // use sendNoteOff for note offs
        MIDI.sendNoteOn(adcNotes[i][j], adcs[i].readADC(adcPins[i][j]) * 127 / adcBits, 1);
        Serial.print(adcs[i].readADC(adcPins[i][j]));
        Serial.print(" ");
      }
    }
    Serial.print("\n");
    // Serial.printf("%d, %d \n", adc1.readADC(0), adc1.readADC(1));
    
    start_ms += 266;

  }

  // read any new MIDI messages
  MIDI.read();  
}
