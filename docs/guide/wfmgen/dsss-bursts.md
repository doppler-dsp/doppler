# DSSS bursts — a burst train in one declaration

The canonical spread-spectrum test capture — *N bursts at a specific
data-symbol Es/N0, each a repeated PN preamble followed by a payload spread
with a second code, randomly placed with a minimum gap* — is one `Segment`:

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
    off_samples=(4_000, 12_000),          # jittered gap, min 4k samples
    repeats=5,                            # -> a 5-burst train
)
x = Composer([burst]).compose()
```

Everything below unpacks what that declaration means and how to verify the
capture decodes.

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
- The **frame** is `sync | payload | CRC-16` (CCITT, over the payload bits),
    each bit XOR-spread by the full `data_code`. The sync word is optional;
    `crc="none"` drops the trailer.
- Codes are plain 0/1 arrays of **any length** — no `2^n - 1` restriction —
    so the geometry matches whatever the receiver expects.
- `sps` is samples per **chip**; the burst's span is intrinsic
    (`n_chips x sps` samples), so `num_samples` is derived and `--record`
    always carries the real length.

## Es/N0 that means what the receiver measures

`snr_mode="esno"` (and `auto`) targets the Es/N0 of the outer **data
symbol** — `len(data_code)` chips × `sps` samples — which is the number a
despreader's data-aided estimator recovers. The
`10*log10(sf*sps)` spreading conversion happens internally; see
[Levels & SNR](levels.md#snr-noise) for the chips-vs-symbols trap this
fixes. The payload is BPSK, so `ebno` and `esno` coincide.

## Random placement, deterministically

`repeats=5` plays the segment five times back-to-back; each **instance** is
one burst plus its trailing gap (see [Scenes](scenes.md#burst-trains-repeats)
for the exact semantics):

- the ranged `off_samples=(lo, hi)` gap **re-draws per instance** — `lo` is
    the guaranteed minimum gap;
- the AWGN is **fresh per instance** (bursts never share a noise
    realization) while the signal — codes, payload, code phase — is fixed;
- every draw is a deterministic hash, so `to_json()`/`--record` stores the
    *range* and a replay is byte-identical.

```python
# The whole train replays byte-for-byte from its recorded spec:
assert np.array_equal(
    Composer.from_json(Composer([burst]).to_json()).compose(), x
)
```

## Decode it back

The same codes and sync word seed the receiver; every burst comes back
CRC-valid with the exact payload:

```python
from doppler.dsss import BurstDemod

burst_len = (128 * 4 + (13 + 200 + 16) * 25) * 4   # n_chips * sps
pos, decoded = 0, 0
for _ in range(5):
    bd = BurstDemod(dat, spc=4, chip_rate=1e6, payload_len=200)
    bd.set_preamble(acq, 4)
    bd.set_sync(BARKER13)
    bd.set_prior(0.0, 0)
    bits = bd.demod(x[pos : pos + burst_len])
    decoded += bool(bd.frame_valid and np.array_equal(bits, pay))
    pos += burst_len                       # skip the zero gap to the next burst
    nz = np.flatnonzero(x[pos:] != 0)
    pos += int(nz[0]) if len(nz) else 0
assert decoded == 5
```

(For a full walkthrough — acquisition search over the capture, false-alarm
rejection, all three wfmgen faces byte-compared — see the
[DSSS burst pipeline gallery](../../gallery/dsss-burst-pipeline.md).)

## The same burst on the other two faces

The JSON scene carries the same keys (codes as `"0/1"` strings, gap range as
a pair, `"repeats": 5`), and the CLI single-segment face is:

```sh
wfmgen --type dsss --fs 4e6 --sps 4 --seed 1 --snr 10 --snr-mode esno \
       --acq-code-hex <hexA> --acq-reps 4 --data-code-hex <hexB> \
       --sync 1111100110101 --bits-hex <payload-hex> \
       --off 4000:12000 --repeats 5 --record train.json -o train.cf32
```

All three faces render byte-identically; `--record` emits the resolved scene
for a byte-exact `--from-file` replay.

## Known limitations

- **Gaps are hard zeros.** Per-segment noise is bundled inside the on-time,
    so the inter-burst region is digital silence — unrealistic for tuning
    acquisition/CFAR thresholds. A timeline-level noise floor is designed in
    [#409](https://github.com/doppler-dsp/doppler/issues/409); until then,
    add a full-length `noise` source via a multi-source `sum()` if the gap
    statistics matter.
- **`Plan` can't sweep a burst timeline.** The prepare-once stimulus cache
    is single-segment/non-ranged in v1 — a Pd-vs-Es/N0 sweep over a burst
    train re-composes per point. Tracked in
    [#410](https://github.com/doppler-dsp/doppler/issues/410).
