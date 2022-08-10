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
// #include <gram_savitzky_golay.h>

// to do:
// try filtering 
// circular buffer - store previous values



// ADCs
// MCP3208 adcs[2];
Adafruit_MCP3008 adcs[2];
int adcCount = 2;
int adcSelects[] = { 22, 17 };

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);





//// circular buffer
// const int buffer_len = 25;
// CircularBuffer<int, buffer_len> rawAdcBuffers[adcCount][8];
// CircularBuffer<double, buffer_len> keySpeedBuffers[adcCount][8];

//// savitzky golay
// // Window size is 2*m+1
// const size_t m = 12;
// // Polynomial Order
// const size_t n = 1;
// // Initial Point Smoothing (ie evaluate polynomial at first point in the window)
// // Points are defined in range [-m;m]
// const size_t t = m;
// // Derivation order? 0: no derivation, 1: first derivative, 2: second derivative...
// const int d = 0;

// Real-time filter (filtering at latest data point)
// gram_sg::SavitzkyGolayFilter filter(m, t, n, d);


// can turn to int like so: int micros = elapsed[i][j];
// and reset to zero: elapsed[i][j] = 0;
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

//////////////////////////////////////////////////////////////
// https://cplusplus.com/doc/tutorial/classes/
// https://paulmurraycbr.github.io/ArduinoTheOOWay.html
// https://www.circuitbasics.com/programming-with-classes-and-objects-on-the-arduino/
// #include <Adafruit_MCP3008.h>
// #include <elapsedMillis.h>

class KeyHammer
{
    // for how to make this work with mutliple types (MCP3008 or MCP3208), see here:
    // https://stackoverflow.com/questions/69441566/how-to-declare-a-class-member-that-may-be-one-of-two-classes
    Adafruit_MCP3008 adc;
    // define the range of the sensors, with sensorFullyOn being the key fully depressed
  // this will work regardless of sensorFullyOn < sensorFullyOff or sensorFullyOff < sensorFullyOn
    int sensorFullyOn;
    int sensorFullyOff;
    // threshold for hammer to activate note
    int noteOnThreshold;
    // threshold for key to trigger noteoff
    int noteOffThreshold;
    int sensorMin;
    int sensorMax;

    int pin;
    int pitch;
    
    int keyPosition;
    int lastKeyPosition;
    // key and hammer speeds are measured in adc bits per microsecond
    double keySpeed;

    double hammerPosition;
    double hammerSpeed;
    double hammer_travel;
    double gravity;

    bool noteOn;
    double velocity;
    int velocityIndex;

    char printMode;


    void update_key();
    void update_hammer();
    void check_note_on();
    void check_note_off();

  public:
    KeyHammer(Adafruit_MCP3008 adc, int pin, int pitch, int sensorFullyOn, int sensorFullyOff);
    void step();

    elapsedMicros elapsed;
};

KeyHammer::KeyHammer (Adafruit_MCP3008 adc, int pin, int pitch, int sensorFullyOn=430, int sensorFullyOff=50) {
  adc = adc;
  pin = pin;
  pitch = pitch;
  sensorFullyOn = sensorFullyOn;
  sensorFullyOff = sensorFullyOff;
  sensorMax = max(sensorFullyOn, sensorFullyOff);
  sensorMin = min(sensorFullyOn, sensorFullyOff);

  noteOnThreshold = sensorFullyOn + 1.2 * (sensorFullyOn - sensorFullyOff);
  noteOffThreshold = sensorFullyOn - 0.5 * (sensorFullyOn - sensorFullyOff);

  // gravity for hammer, measured in adc bits per microsecond per microsecond
  // if the key press is ADC_range, where ADC_range is abs(sensorFullyOn - sensorFullyOff)
  // hammer travel in mm; used to calculate gravity in adc bits
  hammer_travel = 4.5;
  gravity = (sensorFullyOn - sensorFullyOff) / (hammer_travel * (double)9810000000);

  keyPosition = sensorFullyOff;
  lastKeyPosition = sensorFullyOff;
  keySpeed = 0.0;
  hammerPosition = sensorFullyOff;
  hammerSpeed = 0.0;

  noteOn = false;

  elapsed = 0;

  printMode='p';
}

