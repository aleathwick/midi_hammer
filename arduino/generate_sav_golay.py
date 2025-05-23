''' This script generates the Savitzky-Golay filter coefficients for a range of window lengths, saved to
src/SaveGolayFilters.h, to be used by the KeyHammer class.

Perhaps this approach is a bit more convoluted than directly generating the coefficients in the cpp code, but
I was a bit lazy and at least it results in very efficient cpp code with the smallest possible memory footprint.

'''
from scipy import signal

polyorder = 1
use = 'dot'
cpp_str = '''// This file is auto-generated by generate_sav_golay.py
// Except for POS_FILTER_LENGTH and SPEED_FILTER_LENGTH, do not edit this file directly.
// Perhaps this approach is a bit more convoluted than directly generating the coefficients in the cpp code, but
// I was a bit lazy and at least it results in efficient cpp code with the smallest possible memory footprint.
#pragma once

// First define the lengths as preprocessor constants
#define POS_FILTER_LENGTH 21
#define SPEED_FILTER_LENGTH 21

// filters should be in dot product order, i.e. ordered like buffers, which is oldest to newest
namespace SavGolayFilters {
constexpr int posFilterLength = POS_FILTER_LENGTH;
constexpr int speedFilterLength = SPEED_FILTER_LENGTH;

'''
for pp_const, filter_name, deriv in zip(('POS_FILTER_LENGTH', 'SPEED_FILTER_LENGTH'), ('posFilter', 'speedFilter'), (0, 1)):
    for i, window_len in enumerate(range(3, 100, 2)):
        if i == 0:
            cpp_str += f'#if {pp_const} == {window_len}\n'
        else:
            cpp_str += f'#elif {pp_const} == {window_len}\n'
        pos_filter = signal.savgol_coeffs(
                window_length=window_len,
                polyorder=polyorder,
                deriv=deriv,
                pos=window_len-1,
                use=use
        )
        cpp_str += f'inline constexpr float {filter_name}[{window_len}] = ' + '{\n'
        for j, coeff in enumerate(pos_filter):
            cpp_str += f'{coeff:.8f}'
            if j < len(pos_filter) - 1:
                cpp_str += ', '
            if j % 6 == 5:
                cpp_str += '\n'
        cpp_str += '};\n\n'

    cpp_str += f'#endif \n\n'

cpp_str += '} // namespace SavGolayFilters\n\n'
# print(cpp_str)

with open('./src/SavGolayFilters.h', 'w') as f:
    f.write(cpp_str)
