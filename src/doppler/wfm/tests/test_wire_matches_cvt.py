"""The wire's integer codes ARE the cvt converters' output.

doppler#1117: ``wfm`` carried three private float->int copies -- one in the
NATS sink, one in the file writer, one (the inverse) in the reader. All three
truncated toward zero at a full scale of 2^(N-1)-1 where every ``cvt``
converter rounds to nearest at 2^(N-1), which cost 6.0 dB of quantisation
noise on every integer wire type and left ``Reader`` and ``I16ToF32``
disagreeing on 2.3% of int16 codes.

The fix routes all three through the converters. These tests are what keeps
them there: they compare bytes actually written to, and read from, a file
against the converter applied to the same floats, so a reintroduced private
copy fails here with a named cause rather than as an unexplained golden-hash
drift.

The oracle is deliberately the *shipped* converter rather than a numpy
expression -- a hand-written expectation here would be a fourth copy of the
very rule under test, and would drift with the other three.
"""

from __future__ import annotations

import numpy as np
import pytest

from doppler.cvt import (
    F32ToI8,
    F32ToI16,
    F32ToI32,
    I8ToF32,
    I16ToF32,
    I32ToF32,
)
from doppler.wfm import Reader, Writer

# (sample_type, numpy wire dtype, forward converter, inverse converter)
WIRE = [
    ("ci8", "<i1", F32ToI8, I8ToF32),
    ("ci16", "<i2", F32ToI16, I16ToF32),
    ("ci32", "<i4", F32ToI32, I32ToF32),
]


def _signal(n: int = 4096) -> np.ndarray:
    """A spread of magnitudes, including values that straddle a code edge.

    Uniform on (-1, 1) rather than a tone: a tone visits few distinct
    fractions of an LSB, and the difference between rounding and truncating
    only shows on samples that fall between codes.
    """
    rng = np.random.default_rng(1117)
    re = rng.uniform(-1.0, 1.0, n)
    im = rng.uniform(-1.0, 1.0, n)
    return (re + 1j * im).astype(np.complex64)


@pytest.mark.parametrize(
    "stype,dtype,fwd,_inv", WIRE, ids=[w[0] for w in WIRE]
)
def test_written_codes_are_the_converters_output(
    tmp_path, stype, dtype, fwd, _inv
):
    """Writer's bytes == the forward converter over the same floats."""
    x = _signal()
    path = tmp_path / f"cap.{stype}"

    w = Writer(path, file_type="raw", sample_type=stype, fs=1e6)
    w.write(x)
    w.close()

    wire = np.fromfile(path, dtype=dtype)
    # Interleaved I/Q: the converter sees the components in the same order.
    components = np.empty(2 * x.size, dtype=np.float32)
    components[0::2] = x.real
    components[1::2] = x.imag

    assert np.array_equal(wire, fwd().steps(components)), (
        f"{stype}: the writer's codes are not {fwd.__name__}'s -- a private "
        f"quantiser has come back (doppler#1117)"
    )


@pytest.mark.parametrize(
    "stype,dtype,_fwd,inv", WIRE, ids=[w[0] for w in WIRE]
)
def test_read_samples_are_the_converters_output(
    tmp_path, stype, dtype, _fwd, inv
):
    """Reader's floats == the inverse converter over the same codes."""
    rng = np.random.default_rng(7)
    info = np.iinfo(dtype)
    codes = rng.integers(info.min, info.max + 1, 2048, dtype=dtype)
    path = tmp_path / f"raw.{stype}"
    codes.tofile(path)

    got = Reader(path, sample_type=stype).read(codes.size // 2)
    want = inv().steps(codes)

    assert np.array_equal(got.real, want[0::2]), (
        f"{stype}: I differs from {inv.__name__}"
    )
    assert np.array_equal(got.imag, want[1::2]), (
        f"{stype}: Q differs from {inv.__name__}"
    )


@pytest.mark.parametrize(
    "stype,dtype,fwd,_inv", WIRE, ids=[w[0] for w in WIRE]
)
def test_rounding_not_truncation(tmp_path, stype, dtype, fwd, _inv):
    """Half an LSB rounds up. Truncation -- the old behaviour -- gives 0.

    The narrow assertion the 6 dB rests on, stated so a regression names
    itself instead of showing up as a raised noise floor somewhere else.
    """
    full = float(np.iinfo(dtype).max) + 1.0  # 2^(N-1)
    half_lsb = np.float32(0.5 / full)
    x = np.array([half_lsb + 0j, -half_lsb + 0j], dtype=np.complex64)

    path = tmp_path / f"half.{stype}"
    w = Writer(path, file_type="raw", sample_type=stype, fs=1e6)
    w.write(x)
    w.close()

    wire = np.fromfile(path, dtype=dtype)
    assert wire[0] == 1, (
        f"{stype}: +0.5 LSB gave {wire[0]}, not 1 (truncating?)"
    )
    assert wire[2] == -1, (
        f"{stype}: -0.5 LSB gave {wire[2]}, not -1 (truncating?)"
    )


@pytest.mark.parametrize(
    "stype,dtype,_fwd,_inv", WIRE, ids=[w[0] for w in WIRE]
)
def test_full_scale_is_a_power_of_two(tmp_path, stype, dtype, _fwd, _inv):
    """Half scale round-trips EXACTLY, which 2^(N-1)-1 cannot do.

    At a full scale of 32767, 0.5 comes back as 0.5000153. This is the
    assertion that pins the constant rather than the rounding.
    """
    x = np.array([0.5 + 0.25j, -0.5 - 0.25j], dtype=np.complex64)
    path = tmp_path / f"dyadic.{stype}"
    w = Writer(path, file_type="raw", sample_type=stype, fs=1e6)
    w.write(x)
    w.close()

    got = Reader(path, sample_type=stype).read(2)
    assert np.array_equal(got, x), f"{stype}: dyadic values did not survive"


def test_plus_one_saturates_and_minus_one_does_not(tmp_path):
    """+1.0 lands one past the maximum by construction; -1.0 fits exactly.

    The asymmetry is two's complement's, and the converters' saturation is
    what makes it safe -- so it is asserted rather than left implicit.
    """
    x = np.array([1.0 + 1.0j, -1.0 - 1.0j], dtype=np.complex64)
    path = tmp_path / "extremes.ci16"
    w = Writer(path, file_type="raw", sample_type="ci16", fs=1e6)
    w.write(x)
    w.close()

    wire = np.fromfile(path, dtype="<i2")
    assert wire[0] == 32767, "+1.0 must saturate to INT16_MAX, not wrap"
    assert wire[2] == -32768, "-1.0 is representable and must not be clamped"
