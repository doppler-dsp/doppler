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

## Known limitations

- **`Plan` can't sweep a burst timeline.** The prepare-once stimulus cache
    is single-segment/non-ranged in v1 — a Pd-vs-Es/N0 sweep over a burst
    train re-composes per point. Tracked in
    [#410](https://github.com/doppler-dsp/doppler/issues/410).
