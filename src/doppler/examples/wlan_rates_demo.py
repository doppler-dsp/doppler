"""wlan_rates_demo.py — how long a frame takes on the air, rate by rate.

An 802.11 non-HT rate is a constellation and a BCC code rate: the two columns
of the standard's rate table that do not change with the channel clocking.
Everything the PHY does with a frame follows from them. A frame's PSDU is
padded to a whole number of OFDM symbols, each carrying ``n_dbps`` data bits,
and goes on the air behind a preamble and a SIGNAL field:

    airtime = 20 us + n_sym * 4 us           (at 20 MHz)

This demo asks :class:`doppler.wlan.Wlan` for the symbol count of a
1500-octet frame at each rate, and checks it against the airtime the
formula gives from the data rate alone.

Run it::

    python src/doppler/examples/wlan_rates_demo.py
"""

from __future__ import annotations

import math

# --8<-- [start:rates]
from doppler.wlan import Wlan

FRAME = 1500  # the PSDU, in octets
RATES = [
    ("bpsk", "r12"),
    ("qpsk", "r12"),
    ("qam16", "r12"),
    ("qam16", "r34"),
    ("qam64", "r34"),
]


def airtime_us(w: Wlan, psdu_len: int) -> float:
    """The preamble (L-STF, L-LTF: 16 us), the SIGNAL symbol (4 us) and the
    Data symbols (4 us each) of a frame at this rate, at 20 MHz."""
    return 16.0 + 4.0 + 4.0 * w.n_sym(psdu_len)


rows = []
for constellation, code_rate in RATES:
    w = Wlan(constellation=constellation, code_rate=code_rate)
    rows.append((constellation, code_rate, w.rate_mbps, w.n_sym(FRAME)))
# --8<-- [end:rates]


def main() -> None:
    print(f"a {FRAME}-octet frame at 20 MHz")
    print(
        f"{'rate':>14} {'Mb/s':>7} {'OFDM symbols':>13} {'airtime (us)':>13}"
    )
    last = float("inf")
    for constellation, code_rate, mbps, n_sym in rows:
        w = Wlan(constellation=constellation, code_rate=code_rate)
        t = airtime_us(w, FRAME)
        print(
            f"{constellation + ' ' + code_rate:>14} {mbps:7d} "
            f"{n_sym:13d} {t:13.0f}"
        )
        # the symbols a frame needs follow from the data rate: its bits
        # (SERVICE 16, the octets, the tail 6) over the bits a symbol carries
        assert n_sym == math.ceil((16 + 8 * FRAME + 6) / w.n_dbps)
        # a faster rate never takes longer
        assert t <= last
        last = t
    # the 36 Mb/s rate: 144 data bits per 4 us symbol, and the table's own
    # column says the same
    w36 = Wlan(constellation="qam16", code_rate="r34")
    assert w36.n_dbps / 4.0 == 36.0 == w36.rate_mbps
    print("OK: the symbol counts follow from the data rates")


if __name__ == "__main__":
    main()
