# Spectrum Analyzer

`doppler.analyzer.Specan` (C: `dp_specan_create`) takes the instrument
parameters an operator knows: **span** and **RBW**, plus center and reference
level. From those two it derives everything underneath: the decimated rate,
the transform length and the Kaiser window. This page is that derivation.
It lives once, in `native/src/specan/specan_core.c`.

## The three rules

**1. Span sets the rate.**

```text
fs_out = 1.28 · span
±span/2 = ±fs_out/2.56 = bin ±k,   k = nfft/2.56
```

`k` is an integer for every power-of-two `nfft ≥ 256`, so the span edges are
bins, not points between them. The display is the `2k + 1` bins from `−k` to
`+k`. The DDC passband is `±0.4·fs_out = ±0.512·span`, so the whole display
sits inside it.

A span the input cannot supply (`1.28·span > fs`) is **clamped** to
`fs/1.28`. It is not kept with `fs_out = fs`, which would put its edges
between bins. `span = 0` means auto: the whole input band, `fs/1.28`.

**2. The window is a power of two; the transform pads it only if needed.**

```text
base RBW = 2 · fs_out / n       (Kaiser ENBW of 2 bins of the window)
n        = the SMALLEST power of two with base RBW ≤ rbw, at least 16
nfft     = max(n, 512)          →  2k + 1 ≥ 401 display bins
```

The base RBW is the narrowest a given window offers. An ENBW of 2 bins is
Kaiser beta ≈ 12, with peak sidelobes near −90 dB. A narrow RBW needs a long
window, which fills the transform exactly. A wide RBW needs a short one, and
only then is it zero-padded up to the 512 points the display needs.

**3. Beta widens the ENBW to meet the request.**

```text
target ENBW = rbw · n / fs_out         ∈ [2, 4) bins
beta        = cubic fit of ENBW        (below)
auto RBW    = span / 100
widest RBW  = 4 · fs_out / 16 = fs_out / 4
```

`rbw = 0` means auto. A request wider than 4 bins of the shortest window is
clamped.

`Specan.rbw` and `Specan.span` report what was **realised**, which a clamp
can make differ from the request.

```python
from doppler.analyzer import Specan

sa = Specan(fs=2.048e6, span=200e3, rbw=500.0)
assert sa.fs_out == 1.28 * 200e3           # rule 1
assert (sa.n, sa.nfft) == (1024, 1024)     # rule 2: 2·256e3/500, no pad
assert sa.display_size == 801              # 2·round(1024/2.56) + 1
assert abs(sa.rbw / 500.0 - 1) < 0.001     # rule 3, realised
assert sa.beta > 11.5                      # never below 2 bins of ENBW

wide = Specan(fs=2.048e6, span=0.0, rbw=0.0)   # both auto
assert wide.span == 2.048e6 / 1.28             # the whole input band
assert abs(wide.rbw / (wide.span / 100) - 1) < 0.001

coarse = Specan(fs=2.048e6, span=200e3, rbw=20e3)
assert (coarse.n, coarse.nfft) == (32, 512)    # a short window, padded
```

## Why every RBW gets the same skirt

The rule this replaced chose the smallest power of two `≥ fs_out/rbw`. That
left the ENBW target anywhere in `[1, 2)` bins, so the sidelobe level depended
on where the RBW fell against a power of two. At exactly `fs_out/2^k` the
target was 1 bin, which only a rectangle meets: beta 0, −13 dB sidelobes.
Measured on the specan demo's geometry (`fs_out` = 2.048 MHz):

| RBW (Hz) | old beta | old peak sidelobe | new beta | new peak sidelobe |
| -------: | -------: | ----------------: | -------: | ----------------: |
|     2000 |      0.0 |            −13 dB |     11.9 |            −89 dB |
|     3000 |      6.3 |            −46 dB |     11.9 |            −89 dB |
|     3999 |     11.9 |            −89 dB |     11.9 |            −89 dB |
|     4000 |      0.0 |            −13 dB |     11.9 |            −89 dB |
|     6000 |      6.3 |            −46 dB |     11.9 |            −89 dB |

The demo records at 4 kHz. When the C port clamped `fs_out` to the input
rate, it moved the demo from 1024/1.95 bins onto 512/1.00 bins, and the trace
showed rectangle sidelobes. Under these rules the ENBW target is never below
2 bins, so beta is never below ≈ 12.

## The beta fit

Beta comes from a cubic least-squares fit of beta against ENBW, from
`np.kaiser(4096, beta)` for beta in [10, 55], restricted to ENBW in
\[1.98, 4.02\]:

```text
beta(e) = 0.00590559 e³ + 3.07532701 e² + 0.24521102 e − 0.960144
e       = target ENBW · (n − 1) / n
```

The `(n − 1)/n` is the window's own length. A symmetric n-point window spans
n − 1 sample intervals, so its ENBW is the long-window value times
`n/(n − 1)`. Uncorrected, that was the whole fit error: 0.18% at 512 and
6.7% at 16. Corrected, the realised ENBW is within 0.03% of the target for
every `n ≥ 16`; at `n = 8` it breaks (7%), which is why 16 is the shortest
window. The analyzer reports the RBW the window realises, so the fit decides
how close that lands to the request, not what is reported. It replaces a
60-step bisection that built the window once per step.

## How it is checked

All in `native/tests/test_specan_core.c`, over eleven RBWs from a 2048-point
window (250 Hz) to the 16-point minimum (60 kHz, padded 32×). Four of them
(250, 500, 1000 and 2000 Hz at `fs_out` = 256 kHz) are exactly the
`fs_out/2^k` case.

| Claim          | Check                                                                                                                                                                                    |
| -------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Rule 1         | `fs_out = 1.28·span`; `nfft % 256 == 0`; the edge bin is exactly `span/2`; span 0 and an oversize span both give `fs/1.28`                                                               |
| Rule 2         | `n` a power of two `≥ 16`, base RBW `≤ rbw` and half of `n` would not fit; `nfft = max(n, 512)`                                                                                          |
| Rule 3         | ENBW from 2 to 4 bins, beta `> 11.5`, realised RBW within 0.1%; `rbw` 0 is `span/100`; a request past 4 bins of the 16-point window clamps to `fs_out/4`                                 |
| Skirt          | a noiseless tone sits ≥ 80 dB above every bin outside its main lobe, wherever the main lobe leaves room to look                                                                          |
| ENBW, measured | unit-variance white noise reads `rbw/fs` per bin within 0.1 dB, with equal total noise at every RBW: integrated noise power confirms the ENBW from the output, against the RBW requested |

Each was proven by sabotage, and each turns the test red:

| Sabotage                           | Failures |
| ---------------------------------- | -------: |
| restore the old ENBW `[1, 2)` rule |       54 |
| scale the beta fit by 3%           |       14 |
| drop the `(n − 1)/n` correction    |       12 |
| never pad (`nfft = n`)             |        9 |
| solve beta for an ENBW 5% wide     |       25 |

The last one is caught by the noise check on its own at every RBW (+0.15 to
+0.25 dB off `N0·RBW`), as well as by the realised-RBW bound.
