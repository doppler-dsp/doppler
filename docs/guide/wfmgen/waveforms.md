# Waveforms

`--type` selects the waveform; every type shares the same parameter set. The
engine produces eight types from one declarative core.

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

______________________________________________________________________

## Bits — your bit pattern, mapped

A `bits` waveform plays back **your** sequence — a preamble, sync word, or test
vector — given as a 0/1 string (`--bits 10110101`), a hex string
(`--bits-hex AA55`, MSB first), or a file (`--bits-file pattern.txt`).
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
```

______________________________________________________________________

## Engine parameter reference

| Flag             | Type                                         | Default  | Meaning                                                                            |
| ---------------- | -------------------------------------------- | -------- | ---------------------------------------------------------------------------------- |
| `--type`         | `tone noise pn bpsk qpsk chirp bits symbols` | `tone`   | waveform                                                                           |
| `--fs`           | float (Hz)                                   | `1.0`    | sample rate (default `1.0` ⇒ `--freq`/`--f-end` are normalised, cycles/sample)     |
| `--freq`         | float (Hz)                                   | `0`      | frequency offset from baseband (mixed by the LO); chirp start                      |
| `--f-end`        | float (Hz)                                   | `0`      | chirp end frequency (`--type chirp` only)                                          |
| `--snr`          | float (dB)                                   | `100`    | SNR; metric chosen by `--snr-mode` (≈clean at 100) — see [Levels & SNR](levels.md) |
| `--snr-mode`     | `auto fs ebno esno`                          | `auto`   | how `--snr` is interpreted                                                         |
| `--seed`         | uint32                                       | `0`      | PRNG / LFSR seed (deterministic)                                                   |
| `--sps`          | int                                          | `1`      | samples per symbol (`*psk`/`bits`/`symbols`) / per chip (`pn`)                     |
| `--pn-length`    | int (2..64)                                  | `15`     | LFSR register length → period `2ⁿ−1`                                               |
| `--pn-poly`      | uint64                                       | `0`      | LFSR polynomial; `0` ⇒ auto-pick the MLS polynomial                                |
| `--lfsr`         | `galois fibonacci`                           | `galois` | LFSR realization (same polynomial/period, different sequence)                      |
| `--bits`         | 0/1 string                                   | —        | `bits`: pattern, e.g. `10110101` (or `--bits-hex`/`--bits-file`)                   |
| `--modulation`   | `none bpsk qpsk`                             | `bpsk`   | `bits`: how the pattern maps to symbols                                            |
| `--symbols-file` | path (cf32)                                  | —        | `symbols`: raw interleaved-I/Q complex64 constellation stream                      |
| `--pulse`        | `rect rrc`                                   | `rect`   | pulse shape; `rrc` = band-limited RRC shaping                                      |
| `--rrc-beta`     | float                                        | `0.35`   | RRC roll-off (`--pulse rrc`)                                                       |
| `--rrc-span`     | int                                          | `8`      | RRC filter support in symbols (`--pulse rrc`)                                      |
| `--count`        | int                                          | `1024`   | number of complex samples to generate                                              |
