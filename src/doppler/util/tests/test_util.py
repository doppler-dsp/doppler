import pytest

from doppler.util import square_clip


def test_clamps_both_components():
    # Each component is independently limited to [-lin, lin].
    y = square_clip(5.0 + 1.0j, 2.0)
    assert y.real == 2.0  # 5 -> 2
    assert y.imag == 1.0  # 1 is within [-2, 2], unchanged


def test_negative_side():
    y = square_clip(-9.0 - 3.0j, 4.0)
    assert y.real == -4.0
    assert y.imag == -3.0


def test_passthrough():
    # Both components inside the box -> returned unchanged.
    y = square_clip(1.5 - 0.5j, 2.0)
    assert y.real == pytest.approx(1.5)
    assert y.imag == pytest.approx(-0.5)


def test_is_square_not_circular():
    # |y| exceeds lin but the imaginary part is inside the box, so only
    # the real part clamps. A circular (magnitude) clip would scale both
    # components and shrink the imaginary part.
    y = square_clip(10.0 + 1.0j, 3.0)
    assert y.real == 3.0
    assert y.imag == 1.0
