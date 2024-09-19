import time
import math
import random
from ulab import numpy as np
import adafruit_midi
import adafruit_midi.note_on
import adafruit_midi.note_off
import adafruit_midi.control_change

### general simulation parameters
MIN_LOOP_LEN = 100 # us

velocity_map_length = 1024

def log_speed(s, base=10):
    """apply log scaling to speed of hammer"""
    log_multiplier = math.log(s / velocity_map_length * (base - 1) + 1, base)
    return round(s * log_multiplier)
# final velocity map
VELOCITIES = [max(round(i / log_speed(velocity_map_length) * 127), 1) for i in range(velocity_map_length)]

class Key:
    """Key object including all hammer simulation logic and midi triggering"""
    def __init__(self, midi, get_adc, pitch, max_adc_val=64000, min_adc_val=0, hammer_travel=50, min_press_US=15000):
        self.midi = midi
        # save the original adc fn
        # but when using it we might multiply it by -1
        self.get_adc_original = get_adc

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
        self.gravity_mm = gravity_m * 1000 # mm per microsecond^2
        
        # update adc params based on adc min / max values provided
        self._update_adc_params(min_adc_val, max_adc_val)
        
        # initial key position is simply the output of the adc
        self.key_pos = self.get_adc()
        self.key_speed = 0
        # hammer position and speed are measured in the same units as key position and speed
        self.hammer_pos = self.key_pos
        self.hammer_speed = 0

        # hammer is only simulated when key is armed
        self.key_armed = True

        # timestamps for calculating speeds, measured in us
        self.timestamp = time.monotonic_ns() // 1000
        self.elapsed = 0
        # initial elapsed time before any waiting
        self.initial_elapsed = 0

        self.pitch = pitch
        self.note_on = False

    def _update_adc_params(self, min_adc_val, max_adc_val):
        """based on min/max adc values, update related parameters"""
        # if max and min adc values are the 'wrong way' around, then flip the sign on adc values
        # this allows the simulation logic to all work the same, regardless of whether
        # ADC is high or low when the key is depressed
        if max_adc_val < min_adc_val:
            self.max_adc_val = -max_adc_val
            self.min_adc_val = -min_adc_val
            self.get_adc = lambda: -self.get_adc_original()
        else:
            self.max_adc_val = max_adc_val
            self.min_adc_val = min_adc_val
            self.get_adc = self.get_adc_original
        
        # max and min adc values from bottom and top of key travel
        

        # ADC bits per microsecond^2
        self.gravity = self.gravity_mm  / self.hammer_travel * (self.max_adc_val - self.min_adc_val)

        # max speed of hammer (after multiplying by SPEED_MULTIPLIER)
        # in adc bits per us
        hammer_max_speed = (self.max_adc_val -self.min_adc_val) / self.min_press_US
        # multipler for speed of hammer
        # to change hammer speed into velocity map scale
        # i.e. the value returned from round(hammer_speed * hammer_speed_multipler)
        # can be used to index into the velocity lookup table
        self.hammer_speed_multiplier = velocity_map_length / hammer_max_speed

        self._update_thresholds()

    def step(self):
        'perform one step of simulation'
        self._update_time()
        self._update_key()
        if self.key_armed:
            self._update_hammer()
            self._check_note_on()
        self._check_note_off()

    def _update_time(self):
        last_timestamp = self.timestamp
        self.timestamp = time.monotonic_ns() // 1000
        self.initial_elapsed = self.timestamp - last_timestamp
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
            self.midi.send(adafruit_midi.note_on.NoteOn(self.pitch, velocity))
            self.note_on = True
            self.hammer_pos = self.key_reset_threshold
            self.hammer_speed = -self.hammer_speed
            self.key_armed = False

    def _check_note_off(self):
        if self.note_on:
            if (not self.key_armed) and self.key_pos < self.key_reset_threshold:
                self.key_armed = True
            if self.key_pos < self.note_off_threshold:
                self.midi.send(adafruit_midi.note_off.NoteOff(self.pitch, 54))
                self.note_on = False

    def _get_hammer_midi_velocity(self):
        speed_scaled = round(self.hammer_speed * self.hammer_speed_multiplier)
        speed_scaled_clipped = max(min(speed_scaled, velocity_map_length - 1), 0)
        return VELOCITIES[speed_scaled_clipped]
    
    def _update_thresholds(self):
        """based on updated max/min adc values, update thresholds (note on / off / reset key)"""
        # threshold for when a hammer should trigger note on
        self.note_on_threshold = int((self.max_adc_val - self.min_adc_val) * 1.2 + self.min_adc_val)
        # threshold for when key should trigger a note off
        self.note_off_threshold = int((self.max_adc_val - self.min_adc_val) * 0.3 + self.min_adc_val)
        # threshold for when key/hammer should be armed again
        self.key_reset_threshold = int((self.max_adc_val - self.min_adc_val) * 0.75 + self.min_adc_val)

    def print_state(self):
        # print('key pos, elapsed, hammer pos, hammer speed')
        # print((self.key_pos, self.elapsed, self.hammer_pos, self.hammer_speed))
        print('key pos')
        print('self.key_pos')


