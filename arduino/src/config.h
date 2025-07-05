#pragma once

// choose the board type
// either PICO, TEENSY
// #define TEENSY
#define PICO

// number of values to store in buffers
// e.g. for the ADC buffer
// with 8 keys, 3 int buffers, 1 float buffer, this fails above 3000 (not enough ram1)
// we need at least enough for the savitzy-golay filters - longest SG filter is 99
// if dumping note-on adc data, then longer is better but need to baance number of keys with length of buffers
#define BUFFER_SIZE 100
