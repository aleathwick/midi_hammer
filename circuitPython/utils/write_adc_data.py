"""collect some adc samples for analysis"""
import board
import analogio # for AnalogIn
import time

def get_builtin_adc_fn(board_adc):
    adc = analogio.AnalogIn(board_adc)
    return lambda : adc.value

get_adc = get_builtin_adc_fn(board.A2)

columns = [
    'value',
    'time'
]

print(5)
time.sleep(1)
print(4)
time.sleep(1)
print(3)
time.sleep(1)
print(2)
time.sleep(1)
print(1)
time.sleep(1)

n_samples = 1000

values = [0 for i in range(n_samples)]
times = [0 for i in range(n_samples)]
min_elapsed = 1000 # ms

print('GO!')
start = time.monotonic()
timestamp = time.monotonic_ns() // 1000
last_timestamp = time.monotonic_ns() // 1000


# while time.monotonic() - start < 5:
for i in range(n_samples):
    last_timestamp = timestamp
    while (timestamp - last_timestamp) < min_elapsed:
            timestamp = time.monotonic_ns() // 1000
    values[i] = get_adc()
    times[i] = timestamp
    
print(f"collected {n_samples} samples in {time.monotonic() - start} seconds")

with open('adc_data.txt', 'w') as f:
    f.write('value, time\n')
    for v, t in zip(values, times):
        f.write(f'{v}, {t}\n')
    

print('finished')
