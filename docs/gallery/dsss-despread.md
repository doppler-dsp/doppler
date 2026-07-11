# DSSS Acquisition & Despreading

![DSSS despreader demo](../assets/dsss_despread_demo.png)

A complete direct-sequence spread-spectrum (DSSS) BPSK receiver: acquire the
burst, then track and despread it. The transmit burst is an **acquisition
preamble** — 5 repetitions of a long (512-chip) acquisition code, unmodulated —
followed by a **payload** spread by a *distinct*, shorter (32-chip) data code.
`Es/N0 = 10 dB`, with a residual carrier offset.

## What you're seeing

**Top — Acquisition.** A 2-D (Doppler × code-phase) matched-filter search of the
preamble against the acquisition code, coherently summing the 5 periods, via
[`Corr`](../api/python-spectral.md). The plot is the **test statistic** (peak ÷ noise
estimate) across code phase at the winning Doppler bin. The sharp peak clears the
**CFAR detector threshold** (red dashed) from
[`det_threshold`](../api/python-detection.md) — the signal is declared present, and the
peak `(Doppler bin, code phase)` seeds the despreader.

**Middle — Loop stress vs time.** The carrier-frequency estimate (blue) pulling
onto the true offset (green dashed) while the lock metric (red) ramps to 1. The
preamble (shaded) is where `set_acq` pulls the loops in coherently — the
unmodulated repeated code gives a full ±π carrier discriminator — before the
payload begins.

**Bottom — Soft decisions vs time.** The complex prompt symbol's real part as
dots, clustering on ±1 (a 180° flip is don't-care).

## How it works

The receiver is built from two new objects:

- [`track.LoopFilter`](../api/python-track.md) — a reusable 2nd-order PI loop filter,
    the shared engine of the code and carrier loops.
- [`dsss.BurstDespreader`](../api/python-dsss.md) — carrier wipe-off (Costas) + an
    early/prompt/late delay-locked loop (DLL), integrate-and-dump per code period.

```python
import numpy as np
from doppler.dsss import BurstDespreader

# A real DSSS-BPSK payload: 40 symbols spread by a 32-chip data code at 2
# samples/chip, plus the long acq code that seeds the preamble pull-in.
rng = np.random.default_rng(0)
data_code = rng.integers(0, 2, 32).astype(np.uint8)
acq_code = rng.integers(0, 2, 512).astype(np.uint8)
dsig = np.where(data_code & 1, -1.0, 1.0)
syms = (rng.integers(0, 2, 40) * 2 - 1).astype(float)
rx = np.repeat(np.concatenate([s * dsig for s in syms]), 2).astype(
    np.complex64
)
acq_freq, acq_chip = 0.0, 0.0  # seeded from acquisition (genie here)

# seed from acquisition: norm_freq (cycles/sample), chip phase (chips)
d = BurstDespreader(data_code, sf=32, sps=2,
               init_norm_freq=acq_freq, init_chip_phase=acq_chip)
d.set_acq(acq_code, acq_reps=5)    # preamble-aided pull-in (optional)
symbols = d.steps(rx)              # complex prompt symbols
bits    = d.bits(rx)              # or hard BPSK decisions
```

The despreader is seeded by acquisition and is transport-agnostic: its cf32
symbol output chains over the `stream` module's `dp_header_t` framing like any
other DSP block.

## Verifying the handoff — bounded-time accept/reject

Acquisition at `pfa` over many dwells *will* occasionally hand the tracker
a false `(Doppler, code phase)` cell. The continuous `Despreader` closes
the loop: its embedded DLL runs a verify-counted CFAR lock detector, so a
handoff is accepted or rejected inside a window **derived from detection
theory** — no tuned timeout:

```python
import numpy as np

from doppler.detection import det_verify_delay
from doppler.dsss import Despreader

rng = np.random.default_rng(11)
code = rng.integers(0, 2, 127).astype(np.uint8)

# The DLL's default lock rule: pfa = 1e-3 decisions over n_looks = 20
# periods, n_up = 2 consecutive to declare. The verify window prices the
# mean declare latency at the operating pd, with margin for pull-in:
n_up, n_looks = 2, 20
window = int(4 * n_looks * det_verify_delay(0.99, n_up))  # periods

# A FALSE cell hands over garbage — no signal at this phase. Inside the
# window the compounded false-accept probability is
# ~ (window / n_looks) * (1e-3)**n_up ~ 1e-5, so a timeout IS the reject:
noise = (
    rng.normal(0, 1, 127 * 4 * window)
    + 1j * rng.normal(0, 1, 127 * 4 * window)
).astype(np.complex64)
ch = Despreader(code, 4)
ch.steps(noise)
assert ch.code_locked is False  # reject: tear down, back to acquisition
```

A true cell declares `code_locked` within the same window (mean latency
`det_verify_delay(pd, n_up)` decisions), and the carrier side reports
independently through `carrier_locked`. Every constant in the policy —
threshold, verify counts, window — traces back to `(pfa, pd)` budgets via
the [detection helpers](../api/python-detection.md#lock-verification).

Source: `src/doppler/examples/dsss_despread_demo.py`.
