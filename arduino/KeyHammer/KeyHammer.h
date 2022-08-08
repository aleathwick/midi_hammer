// https://cplusplus.com/doc/tutorial/classes/
// https://paulmurraycbr.github.io/ArduinoTheOOWay.html
// https://www.circuitbasics.com/programming-with-classes-and-objects-on-the-arduino/
class KeyHammer
{
  public:
    KeyHammer(int pin);
    void dot();
    void dash();
  private:
    int _pin;
};