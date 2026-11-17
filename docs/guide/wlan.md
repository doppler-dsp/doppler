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

**What is certified, and what is not.** Every number the design page
([`docs/design/wlan.md`](../design/wlan.md)) marks checked against the text of
IEEE Std 802.11-2024 is a constant in `wlan`. Every number it does not mark
checked is not in the tree: the SIGNAL field's RATE bits, the L-STF and L-LTF
subcarrier values, the constellation labels where they differ from the
library's maps, and the second interleaver permutation. A builder that needs
one refuses. Nothing here claims conformance with 802.11a, g, n or ac until
those tables are transcribed from the text and the Annex I example passes.

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

The layout and the constellation points reach Python with the wfmgen `ofdm`
type; from C they are `dp_wlan_points()`, `dp_wlan_layout()` and
`dp_wlan_ppdu()`, which take the tables the standard supplies.
