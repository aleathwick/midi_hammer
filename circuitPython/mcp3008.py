# same logic but condensed from here: https://github.com/adafruit/Adafruit_CircuitPython_MCP3xxx
import time
import busio
import digitalio
import board
from adafruit_bus_device.spi_device import SPIDevice
cs = digitalio.DigitalInOut(board.GP22)
spi = busio.SPI(board.GP18, board.GP19, board.GP16)
spi_device = SPIDevice(spi, cs)
out_buf = bytearray(3)
# for mcp3008
out_buf[0] = 0x01
in_buf = bytearray(3)
pin = 2
while True:
    readings = []
    # for pin in [2, 4, 5, 6, 10, 12, 13, 14]:
    for pin in range(8):
        out_buf[1] = ((True) << 7) | (pin << 4)
        # out_buf[1] = 128
        with spi_device as spi2:
            spi2.write_readinto(out_buf, in_buf)
        value = (((in_buf[1] & 0x03) << 8) | in_buf[2]) << 6
        readings.append(value)
    print(readings)
    time.sleep(0.2)


