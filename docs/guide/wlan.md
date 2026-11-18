---
description: "IEEE 802.11 non-HT OFDM builders: a rate, the minimal MAC with its FCS, and the PPDU's coding chain, for the layouts the OFDM modulator takes"
---

# 802.11 non-HT OFDM (`wlan`)

[`doppler.wlan`][doppler.wlan] is the 802.11 half of OFDM. The
[modulator](../design/ofdm.md) turns constellation points and a layout into
samples and names no standard; `wlan` turns an 802.11 rate and a PSDU into the
layout and the points it takes. It is a table of PPDU formats and a builder
from a format and a frame body, so the rules about what goes in each field are
written once, and a caller builds a frame without knowing them.

**What is held, and what is not.** Every number the design page
([`docs/design/wlan.md`](../design/wlan.md)) marks checked against the text of
IEEE Std 802.11-2024 is a constant in `wlan` or a literal in
`dp_wlan_tables_standard()`. The unit test holds the RATE bits, both training
sequences, the constellation encodings and the 2/3 and 3/4 puncture masks
against their printed form. The second interleaver permutation is held by its
worked 16-QAM example and by a re-derivation of Eq. (17-16) and (17-17) for
every index of all four pairs, which is the printed equation implemented a
second time, not a printed table. The pilot polarity sequence is not held
against a printed form. The 5/6 code rate is Clause 19's and is not
checked. Nothing here claims conformance with 802.11a,
g, n or ac until Annex I's stage-by-stage example passes.

## A rate

A non-HT rate is a constellation and a BCC code rate, the two columns of the
rate table that do not change with the channel clocking. `Wlan` holds one and
says what follows from it:

```python
--8<-- "src/doppler/examples/wlan_rates_demo.py:rates"
```

`n_bpsc` is bits per subcarrier, `n_cbps` the coded bits and `n_dbps` the data
bits in one OFDM symbol, and `n_sym(psdu_len)` the symbols a PSDU of that many
octets fills: its bits, with the 16-bit SERVICE field and six tail bits, padded
to a whole number of symbols. The airtime at 20 MHz is a 20 us preamble and
SIGNAL field and 4 us a symbol; the 10 and 5 MHz clockings double and
quadruple every duration and change nothing else.

## The minimal MAC

The generator wraps the caller's octets as one MPDU and does nothing more:
the whole PSDU as given, an MPDU with its FCS appended, or a header and a
body with their FCS. The header is the caller's raw octets. The FCS is
CRC-32 over every octet before it, appended least significant octet first, so
the CRC over a whole frame is one constant. The C example shows it:

```c
--8<-- "native/examples/wlan_mac_demo.c"
```

## The PPDU

A whole frame, from a PSDU to samples, is one call, `dp_wlan_ppdu()`. The
steps, in order, with the example that runs all of them:

1. **Wrap the body** as a PSDU (`dp_wlan_mpdu()`, above), or bring one.
1. **Choose the rate** (`dp_wlan_rate_t`: bits per subcarrier and a code
    rate) and ask `dp_wlan_rate_n_sym()` how many Data OFDM symbols the PSDU
    fills.
1. **Size the buffer.** `dp_wlan_ppdu_len(n_sym, window)` is the whole PPDU in
    samples, `160 + 160 + 80 + 80 * n_sym` plus the window's tail; the caller
    allocates it, and a buffer one sample short is a refusal, not a
    truncation.
1. **Render.** `dp_wlan_ppdu(tables, &rate, psdu, len, state, window, out,  cap)` runs the SERVICE, tail and pad assembly, the scrambler (`state` is
    its register, 1 to 127), the punctured BCC, the interleaver, the mapper,
    the pilots and the OFDM modulator, and writes the L-STF, the L-LTF, the
    SIGNAL field and the Data field. It returns the samples written, or `0`,
    changing nothing, if any stage refuses.
1. **Check it.** The example tests the length, that the L-STF repeats every
    16 samples, that the L-LTF's two long symbols are alike, and the power of
    each field: exactly one for the two training fields (the standard's
    scale is 1/sqrt 52, one unit over the 52 occupied subcarriers) and, for
    each SIGNAL or Data symbol, the energy of its own grid over 52, which
    Parseval ties to the points it was built from. A Data symbol's power is
    not one: a 16-QAM symbol's 48 points have a mean power of one only on
    average over the constellation, and the cyclic prefix repeats only the
    body's last 16 samples.

```c
--8<-- "native/examples/wlan_ppdu_demo.c"
```

The stages are public if a caller wants one of them: `dp_wlan_points()` is the
constellation points of a PPDU in layout order (the SIGNAL field's 48, then
the Data field's), and `dp_wlan_layout()` builds the matching OFDM layout in a
modulator, which is what the wfmgen `ofdm` waveform type uses. All of them
take the tables the standard supplies, `dp_wlan_tables_standard()`.

**What it costs.** A 1000-octet PSDU at the largest rate pair, 64-QAM rate
3/4 (54 Mb/s, 38 Data symbols), is timed stage by stage in
`native/benchmarks/wlan/` (the values are taken at each release's benchmark
set): the interleaver, the mapper, the points, the layout and the whole PPDU.
