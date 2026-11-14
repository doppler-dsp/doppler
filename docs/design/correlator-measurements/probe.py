"""#2457 cost probe ported to correlation.Correlator (Acquisition renamed).

CASE=design|bench, D1=1 for the D=1 control, REPS dwells (min taken).
Noise-only input: per-sample cost does not depend on content.
"""

import os
import time

import numpy as np

from doppler.correlation import Correlator
from doppler.source import AWGN

SF, SPC = 1023, 2
rng = np.random.default_rng(7)
code = rng.integers(0, 2, SF).astype(np.uint8)
ref = np.where(code & 1, -1, 1).astype(np.complex64)
TE = SF * SPC
CASES = {
    "bench": (5e6, 5000.0, 813, 500.0),
    "design": (3.069e6, 3000.0, 554, 500.0),
    "wide": (3.069e6, float(os.environ.get("DU", "15000")), 554, 500.0),
}
rc, du, coe, rate = CASES[os.environ.get("CASE", "design")]
if os.environ.get("D1") == "1":
    coe, rate = 1, 0.0
a = Correlator(
    ref,
    framing="continuous",
    sample_rate=rc * SPC,
    chip_rate=rc,
    symbol_rate=2700.0,
    cn0_dbhz=37.31,
    doppler_uncertainty=du,
    code_only_epochs=coe,
    doppler_rate=rate,
)
a.set_threads(int(os.environ.get("THREADS", "1")))
D, nc = a.coherent_bins, a.n_noncoh
n = nc * D * TE
x = AWGN(seed=3, amplitude=1.0).generate(n).astype(np.complex64)
a.push(x)
best = []
for _ in range(int(os.environ.get("REPS", "5"))):
    t0 = time.perf_counter()
    a.push(x)
    best.append(time.perf_counter() - t0)
print(
    f"{os.environ.get('CASE', 'design')} D={D} nc={nc} "
    f"{min(best) / n * 1e9:.1f} ns/sample"
)
