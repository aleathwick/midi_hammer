import time
import board
import random
import analogio # for AnalogIn
import usb_midi
# imports for spi with mcp3008
import busio
import digitalio
import adafruit_mcp3xxx.mcp3008 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn
from src.midi_controllers import Key


# initialize midi
midi_channel = 1
midi = adafruit_midi.MIDI(midi_out=usb_midi.ports[1], out_channel=midi_channel - 1)



def get_builtin_adc_fn(board_adc):
    adc = analogio.AnalogIn(board_adc)
    return lambda : adc.value
# e.g. get_builtin_adc_fn(board.A2)


# pin to read value of
signal_pin = analogio.AnalogIn(board.A1)
# pins for turning muxes on/off
enable_pins = [board.GP20, board.GP19]
enable_pins = [digitalio.DigitalInOut(p) for p in enable_pins]
# address for selecting inputs on muxes, shared by all muxes
address_pins = [board.GP18, board.GP17, board.GP16]
address_pins = [digitalio.DigitalInOut(p) for p in address_pins]

for p in enable_pins + address_pins:
    p.direction = digitalio.Direction.OUTPUT
    p.value=0


def update_states(target_address_states, enable_i):
    for target_state, pin in zip(target_address_states, address_pins):
        pin.value = target_state
    for i, pin in enumerate(enable_pins):
        pin.value = i==enable_i

def get_full_adc_fn(target_address_states, enable_i):
    """return adc fn that also updates logic pins for controlling c4051"""
    def read_adc():
        time.sleep(0.0001)
        update_states(target_address_states, enable_i)
        return signal_pin.value
    return read_adc

# states of address and active pins for the last key
# needs to be correct so that minimal adc functions are calculated correctly
# i.e. how to change to the next sensor changing only those signal pins necessary
end_address_states = [1,0,0]
end_enabled_i = 1
address_states = end_address_states.copy()
enabled_i = end_enabled_i
# update_states([1,0,0], 1)

def get_minimal_adc_fn(target_address_states, enable_i):
    """given the target pin logic states, and enable number, create a minimal adc function

    Assuming that the resultant adc functions will be called in the same order as the initial calls
    to get_minimal_adc_fn, get_minimal_adc_fn will update only the logic pins necessary to before
    returning the value of signal_pin.
    """
    global enabled_i
    on_pins = []
    off_pins = []
    for i, (target_state, actual_state, pin) in enumerate(zip(target_address_states,
                                                            address_states,
                                                            address_pins)):
        if target_state != actual_state:
            if target_state:
                on_pins.append(pin)
            else:
                off_pins.append(pin)
            address_states[i] = target_state

    if enabled_i != enable_i:
        on_pins.append(enable_pins[enable_i])
        off_pins.append(enable_pins[enabled_i])
        enabled_i = enable_i
    
    def update_pins():
        for pin in on_pins:
            pin.value = 1
        for pin in off_pins:
            pin.value = 0
        # insert a small delay here?
        return signal_pin.value
    return update_pins




# keys = [
#     Pedal(lambda: mp.read_input(0), 64)
# ]

# or get_full_adc_fn instead, to explicitly set all pin values
max_adc_val=44500
min_adc_val=37500
hammer_travel=20
min_press_US=5000

keys = [
    Key(midi, get_minimal_adc_fn([0,0,0], 0), 64, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
    Key(midi, get_minimal_adc_fn([0,0,0], 1), 65, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
    Key(midi, get_minimal_adc_fn([0,1,0], 0), 68, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
    Key(midi, get_minimal_adc_fn([0,1,0], 1), 69, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
    Key(midi, get_minimal_adc_fn([1,1,0], 0), 68, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
    Key(midi, get_minimal_adc_fn([1,1,0], 1), 69, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
    Key(midi, get_minimal_adc_fn([1,0,0], 0), 64, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
    Key(midi, get_minimal_adc_fn([1,0,0], 1), 65, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US)
]

# keys = [
#     Key(midi, get_full_adc_fn([0,0,0], 0), 64, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
#     Key(midi, get_full_adc_fn([0,0,0], 1), 65, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
#     Key(midi, get_full_adc_fn([0,1,0], 0), 68, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
#     Key(midi, get_full_adc_fn([0,1,0], 1), 69, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
#     Key(midi, get_full_adc_fn([1,0,0], 0), 64, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
#     Key(midi, get_full_adc_fn([1,0,0], 1), 65, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
#     Key(midi, get_full_adc_fn([1,1,0], 0), 68, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
#     Key(midi, get_full_adc_fn([1,1,0], 1), 69, max_adc_val=max_adc_val, min_adc_val=min_adc_val, hammer_travel=hammer_travel, min_press_US=min_press_US),
# ]

time.sleep(0.1)
print_i = 1
print("READY")
# update_states([0,0,0], 0)
while True:
    # print(signal_pin.value)
    # print([p.value for p in address_pins + enable_pins])
    # time.sleep(0.1)
    for k in keys:
        # if print_i % 240 == 0:
            # k.print_state()
            # print('something')
        # print_i += 1
        k.step()
    print_i += 1
    if print_i % 10 == 0:
        print(tuple(k.key_pos for k in keys))
        # if print_i % 240 == 0:
            # print([p.value for p in address_pins + enable_pins])



