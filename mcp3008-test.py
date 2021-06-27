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
chan = AnalogIn(mcp3008, MCP.P0)

# while True:
#     print(chan.value)
#     time.sleep(.2)


GRAVITY = 1e-6
ADC_BITS = 16
MAX_ADC_VALUE = 2 ** ADC_BITS
NOTE_ON_THRESHHOLD = MAX_ADC_VALUE + 100
NOTE_OFF_THRESHHOLD = int(MAX_ADC_VALUE / 2)

midi_channel = 1
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=midi_channel - 1)


# max speed of hammer (after multiplying by SPEED_MULTIPLIER)
MAX_SPEED = 15000
# multipler for speed of hammer (rounded multiplied value will be used to index into velocity lookup table)
SPEED_MULTIPLIER = 1e6
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

key_pos = get_voltage(chan)
hammer_pos = 0.5
hammer_speed = 0
note_on = False

timestamp = time.monotonic_ns()
time.sleep(0.1)
i = 1
print("READY")
while True:
    if i % 100 == 0:
        print(key_pos)
    # print(key_pos)
    i += 1
    [i for i in range(88 * 2)]

    last_timestamp = timestamp
    timestamp = time.monotonic_ns()
    elapsed = timestamp - last_timestamp

    ### update key
    last_key_pos = key_pos
    last_hammer_pos = hammer_pos
    key_pos = get_voltage(chan)
    key_speed = (key_pos - last_key_pos) / elapsed
    # if i % 100 == 0:
    #     print('hammer_pos:', hammer_pos)
    #     print('key_speed:', key_speed)

    ### update hammer
    # preliminary update
    hammer_speed -= GRAVITY
    hammer_pos += round(hammer_speed * elapsed)

    # check for interaction with key
    if hammer_pos < key_pos:
        hammer_pos = key_pos
        if hammer_speed < key_speed:
            hammer_speed = key_speed
    
    # check for note ons
    if hammer_pos > NOTE_ON_THRESHHOLD:
        velocity = get_velocity(hammer_speed)
        print('speed:', hammer_speed * 1000)
        print(velocity)
        midi.send(adafruit_midi.note_on.NoteOn(64, velocity))
        note_on = True
        hammer_pos = NOTE_ON_THRESHHOLD
        hammer_speed = -hammer_speed
    
    # check for note offs:
    if note_on:
        if key_pos < NOTE_OFF_THRESHHOLD:
            midi.send(adafruit_midi.note_off.NoteOff(64, 54))