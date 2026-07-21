# DSSS bursts — a burst train in one declaration

The canonical spread-spectrum test capture — *N bursts at a specific
data-symbol Es/N0, each a repeated PN preamble followed by a payload spread
with a second code, randomly placed with a minimum gap, over a continuous
noise floor* — is one `Segment`:

```python
import numpy as np
from doppler.wfm import Composer, Segment

rng = np.random.default_rng(0)
acq = rng.integers(0, 2, 128, dtype=np.uint8)   # preamble code A
dat = rng.integers(0, 2, 25, dtype=np.uint8)    # data-spreading code B
pay = rng.integers(0, 2, 200, dtype=np.uint8)   # payload bits
BARKER13 = np.array([1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1], np.uint8)

burst = Segment(
    type="dsss", fs=4e6, sps=4, seed=1,
    snr=10.0, snr_mode="esno",            # data-symbol Es/N0 (see below)
    acq_code=acq, acq_reps=4,             # preamble: code A x 4
    data_code=dat,                        # payload spread: code B
    sync=BARKER13, payload=pay,           # CRC-16 auto-appended
    delay_samples=(2_000, 10_000),        # arrival jitter before each burst
    off_samples=(4_000, 12_000),          # trailing gap, min 4k samples
    repeats=5,                            # -> a 5-burst train
)
comp = Composer([burst])
x = comp.compose()
```

This page is the one home for everything `type="dsss"` means; the other
guide pages carry a summary and link here.

______________________________________________________________________

## The burst anatomy

A `dsss` segment is one complete burst honouring the
[`BurstDemod`/`BurstDespreader`](../../api/python-dsss.md) frame contract:

```text
[ acq_code x acq_reps | (sync | payload | crc16(payload)) ^ data_code ]
   unmodulated preamble          every frame bit spread by code B
```

- The **preamble** is `acq_code` repeated `acq_reps` times, transmitted
    unmodulated — the coherent pull-in target the receiver's acquisition
    correlates against.
- The **frame** is `sync | payload | CRC-16` (CCITT, over the payload bits
    — the same `doppler.wfm.crc16` kernel the demod validates), each bit
    XOR-spread by the full `data_code`. The sync word is optional;
    `crc="none"` drops the trailer.
- Codes are plain 0/1 arrays of **any length** — no `2^n - 1` restriction —
    so the geometry matches whatever the receiver expects. The payload rides
    the shared `bits` field (keyword `payload=`; JSON key `"payload"`,
    `"pattern"` accepted).
- `sps` is samples per **chip**; the burst's span is intrinsic
    (`n_chips x sps` samples), so `num_samples` is derived and
    `--record`/`to_json()` always carry the real length. (A dsss source
    inside a multi-source `sum()` keeps the segment's explicit
    `num_samples`.)

## Es/N0 that means what the receiver measures

