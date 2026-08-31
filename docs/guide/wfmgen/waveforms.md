# Waveforms — what you can generate

Every `--type` the engine offers, the knobs that shape it, how loud it
comes out, and the three things you can wrap around it: a frame, a
channel code, and a moving clock.

For putting waveforms in time — scenes, sweeps, streaming — see
[Scenes](scenes.md). The object model is on the
[guide index](index.md#the-ladder-the-whole-mental-model).

______________________________________________________________________

`--type` selects the waveform; every type shares the same parameter set. The
engine produces nine types from one declarative core.

| `--type`  | What it is                                                     | Key parameters                      |
| --------- | -------------------------------------------------------------- | ----------------------------------- |
| `tone`    | a complex sinusoid at `--freq`                                 | `--freq`                            |
| `noise`   | complex AWGN (unit power)                                      | `--snr` (ignored — it *is* noise)   |
| `pn`      | a maximum-length sequence (±1 chips), `--sps` samples/chip     | `--pn-length`, `--pn-poly`, `--sps` |
| `bpsk`    | BPSK symbols (PN-sourced data), `--sps` samples/symbol         | `--sps`, `--snr`                    |
| `qpsk`    | Gray-coded QPSK symbols (PN-sourced data)                      | `--sps`, `--snr`                    |
| `chirp`   | linear-FM sweep `--freq` → `--f-end` over `--count`            | `--freq`, `--f-end`                 |
| `bits`    | a user bit pattern, oversampled `--sps` and cycled             | `--bits`, `--modulation`, `--sps`   |
| `symbols` | **your** complex constellation, oversampled `--sps` and cycled | `--symbols-file`, `--sps`           |

The data bits for `bpsk`/`qpsk` come from a deterministic PN sequence (seeded by
`--seed`), so output is reproducible and receiver-correlatable.

```sh
wfmgen --type tone --freq 1e5 --fs 1e6 --count 4096 -o tone.cf32
wfmgen --type qpsk --sps 8 --snr 12 --count 100000 -o qpsk.cf32
wfmgen --type chirp --freq 100e3 --f-end 300e3 --fs 1e6 --count 10000 -o chirp.cf32
```

______________________________________________________________________

## Chirp — linear-FM sweep

A `chirp` sweeps its instantaneous frequency linearly from `--freq` (the start)
to `--f-end` over the `--count` samples, then holds at `--f-end`;
`--f-end < --freq` is a down-chirp. The phase is continuous across segments, so
concatenated chirps join seamlessly — radar pulse compression, SAR, sonar, and
frequency-response test signals all fall out of this one type. The sweep **span
is the length you ask for**: standalone it sweeps over `--count`; in a `Segment`
it fills `num_samples` — so `--f-end` is reached at the last sample either way.

!!! warning "A standalone chirp's span is fixed by the first read"

    The slope needs a span, and a chirp that nothing pins takes it from the
    first `steps()` call. That is right for the CLI and for a `Segment`, which
    both pin the length up front — but a Python caller reading the raw
    `Synth`/`_SynthEngine` a sample or a block at a time does not: `step()`
    never pins at all and emits a constant tone at `--freq`, and reading in
    64-sample blocks sweeps to `--f-end` within the first block and then holds.
    Render a chirp in one `steps(n)` call, or drive it through
    `Segment`/`Composer`. Tracked as
    [#1115](https://github.com/doppler-dsp/doppler/issues/1115); measured in
    [Synth's validation report](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/wfm/tests/validation/wfm_synth/results.md).

______________________________________________________________________

## Bits — your bit pattern, mapped

A `bits` waveform plays back **your** sequence — a preamble, sync word, or test
vector — given as a 0/1 string (`--bits 10110101`), a hex string
(`--bits-hex AA55`, MSB first), or a **binary** file whose bytes are
the bits, MSB first per byte (`--bits-file frame.bin`) — which is how a
real transfer frame reaches the tool.
`--modulation` (`none` / `bpsk` / `qpsk`) maps the bits to symbols (`none` →
0/1 amplitude, `bpsk` → ±1, `qpsk` → two bits per symbol, Gray-coded). Each bit
is held `--sps` samples and the pattern **cycles** to fill the requested length.

```sh
wfmgen --type bits --bits 10110101 --modulation bpsk --sps 8 --count 64 -o sync.cf32
wfmgen --type bits --bits-hex AA55 --modulation none --sps 4 -o preamble.cf32
```

______________________________________________________________________

## Symbols — bring your own constellation

Where `bits` maps data through a *fixed* modulation, `symbols` skips the map
entirely: `--symbols-file iq.cf32` supplies a raw interleaved-I/Q complex64
constellation stream and **each element becomes an output point directly**,
oversampled by `--sps` and cycled. That expresses any modulation an enum doesn't
— pi/4-QPSK, QAM, APSK — since you compute the constellation yourself and pass
it. `symbols` honours `--pulse rrc` like the built-in modulations.

<!-- docs-snippet: no-exec=qam16.cf32 is written by the Python fence above -->

```sh
wfmgen --type symbols --symbols-file qam16.cf32 --sps 8 --pulse rrc -o qam.cf32
```

In Python the constellation is the `symbols=` keyword on the composer `Synth`
(`Synth(type="symbols", symbols=iq, sps=8)`); on the low-level `_SynthEngine`
it is attached with `set_symbols()` after construction. See the
[Python API](python.md) page for a worked pi/4-QPSK example, and the
[Symbols gallery walkthrough](../../gallery/symbols.md) for pi/4-QPSK and
16-QAM constellations with rect vs RRC pulses.

______________________________________________________________________

## Framing — preamble, sync word, CRC

`--acq-code` / `--sync` / `--crc` describe a frame's **bit layout**:

```text
[ preamble x acq-reps | sync | payload | CRC-16 ]
```

Setting `--acq-code` **or** `--sync` is what makes a source framed. `--crc` on
its own does not — it defaults to `crc16`, so treating it as intent would put a
trailer on every plain bit pattern ever generated.

### A sequence given as numbers, not as a string

A preamble, spreading code or sync word can be **generated** instead of typed
out. `--acq-code-gen`, `--data-code-gen` and `--sync-gen` each take
`KIND:LEN[:...]`, colon-separated like `--freq`'s `LO:HI`:

| spec                                            | means                                                                                       |
| ----------------------------------------------- | ------------------------------------------------------------------------------------------- |
| `pn:LEN:REG_BITS[:SEED[:POLY]]`                 | one LFSR. `SEED` 0 selects 1; `POLY` 0 selects the maximal-length polynomial for `REG_BITS` |
| `gold:LEN:REG_BITS:TAPS_A:SEED_A:TAPS_B:SEED_B` | a Gold pair                                                                                 |
| `dotted:LEN`                                    | alternating `1010…`, a line at Rs/2 to settle on                                            |

```sh
wfmgen --type bits --bits 10110010 --sync-gen pn:1023:10 \
       --sps 4 --count 8192 --record run.json -o framed.cf32

# A DSSS burst with BOTH of its codes generated: a 127-chip preamble a
# receiver correlates against, and a 31-chip code spreading the payload.
wfmgen --type dsss --acq-code-gen pn:127:7 --acq-reps 4 \
       --data-code-gen pn:31:5 --sync 1111100110101 --bits-hex a5c3 \
       --sps 2 --snr 8 --snr-mode esno -o burst.cf32
```

Every number takes hex or decimal, so a tap mask reads as `0x409` the way it
does in the literature.

**Why this exists.** `--sync 1111100110101` is fine for a Barker-13. A
1023-chip sync word is not, and a `--record` of one is a 1023-character
string that says nothing about what produced it. The generated form records
six numbers instead, so a capture is reproducible from its metadata — which
is the point of the kinds, not a shorthand for them:

```json
"sync_gen": { "kind": "pn", "len": 1023, "reg_bits": 10,
              "poly": "0x0", "seed": "0x0", "lfsr": 0 }
```

A field is one or the other. `--sync` and `--sync-gen` together is refused
rather than resolved, because there is no correct answer to which one wins —
in either order, and counting `--acq-code-hex` as the same field as
`--acq-code`. Accepting the pair would write a `--record` carrying both
spellings, which this tool's own `--from-file` then refuses to read.

### The payload, generated or just bounded

The payload is a sequence like the other three, so `--payload-gen` takes the
same `KIND:LEN[:...]` spec:

```sh
wfmgen --type bits --modulation bpsk --payload-gen pn:65535:16 \
       --sync 1111100110101 --sps 4 --count 65536 -o framed.cf32
```

`--payload-len N` is the shorthand for the common case. It bounds the payload
at `N` bits and fills it from the waveform's **own** PN parameters
(`--pn-length`, `--pn-poly`, `--seed`), so the bits a receiver regenerates are
the ones this waveform would have transmitted anyway:

```sh
wfmgen --type bpsk --sync 1111100110101 --payload-len 1024 --pn-length 10 \
       --sps 4 --count 16384 -o framed_bpsk.cf32
```

**That command used to exit 2.** A frame on `--type bpsk`/`qpsk`/`pn` was
refused outright, because those waveforms take their data from the synth's
own endless LFSR and nothing said where the payload stopped. A length is
exactly that bound, so a framed PN-sourced waveform is now the same
descriptor every other source builds — `--conv`, `--rs-depth`, `--randomise`
and the rest all reach it. The frame's bits take the mapping the **type**
names, so a framed `--type qpsk` is Gray-coded QPSK over the frame and there
is no `--modulation` to disagree with it.

A type that carries no bit stream at all — `tone`, `noise`, `chirp`,
`symbols` — still cannot be framed, with or without a payload.

`--payload-len`, `--payload-gen` and `--bits` are three spellings of one
field, so giving two is refused the same way the other sequences are.

For `--type bits` the payload is `--bits*` and `--modulation` maps it to BPSK or
QPSK, so a framed unspread waveform is one command:

```sh
wfmgen --type bits --modulation bpsk --bits 10110010 \
       --acq-code 10101010 --acq-reps 4 --sync 1111100110101 --crc crc16 \
       --sps 4 --count 8192 -o framed.cf32
```

The frame then **cycles** to fill `--count`, exactly as a plain pattern does —
so one description yields a multi-frame record and the repeat count stays out
of the frame. (A `dsss` burst is the exception: its length is intrinsic and
`--count` is derived.)

A frame needs a payload, so the types whose symbols come from the PN LFSR —
`bpsk`, `qpsk`, `pn` — **refuse** these flags and name the replacement rather
than ignoring them. Until [gh-755](https://github.com/doppler-dsp/doppler/issues/755)
the whole unspread path ignored them silently, producing an unframed waveform
at exit 0; that is why the refusal is loud.

The same keywords work on the Python `Synth` and `Segment`
(`sync=`, `acq_code=`, `acq_reps=`, `crc=`), and `--record` carries them, so
`--from-file` rebuilds the framed waveform byte for byte.

The layout is one C descriptor (`native/inc/wfm/wfm_frame.h`) read by the
generator that builds it and the measurer that scores it — see
[Receiver Test Harness](../../design/rx-test.md) §7.

______________________________________________________________________

## DSSS — two-code spread-spectrum bursts

A `dsss` waveform is a complete **direct-sequence spread-spectrum burst** —
an unmodulated repeated preamble (`acq_code` × `acq_reps`) followed by the
frame `sync | payload | CRC-16`, every frame bit spread by a second, distinct
`data_code` — the transmit side of the
[`BurstDemod`](../../api/python-dsss.md) frame contract. `sps` is samples per
*chip*, `esno` refers to the outer *data* symbol, the burst length is
intrinsic, and `repeats`/`delay_samples`/`off_samples` turn one declaration
into a randomly-placed burst train over a continuous noise floor.

**[DSSS bursts](waveforms.md#dsss-bursts) is the full reference** — anatomy, Es/N0
semantics, placement, ground-truth SigMF annotations, and a decode-it-back
walkthrough. CLI flags: `--acq-code[-hex]`, `--acq-reps`,
`--data-code[-hex]`, `--sync`, `--crc none|crc16`, payload via `--bits*`.

______________________________________________________________________

## PN sequences & MLS

`--type pn` emits a maximum-length sequence; `--pn-length n` sets the LFSR
register length (**2 to 64**, period `2ⁿ−1`). The register, polynomial, and
`--pn-poly` are full 64-bit. Leave **`--pn-poly 0`** and the engine selects a
primitive polynomial that yields a true MLS for that length (a built-in table of
verified primitive polynomials for every length 2..64) — verified by period,
balance, and the thumbtack autocorrelation. Supply `--pn-poly` only to force a
specific tap set.

```sh
wfmgen --type pn --pn-length 7   --sps 1 --count 127   # one full period (2⁷−1)
wfmgen --type pn --pn-length 11  --sps 4               # length-11 MLS, 4× oversampled
wfmgen --type pn --pn-length 7   --lfsr fibonacci      # Fibonacci realization
# Force a specific tap set instead of the auto-selected one. 0x60 is the
# table's own entry for length 7 (x⁷+x⁶+1), so this is byte-identical to
# --pn-poly 0 above -- which is how you check a polynomial you were handed.
wfmgen --type pn --pn-length 7 --pn-poly 0x60 --sps 1 --count 127 -o pn7.cf32
```

`--lfsr` selects the LFSR realization: **`galois`** (default, internal XOR
feedback) or **`fibonacci`** (external XOR of the tapped bits). Both use the same
primitive polynomial and have the same period `2ⁿ−1`; they differ only in the
chip sequence/phase. The Fibonacci taps are derived from the same polynomial, so
`--pn-poly 0` still auto-selects the MLS for either mode.

______________________________________________________________________

## RRC pulse shaping — band-limited carriers

The modulated types (`pn` / `bpsk` / `qpsk` / `bits` / `symbols`) default to
**rectangular sample-and-hold** chips — a wide `sinc²` spectrum. Set
`--pulse rrc` for **root-raised-cosine** shaping: the symbol stream is filtered
to a band-limited channel, so a realistic carrier (e.g. a WCDMA QPSK downlink at
roll-off 0.22) comes straight from the generator. `--rrc-beta` is the roll-off
and `--rrc-span` the filter support in symbols. The taps are unit-transmit-power
scaled, so the output stays at unit average power.

```sh
wfmgen --type qpsk --sps 8 --pulse rrc --rrc-beta 0.22 --count 100000 -o wcdma.cf32
# A longer span truncates the RRC tails less, so the stopband is deeper --
# at the cost of `--rrc-span * --sps` more samples of filter delay.
wfmgen --type qpsk --sps 8 --pulse rrc --rrc-beta 0.22 --rrc-span 16 \
       --count 100000 -o wcdma-span16.cf32
```

______________________________________________________________________

## Engine parameter reference

| Flag              | Type                                              | Default  | Meaning                                                                                          |
| ----------------- | ------------------------------------------------- | -------- | ------------------------------------------------------------------------------------------------ |
| `--type`          | `tone noise pn bpsk qpsk chirp bits symbols dsss` | `tone`   | waveform                                                                                         |
| `--fs`            | float (Hz)                                        | `1.0`    | sample rate (default `1.0` ⇒ `--freq`/`--f-end` are normalised, cycles/sample)                   |
| `--freq`          | float (Hz)                                        | `0`      | frequency offset from baseband (mixed by the LO); chirp start                                    |
| `--f-end`         | float (Hz)                                        | `0`      | chirp end frequency (`--type chirp` only)                                                        |
| `--snr`           | float (dB)                                        | `100`    | SNR; metric chosen by `--snr-mode` (≈clean at 100) — see [Levels & SNR](waveforms.md#levels-snr) |
| `--snr-mode`      | `auto fs ebno esno`                               | `auto`   | how `--snr` is interpreted                                                                       |
| `--seed`          | uint32                                            | `0`      | PRNG / LFSR seed (deterministic)                                                                 |
| `--sps`           | int                                               | `1`      | samples per symbol (`*psk`/`bits`/`symbols`) / per chip (`pn`)                                   |
| `--pn-length`     | int (2..64)                                       | `15`     | LFSR register length → period `2ⁿ−1`                                                             |
| `--pn-poly`       | uint64                                            | `0`      | LFSR polynomial; `0` ⇒ auto-pick the MLS polynomial                                              |
| `--lfsr`          | `galois fibonacci`                                | `galois` | LFSR realization (same polynomial/period, different sequence)                                    |
| `--bits`          | 0/1 string                                        | —        | `bits`: pattern, e.g. `10110101` (or `--bits-hex`/`--bits-file`)                                 |
| `--modulation`    | `none bpsk qpsk`                                  | `bpsk`   | `bits`: how the pattern maps to symbols                                                          |
| `--symbols-file`  | path (cf32)                                       | —        | `symbols`: raw interleaved-I/Q complex64 constellation stream                                    |
| `--acq-code`      | 0/1 string                                        | —        | `dsss`: preamble code (or `--acq-code-hex`)                                                      |
| `--acq-code-gen`  | `KIND:LEN[:...]`                                  | —        | preamble as a generated sequence (`pn`/`gold`/`dotted`)                                          |
| `--data-code-gen` | `KIND:LEN[:...]`                                  | —        | spreading code as a generated sequence                                                           |
| `--sync-gen`      | `KIND:LEN[:...]`                                  | —        | sync word as a generated sequence                                                                |
| `--payload-gen`   | `KIND:LEN[:...]`                                  | —        | payload as a generated sequence                                                                  |
| `--payload-len`   | int                                               | —        | payload bounded at N bits, filled from this source's own PN                                      |
| `--acq-reps`      | int                                               | `1`      | `dsss`: preamble repetitions                                                                     |
| `--data-code`     | 0/1 string                                        | —        | `dsss`: payload spreading code (or `--data-code-hex`)                                            |
| `--sync`          | 0/1 string                                        | —        | `dsss`: frame-sync word (optional)                                                               |
| `--crc`           | `none crc16`                                      | `crc16`  | `dsss`: CRC-16 trailer over the payload bits                                                     |
| `--pulse`         | `rect rrc`                                        | `rect`   | pulse shape; `rrc` = band-limited RRC shaping                                                    |
| `--rrc-beta`      | float                                             | `0.35`   | RRC roll-off (`--pulse rrc`)                                                                     |
| `--rrc-span`      | int                                               | `8`      | RRC filter support in symbols (`--pulse rrc`)                                                    |
| `--count`         | int                                               | `1024`   | number of complex samples to generate                                                            |

______________________________________________________________________

## Levels & SNR

### Amplitude & full-scale

The amplitude invariant is **unit average power**: every waveform is normalised
so its mean power is `1.0`. That — *not* a constant envelope — is what the rest
of the system is built on. It is the reference the SNR math uses (signal power
≡ 1, so the noise σ falls straight out of the target SNR), and the level you
control is the SNR, not a signal gain. The I/Q full-scale is **±1.0** per axis
(→ the largest integer code).

Today's built-in types all *happen* to be **constant-envelope**, so for them the
peak equals the average and they sit exactly at ±1.0 — but that is a property of
the current set, **not** a design assumption:

| `--type`      | Sample values                         | Envelope           | Avg. power   |
| ------------- | ------------------------------------- | ------------------ | ------------ |
| `tone`        | `exp(j·2πft)`                         | constant, mag 1    | `1.0`        |
| `bpsk` / `pn` | `±1` (real axis)                      | constant, mag 1    | `1.0`        |
| `qpsk`        | `(±1/√2, ±1/√2)`                      | constant, mag 1    | `1.0`        |
| `chirp`       | `exp(j·φ(t))`, φ′ ramps `freq→f_end`  | constant, mag 1    | `1.0`        |
| `noise`       | complex Gaussian, `σ = 1/√2` per axis | Gaussian, PAPR > 0 | `1.0`        |
| `symbols`     | whatever you supply (e.g. 16-QAM)     | user-defined       | user-defined |

**Don't rely on `|z| = 1`.** A pulse-shaped (RRC), QAM, or OFDM waveform has a
**peak-to-average power ratio (PAPR) above 0 dB**: at unit *average* power its
*peaks* run well past ±1.0. `noise`, and any signal-plus-noise sum, already do.

______________________________________________________________________

### Scaling to the wire, and headroom

`cf32` / `cf64` carry samples verbatim and **never clip** — peaks past ±1.0 are
preserved. The integer types map **±1.0 → ±max-code** by **saturating each axis
to ±1.0, then truncating toward zero** (a plain cast, not round-to-nearest):

| `--sample-type` | Map                   | Full-scale code  |
| --------------- | --------------------- | ---------------- |
| `ci32`          | `clip(v, ±1)·(2³¹−1)` | `±2 147 483 647` |
| `ci16`          | `clip(v, ±1)·32767`   | `±32 767`        |
| `ci8`           | `clip(v, ±1)·127`     | `±127`           |

So clipping is governed by **PAPR**, not by something being "signal" vs "noise":

- A **constant-envelope, clean** signal (a tone/PSK/PN at `--snr 100`) fills the
    integer range exactly, with no clipping.
- **Any PAPR > 0 dB content clips** at the rails — added noise (at `--snr 0`,
    noise power = signal power, ~⅓ of integer I/Q components already saturate)
    and any pulse-shaped / QAM / OFDM mode. Such a signal needs **headroom**:
    **`--headroom <dB>`** (and `Writer(headroom=…)` in Python) scales the whole
    output down to `−H` dBFS so the peaks fit. It is a single common gain, so it
    is **SNR-invariant** — it moves only the absolute level, not any power ratio
    — and `0` dB (the default) is a bit-exact no-op. An integer capture that
    clips reports the exact backoff to use (`remedy: --headroom N`). You can also
    just carry envelope-varying signals as a **float** type (`cf32` / `cf64`),
    which never clips.

`Reader` (see [Output & file types](../wfm-io/writing.md)) inverts the same map, so a float
round-trip is exact and an integer round-trip is exact only where it neither
clipped nor truncated.

**Clipping is observable, not silent.** Two flags turn "the capture looks
wrong" into a number, and they are what to reach for before anything else:
**`--clip-report`** prints the clipped fraction and the peak to stderr, and
**`--clip-error`** makes clipping an *exit status* — which is what you want in
a script that generates a capture and must not hand on a broken one.

```sh
# What does this shaped, noisy QPSK actually do to a 16-bit integer capture?
wfmgen --type qpsk --sps 8 --pulse rrc --snr 6 --count 20000 \
       --sample-type ci16 --clip-report -o shaped.ci16
#   wfmgen: warning: ci16 output clipped — peak is +13.4 dB over full scale.
#     remedy: --headroom 14, or --sample-type cf32.
#     clipped 42.41% of I/Q components

# Refuse to write a clipped capture at all: this one EXITS NON-ZERO.
wfmgen --type qpsk --sps 8 --pulse rrc --snr 6 --count 20000 \
       --sample-type ci16 --clip-error -o shaped.ci16 || echo "clipped, as expected"

# Take the remedy the report named, and it passes.
wfmgen --type qpsk --sps 8 --pulse rrc --snr 6 --count 20000 \
       --sample-type ci16 --headroom 14 --clip-error -o clean.ci16

# --level places a source in dBFS. Backing a source off is how a scene puts
# one signal below another; --headroom scales the composite AFTER summing.
wfmgen --type tone --freq 1e5 --fs 1e6 --level -10 --count 4096 -o quiet.cf32
```

`--level` is per-source and `--headroom` is per-output: a level difference is
part of the scene (and survives into every sweep point a
[`Plan`](scenes.md#prepare-once-sweep-many-plan) renders), while
headroom is a single common gain applied on the way to the wire.

```python
>>> import numpy as np
>>> from doppler.wfm import Synth
>>> # the invariant is unit *average* power (here a clean, constant-envelope QPSK)
>>> x = Synth(type="qpsk", sps=1, snr=100.0).steps(4096)
>>> bool(np.allclose(np.mean(np.abs(x) ** 2), 1.0))
True
>>> # add noise (or pulse-shaping / QAM) and peaks exceed full-scale:
>>> y = Synth(type="qpsk", sps=1, snr=0.0).steps(100000)
>>> float(np.mean(np.abs(y.real) > 1.0)) > 0.1   # many samples clip in ci*
True

```

______________________________________________________________________

### SNR & noise

`--snr` is applied as AWGN; `--snr-mode` chooses the reference:

| Mode   | `--snr` means                                               | Use for              |
| ------ | ----------------------------------------------------------- | -------------------- |
| `fs`   | SNR over the full sample rate (in-band power / noise power) | tones, wideband      |
| `esno` | **Es/No** — energy per *symbol* over noise PSD              | modulated (`*psk`)   |
| `ebno` | **Eb/No** — energy per *bit* over noise PSD                 | link-budget work     |
| `auto` | `esno` for `bpsk`/`qpsk`/`dsss`, `fs` for the other six     | the sensible default |

`auto` splits the nine `--type`s exactly once, and this is the one place that
split is written out — the schema and the API page defer here rather than
carry their own copy:

| `auto` resolves to         | types                                                  |
| -------------------------- | ------------------------------------------------------ |
| **`esno`** (Es/N0)         | `bpsk` · `qpsk` · `dsss`                               |
| **`fs`** (over full scale) | `tone` · `noise` · `pn` · `chirp` · `bits` · `symbols` |

`bits` is on the `fs` side, which surprises people: a `bits` waveform *is*
modulated (`--modulation bpsk|qpsk`), but its symbol rate is whatever the
frame and `--sps` make it, so there is no symbol energy the engine can refer
to without being told. Ask for one explicitly with `--snr-mode esno` if that
is what you want. Verified by construction: `--type bits` renders
byte-identically under `--snr-mode auto` and `--snr-mode fs`, and differently
under `esno`.

**`--snr 100` (the default) is *clean*** — `snr ≥ 100 dB` generates **no AWGN at
all**, so a clean waveform pays no noise cost. Lower `--snr` to add noise; the
signal stays at unit average power, so the per-axis noise σ is
`σ = sqrt(1 / (2·10^(snr_fs/10)))`, where Es/No and Eb/No are first converted to
an over-`fs` SNR using `10·log10(sps)` (and, for Eb/No, the bits/symbol: 1 for
BPSK/PN, 2 for QPSK). (`--type noise` always generates AWGN.) Likewise
**`--freq 0` skips the LO** — the carrier is a constant 1 — so a clean baseband
waveform is pure signal generation.

!!! note "`dsss`: the symbol is the outer *data* symbol"

    For `type="dsss"` the Es/N0 reference is the outer data symbol
    (`len(data_code)` chips × `sps` samples) — what a despreader's
    data-aided estimator measures — not the chip a hand-spread `bits`
    pattern would get. Full semantics: [DSSS bursts](waveforms.md#dsss-bursts).

!!! example "Same QPSK at three references"

    ```sh
    wfmgen --type qpsk --snr 10 --snr-mode esno     # 10 dB Es/No (the auto default)
    wfmgen --type qpsk --snr 7  --snr-mode ebno     # 7 dB Eb/No  (= 10 dB Es/No)
    wfmgen --type qpsk --snr 1  --snr-mode fs        # 1 dB over fs (per-sample)
    ```

______________________________________________________________________

## DSSS bursts

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

The **frame** — `acq_code` / `acq_reps` / `sync` / `crc` — is not DSSS-specific:
the same four describe an unspread `type="bits"` waveform, and the layout comes
from one descriptor either way. See
[Framing](waveforms.md#framing-preamble-sync-word-crc).
What is DSSS's own is everything below: the second code, the chip clock, and
the Es/N0 that refers to the outer data symbol.

______________________________________________________________________

### The burst anatomy

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

### Es/N0 that means what the receiver measures

`snr_mode="esno"` (and `auto`) targets the Es/N0 of the outer **data
symbol** — `len(data_code)` chips × `sps` samples — which is the number a
despreader's data-aided estimator recovers. The `10*log10(sf*sps)`
spreading conversion happens internally. This matters because a hand-spread
`bits` pattern gets the *chip* as its symbol: the same `--snr 10` would
land `10*log10(sf)` dB apart between the two spellings. The payload is
BPSK, so `ebno` and `esno` coincide. (General SNR model:
[Levels & SNR](waveforms.md#snr-noise).)

### Random placement, deterministically

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

### Gaps carry the noise floor

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

On the CLI that switch is **`--gap-noise`**, and the difference is measurable
in the gap itself — the noise floor at 10 dB SNR, or exact zeros:

```sh
wfmgen --type bpsk --snr 10 --count 2048 --off 1024 -o gap-auto.cf32
wfmgen --type bpsk --snr 10 --count 2048 --off 1024 --gap-noise off -o gap-off.cf32
python3 -c "
import numpy as np
for f in ('gap-auto.cf32', 'gap-off.cf32'):
    g = np.fromfile(f, dtype=np.complex64)[2048:]   # the trailing gap only
    print(f, 'gap power', round(float(np.mean(np.abs(g) ** 2)), 4))"
#   gap-auto.cf32 gap power 0.1036
#   gap-off.cf32 gap power 0.0
```

Use `off` when a downstream tool defines a burst by "where the samples are
non-zero"; leave it on `auto` for anything that has to pick the burst out of
a channel, which is the case the default is built for.

### Ground truth for free

The engine knows every drawn instance timing, and the SigMF sidecar emits
**one annotation per burst instance at the exact rendered position** — so a
detector can be scored against the capture without ever walking it:

```python
import json

meta = json.loads(comp.to_sigmf(sample_type="cf32", fs=4e6))
starts = [int(a["core:sample_start"]) for a in meta["annotations"]]
assert len(starts) == 5                     # one per instance, in order
```

### Decode it back

The same codes and sync word seed the receiver; every burst comes back
CRC-valid with the exact payload — through the noisy gaps, using the
sidecar's ground-truth positions:

```python
from doppler.wfm import crc16
from doppler.dsss import BurstDemod

burst_len = (128 * 4 + (13 + 200 + 16) * 25) * 4   # n_chips * sps
decoded = 0
# The demodulator returns the FRAME and no verdict — it stops at
# decisions — so the payload is a slice and the CRC is checked here.
frame_syms = 13 + 200 + 16
for s in starts:
    bd = BurstDemod(dat, spc=4, chip_rate=1e6, frame_syms=frame_syms)
    bd.set_preamble(acq, 4)
    bd.set_sync(BARKER13)
    bd.set_prior(0.0, 0)
    frame = bd.demod(x[s : s + burst_len])
    got = frame[13:13 + 200]
    rx_crc = 0
    for b in frame[13 + 200:][:16]:
        rx_crc = (rx_crc << 1) | int(b)
    decoded += bool(rx_crc == int(crc16(got)) and np.array_equal(got, pay))
assert decoded == 5
```

(For a full walkthrough — acquisition search over the capture, false-alarm
rejection, all three wfmgen faces byte-compared — see the
[DSSS burst pipeline gallery](../../gallery/dsss-burst-pipeline.md).)

### The same burst on the other two faces

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

### Continuous asynchronous DSSS

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

#### Three data sources

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

#### Es/N0 references the data symbol, correctly

`snr_mode="esno"`/`auto` targets the outer data symbol, which spans
`fs / symbol_rate` samples — **not** `sf * sps`. The referencing SSOT takes
`fs` so a non-integer symbol span is exact (a truncated "effective spreading
factor" would silently misplace the noise floor). Nothing to set; it is
picked up from `symbol_rate`.

#### The CLI face

```sh
wfmgen --type dsss --fs 6138000 --sps 2 --seed 1 \
       --symbol-rate 2700 --data-code 111001010110011 \
       --snr 10 --snr-mode esno --count 40000 \
       --record cont.json -o cont.cf32

# The same stream with the data switched OFF -- the pure repeating code,
# which is what you correlate against when bringing a receiver up.
wfmgen --type dsss --fs 6138000 --sps 2 --seed 1 \
       --symbol-rate 2700 --data-code 111001010110011 --data none \
       --snr 10 --snr-mode esno --count 40000 -o code-only.cf32
```

`--data none` selects code-only; a supplied `--bits`/`--bits-hex` selects a
payload. Incompatible combinations are rejected (exit 2), not silently
ignored: `--symbol-rate` with the burst-frame flags (`--acq-code`, `--sync`,
`--crc`), `--data` together with `--bits*`, `--symbol-rate` without
`--data-code`, and a non-positive `--symbol-rate`.

#### SigMF distinguishes the two modes

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

### Sweeping a burst train with `Plan`

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

#### Remaining `Plan` restriction

Ranged **per-source** `freq`/`snr`/`level`/`f_end` stay out of `Plan`'s
scope — redrawing a source's frequency or SNR would invalidate its cached
render, defeating the "expensive DSP once" guarantee `Plan` exists to
provide. A ranged on-time (`num_samples`) is out of scope for the same
reason (it would invalidate the fixed-length signal cache). Both still
raise `ValueError` at `prepare()`; everything else on this page — ranged
gaps, ranged delay, `repeats`, a bundled noisy source — is fully supported.

______________________________________________________________________

## Channel coding

Six `wfmgen` flags turn a framed waveform into a **coded** one:

```text
--rs-depth   --randomise   --asm   --conv   --interleave   --interleave-unit
```

They are not a chain of filters applied to "the frame". Each is a **stage**
that carries the **span it covers**, and the spans deliberately disagree — a
marker, a preamble and a sync word are things a receiver *finds* by
correlation, so they must look the same in every frame and the stages that
scramble or code the data must reach past them. Setting any of them frames
the waveform, exactly as
[`--sync` does](waveforms.md#framing-preamble-sync-word-crc).

!!! warning "This is **block** interleaving, not I/Q interleaving"

    Everywhere else in these guides, *interleaved* describes the **sample**
    layout on disk — `raw` files store I, Q, I, Q, … and `--symbols-file`
    reads that layout back. That is a memory format and has nothing to do
    with this page.

    `--interleave` is a **block interleaver over bits**: it permutes the
    frame's data group before transmission so that a burst of channel errors
    arrives spread across the outer code's codewords instead of destroying
    one of them. It changes *where* errors land and never touches the sample
    layout. The two meanings share a word and no code.

______________________________________________________________________

### The six flags

| Flag                  | Value                         | What it adds                                                     |
| --------------------- | ----------------------------- | ---------------------------------------------------------------- |
| `--rs-depth I`        | `1 2 3 4 5 8`                 | Reed-Solomon (255,223), `E = 16`, interleaved `I` codewords deep |
| `--randomise [G]`     | `ccsds` (bare) `legacy` `off` | XOR a pseudo-random sequence over the data group                 |
| `--asm`               | —                             | Prepend the `0x1ACFFC1D` attached sync marker                    |
| `--conv`              | —                             | Convolutional `K = 7`, rate 1/2 — **doubles** the bit count      |
| `--interleave R`      | depth, in rows                | Block-interleave the data group `R` deep; length-preserving      |
| `--interleave-unit N` | bits (default `1`)            | Bits per permuted unit — use `8` with `--rs-depth`               |

All six apply to `--type bits` and `--type dsss`. Each is separately optional
because the standards make it so.

### Where each stage reaches

The frame is a list of fields and a list of stages, and every stage names the
fields it covers. In application order:

| #   | Stage         | Flag           | Covers                                                    |
| --- | ------------- | -------------- | --------------------------------------------------------- |
| 1   | CRC-16        | `--crc`        | the payload                                               |
| 2   | Reed-Solomon  | `--rs-depth`   | payload + CRC → appends check symbols                     |
| 3   | randomiser    | `--randomise`  | the **data group**: payload, CRC, check symbols           |
| 4   | interleaver   | `--interleave` | the data group — **last**, so it is what the channel sees |
| 5   | convolutional | `--conv`       | **every** field, marker and sync word included            |

The disagreement in the last column is the whole reason a frame is a
description rather than a pipeline: a chain gets three of these boundaries
right and the fourth wrong, in the direction that still decodes against a
receiver of your own construction and syncs to nothing. The standard's own
sections, and the measurement behind each cover, are in
[A CCSDS CADU, as a Frame Description](../../gallery/ccsds-link.md).

______________________________________________________________________

### The stages, one at a time

A framed waveform with no coding, and the same frame scrambled and permuted:

```sh
# [ sync | payload | CRC-16 ] -- no coding
wfmgen --type bits --bits-hex b25a --sync 10110 --count 512 -o plain.cf32

# the data group scrambled, then permuted 4 deep in octets
wfmgen --type bits --bits-hex b25a --sync 10110 \
       --randomise --interleave 4 --interleave-unit 8 \
       --count 512 -o coded.cf32

# identical sizes: neither stage adds a bit
ls -l plain.cf32 coded.cf32
```

#### `--rs-depth` — the outer code

Reed-Solomon (255,223) over `GF(256)`, correcting `E = 16` octets per
codeword, with `I` codewords interleaved by the codeblock layout itself.

The payload plus its CRC must be **exactly** `223 × I` octets. A short frame
is refused rather than padded — virtual fill is not implemented, and padding
would change a length both ends have to agree on. `--crc none` makes the
payload alone the `223 × I`.

Depth-`I` here is *intrinsic to the codeblock*, fused into encode and decode.
It is **not** `--interleave`, which is a permutation applied afterwards over
whatever span it is given — the two share a name and no implementation. See
[Interleaving, way 2](../../design/interleaving.md#2-it-is-not-the-outer-codes-own-depth).

#### `--randomise` — which generator, not whether

```sh
wfmgen --type bits --bits-hex b25a --sync 10110 --randomise \
       --count 512 -o r-default.cf32
wfmgen --type bits --bits-hex b25a --sync 10110 --randomise legacy \
       --count 512 -o r-legacy.cf32

# --randomize is the same flag, spelled the other way. Both spellings are
# accepted everywhere, and produce the same bytes:
wfmgen --type bits --bits-hex b25a --sync 10110 --randomize \
       --count 512 -o r-us.cf32
python3 -c "print(open('r-default.cf32','rb').read() == open('r-us.cf32','rb').read())"
```

A bare `--randomise` selects `ccsds`: the 131071-bit sequence from
`h(x) = x^17 + x^14 + 1`, which 131.0-B-6 §10.4.1 requires. `legacy` selects
§10.4.2's 255-bit `h(x) = x^8 + x^7 + x^5 + x^3 + 1`, kept for backward
compatibility only — it puts spectral lines at 1/255 of the symbol rate.
`off` is the same as omitting the flag.

The sequence is its own inverse, so the receiver runs the identical call. The
two generators are **not interchangeable on the air**: only the matching
receiver derandomises a given waveform, which is why `--record` stores *which*
one rather than a bare `true`.

`--randomize` is accepted as an alias, spelled the other way. CCSDS 131.0-B-6
says "randomizer", this guide says randomiser, and the parser declines to have
an opinion; the two are the same flag and either may be typed.

#### `--asm` — the marker

Prepends the 32-bit `0x1ACFFC1D` attached sync marker. It is a field, not a
transform: nothing scrambles or outer-codes it, because it is the pattern the
receiver correlates against to find the frame at all. The inner code does
cover it.

#### `--conv` — the inner code

Convolutional `K = 7`, rate 1/2, over the **whole** frame including the
marker, so the emitted bit count doubles:

```sh
wfmgen --type bits --bits-hex b25a --asm --count 512 -o uncoded.cf32
wfmgen --type bits --bits-hex b25a --asm --conv --count 512 -o inner.cf32
```

`--count` is samples, not frame bits, so both files are 512 samples — the
doubling shows up as **half as many frames** in the same span: the frame here
is 32 marker + 16 payload + 16 CRC = 64 bits, so the capture carries 8 frames
uncoded and 4 coded. The decoder,
the node synchronization it needs first, and what it can and cannot recover
are in [The FEC Receive Half](../../design/fec-receive.md).

#### `--interleave` and `--interleave-unit`

Write the data group by rows into an `R × C` matrix, read it back by columns.
`R` is `--interleave`; `C` follows from the span the stage covers, because
the stage's cover is what says how much there is to permute.

```sh
# 4 deep over a 32-bit data group: it divides, so 8 columns
wfmgen --type bits --bits-hex b25a --sync 10110 --interleave 4 \
       --count 512 -o ok.cf32

# 5 deep does not divide 32 -- refused, never padded
wfmgen --type bits --bits-hex b25a --sync 10110 --interleave 5 \
       --count 512 -o bad.cf32 || echo "refused, as documented"
```

With one codeword per row, reading by columns transmits one symbol from each
codeword in turn, so a burst of up to `R` consecutive units costs each
codeword at most one and an outer code correcting `t` per codeword survives a
burst of `t × R`. It multiplies the corrigible burst by the depth; it does
not remove the bound.

`--interleave-unit` is the size of a permuted unit in bits. Use **8** with
`--rs-depth`: RS is a code over `GF(256)`, so spreading a burst across
codewords means permuting octets — bit-interleaving an octet code spreads a
burst *inside* symbols that are already wrong. It is also an 8× throughput
parameter, one `memcpy` per eight bits rather than one per bit.

The transform, the three ways the reasoning goes wrong, and the measured
frame-error rates that put a number on all of it are in
[Interleaving — spreading a burst across codewords](../../design/interleaving.md).

______________________________________________________________________

### The canonical arrangement: a CCSDS CADU

Four of the flags — `--rs-depth`, `--randomise`, `--asm`, `--conv` — over a
`223 × I`-octet payload, with no preamble and no sync word, **is** a CADU.
That is a configuration of these flags, not a mode `wfmgen` switches into:

```sh
# a 223-octet Transfer Frame, as hex
python3 -c "print('5a'*223, end='')" > tf.hex

wfmgen --type bits --bits-hex "$(cat tf.hex)" --crc none \
       --rs-depth 1 --randomise --asm --conv \
       --modulation bpsk --sps 1 --count 4144 -o cadu.cf32
```

`--count 4144` is exactly one frame: 32 marker bits plus a 2040-bit data
group (`223 + 32` octets under RS(255,223)) is 2072, doubled by the inner
code, at one sample per symbol.

The gallery page renders what this repairs and what it refuses:
[A CCSDS CADU, as a Frame Description](../../gallery/ccsds-link.md).

#### Adding an interleaver on top

`--interleave` is **not** part of that chain. CCSDS gets its burst protection
from the codeblock depth `I` alone, which is why the standard specifies no
separate permutation. Adding one is a valid arrangement — just not a CCSDS
one — and it is where `--interleave-unit 8` earns its keep, because the code
it is protecting has octets for symbols:

```sh
wfmgen --type bits --bits-hex "$(cat tf.hex)" --crc none \
       --rs-depth 1 --randomise --asm \
       --interleave 5 --interleave-unit 8 --conv \
       --modulation bpsk --sps 1 --count 4144 -o cadu-ilv.cf32
```

Length-preserving, so the count is unchanged; the interleaver divides the
2040-bit data group into `2040 / (5 × 8) = 51` columns and the corrigible
burst goes from `E` = 16 octets to `E × 5` = 80.

______________________________________________________________________

### The coding survives `--record`

`--record` writes the fully-resolved run, coding stages included, and
`--from-file` on that record reproduces the samples byte for byte:

```sh
wfmgen --type bits --bits-hex b25a --sync 10110 \
       --randomise --interleave 4 --interleave-unit 8 \
       --count 512 --record run.json -o a.cf32
wfmgen --from-file run.json -o b.cf32
python3 -c "print(open('a.cf32','rb').read()==open('b.cf32','rb').read())"
cat run.json
```

The record carries `randomise` as the generator's **name**, and `interleave`
/ `interleave_unit` as the geometry — a replay that guessed either would
produce a different waveform of the same length, with nothing to notice it.
The scene schema (`docs/schema/wfmgen.schema.json`) sets
`additionalProperties: false`, so an unlisted key is a validation failure
rather than a silent pass.

______________________________________________________________________

### On `--type dsss`

The stages cover the **spread** frame and not the acquisition preamble: a
preamble is transmitted unmodulated, because it is what a receiver correlates
raw chips against.

```sh
wfmgen --type dsss --bits-hex b25a --acq-code 1101100 --acq-reps 4 \
       --data-code 10110 --sync 10110 \
       --randomise --interleave 4 --sps 2 -o dsss.cf32
```

Everything else `type="dsss"` means — the second code, the chip clock, the
Es/N0 that refers to the outer data symbol — is in
[DSSS bursts](waveforms.md#dsss-bursts).

______________________________________________________________________

## Clock Doppler — a source that is moving

A carrier offset moves the carrier. **Doppler moves the clock.**

That distinction is the whole page. `--freq 12000` puts a signal 12 kHz up
the band and leaves its symbol rate exactly where it was. A real emitter
closing at 7.5 km/s does something else: it rescales the *received time base*,
so the carrier, the symbol rate and the chip rate all move together, by the
same fraction. A receiver's timing loop sees an error that a carrier-only
offset never produces — which is precisely the error you wanted to test.

`--doppler` is that second thing. It is given in **ppm** — parts per million
of time-base scale — because that one number is what moves every clock at
once:

$$
f_\text{rx} = f_\text{tx}\,(1 + d\times10^{-6}),
\qquad
R_\text{sym,rx} = R_\text{sym,tx}\,(1 + d\times10^{-6})
$$

______________________________________________________________________

### The four flags

| Flag                           | Unit  | What it sets                                                                                                     |
| ------------------------------ | ----- | ---------------------------------------------------------------------------------------------------------------- |
| `--doppler PPM[:PPM]`          | ppm   | Time-base scale. `0` (default) builds no channel at all.                                                         |
| `--doppler-rate PPM_S[:PPM_S]` | ppm/s | Linear ramp on the above — an accelerating pass.                                                                 |
| `--carrier-hz HZ`              | Hz    | The RF carrier the ppm is *referred to*, which is what turns a time-base scale into a coherent carrier rotation. |
| `--doppler-lifetime L`         | —     | `per_instance` (default) or `persist`. See [below](#the-lifetime-is-a-choice).                                   |

A LEO pass at S-band is a couple of tens of ppm:

```sh
# 25 ppm closing on a 2.4 GHz carrier ≈ 60 kHz of carrier shift, and a
# symbol rate 25 ppm fast along with it.
wfmgen --type qpsk --fs 1e6 --sps 4 --snr 12 --count 20000 \
       --doppler 25 --carrier-hz 2.4e9 -o pass.cf32
ls -l pass.cf32
```

`--carrier-hz` is **independent** of the two ppm figures, and `0` is a real
answer rather than an unset field: it warps the clock and applies no carrier
rotation, which is the right model for a baseband recording whose carrier was
already stripped. Set it when you want the rotation that accompanies the warp.

`--doppler-rate` ramps the offset as the pass progresses:

```sh
# A pass that sweeps through zero: -20 ppm, closing at 4 ppm/s.
wfmgen --type bpsk --fs 1e6 --sps 8 --count 20000 \
       --doppler -20 --doppler-rate 4 --carrier-hz 2.2e9 -o accel.cf32
ls -l accel.cf32
```

!!! note "The rate is per second of *stream*, not per second of on-time"

    The channel keeps running through a segment's gaps, on the noise floor
    the gap already carries. An emitter does not stop moving because its
    burst ended, so a two-burst scene with a 10 ms gap resumes 10 ms further
    along the pass — not where it left off. Skip the gaps and
    `doppler_rate` would quietly mean "ppm per second of transmission",
    which is not a geometry anything flies.

______________________________________________________________________

### Ranged, like `freq` and `snr`

A pass is a distribution, not a number. Both ppm fields take the same
`LO:HI` form as `--freq`/`--snr`/`--level`, drawn uniformly on each repeat
instance:

```sh
# Eight bursts, eight geometries — a Monte Carlo over the pass.
wfmgen --type qpsk --fs 1e6 --sps 4 --snr 10 --count 4096 --off 2000 \
       --repeats 8 --doppler 2:9 --doppler-rate 0.1:0.5 \
       --carrier-hz 1.5e9 --file-type sigmf -o sweep
ls sweep.sigmf-meta
```

The draw is a hash of `(seed, repeat index, segment, source, field)`, so
`--record` stores the **span** and `--from-file` replays the identical
sequence of draws. What each instance actually flew is recorded separately,
per annotation, in the SigMF sidecar:

```json title="one annotation from sweep.sigmf-meta"
{
  "core:sample_start": 6096,
  "core:sample_count": 4096,
  "wfmgen:doppler_ppm": 2.1624832257080335,
  "wfmgen:doppler_rate_ppm_s": 0.4429440692793868,
  "wfmgen:carrier_hz": 1500000000,
  "wfmgen:doppler_lifetime": "per_instance"
}
```

Those keys appear only when a channel was actually built, so a scene without
Doppler writes exactly the sidecar it always did.

______________________________________________________________________

### The lifetime is a choice

`--repeats 8` plays one segment eight times. Does the emitter restart its
pass on each burst, or fly straight through?

- **`per_instance`** (default) — the channel dies with each instance and the
    geometry restarts. This is the **repeated-trial** shape: eight
    independent looks at a target, which is what composes with a ranged
    `--doppler` re-drawn per instance.
- **`persist`** — one continuous pass carries across the segment's gaps and
    repeat instances. This is the only lifetime under which `--doppler-rate`
    accumulates over a multi-burst scene: burst 8 is further along than burst
    1 by the full elapsed stream time.

```sh
# One pass, eight bursts of it — the offset has ramped by the last one.
wfmgen --type bpsk --fs 1e6 --sps 8 --count 4096 --off 4000 --repeats 8 \
       --doppler 5 --doppler-rate 200 --carrier-hz 2e9 \
       --doppler-lifetime persist -o onepass.cf32
ls -l onepass.cf32
```

A persisting channel is keyed by **(segment, source) position**, which is the
only source identity a scene file carries. So it persists across a segment's
repeat instances and their gaps, and two different segments each get their
own pass — sharing one pass between segments would need a declared source id
the format does not have.

______________________________________________________________________

### In JSON and in Python

Both fields are ordinary source keys, scalar or `[lo, hi]`:

```json title="pass.json"
{
  "version": 1,
  "segments": [
    {
      "type": "qpsk", "fs": 1e6, "sps": 4, "snr": 12,
      "num_samples": 20000,
      "doppler": [2, 9],
      "doppler_rate": 0.4,
      "carrier_hz": 2.4e9,
      "doppler_lifetime": "persist"
    }
  ]
}
```

```sh
wfmgen --from-file pass.json -o from_json.cf32
ls -l from_json.cf32
```

and the same names are `Segment`/`Synth` keyword arguments, with a ranged
field spelled as a tuple:

```python
from doppler.wfm.compose import Composer, Segment

scene = Composer(
    [
        Segment(
            "qpsk",
            fs=1e6,
            sps=4,
            snr=12.0,
            num_samples=20000,
            doppler=(2.0, 9.0),
            doppler_rate=0.4,
            carrier_hz=2.4e9,
            doppler_lifetime="persist",
        )
    ]
)
x = scene.compose()
print(x.shape, x.dtype)
```

______________________________________________________________________

### `Plan` refuses a Doppler source

[`Plan`](scenes.md#prepare-once-sweep-many-plan) caches each source's clean **on-time** once, in isolation,
and then re-weights it. A Doppler channel does not fit in that box, for two
reasons that were measured against `compose()` rather than assumed:

- **A burst depends on what came before it.** The channel is a stateful
    resampler and it runs through the gaps, so a trailing gap carries the
    burst's ring-out and a leading `delay_samples` advances the geometry
    before the burst starts. A cached on-time has nowhere to keep that
    history. Measured on a clean one-source scene: `off_samples = 256`
    diverges from `compose()` across all 256 gap samples, and
    `delay_samples = 256` diverges over the *burst*, from sample 257 on.
- **The noise sits on the wrong side.** `compose()` puts the AWGN inside the
    synth, so the channel resamples it too; the cache holds a clean render
    and adds noise outside the channel. Measured on a bundled single noisy
    source with no gaps at all: max |err| 1.73 on a unit-power signal.

`persist` would be refused even without those, and for its own reason: the
cache builds each source **independently and concurrently**, and
"bit-identical to the serial build" is a documented property of it — a
channel carrying across segments makes segment *i* depend on segments
0…*i*−1 having been rendered, in order.

So `prepare()` refuses, rather than caching a render that differs from
`compose()` in a way nothing downstream can see:

```python
from doppler.wfm import prepare
from doppler.wfm.compose import Composer, Segment

kw = dict(fs=1e6, sps=4, num_samples=4096, carrier_hz=2.2e9)

try:
    prepare(Composer([Segment("bpsk", doppler=5.0, **kw)]).to_json())
except ValueError as exc:
    print("refused:", "doppler" in str(exc))

# ...but the keys alone cost nothing: zero doppler AND zero doppler_rate
# builds no channel, so a declared lifetime describes nothing.
plan = prepare(
    Composer([Segment("bpsk", doppler_lifetime="persist", **kw)]).to_json()
)
print(len(plan))
```

`compose()` and `stream()` honour both lifetimes in full — only the sweep
cache is restricted. Teaching the cache to carry a channel's history is the
follow-up, [doppler#1109](https://github.com/doppler-dsp/doppler/issues/1109);
until then, sweep a Doppler scene by composing it per point.

______________________________________________________________________

## See also

- [Scenes](scenes.md) — putting these waveforms in time.
- [Gallery: wfmgen](../../gallery/wfmgen.md) — the spectra and
    constellations behind each type.
- [Writing captures](../wfm-io/writing.md) — sample types, file types,
    endianness, output targets.
