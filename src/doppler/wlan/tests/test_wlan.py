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
    # zero config is 36 Mb/s, Annex I's rate: 16-QAM at 3/4, 144 data bits
    # per 4 us symbol, and a 100-octet PSDU needs 6 of them
    w = Wlan()
    assert (w.n_bpsc, w.n_dbps, w.rate_mbps) == (4, 144, 36)
    assert w.n_sym(100) == 6
    # the same object by either spelling
    for v in (Wlan(mbps=36), Wlan(constellation="qam16", code_rate="r34")):
        assert (v.n_bpsc, v.n_dbps, v.rate_mbps) == (4, 144, 36)
    # the control: v0.85.0's Wlan() was 16-QAM at 1/2, 24 Mb/s
    assert Wlan(constellation="qam16", code_rate="r12").rate_mbps == 24


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


# ── Wlan.ppdu: a whole PPDU from the object ────────────────────────────────

import re  # noqa: E402

import numpy as np  # noqa: E402

from doppler.tests._repo import repo_root  # noqa: E402

# The relative l2 tolerance the C test uses for every cross-build and
# decimation comparison (test_wlan_core.c, WLAN_FFT_TOL). Its source is
# test_fft_core.c's TOL32 = 5e-4, absolute in each of the real and imaginary
# parts on unit-scale inputs: a complex sample differs by at most
# sqrt(2) * 5e-4 = 7.1e-4; two transforms are compared, so 2 * 7.1e-4 =
# 1.41e-3 per sample at unit rms, which bounds the relative l2 error: 1.5e-3.
_TOL = 1.5e-3


def _psdu(n: int = 100) -> np.ndarray:
    return ((np.arange(n) * 37 + 11) % 256).astype(np.uint8)


def _oracle(name: str) -> np.ndarray:
    """The committed PPDU of wlan_ppdu_m1_oracle.h, which the C test holds
    dp_wlan_ppdu() to, as complex64."""
    # the oracle lives in the source tree, so this needs a checkout; the root
    # is found by walking up, which is right from the coverage harness's copy
    # of the package under build-cov/ as well (doppler.tests._repo)
    path = repo_root() / "native/tests/wlan/wlan_ppdu_m1_oracle.h"
    text = path.read_text(encoding="utf-8")
    body = re.search(name + r"\[\d+\] = \{(.*?)\};", text, re.S).group(1)
    words = np.array(
        [int(w, 16) for w in re.findall(r"0x[0-9a-f]{8}", body)],
        dtype=np.uint32,
    )
    f = words.view(np.float32)
    return (f[0::2] + 1j * f[1::2]).astype(np.complex64)


def _rel_l2(a, b) -> float:
    return float(np.linalg.norm(a - b) / np.linalg.norm(b))


def test_ppdu_is_the_c_output_for_the_36_mbps_ppdu():
    """The 36 Mb/s PPDU of the C test's committed bytes (100 octets,
    scrambler 93, window 4)."""
    w = Wlan(constellation="qam16", code_rate="r34", window=4)
    x = w.ppdu(_psdu(), 93)
    assert x.dtype == np.complex64 and x.shape == (884,)
    ref = _oracle("ORACLE_36_W4")
    assert _rel_l2(x, ref) < _TOL
    # control: the comparison can fail
    assert _rel_l2(x * np.float32(1.05), ref) > _TOL


def test_ppdu_oversampled_decimates_to_the_20_mhz_ppdu():
    """oversample=2 is the same PPDU on a grid twice as fine."""
    psdu = _psdu()
    x1 = Wlan(constellation="qam16", code_rate="r34", window=0).ppdu(psdu, 93)
    x2 = Wlan(
        constellation="qam16", code_rate="r34", window=0, oversample=2
    ).ppdu(psdu, 93)
    assert x1.shape == (880,) and x2.shape == (1760,)
    assert _rel_l2(x2[::2], x1) < _TOL
    # control: a half-coarse-sample shift of the fine grid is far over it
    assert _rel_l2(x2[1::2][:-1], x1[:-1]) > 1e-2


def test_ppdu_length_follows_window_and_oversample():
    w = Wlan(constellation="qam16", code_rate="r34", window=3, oversample=4)
    assert w.ppdu(_psdu(), 93).shape == (4 * (160 + 160 + 80 + 480) + 3,)


