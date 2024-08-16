"""
This script was for testing matrix pcb v0.1, in which matrix scanning is used with korg B2 silicone switches to read key velocities.
Each column of the pcb is a pitch class, and each octave has two rows, one for the two different switch levels.
It is simple, but the code actually works quite well. 
"""
import board
import digitalio
import time
import usb_midi
import adafruit_midi
from adafruit_midi.note_off import NoteOff
from adafruit_midi.note_on import NoteOn

midi_channel=1
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=midi_channel - 1)


# two rows per octave, one for each sensor
octaves = [
    # first pin is mid sensors for an octave, second pin fully depressed sensors
    [board.GP11,board.GP10],
    [board.GP13,board.GP12],
    [board.GP15,board.GP14]
]
octaves = [[digitalio.DigitalInOut(p) for p in o] for o in octaves]
for o in octaves:
    for r in o:
        r.switch_to_input(pull=digitalio.Pull.DOWN)
# Only 6 pitch classes in this prototype
cols = [
    board.GP21,
    board.GP20,
    board.GP19,
    board.GP18,
    board.GP17,
    board.GP16
]
cols = [digitalio.DigitalInOut(p) for p in cols]
# set all columns to low
for c in cols:
    c.switch_to_output()
    c.value = False

# keep track of sensor 0 state
states_0 = [[False for _ in cols] for _ in octaves]
# keep track of times that sensor 0s are activated
times = [[-1 for _ in cols] for _ in octaves]
# keep track of which notes are on
note_ons = [[False for _ in cols] for _ in octaves]



# function for mapping rows / columns to pitches
# either use as is, or use to generate a map?
def get_pitch(octave, col):
    """given octave i and col j, return a midi note number"""
    # each column is a different pitch
    # rows are as follows: (octave0_note-off, octave0_note-on, octave1_note-off, octave1_note-on,...)
    # 60 is C4
    return 60 + col + octave * 12

# above this will result in velocity of 127
min_delta = 4000
# below this will result in velocity of 40000
max_delta = 40000

def get_velocity(delta):
    """transform time delta into velocity"""
    return round(126 * max(0, min(1, 1 - (delta - min_delta) / (max_delta - min_delta))) + 1)

# j is the column index, c is the pin itself
print_i = 0
while True:
    print_i += 1
    for j, c in enumerate(cols):
        # activate the column (i.e. the pitch class)
        c.value = True
        # i is the octave index, r_0, r_1 are the rows for the octave
        for i, (r_0, r_1) in enumerate(octaves):
                # if print_i % 1000 == 0:
                    #  print(j, r_0.value, r_1.value)
                # check for an activated row 0, i.e. note is at least half pressed
                if r_0.value:
                    if not states_0[i][j]:
                         states_0[i][j] = True
                         # measure in us?
                         times[i][j] = time.monotonic_ns() // 1000
                    else:
                         if r_1.value and not note_ons[i][j]:
                              # send note on and turn note_ons to true
                              # do something to get the velocity
                              delta = time.monotonic_ns() // 1000 - times[i][j]
                              p = get_pitch(i, j)
                              v = get_velocity(delta)
                              midi.send(NoteOn(p, v))
                              note_ons[i][j] = True
                              print(f'note on {p} {v}')
                else:
                     if states_0[i][j]:
                          states_0[i][j] = False
                     if note_ons[i][j]:
                          # send note off and turn note_ons to false
                          midi.send(NoteOff(get_pitch(i, j), 64))
                          note_ons[i][j] = False
                          print(f'note off {p} {v}')
        c.value = False
