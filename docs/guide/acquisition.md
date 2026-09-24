# Acquiring a Repeated Preamble

`doppler.acquire` finds bursts that open with a **repeated preamble**: one
complex sequence sent several times back to back. The sequence can be
anything with a sharp autocorrelation, such as a Zadoff-Chu sequence, a
chirp, a PSK sequence or a PN code. The burst arrives at an unknown time and
with an unknown carrier offset (Doppler), and is usually buried in noise.

You hand the engine **one period of the preamble, as samples**, and the rate
they are at. It never asks how the sequence was built, so a Zadoff-Chu
preamble and a spread-spectrum code are searched the same way.

Two objects do the work, one layer apart:

| object             | what it gives you                                                       |
| ------------------ | ----------------------------------------------------------------------- |
| `BurstAcquisition` | **detections**: a Doppler bin and a delay within one preamble period    |
| `BurstCapture`     | **bursts**: each burst's samples, its exact start, a Doppler and a C/N0 |

Most applications want `BurstCapture`: it owns a `BurstAcquisition`, works
out which repetition a detection belongs to, and returns the burst itself.

!!! note "Spread-spectrum links"

    Everything here applies to a DSSS preamble, which is one repeated
    preamble among others. The [DSSS acquisition guide](dsss-acquisition.md)
    covers what is specific to spread spectrum: building the preamble from a
    code, continuous data-modulated signals (`Acquisition`), and a coarse
    Doppler bank for searches far wider than the native span.

______________________________________________________________________

## The 30-second version

A 127-sample Zadoff-Chu preamble, repeated 8 times at 1 MS/s, arrives 300
samples into a stream with a 1.5 kHz carrier offset, at 50 dB-Hz:

```python
import numpy as np
from doppler.acquire import BurstCapture

fs = 1.0e6
k = np.arange(127)
zc = np.exp(-1j * np.pi * 5 * k * (k + 1) / 127).astype(np.complex64)

# The scene: preamble x 8, then 2000 samples of payload, 300 samples in.
rng = np.random.default_rng(1)
payload = (rng.standard_normal(2000)
           + 1j * rng.standard_normal(2000)).astype(np.complex64) / np.sqrt(2)
burst = np.concatenate([np.tile(zc, 8), payload])
x = np.zeros(20_000, np.complex64)
x[300:300 + burst.size] = burst
n = np.arange(x.size)
x = (x * np.exp(2j * np.pi * 1500.0 / fs * n)).astype(np.complex64)
cn0_dbhz = 50.0
sigma = np.sqrt(fs / 10 ** (cn0_dbhz / 10) / 2)       # per I and Q
x += (sigma * (rng.standard_normal(x.size)
               + 1j * rng.standard_normal(x.size))).astype(np.complex64)

cap = BurstCapture(zc, burst_len=burst.size, reps=8, fs=fs,
                   cn0_dbhz=cn0_dbhz)
windows = cap.push(x)                    # every complete burst, back to back
ev = cap.events(windows.size // cap.burst_len)

assert windows.size == burst.size        # one burst found
assert ev["preamble_start"][0] == 300    # its start, to the sample
assert abs(ev["doppler_hz_est"][0] - 1500.0) <= ev["doppler_res_hz"][0] / 2
```

`push` accepts blocks of any size, so a real stream is fed as it arrives.
The windows it returns are identical whatever the block size.

______________________________________________________________________

## The preamble is its samples

The engine needs one period of the preamble as `complex64` samples at the
front end's rate `fs`. That period's length `N` is the search's delay axis
(`code_bins == N`). With `fs` it sets the Doppler axis too:

- **native span** `±fs / (2N)`: the slow-time Nyquist, one repetition apart
- **resolution** `fs / (N · D)`, where `D` (`doppler_bins`) is the coherent
    depth the engine chooses, in repetitions

The period is not a parameter. It is `len(preamble)`, so the engine and your
replica cannot disagree.

Any sequence becomes a preamble the same way. Here are the four common
families at 127 samples:

```python
from doppler.cvt import bin_to_nrz
from doppler.wfm import PN, mls_poly

N = 127
k = np.arange(N)
zadoff_chu = np.exp(-1j * np.pi * 5 * k * (k + 1) / N).astype(np.complex64)
chirp = np.exp(1j * np.pi * k**2 / N).astype(np.complex64)
qpsk = np.exp(1j * (np.pi / 2 * rng.integers(0, 4, N) + np.pi / 4)
              ).astype(np.complex64)

# A PN code: chips to +/-1 by the library's rule (0 -> +1, 1 -> -1). At more
# than one sample per chip, hold each chip: np.repeat(nrz, spc).
chips = PN(poly=mls_poly(7), seed=1, length=7).generate(N)
nrz = np.zeros(N, np.float32)
bin_to_nrz(chips, nrz)
pn = nrz.astype(np.complex64)

for p in (zadoff_chu, chirp, qpsk, pn):
    assert p.dtype == np.complex64 and p.size == N
```