@pytest.mark.parametrize("oversample", [1, 2, 4, 128])
def test_the_default_window_is_the_standards_100_ns(oversample):
    """Nothing passed is the 100 ns transition, `2 * oversample` samples at
    the output rate (17.3.2.5), and 0 is the explicit rectangular choice."""
    default = Wlan(oversample=oversample)
    assert default.window == 2 * oversample
    rect = Wlan(window=0, oversample=oversample)
    assert rect.window == 0
    # the window is real: the PPDU is longer by it, and its samples differ
    xd, xr = (w.ppdu(_psdu(), 93) for w in (default, rect))
    assert xd.shape[0] - xr.shape[0] == 2 * oversample
    assert not np.array_equal(xd[: xr.shape[0]], xr)
    # control: a window of the same length asked for by count is the default
    same = Wlan(window=2 * oversample, oversample=oversample)
    np.testing.assert_array_equal(same.ppdu(_psdu(), 93), xd)


def test_window_and_oversample_read_back():
    w = Wlan(constellation="qpsk", code_rate="r12", window=5, oversample=3)
    assert (w.window, w.oversample) == (5, 3)


@pytest.mark.parametrize(
    ("window", "oversample", "argument"),
    [
        (81, 1, "window"),  # oversample good, window bad
        (161, 2, "window"),
        (10241, 128, "window"),
        (0, 0, "oversample"),  # window good, oversample bad
        (0, 129, "oversample"),
        (20000, 129, "oversample"),  # both bad: oversample is named first
    ],
)
def test_constructor_refusal_names_the_argument_that_failed(
    window, oversample, argument
):
    with pytest.raises(ValueError, match=f"^{argument} must be"):
        Wlan(window=window, oversample=oversample)


@pytest.mark.parametrize(
    ("window", "oversample"), [(0, 1), (80, 1), (160, 2), (10240, 128)]
)
def test_constructor_accepts_the_edges(window, oversample):
    w = Wlan(window=window, oversample=oversample)
    assert (w.window, w.oversample) == (window, oversample)


@pytest.mark.parametrize(
    ("n_psdu", "state", "why"),
    [
        (0, 93, "the PSDU is 1 to 4095 octets"),
        (4096, 93, "the PSDU is 1 to 4095 octets"),
        (100, 0, "the scrambler state is 1 to 127"),
        (100, 128, "the scrambler state is 1 to 127"),
    ],
)
def test_ppdu_refuses_naming_why(n_psdu, state, why):
    w = Wlan(constellation="qam16", code_rate="r34")
    with pytest.raises(ValueError, match=why):
        w.ppdu(_psdu(n_psdu) if n_psdu else np.zeros(0, np.uint8), state)


def test_ppdu_refuses_a_short_output_buffer_and_leaves_it():
    w = Wlan(constellation="qam16", code_rate="r34", window=0)
    out = np.full(879, 123 + 0j, dtype=np.complex64)
    with pytest.raises(ValueError, match="need >= 880"):
        w.ppdu(_psdu(), 93, out=out)
    assert np.all(out == 123)


def test_a_pair_with_no_table_row_cannot_be_sent_and_says_so():
    """BPSK 2/3 is one of the eight pairs Table 17-6 does not list."""
    w = Wlan(constellation="bpsk", code_rate="r23")
    assert w.rate_mbps == 0
    assert w.ppdu_max_out(100) == 0
    with pytest.raises(ValueError, match="no SIGNAL RATE bits"):
        w.ppdu(_psdu(), 93)
    # the rate is named first, whatever else is wrong
    with pytest.raises(ValueError, match="no SIGNAL RATE bits"):
        w.ppdu(_psdu(), 0)


@pytest.mark.parametrize("n_psdu", [1, 4095])
def test_ppdu_takes_the_psdu_length_edges(n_psdu):
    """4095 octets at 6 Mb/s is the layout's largest PPDU."""
    w = Wlan(constellation="bpsk", code_rate="r12", window=0)
    x = w.ppdu(_psdu(n_psdu), 93)
    assert x.shape == (w.ppdu_max_out(n_psdu),)
    assert x.shape == (160 + 160 + 80 + 80 * w.n_sym(n_psdu),)


@pytest.mark.parametrize("oversample", [1, 2])
@pytest.mark.parametrize("n_psdu", [1, 100, 1500])
def test_ppdu_max_out_is_the_length_ppdu_returns(n_psdu, oversample):
    w = Wlan(
        constellation="qam16",
        code_rate="r34",
        window=3,
        oversample=oversample,
    )
    assert w.ppdu_max_out(n_psdu) == w.ppdu(_psdu(n_psdu), 93).shape[0]
