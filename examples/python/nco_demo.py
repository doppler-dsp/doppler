"""nco_demo.py — smoke test for NCO examples from docs/examples/python-nco.md."""
from doppler.source import NCO
import numpy as np

# Raw uint32 phase — 0.25 * 2^32 = 2^30 = 1073741824 per step
nco = NCO(0.25)
ph = nco.steps_u32(16)
assert ph.dtype == np.uint32
assert ph.shape == (16,)
assert ph[0] == 0
assert ph[1] == 1073741824
assert ph[2] == 2147483648
assert ph[3] == 3221225472
assert ph[4] == 0, "should wrap at index 4"

# Overflow carry: steps_u32_ovf returns (phases, carry)
nco = NCO(0.25)
phases, carry = nco.steps_u32_ovf(16)
assert phases.dtype == np.uint32
assert carry.dtype == np.uint8
assert carry.shape == (16,)
expected_carry = np.array([0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1], dtype=np.uint8)
assert np.array_equal(carry, expected_carry), f"carry: {carry}"

# Scaled output — values in [0, nmax)
nco2 = NCO(0.25, nmax=1000)
scaled = nco2.steps_u32_scaled(16)
assert scaled[0] == 0
assert scaled[1] == 250
assert scaled[2] == 500
assert scaled[3] == 750
assert scaled[4] == 0, "should wrap"

print("nco_demo: OK")
