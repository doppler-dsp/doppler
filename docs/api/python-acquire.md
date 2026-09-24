# Python Acquire API

The `doppler.acquire` module holds the searches that find a signal before
anything tracks it:

- **`Acquisition`** — the streaming code-phase × Doppler engine.
- **`BurstAcquisition`** and **`BurstCapture`** — find a burst of any
    repeated complex preamble (a PN code, Zadoff-Chu, a chirp or a QPSK
    sequence), with **`PersistentBurstCapture`** keeping its ring in a file.
- **`CarrierAcquisition`** — a PSDMF (power-spectral-density
    matched-filter) residual-carrier frequency estimator. It runs after
    `Acquisition`'s coarse Doppler search as a one-shot refinement stage:
    non-coherently average the incoming stream's power spectrum, then
    circularly correlate that average against a known power spectrum shape
    (the default is the average PSD of a random rectangular-pulse BPSK
    stream, a sinc²; `psd_template` overrides it for a different pulse shape
    or modulation) to find the residual carrier offset.
- **`bin_to_signed`** — the Doppler-bin fold convention the searches and
    their hand-offs share.

The DSSS receivers in the [DSSS API](python-dsss.md) compose these objects.

Source:
[`src/doppler/acquire/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/acquire/__init__.py)

See the [CarrierAcquisition: RRC Pulse Shaping gallery page](../gallery/carrier-acq-rrc.md)
for a worked example showing why the template matters.

______________________________________________________________________

## `CarrierAcquisition` — PSDMF residual-carrier estimation

Composes `doppler.spectral.PSD` (FFT + window + non-coherent power
averaging), `doppler.spectral.CorrDetector` (FFT-based correlation of the
averaged power against the known template, plus a noise-referenced test
statistic), and `doppler.detection`'s Pfa/Pd statistics (the same ones
`Acquisition` itself is built on) for the detection gate. `sequential`
(test every block, adaptive) vs. non-sequential (a fixed `dwell_target`
wait) mirror `~/legacy-commz`'s own `FrequencyAcquisition` reference —
`max_n_blocks` is sequential mode's own give-up cap, deliberately
independent of `dwell_target`.

::: doppler.acquire.CarrierAcquisition

## `Acquisition` — streaming burst acquisition

`Acquisition` searches a streamed cf32 signal for a repeated BPSK PN burst over the
joint (Doppler × code-phase) grid, sizing its own search grid — coherent depth,
CFAR threshold, non-coherent looks — from the physics `(chip_rate, cn0_dbhz, pfa, pd)` using `doppler.detection`. Push
arbitrary-length blocks; it yields one record per detection — `(doppler_bin, code_phase, peak_mag, noise_est, test_stat, snr_est)` — whose `(doppler_bin, code_phase)` seed the `BurstDespreader`. See the
[DSSS Burst Acquisition guide](../guide/dsss-acquisition.md) for the search-space
sizing and a worked example.

::: doppler.acquire.Acquisition

## `BurstAcquisition` — the burst front door to acquisition

`BurstAcquisition` is the burst-oriented front door to the shared
acquisition engine: a bounded preamble is searched over a (Doppler, code
phase) grid and the peak is reported once, rather than the continuous
streaming push of [`Acquisition`](#acquisition-streaming-burst-acquisition).
Both wrap the same stateless kernel; the two front doors differ only in how
the capture is fed and when the estimate is emitted.

::: doppler.acquire.BurstAcquisition

## `BurstCapture` — acquisition's output, turned into bursts

`BurstCapture` is the stage between a detector and whatever consumes a burst.
[`BurstAcquisition`](#burstacquisition-the-burst-front-door-to-acquisition)
reports an END anchor and a code phase that is a lag **modulo one code
period**, so it names the alignment within a preamble repetition and never
which one — and a burst has a frame that begins in one specific repetition.
`BurstCapture` resolves that (the refine stage), keeps the look-back needed to
reach a start that has already gone past, and emits the burst's **samples**.

It stops there. Demodulating is
[`BurstDemod`](#burstdemod-feedforward-dsss-frame-demodulator)'s job, and
[`DsssBurstReceiver`](../gallery/dsss-burst-receiver.md) is this
plus that. Reach for `BurstCapture` directly when you want the bursts
themselves: a recorder, an offline corpus, or a second consumer fanned out
from one stream.

`push()` returns windows concatenated — burst `i` occupies `burst_len`
samples at `i*burst_len` — and `events()` returns the matching record for
each. It owns its acquisition engine rather than accepting detections from
elsewhere, because a hit's `samples_consumed` is stream-absolute only for an
engine fed continuously and never reset; see
[the design note](../design/burst-capture.md) for why that invariant cannot
be delegated to a caller.

::: doppler.acquire.BurstCapture

### `PersistentBurstCapture` — the same capture, with the ring in a file

The look-back is essentially the whole checkpoint: at a 511-chip code, 5
repetitions and an 8029-symbol frame, `BurstCapture.state_bytes()` is
16.68 MB and all but ~20 kB of it is retained history.
`PersistentBurstCapture` takes a `path` and backs the ring's pages with that
file (`MAP_SHARED`), so the samples **are** the file's contents — there is no
mirror buffer and no flush path. Two things follow: the blob drops to 21.6 kB
because the history is already durable, and the history outlives the process,
so a capture restored over the same file reaches back across a restart into a
burst that began before it.

It is a view over the same core — the constructor differs and nothing else
does — so every method and read-back is shared verbatim. A blob does not
travel between the two flavours in either direction: `state_bytes()` differs,
and accepting one for the other would resume a capture whose history was
somewhere else.

::: doppler.acquire.PersistentBurstCapture

## `bin_to_signed` — read an FFT grid the way numpy does

Maps a reported Doppler **bin index** to its **signed** frequency index —
`numpy.fft.fftfreq(n) * n`, exactly. Multiply by `doppler_res_hz` for Hz:

```python
import numpy as np

from doppler.cvt import bin_to_nrz
from doppler.acquire import BurstAcquisition, bin_to_signed
from doppler.wfm import PN, mls_poly

code = np.asarray(
    PN(poly=mls_poly(5), seed=1, length=5).generate(31)
).astype(np.uint8)
# The preamble is its samples: chips by bin_to_nrz, held 4 samples a chip.
nrz = np.zeros(code.size, np.float32)
bin_to_nrz(code, nrz)
preamble = np.repeat(nrz, 4).astype(np.complex64)
acq = BurstAcquisition(preamble, reps=4, fs=4e6, cn0_dbhz=55.0)

# One repeated-code burst, so push() reports a hit to read the bin off.
burst = np.tile(preamble, 8)
hit_bin = acq.push(burst)[0][0]

f0_hz = bin_to_signed(hit_bin, acq.doppler_bins) * acq.doppler_res_hz
print(f"bin {hit_bin} -> {f0_hz:+.0f} Hz")
```

Call it rather than writing the fold out. The search and its hand-off must
agree on the convention, and a consumer seeded on the wrong side of it is off
by the **full search span** — a failure that once surfaced here as a receiver
reporting `tracking == 1` while decoding noise. It is a thin wrapper over
`dp_fftfreq_index()` in `clib_common.h`, so C callers inline the same code.

Two things worth knowing. An **even** grid's Nyquist bin is `-n/2`, following
numpy; this engine reported `+n/2` there until the burst-chain certification,
so a formula ported in from numpy now agrees with it. And the C companion
`dp_fftfreq(bin, n, fs)` returns the bin's frequency directly, taking the
sample **rate** where numpy takes the sample **spacing**.

::: doppler.acquire.bin_to_signed

## Related pages

<!-- related-pages:start -->

**Gallery** — [Async DSSS Receiver: the SPEC waveform through coupled Doppler](../gallery/async-dsss-receiver-spec.md), [CarrierAcquisition: RRC Pulse Shaping](../gallery/carrier-acq-rrc.md), [Correlation and Detection](../gallery/corr.md), [DSSS Acquisition — Pd / Pfa vs Es/N0](../gallery/dsss-acq-characterization.md), [A 5-Burst DSSS Link — wfmgen's Three Faces, the Full Receiver Chain](../gallery/dsss-burst-pipeline.md), [DsssBurstReceiver — the Composed Burst Chain](../gallery/dsss-burst-receiver.md), [DsssReceiver — the Composed Continuous DSSS Receiver](../gallery/dsss-receiver.md), [Gallery](../gallery/index.md)
**Guides** — [Tracking a Population of DSSS Emitters with `AsyncDsssPool`](../guide/async-dsss-pool.md), [DSSS Burst Acquisition](../guide/dsss-acquisition.md), [Guides](../guide/index.md), [Checkpoint & Resume](../guide/state-serialization.md)
**Design** — [Design — pure-functional acquisition kernel (elastic fleet)](../design/acq-fn.md), [API taxonomy: the DSP building-block hierarchy and its naming axis](../design/api-taxonomy.md), [AsyncDsssReceiver — the continuous DSSS receiver, from spec to object](../design/async-dsss-receiver.md), [BurstBank — the coarse-Doppler bank as one C object](../design/burst-bank.md), [`BurstCapture`: acquisition's output, turned into bursts](../design/burst-capture.md), [CoarseChannel — is a channel an object, or a slice of the bank?](../design/coarse-channel.md), [Corr2D: decoupled (interpolated) inverse length](../design/corr2d-interpolated-inverse.md), [Detection Sizing — the four laws behind one prefix](../design/detection.md), [DSSS acquisition: stateless, parallel, dynamics-capable](../design/dsss-acquisition.md), [`DsssBurstReceiver`: the burst chain, composed in C](../design/dsss-burst-receiver.md), [Design](../design/index.md), [State Serialization — the standard bytes interface](../design/state-serialization.md)
**Contributing** — [DSSS Primary Use Cases for Code Acquisition Design](../dev/contributing/dsss-use-cases.md), [Validation log](../dev/contributing/validation-log.md), [Contributing](../dev/index.md)

<!-- related-pages:end -->
