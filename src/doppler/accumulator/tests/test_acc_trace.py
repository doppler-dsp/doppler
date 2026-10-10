import struct

import numpy as np
import pytest

from doppler.accumulator import AccTrace


def test_create_props():
    a = AccTrace(n=8, mode="mean")
    assert a.n == 8
    assert a.count == 0
    assert a.value() is None  # None before any accumulate


def test_mean():
    a = AccTrace(n=4, mode="mean")
    a.accumulate(np.array([1, 3, 5, 7], dtype=np.float32))
    a.accumulate(np.array([3, 5, 7, 9], dtype=np.float32))
    v = a.value()
    assert v.dtype == np.float32
    np.testing.assert_allclose(v, [2, 4, 6, 8], rtol=1e-6)
    assert a.count == 2


def test_maxhold_minhold():
    for mode, want in [("maxhold", [4, 5, 6]), ("minhold", [1, 3, 2])]:
        a = AccTrace(n=3, mode=mode)
        a.accumulate(np.array([1, 5, 2], dtype=np.float32))
        a.accumulate(np.array([4, 3, 6], dtype=np.float32))
        np.testing.assert_allclose(a.value(), want, rtol=1e-6)


def test_exp():
    a = AccTrace(n=2, mode="exp", alpha=0.5)
    a.accumulate(np.array([10, 20], dtype=np.float32))  # seed
    a.accumulate(np.array([2, 4], dtype=np.float32))  # 0.5*p + 0.5*acc
    np.testing.assert_allclose(a.value(), [6, 12], rtol=1e-6)


def test_reset():
    a = AccTrace(n=4, mode="mean")
    a.accumulate(np.ones(4, dtype=np.float32))
    a.reset()
    assert a.count == 0
    assert a.value() is None


def test_mode_property():
    assert AccTrace(n=4, mode="maxhold").mode == 2


def test_alpha_writable():
    a = AccTrace(n=4, mode="exp", alpha=0.1)
    a.alpha = 0.25
    assert abs(a.alpha - 0.25) < 1e-9


BAD_ALPHAS = [0.0, -0.5, 1.5, float("nan")]


@pytest.mark.parametrize("alpha", BAD_ALPHAS)
def test_exp_refuses_alpha_outside_unit_interval(alpha):
    # Outside (0, 1] the EMA is not an average: 0 never leaves the first
    # frame, -0.5 reads negative power, 1.5 saturates to pass-through, NaN
    # poisons every bin. create() refuses, and the declared create_error
    # says why (#1986) -- a bare NULL used to read as MemoryError.
    with pytest.raises(ValueError, match="in exp mode 0 < alpha <= 1"):
        AccTrace(n=4, mode="exp", alpha=alpha)


@pytest.mark.parametrize("n", [0, 2**62])  # empty; n doubles past a size_t
def test_create_refuses_a_trace_it_cannot_size(n):
    with pytest.raises(ValueError, match=r"n >= 1 with n \* 8 bytes"):
        AccTrace(n=n)


@pytest.mark.parametrize("alpha", [1.0, 0.25, 1e-9])
def test_exp_accepts_alpha_inside_unit_interval(alpha):
    # The refusal's precondition: the nearest valid values still build.
    assert AccTrace(n=4, mode="exp", alpha=alpha).alpha == alpha


@pytest.mark.parametrize("mode", ["mean", "maxhold", "minhold"])
@pytest.mark.parametrize("alpha", BAD_ALPHAS)
def test_modes_that_never_read_alpha_accept_any(mode, alpha):
    a = AccTrace(n=4, mode=mode, alpha=alpha)
    a.alpha = alpha
    assert a.alpha == alpha or (np.isnan(alpha) and np.isnan(a.alpha))


@pytest.mark.parametrize("alpha", BAD_ALPHAS)
def test_exp_setter_keeps_alpha_on_refusal(alpha):
    # The setter applies create()'s rule in C (dp_acc_trace_set_alpha): a
    # refused value leaves alpha as it was, so the trace stays an average.
    # It does so SILENTLY: jm cannot yet render a property setter that
    # raises (just-makeit#2182). When it can, this becomes pytest.raises
    # (#1987).
    a = AccTrace(n=2, mode="exp", alpha=0.5)
    a.alpha = alpha
    assert a.alpha == 0.5
    a.accumulate(np.array([10.0, 20.0], dtype=np.float32))
    a.accumulate(np.array([2.0, 4.0], dtype=np.float32))
    np.testing.assert_allclose(a.value(), [6.0, 12.0])


