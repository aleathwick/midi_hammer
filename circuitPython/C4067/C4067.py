import digitalio
import analogio

class C4067:
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
            address = '{0:04b}'.format(i)
            for pin, bit in zip(self.address_pins, address):
                pin.value = bool(int(bit))

    def read_input(self, i):
        self.choose_input(i)
        return self.signal_pin.value
