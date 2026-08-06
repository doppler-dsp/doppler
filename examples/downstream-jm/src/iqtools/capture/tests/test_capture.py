"""Tests for the two classes the `capture` view generates.

The point being tested is not that a file can be read -- doppler already
guarantees that, and this project would be pointless if it re-tested it. What
is tested here is what this project actually adds: that `Capture` and
`RawCapture` are two constructors over ONE C core, and that the second gets a
headerless capture right where the first cannot.

Captures are written with doppler itself, so the fixtures exercise the same
round trip a user would.
"""

import math

import numpy as np
import pytest

from doppler.util import square_clip
from doppler.wfm import Composer, Segment, Writer
from iqtools.capture import Capture, RawCapture

FS = 2.4e6
FC = 1.2e9
NUM_SAMPLES = 8192

# A QPSK scene peaks above full scale, and an integer wire type maps +/-1.0 to
# +/-max-code -- so writing it at ci16 with no backoff clips the peaks and no
# round-trip comparison can hold. `headroom` is doppler's knob for exactly
# this: a single pre-quantisation scale, so it moves the absolute level and no
# power ratio (SNR is invariant).
#
# Do NOT guess the number. doppler's writer tracks the running peak for free on
# every write, and `wfm_writer_core.h` states the remedy exactly:
#
#     peak > 1.0 => clipped, and the remedy is ceil(20*log10(peak)) dB
#
# so derive it from the data rather than hand-tuning. Guessing here first cost
# two wrong answers: 6 dB left exactly one sample of 8192 clipped, which
# surfaces as a lone 0.046 outlier against a one-LSB (2.4e-5) median error --
# a failure that does not name its own cause.


def per_axis_peak(x):
    """The peak that full scale is measured against: max(|Re|, |Im|).

    Not the complex magnitude. The saturating region is a SQUARE in the IQ
    plane -- which is not a claim made here, it is what
    `doppler.util.square_clip` implements: it clamps the real and imaginary
    parts *independently*. A sample at 0.9+0.9j has magnitude 1.27 and does
    not clip; one at 1.1+0j has magnitude 1.1 and does.
    """
    return float(np.maximum(np.abs(x.real), np.abs(x.imag)).max())


def required_headroom_db(x):
    """The exact backoff this payload needs, per the writer's own rule."""
    peak = per_axis_peak(x)
    return math.ceil(20.0 * math.log10(peak)) if peak > 1.0 else 0.0


@pytest.fixture(scope="module")
def samples():
    """A QPSK burst -- the payload every fixture below writes."""
    scene = Segment("qpsk", sps=8, snr=15, fs=FS, num_samples=NUM_SAMPLES)
    return Composer([scene]).compose()


@pytest.fixture(scope="module")
def headroom(samples):
    """dB of backoff this payload needs, and the matching linear gain."""
    db = required_headroom_db(samples)
    gain = 10.0 ** (-db / 20.0)

    # State "this does not clip" with doppler's own primitive rather than an
    # inequality restated here: square_clip IS the full-scale region, so the
    # worst sample passing through it unchanged is exactly the invariant.
    #
    # Do the scaling in complex64 first. square_clip is a CF32 kernel, so it
    # narrows whatever it is handed; comparing a float64 product against a
    # float32 return fails on the round-trip alone (~2.5e-8) and would read as
    # a clip that never happened.
    idx = np.argmax(np.maximum(np.abs(samples.real), np.abs(samples.imag)))
    worst = complex(np.complex64(samples[idx] * gain))
    assert square_clip(worst, 1.0) == worst

    return db, gain


@pytest.fixture(scope="module")
def blue(tmp_path_factory, samples, headroom):
    """A self-describing BLUE capture: carries sample type, fs and fc."""
    path = tmp_path_factory.mktemp("cap") / "capture.blue"
    with Writer(
        path,
        file_type="blue",
        sample_type="ci16",
        fs=FS,
        fc=FC,
        headroom=headroom[0],
    ) as w:
        w.write(samples)
        # The writer measured it too -- confirm the derived backoff was enough
        # instead of trusting the arithmetic.
        assert w.peak_dbfs <= 0.0
    return path


@pytest.fixture(scope="module")
def raw(tmp_path_factory, samples, headroom):
    """A headerless ci16 capture: carries nothing but the samples."""
    path = tmp_path_factory.mktemp("cap") / "capture.raw"
    with Writer(
        path, fs=1e6, file_type="raw", sample_type="ci16", headroom=headroom[0]
    ) as w:
        w.write(samples)
        assert w.peak_dbfs <= 0.0
    return path


# -- Capture: the auto-detecting constructor ---------------------------------


def test_capture_reads_metadata_from_a_self_describing_file(blue):
    cap = Capture(str(blue))
    assert cap.fs == FS
    assert cap.fc == FC
    assert cap.num_samples == NUM_SAMPLES
    assert cap.metadata_source == "file"


