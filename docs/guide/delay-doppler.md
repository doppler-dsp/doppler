# Delay-Doppler Search

`doppler.spectral.DelayDoppler` answers one question about a stretch of
samples: **where is this code, and how fast is it rotating?** You give it
one period of a code (a PN code, a Zadoff-Chu sequence, any sequence with a
sharp autocorrelation) and a frame of the received stream. It hands back a
**delay-Doppler map**: a table with a row for every Doppler offset and a
column for every delay, whose largest cell is the best guess for both.

Use it when you need the *map itself*: to see two emitters at once, to
read a Doppler to a fraction of a bin, to rank many candidate frames, or to
search a carrier offset that is larger than a code bin. If you only want
bursts found in a stream and handed back, start with
[Acquiring a Repeated Preamble](acquisition.md) instead; it runs this same
search for you.

______________________________________________________________________

## What a map is

A frame is `ny` code periods of `nx` samples each, back to back, so
`ny * nx` samples in all. Each period is correlated against the code (that
is the **delay** axis, `nx` lags). The slow-time FFT then runs down every
delay column across the `ny` periods (that is the **Doppler** axis).

| axis        | what a cell index means                                                     |
| ----------- | --------------------------------------------------------------------------- |
| rows (0)    | Doppler. Zero Doppler is row `ny_out // 2`; each row is `doppler_step` more |
| columns (1) | delay: how many samples the code was late                                   |

`doppler_step` is `1 / (ny_out * nx)` cycles per sample, so multiply by the
sample rate for Hz. The map comes back **flat** (`ny_out * nx` values, row
by row); reshape it to `(ny_out, nx)`.

______________________________________________________________________

## One map

A code delayed by 5 samples and rotating at 3 Doppler bins, in a frame of 16
periods:

```python
import numpy as np
from doppler.spectral import DelayDoppler

rng = np.random.default_rng(0)
NX, NY = 64, 16                       # code length, periods per frame
DELAY, DOPPLER_BINS = 5, 3
code = rng.choice([-1.0, 1.0], NX).astype(np.complex64)

n = np.arange(NY * NX)
f = DOPPLER_BINS / (NY * NX)          # cycles per sample
x = (np.tile(np.roll(code, DELAY), NY)
     * np.exp(2j * np.pi * f * n)).astype(np.complex64)

dd = DelayDoppler(code, ny=NY)
m = dd.execute(x).reshape(NY, NX)     # a dump on every frame (dwell 1)

row, col = np.unravel_index(np.argmax(np.abs(m)), m.shape)
print(int(row) - NY // 2, int(col))   # (Doppler bins, delay)
assert (int(row) - NY // 2, int(col)) == (3, 5)
```