`snr_mode="esno"` (and `auto`) targets the Es/N0 of the outer **data
symbol** — `len(data_code)` chips × `sps` samples — which is the number a
despreader's data-aided estimator recovers. The `10*log10(sf*sps)`
spreading conversion happens internally. This matters because a hand-spread
`bits` pattern gets the *chip* as its symbol: the same `--snr 10` would
land `10*log10(sf)` dB apart between the two spellings. The payload is
BPSK, so `ebno` and `esno` coincide. (General SNR model:
[Levels & SNR](levels.md#snr-noise).)

## Random placement, deterministically

`repeats=5` plays the segment five times back-to-back; each **instance** is
`delay | burst | gap` (see [Scenes](scenes.md#burst-trains-repeats)):

- **`delay_samples=(lo, hi)`** re-draws per instance — per-burst *arrival
    jitter*. **`off_samples=(lo, hi)`** re-draws per instance — inter-burst
    *spacing*, with `lo` the guaranteed minimum gap. (Spacing between bursts
    composes as `off(k) + delay(k+1)`.)
- The AWGN is **fresh per instance** (bursts never share a noise
    realization) while the signal — codes, payload, code phase — is fixed.
- Every draw is a deterministic hash, so `to_json()`/`--record` stores the
    *ranges* and a replay is byte-identical.

## Gaps carry the noise floor

By default the delay and trailing gaps are **not silent**: every source's
additive-AWGN term keeps running through them — the same stream, the same
power — so the inter-burst region is the channel, exactly what an
acquisition/CFAR front-end needs to see for honest threshold tuning
(gh-409). A clean scene (no AWGN anywhere) still renders exact-zero gaps,
and `gap_noise="off"` restores hard zeros per segment:

```python
floor = 10 ** (-(10.0 - 10 * np.log10(25 * 4)) / 10)   # esno → over-fs power
gap = x[-4000:]                                        # inside the last gap
assert abs(float(np.mean(np.abs(gap) ** 2)) - floor) / floor < 0.2
```

## Ground truth for free

The engine knows every drawn instance timing, and the SigMF sidecar emits
**one annotation per burst instance at the exact rendered position** — so a
detector can be scored against the capture without ever walking it:

```python
import json

meta = json.loads(comp.to_sigmf(sample_type="cf32", fs=4e6))
starts = [int(a["core:sample_start"]) for a in meta["annotations"]]
assert len(starts) == 5                     # one per instance, in order
```

## Decode it back

The same codes and sync word seed the receiver; every burst comes back
CRC-valid with the exact payload — through the noisy gaps, using the
sidecar's ground-truth positions:

```python
from doppler.dsss import BurstDemod

burst_len = (128 * 4 + (13 + 200 + 16) * 25) * 4   # n_chips * sps
decoded = 0
for s in starts:
    bd = BurstDemod(dat, spc=4, chip_rate=1e6, payload_len=200)
    bd.set_preamble(acq, 4)
    bd.set_sync(BARKER13)
    bd.set_prior(0.0, 0)
    bits = bd.demod(x[s : s + burst_len])
    decoded += bool(bd.frame_valid and np.array_equal(bits, pay))
assert decoded == 5
```

(For a full walkthrough — acquisition search over the capture, false-alarm
rejection, all three wfmgen faces byte-compared — see the
[DSSS burst pipeline gallery](../../gallery/dsss-burst-pipeline.md).)

## The same burst on the other two faces

The JSON scene carries the same keys (codes as `"0/1"` strings, ranges as
pairs, `"repeats": 5`; `"gap_noise"`/`"delay_samples"` only when
non-default), and the CLI single-segment face is:

```sh
wfmgen --type dsss --fs 4e6 --sps 4 --seed 1 --snr 10 --snr-mode esno \
       --acq-code-hex <hexA> --acq-reps 4 --data-code-hex <hexB> \
       --sync 1111100110101 --bits-hex <payload-hex> \
       --delay 2000:10000 --off 4000:12000 --repeats 5 \
       --record train.json -o train.cf32
```

All three faces render byte-identically; `--record` emits the resolved scene
for a byte-exact `--from-file` replay, and `--file-type sigmf` writes the
annotated sidecar.

______________________________________________________________________

## Continuous asynchronous DSSS

A **burst** is a bounded frame with an integer number of chips per data
symbol (one bit spread by exactly one code period). Some links instead run a
**continuous** stream: the spreading code repeats forever and the data rides
it at a symbol rate that is *independent* of the code epoch — so the chips per
symbol are **non-integer** and symbol edges land mid-code:

```text
chip[i] = code[i mod code_len] ^ data[floor(i / chips_per_symbol)]
```

Adding `symbol_rate=` (Hz) to a `dsss` source selects this mode. There is no
preamble/sync/CRC frame — only the spreading code and the outer data clock:

```python
import numpy as np
from doppler.wfm import Composer, Segment

fs, spc, chip_rate = 6.138e6, 2, 3.069e6
code = np.array([1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1], np.uint8)

stream = Segment(
    type="dsss", fs=fs, sps=spc, seed=1,
    symbol_rate=2700.0,          # data clock, independent of the chip clock
    data_code=code,              # spreading code; repeats forever
    snr=10.0, snr_mode="esno",   # Es/N0 of the 2700 bps data symbol
    num_samples=40_000,
)
x = Composer([stream]).compose()

cps = (fs / spc) / 2700.0        # chips per symbol
assert abs(cps - 1136.667) < 1e-3        # non-integer: the asynchronicity
```

Here `sps` is samples per **chip** (as for a burst), so the chip rate is
`fs / sps` and `chips_per_symbol = (fs / sps) / symbol_rate` — `1136.667` at
these numbers, spanning `1137, 1137, 1136` chips in a repeating cycle with
boundaries falling inside a code period. Unlike a burst, the stream has no
intrinsic length, so `num_samples` (`--count`) is honoured verbatim.

### Three data sources

The data modulating the code has three legitimate origins, selected the same
way on every face:

| source             | selector                              | what it emits                                                                            |
| ------------------ | ------------------------------------- | ---------------------------------------------------------------------------------------- |
| **PRBS** (default) | —                                     | bits from the source's seeded PN — endless, and a receiver regenerates them to score BER |
| **code-only**      | `dsss_code_only=True` (`--data none`) | constant bit 0 → the pure spreading code, `+code` polarity, no data transitions          |
| **payload**        | `payload=` (`--bits*`)                | a caller bit pattern, cycled `mod len`                                                   |

The default **PRBS** is the useful one for a stream with no finite truth
array: the data is a pure function of `(pn_poly, seed, pn_length, lfsr)`, so a
scorer regenerates exactly the transmitted bits — chip `i` carries data symbol
`floor(i / chips_per_symbol)`, and that bit is the `floor(i/cps)`-th PN output.
On a clean capture the chip-centre sample signs match bit-for-bit:

```python
from doppler.wfm import PN, mls_poly

clean = Segment(
    type="dsss", fs=fs, sps=spc, seed=1, symbol_rate=2700.0,
    data_code=code, snr=100.0, snr_mode="fs", num_samples=40_000,
)
xc = Composer([clean]).compose()

L = 15                                    # default --pn-length (register size)
bits = PN(poly=mls_poly(L), seed=1, length=L).generate(64)
i = np.arange(len(xc) // spc)             # one entry per chip
sym = np.floor(i / cps).astype(int)
chip = code.astype(int)[i % len(code)] ^ bits[sym]     # transmitted chip bit
centres = i * spc + spc // 2
assert np.array_equal(np.sign(xc[centres].real).astype(int), 1 - 2 * chip)
```

(`--seed-advance all` makes the per-epoch signal seed `seed + epoch`, so a
long multi-epoch scorer must track the epoch; the safe scoring modes are the
default `none` and `noise`. Code-only and payload are seed-independent.)

### Es/N0 references the data symbol, correctly

`snr_mode="esno"`/`auto` targets the outer data symbol, which spans
`fs / symbol_rate` samples — **not** `sf * sps`. The referencing SSOT takes
`fs` so a non-integer symbol span is exact (a truncated "effective spreading
factor" would silently misplace the noise floor). Nothing to set; it is
picked up from `symbol_rate`.

### The CLI face

```sh
wfmgen --type dsss --fs 6138000 --sps 2 --seed 1 \
       --symbol-rate 2700 --data-code 111001010110011 \
       --snr 10 --snr-mode esno --count 40000 \
       --record cont.json -o cont.cf32
```

`--data none` selects code-only; a supplied `--bits`/`--bits-hex` selects a
payload. Incompatible combinations are rejected (exit 2), not silently
ignored: `--symbol-rate` with the burst-frame flags (`--acq-code`, `--sync`,
`--crc`), `--data` together with `--bits*`, `--symbol-rate` without
`--data-code`, and a non-positive `--symbol-rate`.

### SigMF distinguishes the two modes

Both modes carry the `"dsss"` `core:label`, so the sidecar adds a
`wfmgen:symbol_rate` key for a continuous source (a burst omits it), and
`wfmgen:data: "none"` for a code-only stream — enough for a scorer to know the
outer symbol clock and how the code is (un)modulated:

```python
import json

meta = json.loads(Composer([stream]).to_sigmf(sample_type="cf32", fs=fs))
assert meta["annotations"][0]["wfmgen:symbol_rate"] == 2700
```

A continuous stream has no defined end, so `--file-type sigmf` (whose sidecar
is written after the capture) requires a finite `--count`, not `--continuous`.

______________________________________________________________________

## Sweeping a burst train with `Plan`

`Plan` supports the same `repeats` + ranged-`off_samples`/`delay_samples`
declaration used above — prepare the 5-burst scene once, then sweep Es/N0
(or redraw the inter-burst jitter via a Monte-Carlo `seed`) without
re-running the DSSS spreading/pulse-shaping per point:

```python
import numpy as np
from doppler.wfm import Composer, Segment, prepare

rng = np.random.default_rng(0)
acq = rng.integers(0, 2, 128, dtype=np.uint8)
dat = rng.integers(0, 2, 25, dtype=np.uint8)
pay = rng.integers(0, 2, 200, dtype=np.uint8)
BARKER13 = np.array([1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1], np.uint8)

burst = Segment(
    type="dsss", fs=4e6, sps=4, seed=1,
    snr=10.0, snr_mode="esno",
    acq_code=acq, acq_reps=4,
    data_code=dat,
    sync=BARKER13, payload=pay,
    delay_samples=(2_000, 10_000),
    off_samples=(4_000, 12_000),
    repeats=5,
)
scene = Composer([burst])

plan = prepare(scene)                    # spreading/pulse-shaping ONCE
np.array_equal(plan.render(), scene.compose())   # bit-identical baseline
# each point below is a cheap re-weighted sum + a regenerated noise synth,
# not a re-synthesis -- both the Es/N0 AND the inter-burst jitter move
for esn0_db in (4.0, 7.0, 10.0, 13.0):
    for mc_seed in (1000, 1001, 1002):
        draw = plan.render(snr=esn0_db, seed=mc_seed)
        # feed `draw` to Acquisition / BurstDespreader / BurstDemod per
        # the pipeline walkthrough above for a Pd/Pfa-vs-Es/N0 curve
len(plan)   # worst-case capacity (every ranged gap at its `hi` bound)
```

A lone bundled noisy source (like this DSSS burst — one source carrying its
own `snr`) is supported: its AWGN is reconstructed via a per-instance noise
synth rather than an external multiply, matching a full compose bit-for-bit
at every Es/N0 and seed.

### Remaining `Plan` restriction

Ranged **per-source** `freq`/`snr`/`level`/`f_end` stay out of `Plan`'s
scope — redrawing a source's frequency or SNR would invalidate its cached
render, defeating the "expensive DSP once" guarantee `Plan` exists to
provide. A ranged on-time (`num_samples`) is out of scope for the same
reason (it would invalidate the fixed-length signal cache). Both still
raise `ValueError` at `prepare()`; everything else on this page — ranged
gaps, ranged delay, `repeats`, a bundled noisy source — is fully supported.
