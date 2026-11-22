"""The composite's stages on the Python face (doppler#3175 step C).

``Wlan.signal_bits``, ``data_bits``, ``map``, ``points`` and ``mpdu`` bind
the C stage functions over the object's rate and the
standard tables; their numbers are pinned in
``native/tests/wlan/test_wlan_core.c``. These hold the binding to the
standard's own structure with oracles of their own (the FCS is zlib's CRC-32,
the scrambler is the polynomial ``x^7 + x^4 + 1`` run here, the SIGNAL field is
Figure 17-5's layout), and hold every refusal's sentence.
"""

import zlib

import numpy as np
import pytest

from doppler.wlan import Wlan

PAIRS = [
    ("bpsk", "r12"),
    ("bpsk", "r34"),
    ("qpsk", "r12"),
    ("qpsk", "r34"),
    ("qam16", "r12"),
    ("qam16", "r34"),
    ("qam64", "r23"),
    ("qam64", "r34"),
]


def _rates():
    """The eight pairs of Table 17-6, found by the table's own column."""
    return [
        Wlan(constellation=c, code_rate=r)
        for c, r in PAIRS
        if Wlan(constellation=c, code_rate=r).rate_mbps
    ]


def _scrambler(state: int, n: int) -> np.ndarray:
    """x^7 + x^4 + 1 from a register state (x1 the most significant bit),
    the standard's recurrence s[n] = s[n-7] xor s[n-4]."""
    reg = [(state >> (6 - i)) & 1 for i in range(7)]  # x1..x7
    out = []
    for _ in range(n):
        b = reg[6] ^ reg[3]
        out.append(b)
        reg = [b, *reg[:6]]
    return np.array(out, dtype=np.uint8)


# ── signal_bits ──────────────────────────────────────────────────────────


@pytest.mark.parametrize("length", [1, 100, 1500, 4095])
def test_the_signal_field_has_figure_17_5s_layout(length):
    for w in _rates():
        b = w.signal_bits(length)
        assert b.dtype == np.uint8 and b.shape == (24,)
        assert b[4] == 0  # reserved
        field = sum(int(b[5 + i]) << i for i in range(12))  # lsb first
        assert field == length
        assert b[:17].sum() % 2 == b[17]  # even parity over the first 17
        assert not b[18:].any()  # six tail bits


def test_each_rate_names_itself_in_the_signal_field():
    rates = {tuple(w.signal_bits(100)[:4]) for w in _rates()}
    assert len(rates) == len(_rates()) == 8


@pytest.mark.parametrize("length", [0, 4096])
def test_signal_bits_refuses_a_length_outside_the_psdu(length):
    with pytest.raises(ValueError, match="the PSDU is 1 to 4095 octets"):
        Wlan().signal_bits(length)


def test_signal_bits_refuses_a_rate_with_no_row():
    w = Wlan(constellation="bpsk", code_rate="r56")
    assert w.rate_mbps == 0
    with pytest.raises(ValueError, match="no SIGNAL RATE bits"):
        w.signal_bits(100)


# ── mpdu: the oracle is zlib's CRC-32 ─────────────────────


def _fcs(octets: bytes) -> bytes:
    return zlib.crc32(octets).to_bytes(4, "little")


@pytest.mark.parametrize("n_hdr, n_body", [(24, 10), (30, 1), (24, 1500)])
def test_mpdu_is_header_body_and_the_fcs(n_hdr, n_body):
    rng = np.random.default_rng(n_hdr + n_body)
    hdr = rng.integers(0, 256, n_hdr, dtype=np.uint8)
    body = rng.integers(0, 256, n_body, dtype=np.uint8)
    got = Wlan().mpdu(body, hdr)
    want = bytes(hdr) + bytes(body)
    assert bytes(got) == want + _fcs(want)


def test_mpdu_with_no_header_is_the_body_and_its_fcs():
    """The FCS wrap of C (``WLAN_MAC_FCS``): the body is an MPDU that
    lacks its FCS."""
    body = np.arange(40, dtype=np.uint8)
    got = Wlan().mpdu(body)
    assert bytes(got) == bytes(body) + _fcs(bytes(body))
    np.testing.assert_array_equal(Wlan().mpdu(body, None), got)


def test_an_empty_header_is_no_header():
    """In the data wrap an empty header gives the FCS wrap's octets, so the
    two spellings are one result and the method infers nothing C would
    dispute."""
    body = np.arange(40, dtype=np.uint8)
    np.testing.assert_array_equal(
        Wlan().mpdu(body, np.zeros(0, np.uint8)), Wlan().mpdu(body)
    )


def test_the_header_is_what_the_split_follows():
    """The same octets, split or not, are the same frame."""
    octets = np.arange(40, dtype=np.uint8)
    np.testing.assert_array_equal(
        Wlan().mpdu(octets[24:], octets[:24]), Wlan().mpdu(octets)
    )


def test_a_flipped_bit_changes_the_fcs():
    """The control: the oracle is sensitive to one octet."""
    a = np.arange(40, dtype=np.uint8)
    b = a.copy()
    b[7] ^= 1
    assert bytes(Wlan().mpdu(a)[-4:]) != bytes(Wlan().mpdu(b)[-4:])