class Pedal(Key):
    def __init__(self, midi, get_adc, control_number, binary=False, **kwargs):
        super().__init__(midi, get_adc, -1, **kwargs)
        self.control_number = control_number
        self.control_val = 0
        self.binary = binary
        self.switch_on = False
    
    def _update_adc_params(self, min_adc_val, max_adc_val):
        super()._update_adc_params(min_adc_val, max_adc_val)
        self.adc_mid_point = self.min_adc_val + (self.max_adc_val - self.min_adc_val) / 2

    def step(self):
        'perform one step of simulation'
        self._update_time()
        self._update_key()
        last_control_val = self.control_val
        self.control_val = round(((self.key_pos - self.min_adc_val) / (self.max_adc_val - self.min_adc_val) * 127))
        self.control_val = min(max(self.control_val, 0), 127)
        if not self.binary and (self.control_val != last_control_val):
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
            self.midi.send(adafruit_midi.control_change.ControlChange(self.control_number, self.control_val))
        # if in binary mode, check that we are not near the midpoint
        # this avoids changing too easily
        elif abs(self.control_val - 64) < 62:
            # check that the switch has changed position
            if (self.control_val - 64 > 0) != self.switch_on:
                self.switch_on = not self.switch_on
                self.midi.send(adafruit_midi.control_change.ControlChange(self.control_number, self.switch_on * 127))

    def print_state(self):
        print('key pos, elapsed, control val')
        print((self.key_pos, self.elapsed, self.control_val))



class Keys:
    """Keys object including all hammer simulation logic and midi triggering for multiple keys
    
    Like Key class, but vectorized; get_adc and pitches should be lists.
    Really, min/max adc values etc. should be lists as well, so that vectors of positions can be compared
    to vectors of thresholds.

    e.g.
    keys = [
    Keys([get_test_sin_adc_fn(period = 0.5) for _ in range(40, 80)],
         [i for i in range(40, 80)])
    ]
    """
    def __init__(self, midi, get_adc, pitches, max_adc_val=64000, min_adc_val=0, hammer_travel=50, min_press_US=15000):
        # if max and min adc values are the 'wrong way' around, then flip the sign on adc values
        # this allows the simulation logic to all work the same, regardless of whether
        # ADC is high or low when the key is depressed
        self.midi = midi
        if max_adc_val < min_adc_val:
            max_adc_val = -max_adc_val
            min_adc_val = -min_adc_val
            get_adc_original = get_adc
            get_adc = lambda: -get_adc_original()

        assert len(set([len(get_adc), len(pitches)])) == 1, "Number of adc functions must equal the number of pitches"

        self.n_keys = len(get_adc)
        self.get_adc = get_adc
        # key position is simply the output of the adc
        self.key_pos = np.array([fn() for fn in self.get_adc])#, dtype=np.int16) # with int16 division is floor division
        self.last_key_pos = self.key_pos.copy()
        self.key_speeds = np.array([0 for _ in range(self.n_keys)])
        # hammer position and speed are measured in the same units as key position and speed
        self.hammer_pos = self.key_pos.copy()
        self.hammer_speeds = self.key_speeds.copy()
        self.last_hammer_speeds = self.key_speeds.copy()

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
        # initial elapsed time before any waiting
        self.initial_elapsed = 0

        self.pitches = np.array(pitches, dtype=np.int8)
        self.note_on = np.array([False for _ in range(self.n_keys)], dtype=np.bool)

    def step(self):
        'perform one step of simulation'
        self._update_time()
        self._update_key()
        self._update_hammer()
        self._check_note_on()
        self._check_note_off()

    def _update_time(self):
        last_timestamp = self.timestamp
        self.timestamp = time.monotonic_ns() // 1000
        self.initial_elapsed = self.timestamp - last_timestamp
        while (self.timestamp - last_timestamp) < MIN_LOOP_LEN:
            self.timestamp = time.monotonic_ns() // 1000
        self.elapsed = self.timestamp - last_timestamp

    def _update_key(self):
        self.last_key_pos[:] = self.key_pos[:]
        for i in range(self.n_keys):
            self.key_pos[i] = self.get_adc[i]()
        self.key_speeds = (self.key_pos - self.last_key_pos) / self.elapsed

    def _update_hammer(self):
        # preliminary update
        self.last_hammer_speeds[:] = self.hammer_speeds[:]
        self.hammer_speeds -= self.gravity * self.elapsed
        self.hammer_pos += (self.hammer_speeds + self.last_hammer_speeds) * self.elapsed / 2
        # check for interaction with key
        interaction_map = self.hammer_pos < self.key_pos
        # alternatively, self.hammer_pos = np.maximum(self.hammer_pos, self.key_pos)
        self.hammer_pos[interaction_map] = self.key_pos[interaction_map]
        self.hammer_speeds[interaction_map] = np.maximum(self.hammer_speeds[interaction_map], self.key_speeds[interaction_map])

    def _check_note_on(self):
        note_on_map = self.hammer_pos > self.note_on_threshold
        velocities = [self._get_hammer_midi_velocity(s) for s in self.hammer_speeds[note_on_map]]
        for p, v in zip(self.pitches[note_on_map], velocities):
            self.midi.send(adafruit_midi.note_on.NoteOn(p, v))
        self.hammer_pos[note_on_map] = self.note_on_threshold
        self.hammer_speeds[note_on_map] = -self.hammer_speeds[note_on_map]
        self.note_on[note_on_map] = 1 # using True doesn't work, must use 0


    def _check_note_off(self):
        # bitwise and (&) doesn't work for some reason
        note_off_map = (self.note_on + (self.key_pos < self.note_off_threshold)) == 2
        for p in self.pitches[note_off_map]:
            self.midi.send(adafruit_midi.note_off.NoteOff(p, 54))
        self.note_on[note_off_map] = 0 # using False doesn't work, must use 0

    def _get_hammer_midi_velocity(self, hammer_speed):
        speed_scaled = round(hammer_speed * self.hammer_speed_multiplier)
        speed_scaled_clipped = max(min(speed_scaled, velocity_map_length - 1), 0)
        return VELOCITIES[speed_scaled_clipped]

    def print_state(self):
        print('key pos, elapsed, hammer pos, hammer speed')
        print((self.key_pos[0], self.elapsed, self.hammer_pos[0], self.hammer_speeds[0]))