# The blob: a 16-byte dp_state_hdr_t, the u32 mode, the u64 fold count, then
# alpha.
_ALPHA_AT = 16 + 4 + 8


def test_state_carries_a_runtime_alpha():
    # alpha can change after create, so it travels in the blob (#2000). A
    # resume into an instance built with the ORIGINAL alpha -- the documented
    # path, create with the same config then set_state -- used to go on
    # averaging with that one: 2.0 against 1.2 after one more frame.
    first = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32)
    after = np.array([9.0, 7.0, 5.0, 3.0], dtype=np.float32)
    a = AccTrace(n=4, mode="exp", alpha=0.1)
    a.accumulate(first)
    a.alpha = 0.5
    b = AccTrace(n=4, mode="exp", alpha=0.1)
    b.set_state(a.get_state())
    assert b.alpha == 0.5
    a.accumulate(after)
    b.accumulate(after)
    assert np.array_equal(a.value(), b.value())


@pytest.mark.parametrize("alpha", BAD_ALPHAS)
def test_state_refuses_an_alpha_the_setter_would(alpha):
    # A blob cannot install what dp_acc_trace_set_alpha refuses, and a
    # refused blob leaves the state as it was.
    a = AccTrace(n=4, mode="exp", alpha=0.1)
    a.accumulate(np.ones(4, dtype=np.float32))
    blob = bytearray(a.get_state())
    struct.pack_into("=d", blob, _ALPHA_AT, alpha)
    b = AccTrace(n=4, mode="exp", alpha=0.3)
    b.accumulate(np.full(4, 2.0, dtype=np.float32))
    b.accumulate(np.full(4, 3.0, dtype=np.float32))  # a count unlike a's
    before = b.get_state()
    with pytest.raises(ValueError):
        b.set_state(bytes(blob))
    assert b.get_state() == before


def test_state_from_another_mode_is_refused():
    # mode is a reject key: a mean trace's blob restored into an exp instance
    # used to come back OK, with an alpha mean never checked, and the mean
    # trace went on as an EMA. Refused now, and the target is left as it was.
    src = AccTrace(n=4, mode="mean")
    src.accumulate(np.ones(4, dtype=np.float32))
    dst = AccTrace(n=4, mode="exp", alpha=0.5)
    dst.accumulate(np.full(4, 2.0, dtype=np.float32))
    before = dst.get_state()
    with pytest.raises(ValueError):
        dst.set_state(src.get_state())
    assert dst.get_state() == before
    assert dst.alpha == 0.5


def test_state_in_a_mode_that_never_reads_alpha_accepts_any():
    # The setter's rule, not a stricter one: mean ignores alpha.
    a = AccTrace(n=4, mode="mean", alpha=0.1)
    a.accumulate(np.ones(4, dtype=np.float32))
    blob = bytearray(a.get_state())
    struct.pack_into("=d", blob, _ALPHA_AT, -0.5)
    b = AccTrace(n=4, mode="mean", alpha=0.1)
    b.set_state(bytes(blob))
    assert b.alpha == -0.5


def test_bad_mode_raises():
    with pytest.raises(ValueError):
        AccTrace(n=4, mode="bogus")


def test_context_manager():
    with AccTrace(n=4, mode="mean") as a:
        a.accumulate(np.ones(4, dtype=np.float32))
        assert a.value() is not None


def test_value_out_writes_into_callers_buffer():
    a = AccTrace(n=4, mode="mean")
    a.accumulate(np.ones(4, dtype=np.float32))
    out = np.zeros(max(a.value_max_out(), a.n), dtype=np.float32)
    y = a.value(out=out)
    assert np.shares_memory(y, out)


def test_value_out_undersized_raises():
    a = AccTrace(n=4, mode="mean")
    a.accumulate(np.ones(4, dtype=np.float32))
    with pytest.raises(ValueError):
        a.value(out=np.zeros(1, dtype=np.float32))