void KeyHammer::update_key () {
  lastKeyPosition = keyPosition;
  keyPosition = adc.readADC(pin);
  // constrain key position to be within the range determined by sensor max and min
  keyPosition = min(keyPosition, sensorMax);
  keyPosition = max(keyPosition, sensorMin);

  keySpeed = (keyPosition - lastKeyPosition) / (double)elapsed;
}

void KeyHammer::update_hammer () {
  hammerSpeed = hammerSpeed - gravity * elapsed;
  hammerPosition = hammerPosition + hammerSpeed * elapsed;
  // check for interaction with key
  if ((hammerPosition > keyPosition) == (sensorFullyOff > sensorFullyOn)) {
          hammerPosition = keyPosition;
          // we could check to see if the hammer speed is greater then key speed, but probably not necessary
          // after all, the key as 'caught up' to the hammer
          // if ((hammerSpeed > keySpeed) == (sensorFullyOff > sensorFullyOn)) {
            // if (abs(hammerSpeed) < abs(keySpeed)) {
          hammerSpeed = keySpeed;
          // }
        // }
  }
}

void KeyHammer::check_note_on () {
  // check for note ons
  if ((hammerPosition < noteOnThreshold) == (sensorFullyOff > sensorFullyOn)) {
    // do something with hammer speed to get velocity
    velocity = hammerSpeed;
    velocityIndex = round(hammerSpeed * hammerSpeedScaler);
    velocityIndex = min(velocityIndex, velocityMapLength);
    MIDI.sendNoteOn(pitch, velocityMap[velocityIndex], 1);
    noteOn = true;
    if (printMode == 'i'){ //&& ((i == 0 && j == 0) || (i == 1 && j == 2))){
      Serial.printf("\n note on: hammerSpeed %f, velocityIndex %d, velocity %d pitch %d \n", velocity, velocityIndex, velocityMap[velocityIndex], pitch);
    }
    hammerPosition = noteOnThreshold;
    hammerSpeed = -hammerSpeed;
    }
}

void KeyHammer::check_note_off () {
  if (noteOn){
    if ((keyPosition > noteOffThreshold) == (sensorFullyOff > sensorFullyOn)) {
      MIDI.sendNoteOff(pitch, 64, 1);
      if (printMode == 'i'){
        Serial.printf("note off: noteOffThreshold %d, adcValue %d, velocity %d  pitch %d \n", noteOffThreshold, keyPosition, 64, pitch);
      }
      noteOn = false;
    }
  }
}

void KeyHammer::step () {
  update_key();
  update_hammer();
  check_note_on();
  check_note_off();
  // print some stuff
  if (printMode == 'p'){
    // Serial.printf("%d %f %f ", keyPosition, hammerPosition, hammerSpeed);
    Serial.printf("hammer_%d:%f,", pitch, hammerPosition);
    Serial.printf("keySpeed_%d:%f,", pitch, keySpeed * 10000);
    Serial.printf("hammerSpeed_%d:%f,", pitch, hammerSpeed * 10000);
    // this newline may need to go after all keys have printed? I'm unsure how the serial plotter works.
    Serial.print('\n');
  }
  elapsed = 0;
}



//////////////////////////////////////////////////////////////


const int n_keys = 1;
KeyHammer keys[1] = { { adcs[1], 2, 64 } };
// KeyHammer[] keys = 
//     new KeyHammer[] { new KeyHammer(adcs[1],2,64)};//,
//                       // new KeyHammer(adc,pin,pitch) };




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
  }

  pinMode(LED_BUILTIN, OUTPUT);

  usb_midi.setStringDescriptor("Laser Piano");

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
  
  if (keys[0].elapsed >= 2500) {
    for (int i = 0; i < n_keys; i++) {
      keys[i].step();
    }
    loopTimer = 0;
    // read any new MIDI messages
    MIDI.read();
  }
}
  

// void loop1() {
//   // uint32_t rp2040.fifo.pop() will block if the FIFO is empty, there is also a bool rp2040.fifo.pop_nb version
//   Serial.printf("C1: Read value from FIFO: %d\n", rp2040.fifo.pop());
//   // int rp2040.fifo.available() - get number of values available in this core's FIFO
  
// }