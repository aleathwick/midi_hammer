import time
import math
import board
import random
import analogio # for AnalogIn
import usb_midi
import adafruit_midi
import adafruit_midi.note_on
import adafruit_midi.note_off
import adafruit_midi.control_change
# imports for spi with mcp3008
import busio
import digitalio
import adafruit_mcp3xxx.mcp3008 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn

# initialize midi
midi_channel = 1
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=midi_channel - 1)

### general simulation parameters
MIN_LOOP_LEN = 2500 # us

velocity_map_length = 1024

def log_speed(s, base=10):
    """apply log scaling to speed of hammer"""
    log_multiplier = math.log(s / velocity_map_length * (base - 1) + 1, base)
    return round(s * log_multiplier)
# final velocity map
VELOCITIES = [max(round(i / log_speed(velocity_map_length) * 127), 1) for i in range(velocity_map_length)]

class Key:
    """Key object including all hammer simulation logic and midi triggering"""
    def __init__(self, get_adc, pitch, max_adc_val=64000, min_adc_val=0, hammer_travel=50, min_press_US=15000):
        self.get_adc = get_adc
        # key position is simply the output of the adc
        self.key_pos = self.get_adc()
        self.key_speed = 0
        # hammer position and speed are measured in the same units as key position and speed
        self.hammer_pos = self.key_pos
        self.hammer_speed = 0

        # max and min adc values from bottom and top of key travel
        self.max_adc_val = max_adc_val
        self.min_adc_val = min_adc_val
        
        # on a piano, hammer travel is just under 2", say 5cm
        # time from finger key contact to hammer hitting string:
        # "...travel times ranged from 20 ms to around 200 ms..."
        # see DOI 10.1121/1.1944648
        # https://www.researchgate.net/publication/7603558_Touch_and_temporal_behavior_of_grand_piano_actions
        self.hammer_travel=hammer_travel # travel in mm
        self.min_press_US=min_press_US # fastest possible key press
        # gravity for hammer, measured in adc bits per microsecond per microsecond
        # if the key press is ADC_range, where ADC_range is abs(sensorFullyOn - sensorFullyOff)
        # hammer travel in mm; used to calculate gravity in adc bits
        gravity_m = 9.81e-12 # metres per microsecond^2
        gravity_mm = gravity_m * 1000 # mm per microsecond^2
        # ADC bits per microsecond^2
        self.gravity = gravity_mm  / hammer_travel * (max_adc_val - min_adc_val)

        # threshold for when a hammer should trigger note on
        self.note_on_threshold = max_adc_val * 1.1
        # threshold for when key should trigger a note off
        self.note_off_threshold = int((max_adc_val - min_adc_val) / 2 + min_adc_val)

        # max speed of hammer (after multiplying by SPEED_MULTIPLIER)
        # in adc bits per us
        hammer_max_speed = (max_adc_val -min_adc_val) / min_press_US
        # multipler for speed of hammer
        # to change hammer speed into velocity map scale
        # i.e. the value returned from round(hammer_speed * hammer_speed_multipler)
        # can be used to index into the velocity lookup table
        self.hammer_speed_multiplier = velocity_map_length / hammer_max_speed
        
        # timestamps for calculating speeds, measured in us
        self.timestamp = time.monotonic_ns() // 1000
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
        self.hammer_speed -= self.gravity * self.elapsed
        self.hammer_pos += (self.hammer_speed + original_speed) * self.elapsed / 2
        # check for interaction with key
        if self.hammer_pos < self.key_pos:
            self.hammer_pos = self.key_pos
            if self.hammer_speed < self.key_speed:
                self.hammer_speed = self.key_speed

    def _check_note_on(self):
        if self.hammer_pos > self.note_on_threshold:
            velocity = self._get_hammer_midi_velocity()
            # print('speed:', hammer_speed * 1000)
            # print(velocity)
            midi.send(adafruit_midi.note_on.NoteOn(self.pitch, velocity))
            self.note_on = True
            self.hammer_pos = self.note_on_threshold
            self.hammer_speed = -self.hammer_speed
        
    def _check_note_off(self):
        if self.note_on:
            if self.key_pos < self.note_off_threshold:
                midi.send(adafruit_midi.note_off.NoteOff(self.pitch, 54))
                self.note_on = False

    def _get_hammer_midi_velocity(self):
        speed_scaled = round(self.hammer_speed * self.hammer_speed_multiplier)
        speed_scaled_clipped = max(min(speed_scaled, velocity_map_length - 1), 0)
        return VELOCITIES[speed_scaled_clipped]

class Expression(Key):
    def __init__(self, get_adc, control_number**kwargs):
        super().__init__(get_adc, -1, **kwargs)
        self.control_number = control_number
        self.control_val = 0
    def step(self):
        'perform one step of simulation'
        self._update_time()
        self._update_key()
        last_control_val = self.control_val
        self.control_val = round(((self.key_pos - self.min_adc_val) / (self.max_adc_val - self.min_adc_val) * 127))
        self.control_val = min(max(self.control_val, 0), 127)
        if (self.control_val != last_control_val):
            # control numbers:
            # 1 = Modulation wheel
            # 2 = Breath Control
            # 7 = Volume
            # 10 = Pan
            # 11 = Expression
            # 64 = Sustain Pedal (on/off)
            # 65 = Portamento (on/off)
            # 67 = Soft Pedal
            # 71 = Resonance (filter)
            # 74 = Frequency Cutoff (filter)
            midi.send(adafruit_midi.control_change.ControlChange(self.control_number, self.control_val))

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

keys = [
        Key(get_builtin_adc_fn(board.A2), 62),
        Expression(get_builtin_adc_fn(board.A1), 64)
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
        k.step()

       