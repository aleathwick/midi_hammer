// See here for documentation on multicore pico:
// https://arduino-pico.readthedocs.io/en/latest/multicore.html
// See here for original example:
// https://github.com/earlephilhower/arduino-pico/blob/master/libraries/rp2040/examples/Multicore/Multicore.ino
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
void setup() {
  Serial.begin(115200);
  Serial.printf("C0: ready\n");
}

void loop() {
  Serial.printf("C0: pushing value to other core...\n");
  // returns false if FIFO is full, true if push is successful
  rp2040.fifo.push_nb(10);
  // rp2040.fifo.push(uint32_t) - will block if FIFO is full
  delay(1000);
}

// Running on core1
void setup1() {
  delay(10);
  Serial.printf("C1: ready\n");
}

void loop1() {
  // uint32_t rp2040.fifo.pop() will block if the FIFO is empty, there is also a bool rp2040.fifo.pop_nb version
  Serial.printf("C1: Read value from FIFO: %d\n", rp2040.fifo.pop());
  // int rp2040.fifo.available() - get number of values available in this core's FIFO
  
}
