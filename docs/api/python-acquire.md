# Python Acquire API

The `doppler.acquire` module holds the searches that find a signal before
anything tracks it:

- **`BurstCapture`** — finds a burst of any repeated complex preamble (a
    PN code, Zadoff-Chu, a chirp or a QPSK sequence) and emits its samples,
    with **`PersistentBurstCapture`** keeping its ring in a file.
- **`CarrierAcquisition`** — a PSDMF (power-spectral-density
    matched-filter) residual-carrier frequency estimator. It runs after
    the code-phase search's coarse Doppler search as a one-shot refinement stage:
    non-coherently average the incoming stream's power spectrum, then
    circularly correlate that average against a known power spectrum shape
    (the default is the average PSD of a random rectangular-pulse BPSK
    stream, a sinc²; `psd_template` overrides it for a different pulse shape
    or modulation) to find the residual carrier offset.
- **`bin_to_signed`** — the Doppler-bin fold convention the searches and
    their hand-offs share.

The code-phase × Doppler searches themselves, continuous and burst, are
flavors of one class, `Correlator`, on the
[Correlation page](python-correlation.md) (`framing="continuous"` and
`framing="burst"`). The DSSS receivers in the [DSSS API](python-dsss.md)
compose these objects.

Source:
[`src/doppler/acquire/__init__.py`](https://github.com/doppler-dsp/doppler/blob/v0.82.0/src/doppler/acquire/__init__.py)

See the [CarrierAcquisition: RRC Pulse Shaping gallery page](../gallery/carrier-acq-rrc.md)
for a worked example showing why the template matters.

______________________________________________________________________

## `CarrierAcquisition` — PSDMF residual-carrier estimation

Composes `doppler.spectral.PSD` (FFT + window + non-coherent power
averaging), the 1-D detector flavor of `doppler.correlation.Correlator`
(FFT-based correlation of the averaged power against the known template, plus
a noise-referenced test statistic), and `doppler.detection`'s Pfa/Pd
statistics (the same ones the code-phase searches are built on) for the detection gate. `sequential`
(test every block, adaptive) vs. non-sequential (a fixed `dwell_target`
wait) mirror the two modes of the `FrequencyAcquisition` reference design —
`max_n_blocks` is sequential mode's own give-up cap, deliberately
independent of `dwell_target`.

::: doppler.acquire.CarrierAcquisition

## `BurstCapture` — acquisition's output, turned into bursts

`BurstCapture` is the stage between a detector and whatever consumes a burst.
The burst search,
[`Correlator(preamble, framing="burst", ...)`](python-correlation.md#doppler.correlation.Correlator),
reports an END anchor and a code phase that is a lag **modulo one code
period** (for a Zadoff-Chu preamble, shifted by any Doppler the search did not
resolve first), so it names the alignment within a preamble repetition and
never which one — and a burst has a frame that begins in one specific repetition.
`BurstCapture` resolves that (the refine stage), keeps the look-back needed to
reach a start that has already gone past, and emits the burst's **samples**.

It stops there. Demodulating is
[`BurstDemod`](python-dsss.md#burstdemod-feedforward-dsss-frame-demodulator)'s job, and
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
16.84 MB, and all but 135 kB of it is retained history.
`PersistentBurstCapture` takes a `path` and backs the ring's pages with that
file (`MAP_SHARED`), so the samples **are** the file's contents — there is no
mirror buffer and no flush path. Two things follow: the blob drops to 135 kB (the
acquisition engine's own state) because the history is already durable, and the history outlives the process,
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
from doppler.acquire import bin_to_signed
from doppler.correlation import Correlator
from doppler.wfm import PN, mls_poly

code = np.asarray(
    PN(poly=mls_poly(5), seed=1, length=5).generate(31)
).astype(np.uint8)
# The preamble is its samples: chips by bin_to_nrz, held 4 samples a chip.
nrz = np.zeros(code.size, np.float32)
bin_to_nrz(code, nrz)
preamble = np.repeat(nrz, 4).astype(np.complex64)
acq = Correlator(preamble, framing="burst", reps=8, sample_rate=4e6,
                 cn0_dbhz=60.0)

# One repeated-code burst, so push() reports a hit to read the bin off.
burst = np.tile(preamble, 8)
hit_bin = int(acq.push(burst)[0]["row"])

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
