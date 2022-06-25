// works with raspberry pico, using Earle Philhower's Raspberry Pico Arduino core:
// https://github.com/earlephilhower/arduino-pico
// needs Adafruit TinyUSB for usb midi
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_MCP3008.h>
// mcp3208 library: 
#include <MCP3208.h>
// MIDI Library: https://github.com/FortySevenEffects/arduino_midi_library
#include <MIDI.h>
// elapsedMillis: https://github.com/pfeerick/elapsedMillis
#include <elapsedMillis.h>
// CircularBuffer: https://github.com/rlogiacco/CircularBuffer
#include <CircularBuffer.h>
// for log
#include <math.h>

// to do:
// try filtering 
// circular buffer - store previous values



// ADCs
MCP3208 adcs[2];
// Adafruit_MCP3008 adcs[2];

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);


// ADC and sensor set up
const int adcCount = 2;  // n adcs
const int adcBits = 2048;
int adcSelects[] = { 17, 22 };     // pins for selecting adcs
int adcSensorCounts[] = { 3, 2 };  // sensors per adc
// last dimension must be at least of size max(adcSensorCounts)
// may as well make it 8, the max possible
int adcPins[][8] = { { 5, 6, 7 },
                     { 0, 1 } };
int adcNotes[][8] = { { 65, 66, 67 },
                      { 68, 69, 70 } };
// define the range of the sensors, with sensorFullyOn being the key fully depressed
// this will work regardless of sensorFullyOn < sensorFullyOff or sensorFullyOff < sensorFullyOn
const int sensorFullyOn = 700;
const int sensorFullyOff = 2048;
const int sensorLow = min(sensorFullyOn, sensorFullyOff);
const int sensorHigh = max(sensorFullyOn, sensorFullyOff);

// simulation parameters
// threshold for hammer to activate note
const int noteOnThreshold = sensorFullyOn + 0.1 * (sensorFullyOn - sensorFullyOff);
// threshold for key to trigger noteoff
const int noteOffThreshold = sensorFullyOn - 0.5 * (sensorFullyOn - sensorFullyOff);
// gravity for hammer, measured in adc bits per microsecond per microsecond
double gravity = (sensorFullyOn - sensorFullyOff) / (double)1000000000;

// keeping track of simulation states
// raw ADC - may apply some processing to signal
int rawAdcValues[adcCount][8] = {0};
// key positions
int adcValues[adcCount][8] = {0};
// https://forum.arduino.cc/t/initializing-an-array-of-structs-with-templates/478604
// <> indicates a template object, from a template class
// template classes can work with various data types
// if I wanted circular buffers with different dtypes, I'd need to use a base class and derive CircularBuffer from that:
// https://stackoverflow.com/questions/33507697/holding-template-class-objects-in-array 
// https://stackoverflow.com/questions/12009314/how-to-create-an-array-of-templated-class-objects
const int buffer_len = 10;
CircularBuffer<int, buffer_len> rawAdcBuffers[adcCount][8];
// CircularBuffer<int, buffer_len> processedAdcBuffers[adcCount][8];

int lastAdcValues[adcCount][8] = {0};
double keySpeed = 0;
double hammerPositions[adcCount][8] = {0};
double hammerSpeeds[adcCount][8] = {0};
bool noteOns[adcCount][8] = {0};
// can turn to int like so: int micros = elapsed[i][j];
// and reset to zero: elapsed[i][j] = 0;
elapsedMicros elapsed[adcCount][8];
elapsedMillis infoTimer;
elapsedMicros loopTimer;

// set up map of velocities, mapping from hammer speed to midi value
// hammer speed seems to range from ~0.005 to ~0.05 adc bits per microsecond
// ~5 to ~50 adc bits per millisecond, ~5000 to ~50,000 adc bits per second
// scale 

// this and over will result in velocity of 127
const double maxHammerSpeed = 0.06; // adc bits per microsecond
const int velocityMapLength = 1024;
const double logBase = 5; // base used for log multiplier, with 1 setting the multiplier to always 1
int velocityMap[velocityMapLength];
// used to put hammer speed on an appropriate scale for indexing into velocityMap
double hammerSpeedScaler = velocityMapLength / maxHammerSpeed;

// whether or not to print in the current loop
bool printInfo = true;
// whether to use print statements for arduino serial plotter; if false, print text and disregard serial plotter formatting
const bool plotSerial = true;
// used to restrict printing to only a few iterations after certain events
int printTime = 0;

// initalize velocity variables
double velocity;
int velocityIndex;

void setup() {
  Serial.begin(57600);
  // begin ADC
  // can set SPI default pins
  //  SPI.setCS(5);
  //  SPI.setSCK(2);
  //  SPI.setTX(3);
  //  SPI.setRX(4);
  for (int i = 0; i < adcCount; i++) {
    adcs[i].begin(adcSelects[i]);
    // fill in ADC buffers with the default value
    for (int j = 0; j < adcSensorCounts[i]; j++) {
      while(rawAdcBuffers[i][j].push(sensorFullyOff));
    }
  }

  pinMode(LED_BUILTIN, OUTPUT);

  usb_midi.setStringDescriptor("Laser Piano");
  Serial.printf("noteOnThreshold: %d \n noteOffThreshold: %d \n gravity: %f \n", noteOnThreshold, noteOffThreshold, gravity);

  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MIDI.begin(MIDI_CHANNEL_OMNI);

  // generate values for velocity map
  double logMultiplier;
  for (int i = 0; i < velocityMapLength; i++) {
    // logMultiplier will always be between 0 and 1
    logMultiplier = log(i / (double)velocityMapLength * (logBase - 1) + 1) / log(logBase);
    velocityMap[i] = round(127 * i / (double)velocityMapLength);
  }

  // wait until device mounted
  while (!USBDevice.mounted()) delay(1);
}

