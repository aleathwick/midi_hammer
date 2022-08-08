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
    int sensorFullyOn;
    int sensorFullyOff;
    int noteOnThreshold;
    int noteOffThreshold;

    int pin;
    int pitch;
    
    int keyPosition;
    int lastKeyPosition;
    double keySpeed;

    int hammerPosition;
    double hammerSpeed;

    elapsedMicros elapsed;

    bool noteOn;
    double velocity;
    int velocityIndex;


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
  noteOnThreshold = sensorFullyOn + 1.2 * (sensorFullyOn - sensorFullyOff);
  noteOffThreshold = sensorFullyOn - 0.5 * (sensorFullyOn - sensorFullyOff);

  keyPosition = sensorFullyOff;
  lastKeyPosition = sensorFullyOff;
  keySpeed = 0.0;
  hammerPosition = sensorFullyOff;
  hammerSpeed = 0.0;

  noteOn = false;

  elapsed = 0;
}

KeyHammer::update_key () {
  lastKeyPosition = keyPosition;
  keyPosition = adcs.readADC(pin);
  keySpeed = (keyPosition - lastKeyPosition) / (double)elapsed);
}

KeyHammer::update_hammer () {
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

KeyHammer::check_note_on () {
  // check for note ons
  if ((hammerPosition < noteOnThreshold) == (sensorFullyOff > sensorFullyOn)) {
    // do something with hammer speed to get velocity
    velocity = hammerSpeed;
    velocityIndex = round(hammerSpeed * hammerSpeedScaler);
    velocityIndex = min(velocityIndex, velocityMapLength);
    MIDI.sendNoteOn(pitch, velocityMap[velocityIndex], 1);
    noteOn = true;
    if (!plotSerial){ //&& ((i == 0 && j == 0) || (i == 1 && j == 2))){
      Serial.printf("\n note on: hammerSpeed %f, velocityIndex %d, velocity %d pitch %d \n", velocity, velocityIndex, velocityMap[velocityIndex], pitch);
    }
    hammerPosition = noteOnThreshold;
    hammerSpeed = -hammerSpeed;
    }
}

KeyHammer::check_note_off () {
  if ((keyPosition > noteOffThreshold) == (sensorFullyOff > sensorFullyOn)) {
    MIDI.sendNoteOff(pitch, 64, 1);
    if (!plotSerial){
      Serial.printf("note off: noteOffThreshold %d, adcValue %d, velocity %d  pitch %d \n", noteOffThreshold, keyPosition, 64, pitch);
    }
    noteOn = false;
  }
}

KeyHammer::step () {
  //something here
  elapsed = 0;
}

