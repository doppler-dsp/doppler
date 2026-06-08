"""lo_demo.py — smoke test for LO examples from docs/examples/python-lo.md."""

from doppler.source import LO
import numpy as np

# Free-running quarter-rate tone
lo = LO(0.25)
iq = lo.steps(8)
assert iq.dtype == np.complex64
assert iq.shape == (8,)
assert abs(iq[0] - 1.0) < 1e-4, f"sample 0: {iq[0]}"
assert abs(iq[1] - 1j) < 1e-4, f"sample 1: {iq[1]}"
assert abs(iq[2] - (-1.0)) < 1e-4, f"sample 2: {iq[2]}"
assert abs(iq[3] - (-1j)) < 1e-4, f"sample 3: {iq[3]}"
assert np.allclose(iq[:4], iq[4:], atol=1e-4), (
    "quarter-rate must repeat every 4 samples"
)

# FM control port
ctrl = (0.002 * np.sin(2 * np.pi * 0.01 * np.arange(1024))).astype(np.float32)
lo2 = LO(0.1)
iq2 = lo2.steps_ctrl(ctrl)
assert iq2.dtype == np.complex64
assert iq2.shape == (1024,)

# Phase continuity: two successive calls produce the same 8-sample sequence
# as a single call of length 8.
lo3 = LO(0.25)
ref = lo3.steps(8)

lo4 = LO(0.25)
a = lo4.steps(4)
b = lo4.steps(4)
combined = np.concatenate([a, b])
assert np.allclose(combined, ref, atol=1e-4), "phase continuity broken"

print("lo_demo: OK")
