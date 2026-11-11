"""halfband_r2c_demo.py -- real ADC samples to complex baseband in one step.

``HalfbandDecimatorR2C`` takes a real stream at fs to complex at fs/2 and
shifts the band at fs/4 to DC on the way, so a real tone at 0.30 fs leaves
at +0.05 fs, which is +0.10 of the output rate. A real tone is two complex
ones; the shift moves the other, at -0.30 fs, into the halfband's
stopband, and the filter removes it.
"""

import numpy as np

from doppler.resample import HalfbandDecimatorR2C, _halfband_bank

# The halfband's FIR branch: the row of the polyphase bank whose centre tap
# is the small one (the other branch is a pure 0.5 delay).
bank = _halfband_bank(atten=60.0, pb=0.4, sb=0.6)
centre = bank.shape[1] // 2
fir_row = 0 if abs(bank[0, centre]) < abs(bank[1, centre]) else 1
h = np.ascontiguousarray(bank[fir_row])

dec = HalfbandDecimatorR2C(h)
n = 16384
x = np.cos(2 * np.pi * 0.30 * np.arange(n)).astype(np.float32)  # real ADC
y = dec.execute(x)[256:]  # drop the filter's start-up transient
assert y.dtype == np.complex64 and len(y) == n // 2 - 256

window = np.blackman(len(y))
spectrum = np.fft.fftshift(np.fft.fft(y * window))
freq = np.fft.fftshift(np.fft.fftfreq(len(y)))  # cycles per output sample
db = 20 * np.log10(np.abs(spectrum) / np.abs(spectrum).max() + 1e-15)

peak = float(freq[np.argmax(db)])
image = float(db[np.argmin(np.abs(freq + 0.10))])
print(f"tone at {peak:+.4f} of the output rate (expected +0.1000)")
print(f"its mirror at -0.10: {image:.1f} dB below it")

assert abs(peak - 0.10) < 0.002, peak
assert image < -60.0, image  # the 60 dB halfband rejects the mirror

print("halfband_r2c_demo: OK")
