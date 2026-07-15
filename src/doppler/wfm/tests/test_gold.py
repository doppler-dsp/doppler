"""Integration tests for the Gold (CCSDS command-link Gold code) generator.

Covers construction validation, the CCSDS 415.0-G-1 5.2.2.4 worked example
(Figure 5-2, Code #365), the balance property, the three-valued Gold
autocorrelation/cross-correlation set {-1, -65, 63}, reset reproducibility,
and the lifecycle surface.
"""

import numpy as np
import pytest

from doppler.wfm import Gold

SF = 1023  # 2**10 - 1

# CCSDS-fixed Register A / Register B taps (Figure 5-1) and Register B's
# fixed initial value; these are Gold()'s own defaults.
TAPS_A = 934
TAPS_B = 567
SEED_B = 73
SEED_A_EXAMPLE = 350  # Figure 5-2 worked example (PN Code Library Code #365)
SEED_A_OTHER = 595  # a different, arbitrary nonzero family member


def _xcorr(x, y):
    """Circular periodic cross-correlation of two {0,1} chip arrays,
    returning a length-SF array of the +-1-mapped correlation values."""
    xs = 1 - 2 * x.astype(np.int64)
    ys = 1 - 2 * y.astype(np.int64)
    return np.array([int(np.dot(xs, np.roll(ys, -k))) for k in range(SF)])


def test_create():
    assert Gold() is not None


def test_context_manager():
    with Gold():
        pass


def test_destroy():
    Gold().destroy()


def test_construction_invalid():
    # Matches PN's own convention (see CLAUDE.md's "Known post-apply
    # patches"): an invalid constructor arg maps to gold_create() -> NULL,
    # surfaced as MemoryError by the generated binding.
    with pytest.raises(MemoryError):
        Gold(seed_a=0)
    with pytest.raises(MemoryError):
        Gold(seed_b=0)
    with pytest.raises(MemoryError):
        Gold(length=0)
    with pytest.raises(MemoryError):
        Gold(length=65)


def test_generate_is_binary():
    chips = np.asarray(Gold().generate(2 * SF))
    assert chips.dtype == np.uint8
    assert set(np.unique(chips).tolist()) <= {0, 1}


def test_period_is_1023():
    chips = np.asarray(Gold().generate(2 * SF))
    assert np.array_equal(chips[:SF], chips[SF : 2 * SF])


def test_ccsds_worked_example_code_365():
    """CCSDS 415.0-G-1 Figure 5-2: Register A initial value 0111101010
    (PN Code Library Table 1, Code Number 365) against the CCSDS-fixed
    Register B. Defaults reproduce this example exactly."""
    chips = np.asarray(Gold().generate(SF))
    expected_first_15 = [0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1]
    assert chips[:15].tolist() == expected_first_15


def test_balanced_code():
    """CCSDS 415.0-G-1 5.2.2.2(c): the Code #365 example is one of the 767
    balanced codes -- 512 ones and 511 zeros."""
    chips = np.asarray(Gold().generate(SF))
    assert int(chips.sum()) == 512
    assert int((1 - chips).sum()) == 511


def test_three_valued_autocorrelation():
    """A genuine Gold preferred pair has a strict three-valued periodic
    autocorrelation set {-1, -65, 63} (t(10) = 2**6 + 1 = 65) off-peak."""
    chips = np.asarray(Gold().generate(SF))
    acorr = _xcorr(chips, chips)
    assert acorr[0] == SF
    assert set(acorr[1:].tolist()) <= {-1, -65, 63}


def test_three_valued_cross_correlation():
    """Cross-correlation between two distinct family members (different
    Register-A seeds) is also confined to {-1, -65, 63}."""
    c1 = np.asarray(Gold(seed_a=SEED_A_EXAMPLE).generate(SF))
    c2 = np.asarray(Gold(seed_a=SEED_A_OTHER).generate(SF))
    xcorr = _xcorr(c1, c2)
    assert set(xcorr.tolist()) <= {-1, -65, 63}


def test_different_seed_a_selects_different_family_member():
    a = np.asarray(Gold(seed_a=SEED_A_EXAMPLE).generate(SF))
    b = np.asarray(Gold(seed_a=SEED_A_OTHER).generate(SF))
    assert not np.array_equal(a, b)


def test_reset_reproduces():
    g = Gold()
    a = np.asarray(g.generate(64)).copy()
    g.reset()
    assert np.array_equal(a, np.asarray(g.generate(64)))


def test_generate_out_writes_into_callers_buffer():
    g = Gold()
    out = np.zeros(max(g.generate_max_out(), 100), dtype=np.uint8)
    y = g.generate(100, out=out)
    assert np.shares_memory(y, out)


def test_generate_out_undersized_raises():
    g = Gold()
    with pytest.raises(ValueError):
        g.generate(100, out=np.zeros(1, dtype=np.uint8))


def test_state_roundtrip():
    a = Gold()
    a.generate(17)  # advance mid-stream, off any epoch boundary
    blob = a.get_state()
    ref = np.asarray(a.generate(64)).copy()

    b = Gold()
    b.set_state(blob)
    got = np.asarray(b.generate(64))
    assert np.array_equal(ref, got)


def test_state_roundtrip_rejects_clobbered_blob():
    a = Gold()
    blob = bytearray(a.get_state())
    blob[0] ^= 0xFF
    with pytest.raises(ValueError):
        a.set_state(bytes(blob))


def test_state_roundtrip_rejects_wrong_size():
    a = Gold()
    with pytest.raises(ValueError):
        a.set_state(b"\x00" * 4)


def test_state_roundtrip_rejects_non_bytes():
    a = Gold()
    with pytest.raises(TypeError):
        a.set_state("not bytes")
