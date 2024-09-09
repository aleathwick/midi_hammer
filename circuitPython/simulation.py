import time
import math
import board
import random
import analogio # for AnalogIn
import usb_midi
# imports for spi with mcp3008
import busio
import digitalio
import adafruit_mcp3xxx.mcp3008 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn
from src.midi_controllers import Keys

# initialize midi
midi_channel = 1
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=midi_channel - 1)

# this needs the following pins: busio.SPI(board.SCK0, board.MOSI0, board.MISO0)
# spi = busio.SPI(board.GP18, board.GP19, board.GP16)

# # SPI chip selects for ADCs 
# cs = [  
#         digitalio.DigitalInOut(board.GP28),
#         digitalio.DigitalInOut(board.GP27)
#     ]

# adcs = [MCP.MCP3008(spi, p) for p in cs]

# def get_mcp_adc_fn(adc, mcp_pin):        
#     return lambda : adc.read(mcp_pin) << 6
# # e.g. get_mcp_adc_fn(adcs[0], MCP.P4)

def get_builtin_adc_fn(board_adc):
    adc = analogio.AnalogIn(board_adc)
    return lambda : adc.value
# e.g. get_builtin_adc_fn(board.A2)

def get_test_sin_adc_fn(period = 1, min_adc_val=0, max_adc_val=64000):
    '''get function simulating adc values
    
    period: period of sin wave in seconds
    '''
    # timestamp in seconds, rounded to nearest us
    start = time.monotonic_ns() // 1000 / 1e6
    # ensure that if multiple sin functions of same period are used, they don't align
    start += period * random.random()
    # midpoint adc value
    midpoint = min_adc_val + (max_adc_val - min_adc_val) / 2
    def test_fn():
        now = time.monotonic_ns() // 1000 / 1e6 * math.pi * 2 / period
        return midpoint + math.sin(now) * (max_adc_val - min_adc_val) / 2
    return test_fn

class Cat():
    '''the cat plays the piano
    
    give Cat().get_adc to a key as the adc function
    '''
    def __init__(self, press_US=50000, release_US=500000, min_adc_val=0, max_adc_val=64000):
        self.state = 'rest' # one of rest, depress, release
        self.last_state = ''
        self.key_pos = min_adc_val
        self.speed_depress = (max_adc_val - min_adc_val) / press_US
        self.speed_release = (max_adc_val - min_adc_val) / release_US
        self.elapsed = 0
        self.timestamp = time.monotonic_ns() // 1000
        self.enter_rest_timestamp = self.timestamp
        self.max_adc_val = max_adc_val
        self.min_adc_val = min_adc_val
        # time to wait until changing state from rest
        self.wait_len = 1
    def get_adc(self):
        last_timestamp = self.timestamp
        self.timestamp = time.monotonic_ns() // 1000
        self.elapsed = self.timestamp - last_timestamp
        if self.state == 'depress':
            self.key_pos += self.elapsed * self.speed_depress
            if self.key_pos > self.max_adc_val:
                self.key_pos = self.max_adc_val
                self.last_state = self.state
                self.state = 'rest'
                self.enter_rest_timestamp = self.timestamp
        elif self.state == 'release':
            self.key_pos -= self.elapsed * self.speed_release
            if self.key_pos < self.min_adc_val:
                    self.key_pos = self.min_adc_val
                    self.last_state = self.state
                    self.state = 'rest'
                    self.enter_rest_timestamp = self.timestamp
        elif self.state == 'rest':
            if self.timestamp - self.enter_rest_timestamp > (self.wait_len * 1e6):
                if self.last_state == 'depress':
                    self.state = 'release'
                else:
                    self.state = 'depress'
        return self.key_pos

keys = [
    Keys(midi, [get_builtin_adc_fn(board.A2), get_builtin_adc_fn(board.A1)],
         [64,65])
]

## possible alternative approach: use adc_pin_groups and note_pitches to generate keys
# adc_pin_groups = [
#                     [0],
#                     [1]
#                 ]

# note_pitches = [
#                     [62],
#                     [64]
#                 ]

# chan_groups = [[AnalogIn(adc, getattr(MCP, 'P' + str(p))) for p in adc_pins] for adc, adc_pins in zip(adcs, adc_pin_groups)]


time.sleep(0.1)
print_i = 1
print("READY")
while True:
    for k in keys:
        if print_i % 50 == 0:
            k.print_state()
        print_i += 1
        k.step()

       