"""ring_iq16_demo.py — 16-bit I/Q through a ring, without a copy.

An ADC or an SDR hands you interleaved int16: I, Q, I, Q, ... numpy has no
complex-integer dtype, so `I16Buffer` speaks a RECORD instead:

    IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])

One element per sample, 1-D -- the same shape `F32Buffer` and `F64Buffer`
have, so code written against one width reads the same against another.
The record is exactly the four bytes the hardware delivered, which is why
every conversion below is a `.view()` and none of them copies.

Why a record and not two int16 packed into an int32? Both are one element
per sample. But arithmetic on the packed form carries across the I/Q
boundary -- `packed + 1` increments I only, silently -- while a record
refuses arithmetic outright. Wrong loudly beats wrong quietly.

Run:
  python ring_iq16_demo.py
"""

# --8<-- [start:setup]
import numpy as np

from doppler.buffer import I16Buffer
from doppler.cvt import I16ToF32

# --8<-- [end:setup]

# --8<-- [start:record]
IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])

# What the hardware gives you: bytes. Eight samples, I = k, Q = -k.
adc = np.empty(16, dtype=np.int16)
adc[0::2] = np.arange(8)
adc[1::2] = -np.arange(8)
capture = adc.tobytes()

samples = np.frombuffer(capture, dtype=IQ16)  # zero-copy: 8 records
assert samples.shape == (8,)

buf = I16Buffer(1024)
assert buf.write(samples)
assert buf.available == 8, "counted in SAMPLES, like every other width"
# --8<-- [end:record]

# --8<-- [start:read]
view = buf.wait(8)
assert view.dtype == IQ16 and view.shape == (8,)

i, q = view["i"], view["q"]  # strided int16 views of the ring -- no copy
assert not i.flags.owndata and not q.flags.owndata
assert i.tolist() == list(range(8))
assert q.tolist() == [-k for k in range(8)]

flat = view.view(np.int16)  # back to I, Q, I, Q, ... -- also no copy
assert np.array_equal(flat, adc)
# --8<-- [end:read]

# --8<-- [start:convert]
# To floating point with doppler's own converter, which takes the
# interleaved form: full scale becomes +-1.0.
to_float = I16ToF32()  # 1/32768
f = to_float.steps(flat)
z = f[0::2] + 1j * f[1::2]
assert f.dtype == np.float32 and z.dtype == np.complex64
assert np.allclose(z, (np.arange(8) - 1j * np.arange(8)) / 32768.0)
buf.consume()
# --8<-- [end:convert]

# --8<-- [start:refused]
buf.write(samples)
view = buf.wait(8)

# Arithmetic on a record is refused, where a packed int32 would corrupt I.
try:
    _ = view + 1
    raise AssertionError("arithmetic on a record went through")
except TypeError:
    pass

# And a bare int16 array is not an array of samples -- flat or (n, 2).
for not_samples in (adc, adc.reshape(-1, 2)):
    try:
        buf.write(not_samples)
        raise AssertionError("a bare int16 array was accepted")
    except TypeError:
        pass
assert buf.available == 8, "a refused write takes nothing"
# --8<-- [end:refused]

buf.destroy()
print(
    "iq16: bytes -> records -> ring -> fields, interleaved and float, "
    "all by view; arithmetic and bare int16 refused"
)
