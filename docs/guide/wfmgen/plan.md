# Parameter Sweeps — the `Plan` engine

Evaluating a system — a detector, a demodulator, a synchroniser — means feeding
it the **same scene at many operating points**: a detection or BER curve is a
sweep over SNR; a robustness check nudges a gain or a phase; a Monte-Carlo run
repeats one scene under fresh noise. Re-composing from scratch at every point is
wasteful, because a composed scene is already a **linear form**,

$$
\text{out} \;=\; \sum_k \text{gain}_k \cdot \text{signal}_k \;+\; \text{noise},
$$

and the expensive DSP — spreading, root-raised-cosine pulse shaping, the local
oscillator — lives entirely in the **signal** terms. Those do not change when you
sweep a level, a phase, the SNR, or the noise seed. Only cheap coefficients do.

`prepare(scene)` renders and caches each source **once**, returning a `Plan`.
Every subsequent render is a cheap re-weighted sum of the cache — and **bit-for-bit
identical** to a full compose. It is not a fifth rung on the
[object-model ladder](concepts.md); it is a *cache over a finished scene*, for
when you need that scene many times.

## Preparing a scene

Build a [scene](scenes.md) exactly as you would for `compose()`, then prepare it.
The baseline `render()` (no overrides) reproduces `Composer(scene).compose()`
exactly:

```python
import numpy as np
from doppler.wfm import Composer, Segment, prepare, qpsk

scene = Composer(Segment.sum(
    qpsk(snr=8.0, seed=7, sps=8, pn_length=9),        # the wanted user (anchor)
    qpsk(seed=101, sps=8, pn_length=9, level=-6.0),   # a co-channel interferer
    fs=1e6, num_samples=4096,
))

plan = prepare(scene)                                 # render + cache ONCE
assert np.array_equal(plan.render(), scene.compose())  # baseline is bit-exact
len(plan), plan.n_sources                             # samples, signal sources
```

`len(plan)` is the sample count; `plan.n_sources` counts the **signal** sources
(the resolved noise floor is separate). `prepare(scene)` is shorthand for
`Plan(scene)` — either works, and a `Plan` is a context manager if you want to
free its cache promptly.

## The overridable axes

`render()` takes five optional overrides. Omit them all for the baseline; pass
any subset to vary that axis. The three per-source axes are lists in scene order,
length `n_sources`:

