import pytest

from doppler.track import MpskReceiverR


def test_create():
    obj = MpskReceiverR(
        4,
        16.0,
        4,
        "iandd",
        0.35,
        8,
        0.01,
        0.707,
        0.01,
        0,
        0.5,
        0.0,
        100,
        0,
        1024,
    )
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with MpskReceiverR(
        4,
        16.0,
        4,
        "iandd",
        0.35,
        8,
        0.01,
        0.707,
        0.01,
        0,
        0.5,
        0.0,
        100,
        0,
        1024,
    ):
        pass


def test_destroy():
    obj = MpskReceiverR(
        4,
        16.0,
        4,
        "iandd",
        0.35,
        8,
        0.01,
        0.707,
        0.01,
        0,
        0.5,
        0.0,
        100,
        0,
        1024,
    )
    obj.destroy()


def test_defaults_construct_and_clear_the_sps_bound():
    """The default `sps` exists to clear `sps > 2 * m_out`.

    `MpskReceiverR` needs `sps > 2 * m_out` (the cascade behind the R2C
    halfband runs at twice the overall rate, and `Ddcr` needs that below
    0.5), so this type's two defaults are coupled in a way the complex
    twin's are not: raising `m_out` to 8 is what forces `sps` to 32.  A
    regression here does not degrade quietly -- the no-argument
    constructor stops working at all -- so pin both, and pin the bound
    that ties them together.
    """
    rx = MpskReceiverR()
    assert rx.m == 4 and rx.m_out == 8 and rx.sps == 32.0
    assert rx.sps > 2 * rx.m_out  # the constraint the default must clear

    # One notch below the bound is a ValueError, not a silent re-plan:
    # 2 * m_out is excluded (strictly greater), so 16.0 must fail at 8.
    with pytest.raises(ValueError):
        MpskReceiverR(m=4, sps=16.0, m_out=8)
