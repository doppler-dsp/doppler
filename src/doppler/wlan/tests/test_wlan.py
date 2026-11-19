"""Integration tests for :class:`doppler.wlan.Wlan`.

The builders are C and are pinned in
``native/tests/wlan/test_wlan_core.c``; the Python face is the rate, the two
columns of Table 17-4 that do not change with the clocking, and what follows
from it. These hold the binding to that, with an oracle of its own: the
counts from the formulas (``48 * n_bpsc`` coded bits per OFDM symbol, that
times the code rate for the data bits, and the symbol count a PSDU needs).
"""

import math

import pytest

from doppler.wlan import Wlan

BPSC = {"bpsk": 1, "qpsk": 2, "qam16": 4, "qam64": 6}
RATE = {"r12": (1, 2), "r23": (2, 3), "r34": (3, 4), "r56": (5, 6)}


def n_sym(n_dbps: int, psdu_len: int) -> int:
    """The Data OFDM symbols a PSDU needs: (16 + 8L + 6) / N_DBPS, up."""
    return math.ceil((16 + 8 * psdu_len + 6) / n_dbps)


@pytest.mark.parametrize("constellation", sorted(BPSC))
@pytest.mark.parametrize("code_rate", sorted(RATE))
def test_the_counts_follow_from_the_rate(constellation, code_rate):
    w = Wlan(constellation=constellation, code_rate=code_rate)
    num, den = RATE[code_rate]
    n_cbps = 48 * BPSC[constellation]
    assert w.n_bpsc == BPSC[constellation]
    assert w.n_cbps == n_cbps
    assert w.n_dbps * den == n_cbps * num
    for length in (1, 100, 1500, 4095):
        assert w.n_sym(length) == n_sym(w.n_dbps, length)


def test_defaults_and_the_annex_i_rate():
    w = Wlan()
    assert (w.n_bpsc, w.n_dbps) == (4, 96)  # 16-QAM, rate 1/2
    # 16-QAM at 3/4 is 36 Mb/s: 144 data bits per 4 us symbol, and a
    # 100-octet PSDU needs 6 of them
    w = Wlan(constellation="qam16", code_rate="r34")
    assert w.n_dbps == 144
    assert w.n_sym(100) == 6


def test_n_sym_refuses_a_length_outside_the_psdu_range():
    w = Wlan(constellation="qam16", code_rate="r34")
    assert w.n_sym(0) == 0
    assert w.n_sym(4095) > 0
    assert w.n_sym(4096) == 0


@pytest.mark.parametrize(
    "kwargs",
    [{"constellation": "qam256"}, {"code_rate": "r45"}, {"constellation": ""}],
)
def test_create_refuses_a_value_outside_its_choices(kwargs):
    with pytest.raises(ValueError):
        Wlan(**kwargs)


def test_reset_context_manager_and_destroy():
    w = Wlan(constellation="qpsk", code_rate="r34")
    w.reset()  # the rate is configuration: nothing to reset
    assert (w.n_bpsc, w.n_dbps) == (2, 72)
    with Wlan() as obj:
        assert obj.n_sym(10) > 0
    w.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        w.n_sym(10)


# The data rate at 20 MHz, Table 17-6's first column: the pair is
# (constellation, code_rate) and the value is what the table prints.
_TABLE_17_6 = {
    ("bpsk", "r12"): 6,
    ("bpsk", "r34"): 9,
    ("qpsk", "r12"): 12,
    ("qpsk", "r34"): 18,
    ("qam16", "r12"): 24,
    ("qam16", "r34"): 36,
    ("qam64", "r23"): 48,
    ("qam64", "r34"): 54,
}


@pytest.mark.parametrize("constellation", ["bpsk", "qpsk", "qam16", "qam64"])
@pytest.mark.parametrize("code_rate", ["r12", "r23", "r34", "r56"])
def test_rate_mbps_is_the_printed_rate_or_zero(constellation, code_rate):
    """Eight of the sixteen pairs have a Table 17-6 row; the rest read 0."""
    w = Wlan(constellation=constellation, code_rate=code_rate)
    want = _TABLE_17_6.get((constellation, code_rate), 0)
    assert w.rate_mbps == want
    if want:
        assert w.rate_mbps * 4 == w.n_dbps  # the 4 us symbol, independently


def test_rate_mbps_has_eight_distinct_rows():
    got = {
        Wlan(constellation=c, code_rate=r).rate_mbps for c, r in _TABLE_17_6
    }
    assert got == {6, 9, 12, 18, 24, 36, 48, 54}