// can do setup on the other core too
// void setup1() {

// }

void loop() {
  // if (infoTimer >= 500) {
  //   printInfo = true;
  //   infoTimer = 0;
  //   // Serial.print("\n");
  // } else {
  //   printInfo = false;
  // }
  

  for (int i = 0; i < adcCount; i++) {
    for (int j = 0; j < adcSensorCounts[i]; j++) {
      if (elapsed[i][j] >= 5) {
        // update key position and speed
        lastAdcValues[i][j] = adcValues[i][j];
        rawAdcBuffers[i][j].push(adcs[i].readADC(adcPins[i][j]));
        // Could do some processing based on previous raw ADC values
        adcValues[i][j] = rawAdcBuffers[i][j].last();
        adcValues[i][j] = min(adcValues[i][j], sensorHigh);
        adcValues[i][j] = max(adcValues[i][j], sensorLow);
        keySpeed = (adcValues[i][j] - lastAdcValues[i][j]) / (double)elapsed[i][j];
        if (!plotSerial && printInfo && i == 0 && j == 0) {
          Serial.printf("keySpeed: %f elapsed: %d  double elapsed: %f \n", keySpeed, (int)elapsed[i][j], (double)elapsed[i][j]);
        }
        // update hammer position
        // gravity is measured in adc bits per millisecond
        // key and hammer speeds are measured in adc bits per microsecond
        hammerSpeeds[i][j] = hammerSpeeds[i][j] - gravity * elapsed[i][j];
        // update according to hammer velocity
        hammerPositions[i][j] = hammerPositions[i][j] + hammerSpeeds[i][j] * elapsed[i][j];

        // hammer update based on interaction with key
        if ((hammerPositions[i][j] > adcValues[i][j]) == (sensorFullyOff > sensorFullyOn)) {
          hammerPositions[i][j] = adcValues[i][j];
            // if (abs(hammerSpeeds[i][j]) < abs(keySpeed)) {
              hammerSpeeds[i][j] = keySpeed;
            // }
        }

        // check for note ons
        if ((hammerPositions[i][j] < noteOnThreshold) == (sensorFullyOff > sensorFullyOn)) {
          // do something with hammer speed to get velocity
          printTime = 0;
          velocity = hammerSpeeds[i][j];
          velocityIndex = round(hammerSpeeds[i][j] * hammerSpeedScaler);
          velocityIndex = min(velocityIndex, velocityMapLength);
          MIDI.sendNoteOn(adcNotes[i][j], velocityMap[velocityIndex], 1);
          noteOns[i][j] = true;
          if (!plotSerial){ //&& ((i == 0 && j == 0) || (i == 1 && j == 2))){
            Serial.printf("\n note on: hammerSpeed %f, velocityIndex %d, velocity %d on adc %d sensor %d \n", velocity, velocityIndex, velocityMap[velocityIndex], i, j);
          }
          hammerPositions[i][j] = noteOnThreshold;
          hammerSpeeds[i][j] = -hammerSpeeds[i][j];
          }
        
        // check for note offs
        if (noteOns[i][j]) {
          if ((adcValues[i][j] > noteOffThreshold) == (sensorFullyOff > sensorFullyOn)) {
            // could get key velocity for note off velocity
            printTime = 0;
            MIDI.sendNoteOff(adcNotes[i][j], 64, 1);
            if (!plotSerial){
              Serial.printf("note off: noteOffThreshold %d, adcValue %d, velocity %d on adc %d sensor %d \n", noteOffThreshold, adcValues[i][j], 64, i, j);
            }
            noteOns[i][j] = false;
          }
        }
        // finally, reset timer for this sensor
        // doing this at end may produce a low estimate of key speed (not set to zero immediately on adc read)
        elapsed[i][j] = 0;

        // possibly print some stuff
        if (!(plotSerial) && printInfo) {
          Serial.printf("%d %f %f ", adcValues[i][j], hammerPositions[i][j], hammerSpeeds[i][j]);
        }
      
      }
    }
  }

  // post simulation loop, do some printing
  printTime += 1;
  if (plotSerial && printInfo && (printTime > 10)){
    // Serial.printf("hammer:%f\n", hammerPositions[0][0]);
    // Serial.printf("key:%d hammer:%f hammerSpeed:%f\n", adcValues[0][0], hammerPositions[0][0], abs(hammerSpeeds[0][0]) * 20000);
    // Serial.printf("%d\n", (int)loopTimer);
    for (int i = 0; i < adcCount; i++) {
      for (int j = 0; j < adcSensorCounts[i]; j++) {
        Serial.printf("key_%d_%d:%d,", i, j, adcValues[i][j]);
        Serial.printf("ADC_%d_%d:%d,", i, j, rawAdcBuffers[i][j].last());
        Serial.printf("hammer_%d_%d:%f,", i, j, hammerPositions[i][j]);
        
      }
    }
    Serial.print('\n');
    printTime = 0;
  }
    
  loopTimer = 0;
  // read any new MIDI messages
  MIDI.read();
}
  

// void loop1() {
//   // uint32_t rp2040.fifo.pop() will block if the FIFO is empty, there is also a bool rp2040.fifo.pop_nb version
//   Serial.printf("C1: Read value from FIFO: %d\n", rp2040.fifo.pop());
//   // int rp2040.fifo.available() - get number of values available in this core's FIFO
  
// }