A pulse-shaped preamble is the same idea: filter the repeated sequence and
take one period of the steady-state output. Filtering one period on its own
is not the same, because its edges lack the neighbouring repetitions.

### What the choice of sequence costs

The gate divides each cell by the mean magnitude of the whole surface, and
the preamble's own correlation energy away from its peak lands in that mean.
The sizer prices that. So a sequence with high autocorrelation sidelobes
needs a little more signal for the same detection probability, and a very
short one needs a lot more. At 127 samples the four families above barely
differ:

| preamble (N = 127) | worst sidelobe | depth chosen at 50 dB-Hz | `pd_burst` |
| ------------------ | -------------- | ------------------------ | ---------- |
| Zadoff-Chu         | 0.000          | 4                        | 0.924      |
| m-sequence (PN)    | 0.008          | 4                        | 0.923      |
| chirp              | 0.086          | 4                        | 0.921      |
| random QPSK        | 0.184          | 4                        | 0.905      |

(`reps=8`, `fs = 1 MS/s`. Sidelobes are cyclic, relative to the peak.)

!!! warning "Zadoff-Chu couples delay and Doppler"

    A Zadoff-Chu sequence's ambiguity function is a ridge, not a spike. A
    carrier offset of one native bin (`fs / N`) moves its correlation peak
    by `u⁻¹ mod N` samples, where `u` is the root. With `u = 5`, `N = 127`
    that is 51 samples a bin. Inside the native span the engine resolves
    the Doppler before it correlates, so the delay comes out right. Beyond
    it (see [Doppler](#doppler-span-uncertainty-and-rate)), a window tile's
    reported delay carries that shift. A PN code has no such ridge.

______________________________________________________________________

## Detections: `BurstAcquisition`

`BurstAcquisition` is the search on its own. Each call to `push` returns the
detections completed in that block, as 7-tuples:

```python
from doppler.acquire import BurstAcquisition, bin_to_signed

acq = BurstAcquisition(zc, reps=8, fs=fs, cn0_dbhz=cn0_dbhz)
hits = []
for block in np.array_split(x, 7):              # any block size
    hits += acq.push(block)

dop, delay, peak, noise, stat, cn0_est, consumed = hits[0]
doppler_hz = bin_to_signed(dop, acq.doppler_bins) * acq.doppler_res_hz
assert delay == 300 % 127                       # the delay WITHIN a period
assert abs(doppler_hz - 1500.0) <= acq.doppler_res_hz / 2
```

| field              | meaning                                                           |
| ------------------ | ----------------------------------------------------------------- |
| `doppler_bin`      | Doppler bin, `0 … doppler_bins-1`; `bin_to_signed` folds it       |
| `code_phase`       | delay into one preamble period, in samples (`0 … N-1`)            |
| `peak_mag`         | the peak's correlation magnitude                                  |
| `noise_est`        | the CFAR reference it was divided by                              |
| `test_stat`        | `peak_mag / noise_est`, compared against `threshold`              |
| `cn0_dbhz_est`     | estimated C/N0 (dB-Hz), comparable to the `cn0_dbhz` you designed |
| `samples_consumed` | the stream position (from this engine's start) the dwell ended at |

Two things a detection does **not** tell you:

- **Which repetition.** `code_phase` is a delay modulo one period, so it
    fixes the alignment inside a repetition and never says which one. A
    burst's frame begins after one specific repetition.
- **One hit per burst.** Every dwell that clears the gate reports, so a
    burst typically produces several hits.

Use `bin_to_signed` for the Doppler bin rather than writing the fold out.
It is numpy's `fftfreq` convention, the one the engine uses, and a
hand-written fold that disagrees at an even grid's Nyquist bin is off by the
whole span.

______________________________________________________________________

## Bursts: `BurstCapture`

`BurstCapture` answers both of those questions. It works out which
repetition each detection belongs to (the **refine** stage), keeps enough
history to reach back to a start that has already gone past, and returns
each burst once:

- `push(x)` returns every burst completed so far, concatenated: burst `i`
    occupies `burst_len` samples at `i * burst_len`. It never returns part
    of a burst. A burst whose end has not arrived is held, and `pending()`
    says so.
- `events(count)` returns one record per window, in the same order:

| field            | meaning                                                   |
| ---------------- | --------------------------------------------------------- |
| `preamble_start` | the stream position of the preamble's first sample, exact |
| `doppler_hz_est` | the carrier offset, in Hz                                 |
| `doppler_res_hz` | its resolution: the estimate is good to about half this   |
| `cn0_dbhz_est`   | estimated C/N0 (dB-Hz)                                    |

`detections(count)` returns what the search itself found, for a caller that
wants both views from one engine.

It owns its acquisition engine rather than taking detections from outside.
A detection's `samples_consumed` is a stream position only for an engine fed
continuously and never reset, so the capture keeps that engine to itself.
The [design note](../design/burst-capture.md) has the reasoning.

### Bursts close together

The history is finite, so two bursts too close together cannot both be
reached. `min_gap` is the smallest separation the capture guarantees,
derived from the geometry:

```python
assert cap.min_gap >= 0
assert cap.retain_span == cap.refine_span + cap.burst_len
```

______________________________________________________________________

## Sizing: what you specify, what it chooses

You state the operating point. The engine chooses the grid.

- **`cn0_dbhz`**: your design C/N0. Leave it out (NaN) and the engine
    integrates the whole preamble, with no target to size against.
- **`reps`**: how many repetitions the preamble has. This is the ceiling on
    coherent depth.
- **`pfa`, `pd`**: the false-alarm and detection targets, `1e-3` and `0.9`
    by default.

It picks the **smallest** coherent depth whose Pd for the whole burst
(`pd_burst`) meets `pd`, which keeps latency low when the signal is strong.
`pd_burst` accounts for the preamble landing across dwells that are not
aligned to it, and for the target sitting between grid cells.
`pd_predicted` is one dwell lying wholly inside the preamble.

The Zadoff-Chu preamble above (`reps=8`, 1 MS/s):

| `cn0_dbhz` | depth chosen | resolution | `pd_burst` | `underpowered` |
| ---------- | ------------ | ---------- | ---------- | -------------- |
| 55         | 1            | 7874 Hz    | 0.98       | False          |
| 50         | 4            | 1969 Hz    | 0.92       | False          |
| 47         | 5            | 1575 Hz    | 0.65       | True           |

When no depth up to `reps` reaches `pd`, the constructor does not raise. It
builds the best grid it can, sets `underpowered`, and warns. The symptom in
operation is bursts that are never captured, so check it:

```python
assert not cap.underpowered, f"pd_burst {cap.pd_burst:.2f} < pd"
```

More repetitions, a stronger link or a narrower `doppler_uncertainty` close
a shortfall. A burst search never adds non-coherent looks: a burst has one
preamble, so a second look would only add noise and arrive late.

______________________________________________________________________

## Telling a real burst from a false alarm

At `pfa = 1e-3` a false alarm is expected now and then, and the capture
returns a window for it like any other. It is a detector's output stage and
does not gate on signal quality. Filter on `cn0_dbhz_est`, which separates
real windows from spurious ones by several dB: in the
[burst capture demo](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/examples/burst_capture_demo.py)
the gap is about 9 dB. Set the threshold from your own link: a real burst's
estimate scatters around the true C/N0 by a few dB, while a false alarm's sits
near the floor the gate allows.

______________________________________________________________________

## Doppler: span, uncertainty and rate

- **Inside the native span** (`±fs / (2N)`) the engine resolves Doppler
    directly, at resolution `fs / (N · D)`. (Once the search is tiled, the
    resolution is one tile's width, `fs / N`.)

- **`doppler_uncertainty`** (Hz, one-sided) narrows or widens the search.
    Narrower than the span, the engine searches fewer bins, so the threshold
    covers fewer cells: more sensitivity at the same `pfa`. Wider than the
    span, `BurstAcquisition` tiles the range with frequency windows of one
    repetition each, and `doppler_bins` counts the tiles: with the Zadoff-Chu
    preamble at 50 dB-Hz, `doppler_uncertainty=20_000` against a ±3.9 kHz
    span builds 7. Mind the [Zadoff-Chu coupling](#what-the-choice-of-sequence-costs)
    there.

    !!! bug "`BurstCapture` does not widen yet"

        `BurstCapture` accepts a wider `doppler_uncertainty` but still
        searches only the native span, so a burst outside it aliases in:
        with Zadoff-Chu, a start 51 samples off and a Doppler of 0
        ([#1512](https://github.com/doppler-dsp/doppler/issues/1512)). Until
        that is fixed, keep a capture's Doppler inside `±fs / (2N)`, or put
        a coarse bank in front of it.

- **`doppler_rate`** (Hz/s) caps the coherent depth, so a carrier that moves
    during the preamble does not smear it. Leave it at 0 for a carrier that
    holds still over the preamble.

For searches many times wider than the span, a bank of coarse mixes in
front of the engine is cheaper. The DSSS guide works one through:
[Widening the Doppler search](dsss-acquisition.md#widening-the-doppler-search).

______________________________________________________________________

## Keeping the history on disk

`BurstCapture`'s history is most of its state. `state_bytes()` is
dominated by it, and it is what lets a restored capture reach a burst that
began before the checkpoint. `PersistentBurstCapture` takes a file `path`
and keeps that history in the file, so a checkpoint shrinks to the search
state and the history survives a restart. It is the same object otherwise,
but a checkpoint does not move between the two kinds. See the
[API reference](../api/python-acquire.md#persistentburstcapture-the-same-capture-with-the-ring-in-a-file).

______________________________________________________________________

## See also

- [Python: Acquire API](../api/python-acquire.md): every class and property
- [Design: BurstCapture](../design/burst-capture.md): the refine stage, the
    history ring and the timing guarantees
- [DSSS acquisition guide](dsss-acquisition.md): spread-spectrum preambles,
    continuous signals and wide Doppler banks
- [Detection statistics](../api/python-detection.md): the threshold and Pd
    functions the sizer is built on
