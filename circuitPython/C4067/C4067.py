import digitalio
import analogio

# I think I found this on stackoverflow somewhere, but doesn't fit my needs
# I need 3 -> (0, 0, 1, 1)
# this gives 3 -> (1, 2), 4 -> (4,) etc.
# def bits(n):
#     while n:
#         b = n & (~n+1)
#         yield b
#         n ^= b

# generating maps is MUCH faster than calculating on the fly
# mapping i's to generators is again faster than mapping i's to lists
# bits = {i: [int(b) for b in '{0:04b}'.format(i)] for i in range(16)}
bits = {i: (int(b) for b in '{0:04b}'.format(i)) for i in range(16)}

class C4067:
    """class for controlling and reading input from C4067 and similar

    adc_pin: board pin, pin to read adc value from
    active_pin: board pin, pin controlling on/off
    address_pins: (board pins,), pins for controlling input select of C4067

    e.g.
    
    import time
    import board
    mp = C4067(
        board.A0,
        board.GP18,
        [
            board.GP14,
            board.GP15,
            board.GP17,
            board.GP16
        ]
    )

    while True:
        print([mp.read_input(i) for i in range(16)])
        time.sleep(0.2)

    """
    def __init__(self, adc_pin, active_pin, address_pins):
        self.signal_pin = analogio.AnalogIn(adc_pin)
        self.active_pin = digitalio.DigitalInOut(active_pin)
        self.active_pin.direction = digitalio.Direction.OUTPUT
        self.address_i = 0
        self.address_pins = [digitalio.DigitalInOut(p) for p in address_pins]
        for p in self.address_pins:
            p.direction = digitalio.Direction.OUTPUT
        self.choose_input(0)
        self.activate()

    def activate(self):
        self.active_pin = False

    def deactivate(self):
        self.active_pin = True

    def choose_input(self, i):
        assert i <= 15 and i >=0, f'i must be in [0, 15], but got {i}'
        if self.address_i != i:
            self.address_i = i
            for pin, bit in zip(self.address_pins, bits[i]):
                pin.value = bool(int(bit))

    def read_input(self, i):
        self.choose_input(i)
        return self.signal_pin.value
