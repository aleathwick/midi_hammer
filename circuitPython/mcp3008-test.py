import time
import math
import board
from analogio import AnalogIn
import usb_midi
import adafruit_midi
import adafruit_midi.note_on
import adafruit_midi.note_off
# imports for spi with mcp3008
import busio
import digitalio
import adafruit_mcp3xxx.mcp3008 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn


# spi = busio.SPI(board.SCK0, board.MOSI0, board.MISO0)
spi = busio.SPI(board.GP2, board.GP3, board.GP4)
 
cs = digitalio.DigitalInOut(board.GP17)

mcp3008 = MCP.MCP3008(spi, cs)
chans = [AnalogIn(mcp3008, MCP.P0), AnalogIn(mcp3008, MCP.P1)]

NOTE_PITCHES = [62, 64]

GRAVITY = 6e-6
ADC_BITS = 16
# MAX_ADC_VALUE = 2 ** ADC_BITS
MAX_ADC_VALUE = 26000
MIN_ADC_VALUE = 13000
NOTE_ON_THRESHHOLD = MAX_ADC_VALUE + 9600
NOTE_OFF_THRESHHOLD = int((MAX_ADC_VALUE - MIN_ADC_VALUE) / 2 + MIN_ADC_VALUE)
NOTE_OFF_THRESHHOLD = 10000

midi_channel = 1
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=midi_channel - 1)


# max speed of hammer (after multiplying by SPEED_MULTIPLIER)
MAX_SPEED = 15000
# multipler for speed of hammer (rounded multiplied value will be used to index into velocity lookup table)
SPEED_MULTIPLIER = 1e7
def log_speed(s, base=10):
    """apply log scaling to speed of hammer"""
    log_multiplier = math.log(s / MAX_SPEED * (base - 1) + 1, base)
    return round(s * log_multiplier)
VELOCITIES = [max(round(i / log_speed(MAX_SPEED) * 127), 1) for i in range(MAX_SPEED)]

def get_velocity(x):
    x_scaled = round(x * SPEED_MULTIPLIER)
    x_scaled_clipped = max(min(x_scaled, MAX_SPEED - 1), 0)
    return VELOCITIES[x_scaled_clipped]

# analog_in = AnalogIn(board.A0)

def get_voltage(chan):
    return (chan.value)

key_pos = [get_voltage(c) for c in chans]
last_key_pos = [get_voltage(c) for c in chans]

hammer_pos = [0.5 for _ in chans]
hammer_speed = [0 for _ in chans]
note_on = [False for _ in chans]

timestamp = [time.monotonic_ns() for _ in chans]
last_timestamp = [time.monotonic_ns() for _ in chans]
time.sleep(0.1)
i = 1
print("READY")
while True:
    for j in range(2):
        if i % 10 == 0:
            # print(key_pos)
            print(hammer_pos)

        i += 1
        [i for i in range(88 * 2)]

        last_timestamp[j] = timestamp[j]
        timestamp[j] = time.monotonic_ns()
        elapsed = timestamp[j] - last_timestamp[j]

        ### update key
        last_key_pos[j] = key_pos[j]
        key_pos[j] = get_voltage(chans[j])
        key_speed = (key_pos[j] - last_key_pos[j]) / elapsed
        # if i % 100 == 0:
        #     print('hammer_pos:', hammer_pos)
        #     print('key_speed:', key_speed)

        ### update hammer
        # preliminary update
        hammer_speed[j] -= GRAVITY
        hammer_pos[j] += round(hammer_speed[j] * elapsed)

        # check for interaction with key
        if hammer_pos[j] < key_pos[j]:
            hammer_pos[j] = key_pos[j]
            if hammer_speed[j] < key_speed:
                hammer_speed[j] = key_speed
        
        # check for note ons
        if hammer_pos[j] > NOTE_ON_THRESHHOLD:
            velocity = get_velocity(hammer_speed[j])
            # print('speed:', hammer_speed * 1000)
            # print(velocity)
            midi.send(adafruit_midi.note_on.NoteOn(NOTE_PITCHES[j], velocity))
            note_on[j] = True
            hammer_pos[j] = NOTE_ON_THRESHHOLD
            hammer_speed[j] = -hammer_speed[j]
        
        # check for note offs:
        if note_on[j]:
            if key_pos[j] < NOTE_OFF_THRESHHOLD:
                midi.send(adafruit_midi.note_off.NoteOff(NOTE_PITCHES[j], 54))
                note_on[j] = False