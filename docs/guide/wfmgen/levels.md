# Levels & SNR

## Amplitude & full-scale

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

## Scaling to the wire, and headroom

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

`Reader` (see [Output & containers](output.md)) inverts the same map, so a float
round-trip is exact and an integer round-trip is exact only where it neither
clipped nor truncated.

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

## SNR & noise

`--snr` is applied as AWGN; `--snr-mode` chooses the reference:

| Mode   | `--snr` means                                                 | Use for              |
| ------ | ------------------------------------------------------------- | -------------------- |
| `fs`   | SNR over the full sample rate (in-band power / noise power)   | tones, wideband      |
| `esno` | **Es/No** — energy per *symbol* over noise PSD                | modulated (`*psk`)   |
| `ebno` | **Eb/No** — energy per *bit* over noise PSD                   | link-budget work     |
| `auto` | `fs` for `tone`/`noise`/`pn`, `esno` for `bpsk`/`qpsk`/`dsss` | the sensible default |

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
    pattern would get. Full semantics: [DSSS bursts](dsss-bursts.md).

!!! example "Same QPSK at three references"

    ```sh
    wfmgen --type qpsk --snr 10 --snr-mode esno     # 10 dB Es/No (the auto default)
    wfmgen --type qpsk --snr 7  --snr-mode ebno     # 7 dB Eb/No  (= 10 dB Es/No)
    wfmgen --type qpsk --snr 1  --snr-mode fs        # 1 dB over fs (per-sample)
    ```
