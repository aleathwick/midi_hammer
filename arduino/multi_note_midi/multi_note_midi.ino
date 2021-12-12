// works with raspberry pico, using Earle Philhower's Raspberry Pico Arduino core:
// https://github.com/earlephilhower/arduino-pico
// needs Adafruit TinyUSB for usb midi
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <Adafruit_MCP3008.h>

// ADCs
Adafruit_MCP3008 adc1;
Adafruit_MCP3008 adc2;

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

int adc1Count = 2;
int adc2Count = 2;
int adc1Bits = 1024;
int adc2Bits = 1024;
int adc1Pins[] = {0,1};
int adc2Pins[] = {0,1};
int adc1Notes[] = {64,65};
int adc2Notes[] = {66,67};

void setup()
{

  // begin ADC
  // can set SPI default pins
  //  SPI.setCS(5);
  //  SPI.setSCK(2);
  //  SPI.setTX(3);
  //  SPI.setRX(4);
  adc1.begin(17);
  adc2.begin(15);

  pinMode(LED_BUILTIN, OUTPUT);
  
  //usb_midi.setStringDescriptor("TinyUSB MIDI");

  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);

  Serial.begin(115200);

  // wait until device mounted
  while( !USBDevice.mounted() ) delay(1);
}

void loop()
{
  static uint32_t start_ms = 0;
  if ( millis() - start_ms > 266 )
  {
    for (int i = 0; i < adc1Count; i++)
    {
      // send with velocity 0 for note off
      MIDI.sendNoteOn(adc1Notes[i], adc1.readADC(adc1Pins[i]) * 127 / adc1Bits, 1);
      Serial.print(adc1.readADC(adc1Pins[i]) * 127 / adc1Bits);
      Serial.print(" ");
      Serial.print(adc1.readADC(adc1Pins[i]));
      Serial.print(" ");
    }
    Serial.print("\n");
    
    // Serial.printf("%d, %d \n", adc1.readADC(0), adc1.readADC(1));
    
    start_ms += 266;

  }

  // read any new MIDI messages
  MIDI.read();  
}
