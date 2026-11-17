"""ofdm_demo.py -- an OFDM signal from a layout, and the grid back out of it.

``Ofdm`` renders any OFDM signal a layout can describe, and names no
standard. This builds a generic one: a 64-point transform with 52 occupied
subcarriers (DC and the band edges null), four pilots that follow a
polarity sequence, a 16-sample cyclic prefix and a 2-sample window, carrying
QPSK. Ten OFDM symbols are modulated; a forward FFT of each symbol's body
then returns the grid that went in, which is how the structure is checked:
data points on the data subcarriers, pilots at their values, nothing on the
null ones.
"""

# --8<-- [start:ofdm]
import numpy as np

from doppler.mpsk import mpsk_map
from doppler.ofdm import Ofdm

N, CP, WINDOW, N_SYM = 64, 16, 2, 10
used = np.zeros(N, np.uint8)  # index k + N/2
used[N // 2 - 26 : N // 2 + 27] = 1
used[N // 2] = 0  # DC is null
PILOTS = (-21, -7, 7, 21)

ofdm = Ofdm()
cls = ofdm.add_class(N, used, cyclic_prefix=CP, window=WINDOW)
ofdm.add_polarity(cls, np.array([1, 1, 1, -1], np.complex64))
for k in PILOTS:
    ofdm.add_fixed(cls, 0xFFFFFFFF, k, 1.0)  # every pattern phase
ofdm.add_run(cls, N_SYM)

rng = np.random.default_rng(0)
n_points = ofdm.points_remaining()  # 48 data subcarriers x 10 symbols
points = mpsk_map(rng.integers(0, 4, n_points).astype(np.uint8), 4)
samples = np.concatenate([ofdm.modulate(points), ofdm.finish()])
# --8<-- [end:ofdm]

assert n_points == 48 * N_SYM
assert samples.size == N_SYM * (N + CP) + WINDOW

# Invert it: the body of each OFDM symbol, forward transformed, is its grid.
data_k = [
    k for k in range(-N // 2, N // 2) if used[k + N // 2] and k not in PILOTS
]
for s in range(N_SYM):
    body = samples[s * (N + CP) + CP : (s + 1) * (N + CP)]
    grid = np.fft.fftshift(np.fft.fft(body)) / N
    pol = [1, 1, 1, -1][s % 4]
    for k in range(-N // 2, N // 2):
        want = 0j
        if k in PILOTS:
            want = pol
        elif k in data_k:
            want = points[s * 48 + data_k.index(k)]
        assert abs(grid[k + N // 2] - want) < 1e-5, (s, k)

# The transform is unnormalized, so the mean power is the grid's energy: 48
# unit data points and 4 unit pilots.
power = float(np.mean(np.abs(samples[CP:-WINDOW]) ** 2))
print(f"{samples.size} samples, mean power {power:.1f} (the grid's 52)")
assert abs(power - 52.0) < 2.0, power

print("ofdm_demo: OK")
