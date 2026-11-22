#!/usr/bin/env python3
"""wfmgen_ofdm_demo.py -- 802.11 PPDUs from a data source.

The Python twin of ``native/examples/wfmgen_ofdm_demo.c``. ``type="ofdm"``
sends one PPDU per frame of a data source, built through ``wlan``: the frame
is the PSDU's octets (the first bit of each eight the most significant, as a
file's bytes and a hex Field unpack), ``wlan=36`` is the 20 MHz data rate in
Mb/s, and ``mac="data"`` puts a MAC header (``mac_header``) in front and the
FCS behind.

What it shows, in order:

1. One source is the PPDU ``Wlan.ppdu`` builds from the same octets: the
   header, the body and the FCS, the FCS computed here with ``zlib``.
2. A source with two frames sends two PPDUs, and with ``scrambler_init`` 0
   each draws its scrambler's initial state from ``seed`` and its index, so
   they differ and the run is reproducible.
3. ``fs`` twice the bandwidth is a larger transform, not a resampler: with
   no window (``window=0``) every second sample is the 20 MHz PPDU's, and a
   ``gap`` after each PPDU is noise at the source's noise variance. The
   source's default window is the standard's 100 ns, ``2 * fs / bandwidth``
   samples, as ``Wlan``'s is.

Every claim is an explicit check that exits non-zero on failure.

Run: python src/doppler/examples/wfmgen_ofdm_demo.py
"""

# --8<-- [start:ofdm]
import zlib

import numpy as np

from doppler.cvt import bytes_to_bin
from doppler.wfm import Composer, Segment
from doppler.wlan import Wlan

HEADER = bytes.fromhex("08020000ffffffffffff0200000000010200000000020000")
body = bytes((7 * k + 3) % 256 for k in range(100))  # 100 octets
octets = np.frombuffer(body, np.uint8)
bits = np.zeros(8 * octets.size, np.uint8)
bytes_to_bin(octets, bits, 0)  # MSB first, as a file is read


def ppdu(fs=20e6, **overrides):
    """The PPDUs of `bits` (two copies: two frames), as samples."""
    args = {
        "type": "ofdm",
        "fs": fs,
        "wlan": 36,  # 36 Mb/s: 16-QAM, rate 3/4
        "mac": "data",  # header + body + FCS
        "mac_header": np.unpackbits(np.frombuffer(HEADER, np.uint8)),
        "data": np.concatenate([bits, bits]),
        "data_len": bits.size,  # one PPDU per 800 bits
        "scrambler_init": 93,
        "snr": 100.0,
        "snr_mode": "fs",
    }
    return np.asarray(Composer([Segment(**{**args, **overrides})]).compose())


x = ppdu()
# --8<-- [end:ofdm]

psdu = HEADER + body
psdu += zlib.crc32(psdu).to_bytes(4, "little")  # the FCS, least significant
# octet first
want = Wlan(constellation="qam16", code_rate="r34").ppdu(
    np.frombuffer(psdu, np.uint8), 93
)
n = len(want)
ok = True


def check(name: str, cond: bool) -> None:
    global ok
    print(f"{'ok  ' if cond else 'FAIL'} {name}")
    ok &= bool(cond)


# 1. the PPDU wlan builds from header + body + FCS
check("two PPDUs, one per frame", len(x) == 2 * n)
check(
    "the first PPDU is Wlan.ppdu of header + body + FCS",
    np.allclose(x[:n], want, atol=1e-5),
)

# 2. with the scrambler drawn per PPDU the frames differ, reproducibly
y = ppdu(scrambler_init=0, seed=5)
check(
    "a drawn scrambler gives each PPDU its own state",
    not np.allclose(y[:n], y[n:], atol=1e-5),
)
check(
    "and the run is reproducible",
    np.array_equal(y, ppdu(scrambler_init=0, seed=5)),
)

# 3. twice the bandwidth: a larger transform, and the gap's noise
z = ppdu(fs=40e6)
check("at 40 MHz a PPDU is twice as long", len(z) == 4 * n)
# The same 100 ns window is `2 M` samples, and a window cross-fades the
# transition on a finer grid, so the two rates agree outside it; with no
# window (window=0) the 40 MHz samples are the 20 MHz ones, every second.
r20 = ppdu(window=0)
r40 = ppdu(fs=40e6, window=0)
m = len(r20) // 2  # one PPDU, no window
d = np.abs(r40[0 : 2 * m : 2] - r20[:m])
check(
    "with no window every second sample is the 20 MHz PPDU's",
    np.sqrt(np.sum(d**2) / np.sum(np.abs(r20[:m]) ** 2)) < 1.5e-3,
)
GAP = 20000
g = np.asarray(
    Composer(
        [
            Segment(
                type="ofdm",
                fs=20e6,
                wlan=36,
                data=bits,
                data_len=bits.size,
                scrambler_init=93,
                gap=GAP,
                snr=10.0,
                snr_mode="fs",
                seed=3,
            )
        ]
    ).compose()
)
noise = g[len(g) - GAP :]
p = float(np.mean(np.abs(noise) ** 2))
check(
    "the gap is noise at the noise variance (10 dB below unit power)",
    abs(p / 0.1 - 1.0) < 5.0 / np.sqrt(GAP),
)

print("wfmgen_ofdm_demo:", "OK" if ok else "FAILED")
raise SystemExit(0 if ok else 1)
