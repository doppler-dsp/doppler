"""Domain tests for doppler.arith fixed-point arithmetic functions."""

import numpy as np

from doppler.arith import (
    AccQ8,
    AccQ15,
    add_q8,
    add_q15,
    dot_q8,
    dot_q15,
    mul_q8,
    mul_q15,
    shl_i64,
    shl_q15,
    shr_i64,
    shr_q15,
    sub_q15,
)

Q15_MAX = 32767
Q15_MIN = -32768
Q8_MAX = 127
Q8_MIN = -128

# ------------------------------------------------------------------ Q15 add --


def test_add_q15_basic():
    a = np.array([100, 200, -300], dtype=np.int16)
    b = np.array([50, -50, 100], dtype=np.int16)
    out = add_q15(a, b)
    np.testing.assert_array_equal(out, [150, 150, -200])


def test_add_q15_positive_saturation():
    a = np.array([Q15_MAX], dtype=np.int16)
    b = np.array([1], dtype=np.int16)
    assert add_q15(a, b)[0] == Q15_MAX


def test_add_q15_negative_saturation():
    a = np.array([Q15_MIN], dtype=np.int16)
    b = np.array([-1], dtype=np.int16)
    assert add_q15(a, b)[0] == Q15_MIN


# ------------------------------------------------------------------ Q15 sub --


def test_sub_q15_basic():
    a = np.array([500, -100], dtype=np.int16)
    b = np.array([200, 200], dtype=np.int16)
    np.testing.assert_array_equal(sub_q15(a, b), [300, -300])


def test_sub_q15_saturation():
    a = np.array([Q15_MIN], dtype=np.int16)
    b = np.array([1], dtype=np.int16)
    assert sub_q15(a, b)[0] == Q15_MIN


# ------------------------------------------------------------------ Q15 mul --


def test_mul_q15_half():
    # 0.5 in Q15 = 16384; 0.5 × 0.5 = 0.25 → 8192
    a = np.array([16384], dtype=np.int16)
    b = np.array([16384], dtype=np.int16)
    assert mul_q15(a, b)[0] == 8192


def test_mul_q15_unity():
    # 32767 × 32767 + 16384 = 1_073_692_673 >> 15 = 32766 (no saturation)
    a = np.array([Q15_MAX], dtype=np.int16)
    b = np.array([Q15_MAX], dtype=np.int16)
    assert mul_q15(a, b)[0] == 32766


def test_mul_q15_identity():
    # 1000 × 32767 + 16384 >> 15 = 1000 (round-half-up preserves the value)
    a = np.array([1000, -1000], dtype=np.int16)
    b = np.full(2, Q15_MAX, dtype=np.int16)
    out = mul_q15(a, b)
    np.testing.assert_array_equal(out, [1000, -1000])


# ------------------------------------------------------------------ Q15 dot --


def test_dot_q15_known():
    a = np.array([16384, 16384], dtype=np.int16)  # [0.5, 0.5]
    b = np.array([16384, 16384], dtype=np.int16)  # [0.5, 0.5]
    # raw Q30: 16384*16384 * 2 = 536_870_912
    assert dot_q15(a, b) == 16384 * 16384 * 2


def test_dot_q15_orthogonal():
    n = 256
    a = np.ones(n, dtype=np.int16)
    b = np.zeros(n, dtype=np.int16)
    assert dot_q15(a, b) == 0


def test_dot_q15_large():
    n = 1024
    a = np.full(n, Q15_MAX, dtype=np.int16)
    b = np.full(n, Q15_MAX, dtype=np.int16)
    expected = int(Q15_MAX) * int(Q15_MAX) * n
    assert dot_q15(a, b) == expected


# ------------------------------------------------------------------ Q15 shl --


def test_shl_q15_basic():
    a = np.array([1000, -1000], dtype=np.int16)
    np.testing.assert_array_equal(shl_q15(a, 1), [2000, -2000])


def test_shl_q15_saturation():
    a = np.array([Q15_MAX], dtype=np.int16)
    assert shl_q15(a, 1)[0] == Q15_MAX  # saturated


def test_shl_q15_zero():
    a = np.array([100, -100], dtype=np.int16)
    np.testing.assert_array_equal(shl_q15(a, 0), a)


# ------------------------------------------------------------------ Q15 shr --


def test_shr_q15_basic():
    a = np.array([2000, -2000], dtype=np.int16)
    np.testing.assert_array_equal(shr_q15(a, 1), [1000, -1000])