def test_capture_cannot_invent_metadata_a_headerless_file_lacks(raw):
    """The honest failure this project exists to make visible.

    A raw capture declares nothing, so `fs`/`fc` are defaults rather than
    readings -- and `metadata_source` is the only thing that says so.
    """
    cap = Capture(str(raw))
    assert cap.fs == 0.0
    assert cap.fc == 0.0
    assert cap.metadata_source == "none"


def test_capture_gets_the_stride_wrong_on_a_headerless_file(raw):
    """Not a bug -- a demonstration, and the reason the view exists.

    Nothing in a raw file states its sample type, so `Capture` falls back to
    cf32 and reads a ci16 capture at the wrong stride: it reports half the
    samples and never errors. This is what silently-wrong looks like.
    """
    cap = Capture(str(raw))
    assert cap.num_samples == NUM_SAMPLES // 2
    assert cap.num_samples != NUM_SAMPLES


# -- RawCapture: the view ----------------------------------------------------


def test_view_uses_the_metadata_you_supply(raw):
    cap = RawCapture(str(raw), sample_type="ci16", endian="le", fs=FS, fc=FC)
    assert cap.fs == FS
    assert cap.fc == FC
    assert cap.metadata_source == "supplied"


def test_view_reads_the_headerless_capture_correctly(raw):
    """The payoff: told the sample type, it recovers the true length."""
    cap = RawCapture(str(raw), sample_type="ci16", fs=FS)
    assert cap.num_samples == NUM_SAMPLES


def test_view_round_trips_the_samples(raw, samples, headroom):
    """Block-streamed through the view, the payload comes back intact."""
    got = []
    cap = RawCapture(str(raw), sample_type="ci16", fs=FS)
    while len(block := cap.read(4096)):
        got.append(block)
    back = np.concatenate(got)

    assert len(back) == len(samples)
    # Two lossy steps to allow for, and only two: the writer's headroom scale
    # (exact, known) and ci16's 16-bit quantisation (bounded by one LSB).
    assert np.allclose(back, samples * headroom[1], atol=2e-4)


# -- The two classes are one core --------------------------------------------


def test_both_classes_are_distinct_types_from_one_module(raw, blue):
    assert Capture is not RawCapture
    assert type(Capture(str(blue))) is not type(RawCapture(str(raw)))
    # ...but they come from the one extension module the view generated.
    assert Capture.__module__ == RawCapture.__module__


def test_the_view_shares_the_parents_methods_verbatim():
    """A view does not redeclare methods; it exposes the core's.

    If these ever diverge, the view has stopped being a second constructor and
    become a second implementation -- which is the thing to avoid.
    """
    shared = {
        "read",
        "reset",
        "destroy",
        "fs",
        "fc",
        "num_samples",
        "metadata_source",
    }
    assert shared <= set(dir(Capture))
    assert shared <= set(dir(RawCapture))


def test_reset_rewinds_without_disturbing_metadata(raw):
    cap = RawCapture(str(raw), sample_type="ci16", fs=FS)
    first = cap.read(64)
    cap.reset()
    again = cap.read(64)

    assert np.array_equal(first, again)
    assert cap.fs == FS  # metadata is not a read cursor
    assert cap.metadata_source == "supplied"


# -- Lifetime ----------------------------------------------------------------


def test_context_manager_closes(blue):
    with Capture(str(blue)) as cap:
        assert cap.num_samples == NUM_SAMPLES


def test_summary_returns_a_named_record(blue):
    """`summary()` is a `single = true` method: one call, one named record.

    The `CaptureSummary` class, its `.pyi`, and every field docstring are
    generated — no hand-written CPython. The field prose comes from the `///<`
    comments on `capture_summary_t` in `capture_summary.h`, reached across the
    `#include` in `capture_core.h` (just-makeit gh-724).
    """
    s = Capture(str(blue)).summary()

    # Named access, and it still unpacks like the tuple it subclasses.
    assert s.num_samples == NUM_SAMPLES
    assert s.fs_hz == FS
    assert s.fc_hz == FC
    n, fs, fc = s
    assert (n, fs, fc) == (NUM_SAMPLES, FS, FC)

    # The record type carries its identity and its docs at runtime, too.
    assert type(s).__name__ == "CaptureSummary"
    assert type(s).__module__ == "iqtools.capture"
    assert "Sample rate" in type(s).fs_hz.__doc__


def test_summary_reflects_what_the_view_was_told(raw):
    """A headerless capture read through the view: the record shows the
    supplied tuning, the same numbers the individual accessors report."""
    cap = RawCapture(str(raw), sample_type="ci16", fs=FS, fc=FC)
    s = cap.summary()
    assert (s.fs_hz, s.fc_hz) == (cap.fs, cap.fc)
    assert s.num_samples == cap.num_samples


def test_missing_file_raises_the_declared_error(tmp_path):
    """From `create_error = "ValueError"`, not a bare MemoryError."""
    with pytest.raises(ValueError):
        Capture(str(tmp_path / "does-not-exist.blue"))


def test_view_reports_the_same_error_for_a_missing_file(tmp_path):
    with pytest.raises(ValueError):
        RawCapture(str(tmp_path / "does-not-exist.raw"), sample_type="ci16")
