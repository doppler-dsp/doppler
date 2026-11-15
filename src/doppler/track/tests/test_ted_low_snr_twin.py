"""Fast twin of the `ted_low_snr` characterization subject.

The subject runs 1000 frames of the specification's gapped stream through
`RateSync` at Es/N0 = 3 dB, for both detectors, two bandwidths and three
levels, in under a minute. This runs five rows of it at the receiver's
bandwidth -- Gardner at 0.5x, 1x and 2x of the contracted level, the DTTL
at 1x and 2x -- over 300 frames at the sweep's own seed, and pins the
three results the full run found there, plus the calibrations every
column rests on.

**What it pins**, each held in all 18 runs of a scratch check (seeds 1-8
and 751, both rates, 300 frames), so none rests on a chosen seed:

- Gardner at half the contracted level slips nothing; at the contract it
  does. The level, through Gardner's `A^2` loop gain, decides whether the
  loop slips at all.
- At the contract, both detectors slip in the idle run and its dotted
  preamble more than twice as often as in data.
- Over-driven 2x, Gardner slips in data, where a slip costs the frame, at
  least five times as often as the DTTL.

The full run's DTTL-against-Gardner count IN the gap is not pinned: it
reads 81 against 47 at 3000 sym/s and 65 against 62 at 7000, and in the
scratch check the DTTL had more in 12 of 18 runs. That is not a result.

**What it does not cover.** The other rows, and whether the counts hold at
1000 frames -- that waits for the next `make characterize` (doppler#692).
"""

from __future__ import annotations

import re

import numpy as np
import pytest

from doppler.tests._repo import repo_root
from doppler.track.tests.characterization.ted_low_snr.characterize import (
    BN_RECEIVER,
    DOTS,
    GAP_SYMBOLS,
    SEED,
    SPS,
    SYNC,
    contract_scale,
    frame_segment,
    make_stream,
    run_row,
    stimulus,
)
from doppler.wfm import field_bits

TWIN_FRAMES = 300
TWIN_SEED = SEED
RATE = 3000

ROOT = repo_root(__file__)
RX_H = (
    ROOT
    / "native/inc/doppler"
    / "async_dsss_receiver"
    / ("async_dsss_receiver_core.h")
)

#: (detector, level) rows the twin runs, all at `BN_RECEIVER`.
ROWS = (
    ("gardner", 0.5),
    ("gardner", 1.0),
    ("dttl", 1.0),
    ("gardner", 2.0),
    ("dttl", 2.0),
)


@pytest.fixture(scope="module")
def rows():
    """The five rows, on one stream and one noise realisation."""
    stream = make_stream(GAP_SYMBOLS[RATE], TWIN_FRAMES, TWIN_SEED)
    x = stimulus(stream, TWIN_SEED)
    return {
        (ted, lv): run_row(
            stream, x, rate=RATE, ted=ted, bn=BN_RECEIVER, level=lv
        )
        for ted, lv in ROWS
    }


def _slips(r) -> str:
    return (
        f"{r.ted} {r.level:g}x: gap {r.slips_gap}, data {r.slips_data}, "
        f"other {r.slips_other}"
    )


def test_level_decides_whether_gardner_slips(rows):
    """Half the contracted level: no slips. The contract: some."""
    half, unit = rows["gardner", 0.5], rows["gardner", 1.0]
    assert half.slips == 0 and unit.slips >= 5, (_slips(half), _slips(unit))


@pytest.mark.parametrize("ted", ["gardner", "dttl"])
def test_slips_land_in_the_gap_not_the_data(rows, ted):
    """At the contract the idle run and its dots take the slips."""
    r = rows[ted, 1.0]
    assert r.slips_gap >= 5 and r.slips_gap > 2 * r.slips_data, _slips(r)


def test_hot_gardner_slips_in_data_where_dttl_does_not(rows):
    """At 2x, Gardner's `A^2` gain costs frames; the DTTL's `A^1` does not."""
    g, d = rows["gardner", 2.0], rows["dttl", 2.0]
    assert g.slips_data >= 5 * max(d.slips_data, 1), (_slips(g), _slips(d))


def test_dttl_ser_is_no_worse_at_the_floor(rows):
    """The header's "degrades faster at low SNR" does not show in SER.

    Both are scored between slips on the same symbols and noise, and the
    loss is taken on each interval's lower limit -- beside the slip counts,
    never instead of them.
    """
    g, d = rows["gardner", 1.0], rows["dttl", 1.0]
    assert np.isfinite(g.loss_db) and np.isfinite(d.loss_db)
    assert d.loss_db <= g.loss_db + 0.25, (
        f"loss dB: dttl {d.loss_db:+.2f}, gardner {g.loss_db:+.2f}"
    )
    assert g.loss_db <= 1.0 and d.loss_db <= 1.0


def test_bn_receiver_is_the_receivers():
    """`BN_RECEIVER` must be the bandwidth the receiver really builds."""
    m = re.search(
        r"#define\s+ASYNC_DSSS_RX_DEMOD_BN_TIMING\s+([0-9.eE+-]+)",
        RX_H.read_text(encoding="utf-8"),
    )
    assert m is not None, f"ASYNC_DSSS_RX_DEMOD_BN_TIMING not in {RX_H}"
    assert float(m.group(1)) == BN_RECEIVER


def test_stimulus_is_unit_symbol_amplitude():
    """`contract_scale` delivers RateSync's contract: unit symbols.

    Measured against the truth, data-aided, rather than trusting the
    Es/N0 algebra: the mean of each sample times its symbol's sign is the
    symbol amplitude, and the noise averages out over the record.
    """
    stream = make_stream(GAP_SYMBOLS[RATE], 20, TWIN_SEED)
    x = stimulus(stream, TWIN_SEED)
    sign = np.repeat(1.0 - 2.0 * stream.truth, SPS)
    amp = float(np.mean(x.real * sign))
    assert abs(amp - 1.0) < 0.01, (amp, contract_scale())


def test_frame_is_the_specification_frame():
    """The composed stream is the frame layout, CRC included."""
    from doppler.wfm import STAGE_CRC16, FrameDesc

    g = GAP_SYMBOLS[RATE]
    stream = make_stream(g, 3, TWIN_SEED)
    a, e = int(stream.starts[0]), int(stream.ends[0])
    bits = stream.truth[a:e]
    nd, ns = len(field_bits(DOTS)), len(field_bits(SYNC))
    assert not bits[:g].any(), "the idle run is not all zeros"
    np.testing.assert_array_equal(bits[g : g + nd], field_bits(DOTS))
    np.testing.assert_array_equal(bits[g + nd : g + nd + ns], field_bits(SYNC))
    n_payload = e - a - g - nd - ns - 16
    d = FrameDesc()
    d.add_field("gap", np.zeros(g, np.uint8))
    d.add_field("dots", field_bits(DOTS))
    d.add_field("sync", field_bits(SYNC))
    d.add_data("payload", n_payload)
    d.add_derived("fecf", 16)
    d.add_stage_over(STAGE_CRC16, "payload", "fecf")
    d.build()
    assert d.crc_ok(bits) == 1
    flipped = bits.copy()
    flipped[g + nd + ns] ^= 1
    assert d.crc_ok(flipped) == 0
    assert frame_segment(g, np.zeros(1, np.uint8)) is not None