def test_shr_q15_rounding():
    # 3 >> 1 with round-half-up: (3 + 1) >> 1 = 2
    a = np.array([3], dtype=np.int16)
    assert shr_q15(a, 1)[0] == 2


def test_shr_q15_zero():
    a = np.array([1000, -1000], dtype=np.int16)
    np.testing.assert_array_equal(shr_q15(a, 0), a)


# ------------------------------------------------------------------ Q8 add --


def test_add_q8_basic():
    a = np.array([10, 20, -30], dtype=np.int8)
    b = np.array([5, -5, 10], dtype=np.int8)
    np.testing.assert_array_equal(add_q8(a, b), [15, 15, -20])


def test_add_q8_saturation():
    a = np.array([Q8_MAX], dtype=np.int8)
    b = np.array([1], dtype=np.int8)
    assert add_q8(a, b)[0] == Q8_MAX

    a = np.array([Q8_MIN], dtype=np.int8)
    b = np.array([-1], dtype=np.int8)
    assert add_q8(a, b)[0] == Q8_MIN


# ------------------------------------------------------------------ Q8 mul --


def test_mul_q8_quarter():
    # 0.5 in Q8 = 64; 0.5 × 0.5 = 0.25 → 32
    a = np.array([64], dtype=np.int8)
    b = np.array([64], dtype=np.int8)
    assert mul_q8(a, b)[0] == 32


def test_mul_q8_negative():
    a = np.array([-64], dtype=np.int8)
    b = np.array([64], dtype=np.int8)
    assert mul_q8(a, b)[0] == -32


# ------------------------------------------------------------------ Q8 dot --


def test_dot_q8_known():
    a = np.array([64, 64], dtype=np.int8)
    b = np.array([64, 64], dtype=np.int8)
    assert dot_q8(a, b) == 64 * 64 * 2  # raw Q14: 8192


# --------------------------------------------------------------- i64 shift --


def test_shr_i64_normalise_dot():
    a = np.array([16384, 16384], dtype=np.int16)
    b = np.array([16384, 16384], dtype=np.int16)
    raw = dot_q15(a, b)  # Q30 result: 536_870_912
    # normalise back to Q15: shr by 15 → 16384
    arr = np.array([raw], dtype=np.int64)
    assert shr_i64(arr, 15)[0] == 16384


def test_shl_i64_basic():
    a = np.array([1, -1, 0], dtype=np.int64)
    out = shl_i64(a, 10)
    np.testing.assert_array_equal(out, [1024, -1024, 0])


def test_shr_i64_basic():
    a = np.array([1024, -1024], dtype=np.int64)
    np.testing.assert_array_equal(shr_i64(a, 10), [1, -1])


# ------------------------------------------------------------ AccQ15 object --


def test_acc_q15_step():
    acc = AccQ15()
    acc.step(1000)
    acc.step(-500)
    assert acc.get() == 500


def test_acc_q15_steps():
    acc = AccQ15()
    data = np.array([100, 200, 300], dtype=np.int16)
    acc.steps(data)
    assert acc.get() == 600


def test_acc_q15_dump():
    acc = AccQ15()
    acc.step(999)
    v = acc.dump()
    assert v == 999
    assert acc.get() == 0


def test_acc_q15_madd():
    a = np.array([16384, 16384], dtype=np.int16)  # [0.5, 0.5]
    b = np.array([16384, 16384], dtype=np.int16)
    acc = AccQ15()
    acc.madd(a, b)
    # should equal dot_q15(a, b)
    assert acc.dump() == dot_q15(a, b)


def test_acc_q15_madd_matches_dot():
    rng = np.random.default_rng(42)
    a = rng.integers(-32768, 32768, size=512, dtype=np.int16)
    b = rng.integers(-32768, 32768, size=512, dtype=np.int16)
    acc = AccQ15()
    acc.madd(a, b)
    assert acc.dump() == dot_q15(a, b)


# ------------------------------------------------------------ AccQ8 object --


def test_acc_q8_step():
    acc = AccQ8()
    acc.step(np.int8(10))
    acc.step(np.int8(-3))
    assert acc.get() == 7


def test_acc_q8_madd():
    a = np.array([64, 64], dtype=np.int8)
    b = np.array([64, 64], dtype=np.int8)
    acc = AccQ8()
    acc.madd(a, b)
    assert acc.dump() == dot_q8(a, b)
