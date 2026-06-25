# Waveform amplitude & composition

!!! success "Status: Implemented"

    This RFC is now built. Per-source `level`, `--headroom`, multi-source
    `Segment.sum()` over one resolved noise floor, and the `.add()` timeline all
    landed on `main`; the Python API is the unified `Synth` + `tone`/`bpsk`/`qpsk`/`pn`/
    `noise` builders with `Segment.sum` / `Segment.add` (see the
    [Waveform Generator guide](../guide/wfmgen.md)), and the JSON `"sum"` schema
    behind `wfmgen --from-file` is byte-identical to it. Single-source,
    `level = 0`, `--headroom 0` reproduces the pre-0.11 output byte-for-byte.
    `--record` captures the resolved sum **and** the `--headroom`, so
    `--from-file` reproduces a scene exactly. The text below is kept as the
    design rationale.

## Motivation

The generator currently emits **one source per segment**, sequences segments in
time, and offers `--snr` as the only level control. Three things push past that:

1. **Amplitude is a power, not a constant envelope.** Today's tone/PSK/PN
    waveforms happen to be constant-envelope (`|z| = 1`), so they sit exactly at
    full-scale. That is a property of the current set, *not* a design invariant —
    RRC pulse-shaping, QAM, and OFDM all have **PAPR > 0 dB**, so their peaks
    exceed full-scale and clip. The level idiom must be **average power**, and
    peak headroom must be a first-class control. (See
    [Amplitude & full-scale](../guide/wfmgen.md#amplitude-full-scale).)
1. **Real scenes are sums.** A signal of interest plus interferers plus a noise
    floor is an **additive mix** of sources at different frequencies and levels —
    which needs a per-source level, something the single-source model lacks.
1. **Clipping should be observable.** When integer output saturates, the run
    should say so and tell the user exactly how to fix it.

This RFC introduces an average-power **level** per source, a **`--headroom`**
backoff, **peak-driven clip detection**, and a two-verb **composition** model.

## Amplitude model

The anchor is **`0 dBFS = unit average power`** — the existing normalisation.
Every source declares a **`level`** in dBFS (≤ 0): its average power relative to
digital full-scale (`±1.0` per I/Q axis).

- A lone source defaults to `level = 0` → today's behaviour.
- A source's instantaneous power varies with its **PAPR**; `level` fixes the
    *average*. The composite peak depends on how the summed sources align.

The wire mapping is unchanged: `cf32`/`cf64` verbatim (never clip); `ci32`/
`ci16`/`ci8` saturate each axis to `±1.0` then scale to `±(2³¹−1 / 32767 / 127)`.

## SNR lives on the source

SNR is a **per-source** parameter (`snr`, `snr_mode`), as it already is in
`synth`. Two reasons:

- **It is self-referential.** `qpsk(snr=15)` means *this* source at 15 dB — the
    reference is the source it is attached to, by definition. No external "SNR vs
    what?" needs answering.
- **`snr_mode` is intrinsic to the modulation.** Es/No and Eb/No convert to a
    noise level using the source's *bits-per-symbol* and *samples-per-symbol*; a
    `tone` and a `qpsk` cannot share one Eb/No. The reference frame belongs with
    the source.

The conversions (unchanged from today):

```text
SNR_fs = snr                                   # snr_mode = fs
SNR_fs = EsNo − 10·log10(sps)                  # snr_mode = esno
SNR_fs = EbNo + 10·log10(bps) − 10·log10(sps)  # snr_mode = ebno
```

### One shared noise floor

A segment models one receiver, so it has **one** thermal noise floor across the
band. Source-level SNR resolves against it:

- A source's `snr` **anchors** the floor: `floor_dBFS = level(src) − SNR_fs(src)`
    (noise power `= P_src / 10^(SNR_fs/10)`). Put `snr` on the signal of interest.
- Other summed sources give a `level`; their SNRs are then **derived**
    (`level − floor`).
- A floor not tied to any signal is an **explicit noise source**: `noise(level=N0)`
    sets the floor at `N0` dBFS (integrated over `fs`).
- `snr` on a second source is sugar for *"place me `snr` dB above the floor"* →
    `level = floor + snr`.

If nothing anchors a floor (every source clean, `snr ≥ 100`), there is no noise —
exactly as today.

### Worked example

Take the three-source mix used in the Python snippet below — a QPSK SoI plus
two CW interferers — at `fs = 1 MHz`:

```python
Segment.sum(
    qpsk(sps=8, snr=15, snr_mode="esno"),  # anchor; level defaults to 0 dBFS
    tone(freq=200e3, level=-12.0),         # interferer A
    tone(freq=-150e3, level=-20.0),        # interferer B
    num_samples=N,
)
```

Resolving the powers, step by step:

1. **Anchor → floor.** The QPSK source anchors. Convert its Es/No to SNR over
    `fs`: `SNR_fs = 15 − 10·log10(8) = 15 − 9.03 = 5.97 dB`. With `level = 0`,
    `floor_dBFS = 0 − 5.97 = −5.97 dBFS`. The noise floor is placed there,
    integrated over the full band.
1. **Per-source linear gains** (`gain = 10^(level/20)`, `wfm_compose.c`):
    QPSK `level 0 → ×1.000`; interferer A `−12 dBFS → ×0.251` (power `−12 dBc`
    vs the SoI); interferer B `−20 dBFS → ×0.100` (`−20 dBc`).
1. **AWGN amplitude** (`wfm_awgn_amplitude.c`): per-quadrature
    `σ = sqrt(P_sig / (2·10^(SNR_fs/10)))`. With the SoI's average power
    `P_sig = 1` and `SNR_fs = 5.97 dB` (`10^0.597 = 3.95`),
    `σ = sqrt(1 / (2·3.95)) = 0.356`, i.e. complex noise power
    `2σ² = 0.253 ≈ −5.97 dBFS` — the floor, as designed.
1. **Realised SNRs.** SoI: `0 − (−5.97) = 5.97 dB` over `fs` (15 dB Es/No).
    Interferer A sits `−12 − (−5.97) = −6.0 dB` relative to the floor;
    B sits `−20 − (−5.97) = −14.0 dB`.

This is exactly what `TestSNRModes` and `TestNoise` in
`src/doppler/wfm/tests/test_dsp_correctness.py` measure back from the generated
samples (`wfm_awgn_amplitude` / `wfm_ebno_to_snr_db` conversions, plus per-mode
realised-SNR and noise-power estimates).

### Raw kernel vs composer: who applies the gain

A standalone `Synth` (and `_SynthEngine`) is the **raw, scale-free kernel**:
`Synth(type="noise").steps(n)` always emits **unit complex power**
(`σ² = 0.5` per quadrature) *regardless of* `level`, `snr`, or `snr_mode`. The
`level` gain and the multi-source noise-floor placement above are applied by
the **`Composer`** (`wfm_compose.c` post-multiplies each source by
`10^(level/20)`; `wfm_resolve.c` sets the floor). So amplitude only appears
once samples flow through a `Composer`/`Segment`:

```python
>>> import numpy as np, doppler.wfm as w
>>> x = w.Synth(type="noise", level=-20.0, seed=7).steps(100_000)
>>> round(float(np.mean(np.abs(x) ** 2)), 3)   # raw kernel ignores level
0.999
>>> c = w.Composer([w.Segment("noise", level=-20.0, num_samples=100_000, seed=7)])
>>> round(float(np.mean(np.abs(c.compose()) ** 2)), 3)  # composer applies it
0.01
```

This split is intentional — the kernel stays scale-free so it composes
cleanly — but it is easy to trip over: **set `level`/`snr` on a `Segment`/
`Composer`, not on a bare `Synth`, when you want them to take effect.**

### `compose()` needs a finite spec

`Composer.compose()` materialises the **entire** stream into one array, so it
requires a bounded timeline. A `repeat=True` (loop the sequence) or
`continuous=True` (never stop) spec has no end — calling `compose()` on one
loops without bound. Pull bounded blocks instead with `Composer.stream(block=…)`
or `Composer.execute(n)` (the same engine behind the CLI `--continuous`). The
`repeat`/`continuous` flags still round-trip through `to_json`/`to_dict`.

## Headroom

**`--headroom <dB>`** (default `0`) reserves peak room by scaling the **final
composite** (signal + interferers + noise) so its average power sits at
`−H dBFS`. It is a single common gain `g = 10^(−H/20)` applied just before
quantisation.

- **SNR-invariant.** A common scale changes no power *ratio* — only the absolute
    level versus full-scale. Headroom is orthogonal to every `snr` and `level`.
- **Default `0`** = today: composite average power at full-scale; constant-
    envelope clean signals fill it, high-PAPR or noisy peaks clip.
- Rule of thumb: set `H ≳ PAPR(composite)`. Constant-envelope clean → `0`;
    RRC/QAM ≈ 4–8 dB; noisy more.
- **Scope:** a run-level `--headroom` default, overridable **per segment**
    (`headroom=` on a segment).
- Recorded into BLUE/SigMF/`--record` metadata so the absolute level is
    self-describing.

## Clip detection

Clipping is made **observed and quantified** without burdening the hot path.

- **Peak-driven.** Track the running peak `|I|`/`|Q|` of the cf32 composite — a
    single `max` reduction. Peak `> 1.0` ⇒ it clipped (integer types), and the
    remedy falls straight out: `headroom = ⌈20·log10(peak)⌉` dB.
- **Fused, no second pass.** The `max` folds into the loop already touching every
    sample (the quantise/headroom-scale pass). That loop is store/memory-bound, so
    a `vmax` accumulator hides under the stores — effectively free.
- **Opt-in fraction.** The exact clipped *fraction* ("12.4 %") is the one
    per-sample conditional; gate it behind `--clip-report`. The default path is
    byte-identical to today.
- **`--clip-error`** turns a clip into a non-zero exit (for pipelines/CI).
- **Library face.** The writer / `Composer` result exposes `peak_dbfs` and (when
    requested) `clip_fraction`, so Python callers assert on them — no stderr in a
    library. `--record` captures them too.

```text
wfmgen: warning: peak +6.0 dBFS clipped in ci16.
  remedy: add --headroom 6, or use --sample-type cf32.
```

Float types never clip, but `peak_dbfs` is still reported so a `cf32` capture
documents its true PAPR.

## Composition: two verbs, two axes

Composition has two orthogonal operations; keeping them distinct keeps the model
clear:

| Verb         | Axis              | Combines               | Produces   |
| ------------ | ----------------- | ---------------------- | ---------- |
| **`.sum()`** | frequency overlay | sources, same time     | a Segment  |
| **`.add()`** | time sequence     | segments, back-to-back | a Timeline |

```text
Synth   ──sum──▶  Segment  ──add──▶  Timeline
tone/qpsk/…       (mix + noise)      (sequence in time)
```

- **`Segment.sum(*synths, num_samples=…)`** overlays sources at the same time span (the
    additive mix).
- **`segment.add(other)`** concatenates segments in time (the sequence the
    composer already is); `timeline.add(seg)` appends.
- `--off` / `repeat` / `continuous` stay **timeline-level**, where they live now.
- **No `+` operator** — it would be ambiguous between mix and concatenate, the
    very distinction this model draws.

### Python

```python
from doppler.wfm import tone, qpsk, gap, noise, Segment, Composer

# mix: sum sources into one segment (same span)
sig = Segment.sum(
    qpsk(freq=0,      sps=8, snr=15, snr_mode="esno"),  # SoI; snr anchors the floor
    tone(freq=200e3,  level=-12),                       # CW interferer, 12 dB down
    tone(freq=-150e3, level=-20),                       # another, 20 dB down
    n=1_000_000,
)

# concatenate: segments in time
scene = (
    Segment.sum(tone(level=0), n=500_000)   # a tone burst
    .add(gap(n=100_000))                    # then silence
    .add(sig)                               # then the mix
)

Composer(scene, headroom=6).write("scene.cf32")   # 6 dB backoff for the sum's PAPR
```

### CLI / JSON

The composer CLI keeps `--from-file SPEC.json`; each segment grows a `"sum"`
array of sources (the JSON face of `Segment.sum`):

```json
{
  "segments": [
    {
      "n": 1000000,
      "sum": [
        { "type": "qpsk", "freq": 0,      "sps": 8, "snr": 15, "snr_mode": "esno" },
        { "type": "tone", "freq": 200000, "level": -12 },
        { "type": "tone", "freq": -150000, "level": -20 }
      ]
    }
  ],
  "headroom": 6
}
```

### C

`wfm_segment_t` grows from one synth's parameters to a small **list of source
descriptors** (each the current synth params plus `level`). `wfm_compose_execute`
runs each source's `synth_steps` into a scratch buffer and **accumulates** into
the segment output; the noise-anchoring source sets the floor. A single
summation path preserves the **byte-identical CLI ⇄ composer** guarantee. The
headroom gain and peak/clip tracking live in the writer, after composition.

## Compatibility

- A single source with `level = 0` and `--headroom 0` is **byte-identical** to
    today's single-segment output.
- `--snr` / `snr_mode` semantics are unchanged; they simply now live on a source
    inside a `sum` rather than on the segment.

## Open questions

- **`level` vs `snr` precedence** when a source over-specifies (both given) — pick
    one as authoritative or reject.
- **Coherent peaks.** `level` sets per-source *average* power; the composite peak
    depends on phase alignment between sources. Clip detection covers the
    empirical case, but a predicted-headroom hint (from per-source PAPR + count)
    could help before a run.
- **Per-source noise.** The model assumes one shared floor (one receiver). A
    multi-emitter / per-channel-noise scenario is explicitly out of scope.

## Non-goals

- A general DSP graph. This is additive mixing + time sequencing, not arbitrary
    routing.
- Pulse shaping / QAM / OFDM *sources* themselves — this RFC is the **amplitude
    and composition substrate** they will plug into, not those waveforms.
