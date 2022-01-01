// works with raspberry pico, using Earle Philhower's Raspberry Pico Arduino core:
// https://github.com/earlephilhower/arduino-pico
// needs Adafruit TinyUSB for usb midi
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_MCP3008.h>
// MIDI Library: https://github.com/FortySevenEffects/arduino_midi_library
#include <MIDI.h>
// elapsedMillis: https://github.com/pfeerick/elapsedMillis
#include <elapsedMillis.h>


// ADCs
Adafruit_MCP3008 adcs[2];

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);


// ADC and sensor set up
const int adcCount = 2;  // n adcs
const int adcBits = 1024;
int adcSelects[] = { 17, 15 };     // pins for selecting adcs
int adcSensorCounts[] = { 3, 3 };  // sensors per adc
// last dimension must be at least of size max(adcSensorCounts)
// may as well make it 8, the max possible
int adcPins[][8] = { { 0, 1, 2 },
                     { 0, 1, 2 } };
int adcNotes[][8] = { { 64, 65, 66 },
                      { 66, 67, 68 } };
// define the range of the sensors, with sensorFullyOn being the key fully depressed
const int sensorFullyOn = 128;
const int sensorFullyOff = 512;

// simulation parameters
// threshold for hammer to activate note
const int noteOnThreshold = sensorFullyOn + 0.1 * (sensorFullyOn - sensorFullyOff);
// threshold for key to trigger noteoff
const int noteOffThreshold = sensorFullyOn - 0.5 * (sensorFullyOn - sensorFullyOff);
// gravity for hammer
double gravity = (sensorFullyOn - sensorFullyOff) / (double)10000000;

// keeping track of simulation states
// key positions
int adcValues[adcCount][8] = {0};
int lastAdcValues[adcCount][8] = {0};
double keySpeed = 0;
double hammerPositions[adcCount][8] = {0};
double hammerSpeeds[adcCount][8] = {0};
bool noteOn[adcCount][8] = {0};
// can turn to int like so: int micros = microTimer[i][j];
// and reset to zero: microTimer[i][j] = 0;
elapsedMicros microTimer[adcCount][8];

void setup() {
  Serial.begin(115200);
  // begin ADC
  // can set SPI default pins
  //  SPI.setCS(5);
  //  SPI.setSCK(2);
  //  SPI.setTX(3);
  //  SPI.setRX(4);
  for (int i = 0; i < adcCount; i++) {
    adcs[i].begin(adcSelects[i]);
  }

  pinMode(LED_BUILTIN, OUTPUT);

  usb_midi.setStringDescriptor("Laser Piano");

  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);



  // wait until device mounted
  while (!USBDevice.mounted()) delay(1);
}

void loop() {
  static uint32_t start_ms = 0;
  if (millis() - start_ms > 266) {
    for (int i = 0; i < adcCount; i++) {
      for (int j = 0; j < adcSensorCounts[i]; j++) {
        // use sendNoteOff for note offs
        MIDI.sendNoteOn(adcNotes[i][j], adcs[i].readADC(adcPins[i][j]) * 127 / adcBits, 1);
        Serial.print(adcs[i].readADC(adcPins[i][j]));
        Serial.print(" ");

        // update key position and speed
        // don't need to store an array of last adc values...
        lastAdcValues[i][j] = adcValues[i][j];
        adcValues[i][j] = adcs[i].readADC(adcPins[i][j]);
        keySpeed = (adcValues[i][j] - lastAdcValues[i][j]) / microTimer[i][j];

        // update hammer position
        // gravity should really be scaled by elapsed time
        hammerPositions[i][j] = hammerPositions[i][j] - gravity;
        // update according to hammer velocity
        hammerPositions[i][j] = hammerPositions[i][j] + hammerSpeeds[i][j] * microTimer[i][j];

        // hammer update based on interaction with key
        // the sign in this if statement depends on configuration of sensors
        if (hammerPositions[i][j] > adcValues[i][j]) {
          hammerPositions[i][j] = adcValues[i][j];
            if (hammerSpeeds[i][j] < keySpeed) {
              hammerSpeeds[i][j] = keySpeed;
            }    
        }

        // check for note ons

        // check for note offs

        // int adcValues[adcCount][8] = {0};
        // int lastAdcValues[adcCount][8] = {0};
        // double hammerPositions[adcCount][8] = {0};
        // double hammerSpeeds[adcCount][8] = {0};
        // bool noteOn[adcCount][8] = {0};


        // this may produce a low estimate of key speed (not set to zero immediately on adc read)
        microTimer[i][j] = 0;
      }
    }
    Serial.print("\n");
    // Serial.printf("%d, %d \n", adc1.readADC(0), adc1.readADC(1));

    start_ms += 266;
  }

  // read any new MIDI messages
  MIDI.read();
}