| Override | Type          | Meaning                                                     |
| -------- | ------------- | ----------------------------------------------------------- |
| `gains`  | `list[float]` | absolute source levels in dBFS (`0` = unit power)           |
| `phases` | `list[float]` | per-source phase rotation in radians (`0` = identity)       |
| `enable` | `list[bool]`  | `False` drops a source — an exact `gain = 0` term           |
| `snr`    | `float`       | global SNR (dB) — moves **only** the noise floor            |
| `seed`   | `int`         | the noise realization (defaults to the scene's anchor seed) |

```python
# one wanted user, interferer pulled down 6 dB and rotated 90°, floor at 3 dB
x = plan.render(gains=[0.0, -12.0], phases=[0.0, np.pi / 2], snr=3.0)
x.shape, x.dtype
```

Every override composes: `gains` and `phases` and `enable` and `snr` and `seed`
can all be set in one call. Anything you leave out keeps its resolved value from
the scene.

## Which method to reach for

`render()` is the general form. Three convenience methods wrap it for the common
campaigns — reach for whichever names your intent:

- **`at(snr, seed=None)`** — the scalar fast path (no JSON round-trip), the hot
    loop of a sweep. `seed` defaults to the anchor seed, which reproduces a full
    compose at that SNR.
- **`sweep(snrs, seed=None)`** — yields `(snr, samples)` across an SNR list at a
    **held** noise seed, so only the floor moves. The natural stimulus for a
    Pd/BER-vs-SNR curve.
- **`monte_carlo(snr, n, seed0=0)`** — yields `n` independent noise realizations
    at a **fixed** SNR; the signal is identical across draws, only the noise
    differs.

```python
# a held-seed SNR curve — same noise realization, only the floor moves
curve = {snr: x for snr, x in plan.sweep([-3.0, 0.0, 3.0, 6.0, 9.0])}

# 16 independent noise draws at 6 dB — identical signal, different noise
draws = list(plan.monte_carlo(6.0, 16, seed0=1000))
assert len({d.tobytes() for d in draws}) == 16       # every realization differs
```

## Recipe — a detection / BER curve

Sweep the channel SNR and measure a per-point statistic. Here a light
matched-filter peak-SNR against a clean copy of the wanted user (itself produced
by disabling the interferer — see the next recipe). A real campaign averages each
point over Monte-Carlo draws:

```python
# a clean, interference-free copy of the wanted user = the matched filter
template = plan.render(enable=[True, False])[: len(plan) // 2]
template = template / (np.linalg.norm(template) + 1e-30)

def peak_snr(x):
    c = np.abs(np.correlate(x, template, mode="valid"))
    pk = int(c.argmax())
    off = np.delete(c, slice(max(0, pk - 4), pk + 5))
    return 10 * np.log10(c[pk] ** 2 / (np.mean(off ** 2) + 1e-30))

snrs = np.arange(-6.0, 13.0, 3.0)
# each point: mean peak-SNR over 8 independent noise draws
detect = [np.mean([peak_snr(plan.at(s, 2000 + j)) for j in range(8)])
          for s in snrs]
len(detect) == len(snrs)
```

The measured curve climbs with channel SNR and then flattens as the
multiple-access interference floor takes over — and the cache reproduces the
precise noise power the resolver placed at every point.

## Recipe — isolate or recombine sources

`enable` drops a source as an exact `gain = 0` term, so you can pull any subset
out of a scene without rebuilding it — a clean reference, an interference-only
capture, a jammer-free template:

```python
wanted_only = plan.render(enable=[True, False])       # signal + noise, no MAI
interferer_only = plan.render(enable=[False, True])   # co-channel only + noise
```

## Recipe — a gain-imbalance or phase sweep

Because `gains` and `phases` are cheap post-multiplies on the cache, a
sensitivity sweep over a relative level or phase is nearly free:

```python
# sweep the interferer's level from 0 down to -18 dB (wanted user fixed at 0)
gain_sweep = [plan.render(gains=[0.0, g]) for g in range(0, -19, -3)]

# sweep its carrier phase across a full turn
phase_sweep = [plan.render(phases=[0.0, ph])
               for ph in np.linspace(0, 2 * np.pi, 8, endpoint=False)]
len(gain_sweep), len(phase_sweep)
```

## Why it is fast (and exact)

The cost of `prepare()` is one full render of every source. After that, each
`render()`/`at()` is a handful of scaled vector adds over the cache — no LFSR, no
convolution, no transcendentals. So a campaign of `P` points costs roughly
"one compose + `P` cheap sums" instead of "`P` full composes", and the speedup
grows with the number of sources and the sample count, since that is exactly the
signal work the cache elides.

Exactness is guaranteed by construction: the composer's accumulate is
`Σₖ gainₖ·synthₖ` in source order with `gainₖ = 10^(levelₖ/20)`, and `synthₖ`
depends on everything *except* level — so a re-weighted sum of the cached renders
is bitwise identical to a full compose at those gains. `render()` with no
overrides equals `compose()` to the last bit, which is the standing test
contract (checked in both harnesses). Phase is a defined render-time rotation
(`φ = 0` is the identity, skipped entirely), exact by construction rather than
reproduced.

## Scope and limits (v1)

`Plan` v1 covers the common evaluation scene: a **single** finite, non-ranged
`sum` segment with a separable noise floor. A scene that is multi-segment,
`continuous`, `repeat`, or *ranged* (a `[lo, hi]` field), or one whose only
noisy source is a lone **bundled** source — its private RNG fused into the
signal, so the floor cannot be separated — raises `ValueError` at `prepare()`:

```python
# a lone bundled noisy source is not separable → rejected
try:
    prepare(Composer(type="qpsk", snr=6.0, num_samples=1024))
except ValueError as e:
    print("rejected:", str(e)[:38], "…")
```

A `Plan` is **not serializable** — it is a re-creatable cache, so persist the
scene's spec JSON (`Composer.to_json()`) and re-`prepare()` on the far side.
Frequency (Doppler) and delay (multipath) are planned follow-ups on the same
frame — additive axes, not a rewrite.

## See also

- [Concepts](concepts.md) — where `Plan` sits relative to the Synth → Segment →
    Timeline → Composer ladder.
- [Scenes](scenes.md) — building the `Composer` scenes a `Plan` prepares.
- [Plan gallery walkthrough](../../gallery/plan.md) — a five-user CDMA
    detection surface driven by one `Plan`, with the figure.
- [Python API](../../api/python-wfmgen.md#plan-prepare-once-stimulus-engine) —
    the full `Plan` / `prepare` reference.
