# copy this file to pico, renaming to boot.py
# set read only to false on boot
# disables USB write access. 
# see here:
# https://learn.adafruit.com/cpu-temperature-logging-with-circuit-python/writing-to-the-filesystem
import storage
storage.remount("/", False)
