import time
import math
import board
import analogio # for AnalogIn
import usb_midi
import adafruit_midi
import adafruit_midi.note_on
import adafruit_midi.note_off
# imports for spi with mcp3008
import busio
import digitalio
import adafruit_mcp3xxx.mcp3008 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn

# initialize midi
midi_channel = 1
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=midi_channel - 1)

### general simulation parameters
MAX_ADC_VALUE = 58000
MIN_ADC_VALUE = 0
NOTE_ON_THRESHHOLD = MAX_ADC_VALUE * 1.2
NOTE_OFF_THRESHHOLD = int((MAX_ADC_VALUE - MIN_ADC_VALUE) / 2 + MIN_ADC_VALUE)
MIN_LOOP_LEN = 2500 # us

hammer_travel=1 # travel in mm of key
min_press_US=8500 # fastest possible key press
# gravity for hammer, measured in adc bits per microsecond per microsecond
# if the key press is ADC_range, where ADC_range is abs(sensorFullyOn - sensorFullyOff)
# hammer travel in mm; used to calculate gravity in adc bits
gravity_m = 9.81e-12 # metres per microsecond^2
gravity_mm = gravity_m * 1000 # mm per microsecond^2
# ADC bits per microsecond^2
GRAVITY_ADC = gravity_mm  / hammer_travel * (MAX_ADC_VALUE - MIN_ADC_VALUE)

# max speed of hammer (after multiplying by SPEED_MULTIPLIER)
MAX_SPEED = (MAX_ADC_VALUE -MIN_ADC_VALUE) / min_press_US
velocity_map_length = 1024
# multipler for speed of hammer (rounded multiplied value will be used to index into velocity lookup table)
SPEED_MULTIPLIER = velocity_map_length / MAX_SPEED

def log_speed(s, base=10):
    """apply log scaling to speed of hammer"""
    log_multiplier = math.log(s / velocity_map_length * (base - 1) + 1, base)
    return round(s * log_multiplier)
# final velocity map
VELOCITIES = [max(round(i / log_speed(velocity_map_length) * 127), 1) for i in range(velocity_map_length)]

def get_velocity(x):
    x_scaled = round(x * SPEED_MULTIPLIER)
    x_scaled_clipped = max(min(x_scaled, velocity_map_length - 1), 0)
    return VELOCITIES[x_scaled_clipped]

class Key:
    """Key object including all hammer simulation logic and midi triggering"""
    def __init__(self, get_adc, pitch):
        self.get_adc = get_adc
        # key position is simply the output of the adc
        self.key_pos = self.get_adc()
        self.key_speed = 0
        # hammer position and speed are measured in the same units as key position and speed
        self.hammer_pos = 0.5
        self.hammer_speed = 0
        
        # timestamps for calculating speeds, measured in us
        self.timestamp = time.monotonic_ns() // 1000
        self.last_timestamp = time.monotonic_ns() // 1000
        self.elapsed = 0

        self.pitch = pitch
        self.note_on = False

    def step(self):
        'perform one step of simulation'
        self._update_time()
        self._update_key()
        self._update_hammer()
        self._check_note_on()
        self._check_note_off()

    def _update_time(self):
        last_timestamp = self.timestamp
        while (self.timestamp - last_timestamp) < MIN_LOOP_LEN:
            self.timestamp = time.monotonic_ns() // 1000
        self.elapsed = self.timestamp - last_timestamp

    def _update_key(self):
        last_key_pos = self.key_pos
        self.key_pos = self.get_adc()
        self.key_speed = (self.key_pos - last_key_pos) / self.elapsed

    def _update_hammer(self):
        # preliminary update
        original_speed = self.hammer_speed
        self.hammer_speed -= GRAVITY_ADC * self.elapsed
        self.hammer_pos += (self.hammer_speed + original_speed) * self.elapsed / 2
        # check for interaction with key
        if self.hammer_pos < self.key_pos:
            self.hammer_pos = self.key_pos
            if self.hammer_speed < self.key_speed:
                self.hammer_speed = self.key_speed

    def _check_note_on(self):
        if self.hammer_pos > NOTE_ON_THRESHHOLD:
            velocity = get_velocity(self.hammer_speed)
            # print('speed:', hammer_speed * 1000)
            # print(velocity)
            midi.send(adafruit_midi.note_on.NoteOn(self.pitch, velocity))
            self.note_on = True
            self.hammer_pos = NOTE_ON_THRESHHOLD
            self.hammer_speed = -self.hammer_speed
        
    def _check_note_off(self):
        if self.note_on:
            if self.key_pos < NOTE_OFF_THRESHHOLD:
                midi.send(adafruit_midi.note_off.NoteOff(self.pitch, 54))
                self.note_on = False

# this needs the following pins: busio.SPI(board.SCK0, board.MOSI0, board.MISO0)
spi = busio.SPI(board.GP18, board.GP19, board.GP16)

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

keys = [
        Key(get_builtin_adc_fn(board.A2), 62),
        Key(get_builtin_adc_fn(board.A1), 64)
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
        if print_i % 10 == 0:
            print('key pos:', k.key_pos)
            # print(k.hammer_pos)
        print_i += 1
        [i for i in range(88 * 2)]

        # if i % 100 == 0:
        #     print('hammer_pos:', k.hammer_pos)
        #     print('key_speed:', k.key_speed)
        k.step()

       