# Python DSSS API

The `doppler.dsss` module provides a full DSSS receiver: **`Acquisition`** — the
streaming burst-acquisition engine that finds an unknown code phase and
Doppler — and two despreaders that track and despread once acquired:
**`Despreader`**, the continuous DLL+Costas receiver (GPS-like, always
tracking), and **`BurstDespreader`**, the preamble-aware payload tracker for
latency-bound bursts.

Source:
[`src/doppler/dsss/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/__init__.py)

______________________________________________________________________

## `Acquisition` — streaming burst acquisition

`Acquisition` searches a streamed cf32 signal for a repeated BPSK PN burst over the
joint (Doppler × code-phase) grid, sizing its own search grid — coherent depth,
CFAR threshold, non-coherent looks — from the physics `(chip_rate, cn0_dbhz, pfa, pd)` using `doppler.detection`. Push
arbitrary-length blocks; it yields one record per detection — `(doppler_bin, code_phase, peak_mag, noise_est, test_stat, snr_est)` — whose `(doppler_bin, code_phase)` seed the `BurstDespreader`. See the
[DSSS Burst Acquisition guide](../guide/dsss-acquisition.md) for the search-space
sizing and a worked example.

::: doppler.dsss.Acquisition

______________________________________________________________________

## `PolynomialPhaseEstimator` — feedforward frequency + chirp-rate estimator

`PolynomialPhaseEstimator` recovers the **frequency** and **chirp rate** (Doppler and
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
`PolynomialPhaseEstimate(freq_norm, rate_norm, snr_db)` record in normalized units
(cycles/sample and cycles/sample²); scale by the sequence's sample rate for Hz.
It is the feedforward front-end for chirping-burst demodulation.

::: doppler.dsss.PolynomialPhaseEstimator

______________________________________________________________________

## `BurstDemod` — feedforward DSSS frame demodulator

`BurstDemod` is the whole post-acquisition payload chain, in C, with **no
tracking loops**: it estimates the residual Doppler *and* Doppler rate
feedforward (composing `PolynomialPhaseEstimator` over the unmodulated preamble),
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

## `Despreader` — continuous tracking receiver

A complete continuous DSSS-BPSK receiver in one object: `Despreader` composes
a carrier loop (`Costas`, FLL-assisted) and a code loop (`Dll`) on a single
shared per-sample integrate-and-dump. Per sample it wipes the carrier
(integer-NCO) and feeds the de-rotated sample to the DLL's early/prompt/late
correlators; per code period it dumps the prompt and updates both loops — the
code loop on the early/late envelopes, the carrier loop on the same prompt.
`steps()` emits one despread prompt symbol per code period; `bits()` turns the
prompts into hard data bits.

`bn_fll > 0` enables FLL-assisted carrier pull-in. When a data bit spans
`periods_per_bit` code periods (GPS C/A: 20), `bits()` **bit-syncs** — it
histograms the prompt sign-flip positions to find the bit boundary
(`bit_phase`), then coherently sums `periods_per_bit` prompts per bit. The
despreader is seeded by acquisition (coarse carrier frequency + code phase)
and tracks the residual.

```python
import numpy as np
from doppler.dsss import Despreader
from doppler.wfm import Synth

code = np.random.default_rng(1).integers(0, 2, 127).astype(np.uint8)
rx = Synth(type="pn", pn_length=7, sps=8).steps(127 * 8 * 4)  # PN-spread IQ

d = Despreader(code, sps=8, init_norm_freq=0.0, init_chip=0.0,
               bn_carrier=0.05, bn_code=0.005, bn_fll=0.03,
               zeta=0.707, spacing=0.5, periods_per_bit=1)
symbols = d.steps(rx)   # one despread prompt per code period
bits    = d.bits(rx)    # hard data bits (bit-synced when periods_per_bit > 1)
```

::: doppler.dsss.Despreader

______________________________________________________________________

## `BurstDespreader` — tracking receiver

Seeded with a coarse frequency and code-phase estimate (from the
`Corr2D`/`CorrDetector2D` acquisition engine or `Acquisition`), the `BurstDespreader` locks
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
from doppler.dsss import BurstDespreader

# data_code: 0/1 spreading chips; seed from the acquisition peak.
# rx is the received capture (reuse the burst built above).
data_code = ((np.arange(32) * 40503 >> 7) & 1).astype(np.uint8)
acq_freq, acq_chip = 0.012, 0.0
d = BurstDespreader(data_code, sf=32, sps=2,
               init_norm_freq=acq_freq, init_chip_phase=acq_chip)
symbols = d.steps(rx)        # complex64 prompt symbols
bits    = d.bits(rx)         # or hard BPSK bits (0/1)
round(d.lock_metric, 2)      # ~1.0 once locked
```

### Preamble-aided pull-in with a distinct acquisition code

```python
burst = rx                        # a received capture (from above)
d = BurstDespreader(data_code, sf=32, sps=2)
d.set_acq(acq_code, acq_reps=5)   # 5-rep preamble pulls the loops in
symbols = d.steps(burst)          # preamble emits nothing; payload follows
```

______________________________________________________________________

::: doppler.dsss.BurstDespreader

## `DsssReceiver` — the composed continuous receiver

The single-object form of `Acquisition -> Dll(segments) -> RateConverter -> MpskReceiver`: only `code`/`chip_rate`/`symbol_rate` are required,
everything else defaults to this project's own validated values, with
`configure_search_raw`/`configure_lock_raw`/`configure_chain_raw` as the
power-user escape hatches. See the
[DsssReceiver gallery page](../gallery/dsss-receiver.md) for the full
story this composes.

!!! warning "`configure_search_raw` bypasses the mislock-avoiding auto-sizer"

    See the gallery page's own warning before pinning a large `doppler_bins`
    directly — `DsssReceiver` always has `symbol_rate` set, so the default
    auto-sizing exists specifically to avoid a confirmed mislock failure
    mode that a raw pin bypasses.

```python
import numpy as np
from doppler.dsss import DsssReceiver

code = np.random.default_rng(1).integers(0, 2, 127).astype(np.uint8)
rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0)
x = np.zeros(1024, dtype=np.complex64)   # no real signal here -- just show the call
syms = rx.steps(x)   # empty while searching; demodulated symbols once locked
rx.tracking          # 0 = searching, 1 = locked and demodulating
```

::: doppler.dsss.DsssReceiver

## `BurstAcquisition` — the burst front door to acquisition

`BurstAcquisition` is the burst-oriented front door to the shared
acquisition engine: a bounded preamble is searched over a (Doppler, code
phase) grid and the peak is reported once, rather than the continuous
streaming push of [`Acquisition`](#acquisition-streaming-burst-acquisition).
Both wrap the same stateless kernel; the two front doors differ only in how
the capture is fed and when the estimate is emitted.

::: doppler.dsss.BurstAcquisition

## `AsyncDsssReceiver` — the packaged continuous async receiver

`AsyncDsssReceiver` wraps the whole acquire → carrier-refine → track chain
behind one `steps()` call for the continuous *asynchronous* waveform
(non-integer chips/symbol). It carries two carrier loops — a pre-despread
Costas that tracks the Doppler dynamics before the code loop, and a
post-despread mop-up loop — plus carrier→code aiding for coupled clock
Doppler and a binary symbol-lock indicator. See the gallery page
[AsyncDsssReceiver: the SPEC Waveform](../gallery/async-dsss-receiver-spec.md)
for an end-to-end decode through physically-coupled Doppler.

::: doppler.dsss.AsyncDsssReceiver

## Related pages

<!-- related-pages:start -->

**Gallery** — [Streaming Async Despreader](../gallery/async-despread.md), [Async DSSS Receiver: the SPEC waveform through coupled Doppler](../gallery/async-dsss-receiver-spec.md), [Continuous Async DSSS Receiver](../gallery/async-dsss-receiver.md), [CarrierAcquisition: RRC Pulse Shaping](../gallery/carrier-acq-rrc.md), [Correlation and Detection](../gallery/corr.md), [DSSS Acquisition — Pd / Pfa vs Es/N0](../gallery/dsss-acq-characterization.md), [A 5-Burst DSSS Link — wfmgen's Three Faces, the Full Receiver Chain](../gallery/dsss-burst-pipeline.md), [DsssReceiver — the Composed Continuous DSSS Receiver](../gallery/dsss-receiver.md), [Gallery](../gallery/index.md), [Full-Chain Lock-Up](../gallery/receiver-lock.md)
**Guides** — [DSSS Burst Acquisition](../guide/dsss-acquisition.md), [Guides](../guide/index.md), [Lock Detection Across `doppler.track`](../guide/lock-detection.md), [Checkpoint & Resume](../guide/state-serialization.md), [DSSS bursts — a burst train in one declaration](../guide/wfmgen/dsss-bursts.md), [Waveforms](../guide/wfmgen/waveforms.md)
**Design** — [Design — pure-functional acquisition kernel (elastic fleet)](../design/acq-fn.md), [API taxonomy: the DSP building-block hierarchy and its naming axis](../design/api-taxonomy.md), [DsssReceiver Specifications](../design/async-dsss-spec.md), [Asynchronous symbol/code despreading](../design/async-symbol-despreader.md), [Corr2D: decoupled (interpolated) inverse length](../design/corr2d-interpolated-inverse.md), [DSSS acquisition: stateless, parallel, dynamics-capable](../design/dsss-acquisition.md), [Design](../design/index.md), [State Serialization — the standard bytes interface](../design/state-serialization.md)
**Contributing** — [DSSS Primary Use Cases for Code Acquisition Design](../dev/dsss-use-cases.md), [Contributing](../dev/index.md)

<!-- related-pages:end -->