The peak sits at `(3, 5)`: three Doppler bins from zero, five samples late.
Its height is about `ny * nx` for a unit-amplitude signal, a little less
when the carrier turns during a code period (that rotation is not corrected
inside the period; the [bank](#unknown-carrier-the-hypothesis-bank) below is
what searches it).

Three knobs shape the map:

- **`ny_out`** zero-pads the slow-time samples to `ny_out > ny` rows, a
    finer Doppler grid over the same response. A Doppler between two native
    bins lands on its fractional row, and the peak at a bin centre is
    unchanged.
- **`dwell`** coherently sums that many frames before dumping. `execute`
    returns `None` until the last frame of a dwell, and the map after it.
- **`nthreads`** splits the work; the map is bit-identical at any count.

```python
# A Doppler of 3.5 bins is between two native rows. A 4x finer grid finds it.
f = 3.5 / (NY * NX)
x35 = (np.tile(np.roll(code, DELAY), NY)
       * np.exp(2j * np.pi * f * n)).astype(np.complex64)

fine = DelayDoppler(code, ny=NY, ny_out=4 * NY)
mf = fine.execute(x35).reshape(fine.ny_out, NX)
r, c = np.unravel_index(np.argmax(np.abs(mf)), mf.shape)
print((int(r) - fine.ny_out // 2) * fine.doppler_step * (NY * NX), int(c))
assert int(r) - fine.ny_out // 2 == 14 and int(c) == DELAY  # 3.5 bins
```

If you hold the output yourself, `execute(x, out=buf)` writes the map into
`buf` (it must hold `execute_max_out()` values) and returns a view of it;
the next dump into the same `buf` overwrites that view. Without `out=` every
dump is a new array.

______________________________________________________________________

## Batches: many frames, one call

A receiver rarely has one frame. It tries a frame under several hypotheses,
or ranks the candidate frames from several antennas, and keeps the best.
Looping over `execute` works but leaves the loop in Python and, with a
`dwell` above 1, mixes the frames into one dwell. A **batch** is `H` frames back to back
(`H * ny * nx` samples) handed over in one call:

- the object's dwell is **never touched**, so a batch can run in the middle
    of a dwell;
- map `h` is exactly what a fresh `dwell=1` object returns for frame `h`,
    bit for bit;
- a batch that is empty, or not a whole number of frames, is a `ValueError`
    with nothing written.

What comes back depends on how you want the `H` maps combined:

| call                | returns                                                        | use it to                                 |
| ------------------- | -------------------------------------------------------------- | ----------------------------------------- |
| `execute_all`       | `(H, n_out)` complex, one map per frame                        | look at every map                         |
| `execute_coherent`  | one `(n_out,)` complex map, the frames summed **coherently**   | integrate frames that share a phase       |
| `execute_power_sum` | one `(n_out,)` float map, the **powers** summed                | integrate frames whose phases differ      |
| `execute_power_max` | `(power, index)`: the strongest power per cell and which frame | keep the best hypothesis per cell         |
| `execute_peaks`     | `(peak, row, col, mean)`, one record per frame                 | rank the frames by their own peak-to-mean |

`execute_coherent` and `execute_power_sum` differ exactly where it matters:
frames `x` and `-x` cancel in the first and add in the second.
`execute_peaks` and `execute_power_max` differ too, and the difference is
the point: the best frame by its **own** peak-to-mean is not always the frame
that owns the strongest cell of the **pointwise maximum** map, so ask for the
statistic you mean.

Here one frame is tried under eight carrier hypotheses (each removes a
candidate offset from a copy of the frame), and the batch picks the
hypothesis that left the signal most compact:

```python
--8<-- "src/doppler/examples/delay_doppler_batch_demo.py:setup"
```

```python
--8<-- "src/doppler/examples/delay_doppler_batch_demo.py:search"
```

`execute_all` takes `out=` as a 2-D `complex64` array of shape
`(rows, n_out)` with at least `H` rows; the others return new arrays.

______________________________________________________________________

## Unknown carrier: the hypothesis bank

A map corrects no carrier **within** a code period, so a carrier off the
grid costs the Dirichlet factor of its offset: about 0.9 dB at a quarter
bin, 3.9 dB at half a bin, 19 dB at 0.9 bin, and at a whole bin the
correlation falls into its null (the delay is not even found). When the offset is unknown and
can be larger than a bin, you search it, and a batch of corrected frame
copies is the slow way to do that. The **bank** does the same search without
the copies: every carrier shares the frame's row transforms.

The bank speaks in a carrier grid. With `L` fractions per bin, carrier `g`
is an offset of `g / (L * nx)` cycles per sample. `bank_power_max` makes one
map per carrier and keeps the strongest power per cell, and which carrier
it was. `bank_detect` is the whole detector in one call:

1. **Detect** on a coarse grid (`L_d`, say half bins) and take every cell
    that clears `t_candidate` and is a local maximum.
1. **Refine** each candidate at its own cell on a fine grid (`L_r`, say
    quarter bins), which recovers the loss the coarse grid's straddle cost.
1. **Confirm**: keep the candidates whose refined power clears
    `t_confirm`.

The thresholds are powers in the map's own units, so they come from
**noise-only frames**, as any detector's do. The example below measures the
strongest coarse cell in 50 noise frames and sets the candidate and confirm
thresholds a little above it:

```python
--8<-- "src/doppler/examples/delay_doppler_bank_demo.py:setup"
```

```python
--8<-- "src/doppler/examples/delay_doppler_bank_demo.py:search"
```

One record comes back: the code at delay 37, on the quarter-bin carrier
nearest 1.3 bins, stronger than the coarse grid saw it. Records arrive in
cell order, one entry per record in each of the five arrays
`(power, g, rate, row, col)`. `rates` (empty here) adds per-period
weights, a second hypothesis axis for a rate or a window.

!!! note "Only the fixed reductions are in Python"

    The bank and the batch calls above are the reductions Python can ask
    for. The C library also takes your own reducer (`dp_corr2d_execute_fold`,
    `dp_corr2d_bank_fold`); it is not bound for Python.

______________________________________________________________________

## See also

- [Python: Spectral API](../api/python-spectral.md): `DelayDoppler` and
    `Corr2D`, every method and property
- [Gallery: Delay-Doppler Search](../gallery/delay-doppler.md): the map, the
    coarse straddle loss and the refine that recovers it, drawn
- The runnable examples
    [`delay_doppler_batch_demo.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/examples/delay_doppler_batch_demo.py)
    and
    [`delay_doppler_bank_demo.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/examples/delay_doppler_bank_demo.py),
    each with a C twin in `native/examples/`
- [Acquiring a Repeated Preamble](acquisition.md): the burst-level search
    that uses this map
- [Design: batches](../design/corr2d-batch.md) and
    [hypotheses](../design/corr2d-hypotheses.md): why the batch and the bank
    are shaped this way (for those who want the reasoning)
