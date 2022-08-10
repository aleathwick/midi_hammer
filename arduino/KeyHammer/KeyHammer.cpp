// https://cplusplus.com/doc/tutorial/classes/
// https://paulmurraycbr.github.io/ArduinoTheOOWay.html
// https://www.circuitbasics.com/programming-with-classes-and-objects-on-the-arduino/
#include <Adafruit_MCP3008.h>
#include <elapsedMillis.h>


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

  printMode='plot';
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
    if (printMode == "info"){ //&& ((i == 0 && j == 0) || (i == 1 && j == 2))){
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
      if (printMode == "info"){
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
  if (printMode == "plot"){
    // Serial.printf("%d %f %f ", keyPosition, hammerPosition, hammerSpeed);
    Serial.printf("hammer_%d:%f,", pitch, hammerPosition);
    Serial.printf("keySpeed_%d:%f,", pitch, keySpeed * 10000);
    Serial.printf("hammerSpeed_%d:%f,", pitch, hammerSpeed * 10000);
    // this newline may need to go after all keys have printed? I'm unsure how the serial plotter works.
    Serial.print('\n');
  }
  elapsed = 0;
}

