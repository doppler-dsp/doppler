# Python DSSS API

The `doppler.dsss` module provides the two halves of a DSSS receiver:
**`Acquisition`** — the streaming burst-acquisition engine that finds an unknown
code phase and Doppler — and **`Despreader`** — the tracking receiver that locks
and despreads the payload once acquired.

Source:
[`src/doppler/dsss/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/__init__.py)

See the [DSSS acquisition & despreading gallery page](../gallery/dsss-despread.md)
for the full acquire → track → despread chain with plots.

______________________________________________________________________

## `Acquisition` — streaming burst acquisition

`Acquisition` searches a streamed cf32 signal for a repeated BPSK PN burst over the
joint (Doppler × code-phase) grid, sizing its own search grid — coherent depth,
CFAR threshold, non-coherent looks — from the physics `(chip_rate, cn0_dbhz, pfa, pd)` using `doppler.detection`. Push
arbitrary-length blocks; it yields one record per detection — `(doppler_bin, code_phase, peak_mag, noise_est, test_stat, snr_est)` — whose `(doppler_bin, code_phase)` seed the `Despreader`. See the
[DSSS Burst Acquisition guide](../guide/dsss-acquisition.md) for the search-space
sizing and a worked example.

::: doppler.dsss.Acquisition

______________________________________________________________________

## `PolyPhaseEstimator` — feedforward frequency + chirp-rate estimator

`PolyPhaseEstimator` recovers the **frequency** and **chirp rate** (Doppler and
Doppler rate) of a complex sequence in one shot — no tracking loop — via a
**coherent (chirp-rate × frequency) matched-filter surface**: for each rate
hypothesis it dechirps the sequence and FFTs it, and the surface's global peak
(parabola-interpolated in both axes) gives `(r, f)`. Being fully coherent it is
the matched-filter-optimal estimator, so it holds at low SNR. The single
`max_rate` knob spans both regimes: **`max_rate = 0`** collapses to one FFT —
pure Doppler, near-static — while **`max_rate > 0`** searches a `±max_rate`
dechirp bank for a severe LEO chirp (cost scales with the rate span). The caller
strips modulation first (data-aided wipe, or square an M-PSK stream for the
non-data-aided case). `estimate(x)` returns a
`PolyPhaseEstimate(freq_norm, rate_norm, snr_db)` record in normalized units
(cycles/sample and cycles/sample²); scale by the sequence's sample rate for Hz.
It is the feedforward front-end for chirping-burst demodulation.

::: doppler.dsss.PolyPhaseEstimator

______________________________________________________________________

## `BurstDemod` — feedforward DSSS frame demodulator

`BurstDemod` is the whole post-acquisition payload chain, in C, with **no
tracking loops**: it estimates the residual Doppler *and* Doppler rate
feedforward (composing `PolyPhaseEstimator` over the unmodulated preamble),
dechirps the burst at sample rate, despreads the short data code to soft BPSK
symbols, frame-syncs against a known word, and checks a CRC-16 trailer. The one
`max_rate` knob spans both operating points: **near-static Doppler** (`0`, a
single-FFT estimate) and a **severe LEO chirp** (`> 0`, the coherent rate
search). It is one-shot per burst — seed it from acquisition and call `demod`.

The frame is `[sync header][payload][CRC-16 trailer]` in BPSK symbols (no FEC).
`demod(x)` returns the payload bits; the read-back properties report
`frame_valid` (CRC), `est_freq_hz`, `est_rate_hz`, `frame_offset`, and
`n_symbols`.

```python
import numpy as np
from doppler.dsss import BurstDemod

# Build a burst: 5x acquisition preamble, then a spread frame
# [Barker-13 sync | payload | CRC-16]. A real receiver takes (f0, code
# phase) from `Acquisition`; here we seed a known prior so the block runs.
acq_code = ((np.arange(500) * 2654435761 >> 13) & 1).astype(np.uint8)
data_code = ((np.arange(50) * 40503 >> 7) & 1).astype(np.uint8)
sync_word = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
payload = ((np.arange(64) * 7 + 3) & 1).astype(np.uint8)


def _crc16(bits):                          # CRC-16/CCITT, MSB-first
    c = 0xFFFF
    for b in bits:
        c ^= (int(b) & 1) << 15
        c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def _sign(b):                              # 0/1 chips -> +1/-1 BPSK
    return np.where(np.asarray(b) & 1, -1.0, 1.0)


crc = _crc16(payload)
crc_bits = np.array([(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
frame = np.concatenate([sync_word, payload, crc_bits])
chips = [np.tile(_sign(acq_code), 5)]      # unmodulated preamble
chips += [_sign(b) * _sign(data_code) for b in frame]
f0, preamble_start = 0.012, 0              # cyc/sample; from acquisition
bb = np.repeat(np.concatenate(chips), 4).astype(np.complex64)
nn = np.arange(len(bb))
rx = (bb * np.exp(2j * np.pi * f0 * nn)).astype(np.complex64)

d = BurstDemod(data_code, spc=4, chip_rate=1e6, carrier_hz=0.0,
               max_rate=0.0, payload_len=64, est_segments=10)
d.set_preamble(acq_code, reps=5)
d.set_sync(sync_word)              # 0/1 BPSK sync header
d.set_prior(f0, preamble_start)
bits = d.demod(rx)
assert d.frame_valid             # CRC passed
```

::: doppler.dsss.BurstDemod

______________________________________________________________________

## `Despreader` — tracking receiver

Seeded with a coarse frequency and code-phase estimate (from the
`Corr2D`/`Detector2D` acquisition engine or `Acquisition`), the `Despreader` locks
the signal with a code-tracking **delay-locked loop** and a carrier-tracking
**Costas loop**, despreads the payload, and emits symbols.

______________________________________________________________________

## How it works

Every dimension is a run-time parameter — spreading code, spreading factor
(`sf`), samples-per-chip (`sps`), loop bandwidths. Per input sample the
despreader wipes the carrier (an inline NCO driven by the Costas loop), then
correlates against early / prompt / late replicas of the code. Once per code
period it dumps the three accumulators:

- the **prompt** accumulator is the despread symbol — its sign is the BPSK
    decision, its phase/magnitude the soft information;
- the **non-coherent early-minus-late** envelope drives the DLL
    (`track.LoopFilter`) → code phase/rate;
- the **decision-directed** product drives the Costas loop → carrier
    frequency/phase.

**Seeding from acquisition.** `init_norm_freq` is the carrier frequency in
cycles/sample and `init_chip_phase` the code phase in chips; the caller converts
the detector's `(Doppler bin, code-phase chip)` into those units (the bin→Hz map
depends on the search grid, so it stays application-side).

**Distinct acquisition vs data codes.** Real bursts use a long acquisition code
for the preamble and a different (often shorter) data code for the payload.
`set_acq(acq_code, acq_reps)` enables **preamble-aided pull-in** — track the
unmodulated, repeated acquisition preamble coherently (a full ±π discriminator,
so even a wide residual pulls in), then switch to the data code at the payload.
Omit it for payload-only operation (seeded from acquisition).

Tracking state is readable: `norm_freq` (carrier estimate), `code_phase`,
`lock_metric` (0–1), `snr_est`. The cf32 symbol output chains over the `stream`
module's `dp_header_t` framing like any other DSP block.

______________________________________________________________________

## Examples

### Despread a payload seeded from acquisition

```python
import numpy as np
from doppler.dsss import Despreader

# data_code: 0/1 spreading chips; seed from the acquisition peak.
# rx is the received capture (reuse the burst built above).
data_code = ((np.arange(32) * 40503 >> 7) & 1).astype(np.uint8)
acq_freq, acq_chip = 0.012, 0.0
d = Despreader(data_code, sf=32, sps=2,
               init_norm_freq=acq_freq, init_chip_phase=acq_chip)
symbols = d.steps(rx)        # complex64 prompt symbols
bits    = d.bits(rx)         # or hard BPSK bits (0/1)
round(d.lock_metric, 2)      # ~1.0 once locked
```

### Preamble-aided pull-in with a distinct acquisition code

```python
burst = rx                        # a received capture (from above)
d = Despreader(data_code, sf=32, sps=2)
d.set_acq(acq_code, acq_reps=5)   # 5-rep preamble pulls the loops in
symbols = d.steps(burst)          # preamble emits nothing; payload follows
```

______________________________________________________________________

::: doppler.dsss.Despreader