def test_mpdu_refuses_what_exceeds_a_psdu():
    with pytest.raises(ValueError, match="1 to 4095 octets"):
        Wlan().mpdu(np.zeros(4072, np.uint8), np.zeros(24, np.uint8))
    with pytest.raises(ValueError, match="1 to 4095 octets"):
        Wlan().mpdu(np.zeros(4092, np.uint8))


# ── data_bits: undo the scrambler, and the field is the standard's ───────


@pytest.mark.parametrize("state", [1, 93, 127])
def test_data_bits_are_service_psdu_tail_and_pad_scrambled(state):
    psdu = np.arange(37, dtype=np.uint8)
    for w in _rates():
        bits = w.data_bits(psdu, state)
        assert bits.shape == (w.n_sym(len(psdu)) * w.n_dbps,)
        plain = bits ^ _scrambler(state, len(bits))
        n_psdu = 8 * len(psdu)
        assert not plain[:16].any()  # SERVICE
        got = np.packbits(
            plain[16 : 16 + n_psdu].reshape(-1, 8)[:, ::-1], axis=1
        ).ravel()  # each octet least significant bit first
        np.testing.assert_array_equal(got, psdu)
        assert not plain[16 + n_psdu + 6 :].any()  # pad
        # the six tail bits are zero after scrambling, so the stream there is
        # the scrambler's own, not its XOR with zero
        tail = slice(16 + n_psdu, 16 + n_psdu + 6)
        assert not bits[tail].any()


def test_another_state_scrambles_differently():
    psdu = np.arange(37, dtype=np.uint8)
    w = Wlan()
    assert not np.array_equal(w.data_bits(psdu, 93), w.data_bits(psdu, 94))


@pytest.mark.parametrize("state", [0, 128])
def test_data_bits_refuses_a_scrambler_state_outside_the_register(state):
    with pytest.raises(ValueError, match="the scrambler state is 1 to 127"):
        Wlan().data_bits(np.arange(10, dtype=np.uint8), state)


# ── map and points ───────────────────────────────────────────────────────


@pytest.mark.parametrize("constellation", ["bpsk", "qpsk", "qam16", "qam64"])
def test_map_covers_every_group_once_at_unit_power(constellation):
    w = Wlan(constellation=constellation, code_rate="r12")
    n = w.n_bpsc
    groups = np.array(
        [[(g >> (n - 1 - i)) & 1 for i in range(n)] for g in range(1 << n)],
        dtype=np.uint8,
    )
    pts = w.map(groups.ravel())
    assert pts.dtype == np.complex64 and pts.shape == (1 << n,)
    assert len(set(np.round(pts, 5).tolist())) == 1 << n
    assert np.mean(np.abs(pts) ** 2) == pytest.approx(1.0, rel=1e-5)


def test_bpsk_maps_zero_below_and_one_above():
    pts = Wlan(constellation="bpsk", code_rate="r12").map(
        np.array([0, 1], dtype=np.uint8)
    )
    np.testing.assert_allclose(pts, [-1, 1])


@pytest.mark.parametrize("n_bits", [0, 7])
def test_map_refuses_a_partial_group(n_bits):
    w = Wlan(constellation="qam16", code_rate="r12")
    with pytest.raises(ValueError, match="whole number of groups"):
        w.map(np.zeros(n_bits, np.uint8))


@pytest.mark.parametrize("length", [1, 37, 100, 1500])
def test_points_are_signal_then_data_at_the_layouts_count(length):
    psdu = np.arange(length, dtype=np.uint16).astype(np.uint8)
    for w in _rates():
        pts = w.points(psdu, 93)
        assert pts.dtype == np.complex64
        assert pts.shape == (48 + 48 * w.n_sym(length),)
        assert pts.shape == (w.points_max_out(length),)
        head = pts[:48]  # SIGNAL: BPSK, real, unit magnitude
        np.testing.assert_allclose(np.abs(head), 1.0, rtol=1e-6)
        np.testing.assert_allclose(np.imag(head), 0.0, atol=1e-6)


def test_a_different_psdu_or_state_changes_the_points():
    w = Wlan(constellation="qam16", code_rate="r34")
    a = np.arange(100, dtype=np.uint8)
    b = a.copy()
    b[3] ^= 1
    assert not np.array_equal(w.points(a, 93), w.points(b, 93))
    assert not np.array_equal(w.points(a, 93), w.points(a, 92))


@pytest.mark.parametrize("state", [0, 128])
def test_points_refuses_a_state_outside_the_register(state):
    with pytest.raises(ValueError, match="the scrambler state is 1 to 127"):
        Wlan().points(np.arange(10, dtype=np.uint8), state)


def test_points_refuses_a_rate_with_no_row():
    w = Wlan(constellation="bpsk", code_rate="r56")
    with pytest.raises(ValueError, match="no SIGNAL RATE bits"):
        w.points(np.arange(10, dtype=np.uint8), 93)
