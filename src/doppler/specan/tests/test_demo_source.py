"""Unit tests for DemoSource's tone-frequency bookkeeping.

Regression coverage for gh-457: a tone set below center (negative
normalised offset) must be *reported* (``get_tones()``) at its true
negative offset, not wrapped into the near-Nyquist positive alias that
``% 1.0`` alone produces. The NCO's actual output is unaffected either
way (periodic in integer shifts of the normalised frequency) — this is
purely about what's reported back to callers/the web UI.
"""

import pytest

from doppler.specan.source import DemoSource


def _tone_freq_hz(fs: float, fn: float) -> float:
    src = DemoSource(sample_rate=fs, tone_freq=0.0)
    src.set_tone_freq(fn)
    tones = src.get_tones()
    assert len(tones) == 1
    return tones[0]["freq_hz"]


@pytest.mark.parametrize(
    "fn",
    [-0.049, -0.25, -0.49, -1e-6],
)
def test_negative_tone_freq_reported_negative(fn):
    fs = 2.048e6
    freq_hz = _tone_freq_hz(fs, fn)
    assert freq_hz == pytest.approx(fn * fs, abs=1.0)
    assert freq_hz < 0.0


@pytest.mark.parametrize(
    "fn",
    [0.0, 1e-6, 0.25, 0.49],
)
def test_positive_tone_freq_unaffected(fn):
    fs = 2.048e6
    freq_hz = _tone_freq_hz(fs, fn)
    assert freq_hz == pytest.approx(fn * fs, abs=1.0)


def test_add_tone_reports_negative_offset():
    fs = 1.0e6
    src = DemoSource(sample_rate=fs, tone_freq=0.0)
    src.add_tone(-0.1, dbm=-10.0)
    tones = src.get_tones()
    assert len(tones) == 2
    assert tones[1]["freq_hz"] == pytest.approx(-0.1 * fs, abs=1.0)


def test_chirp_advance_keeps_signed_reporting():
    fs = 2.048e6
    src = DemoSource(sample_rate=fs, tone_freq=0.0, chirp_rate=0.02)
    src.set_chirp(0.02)
    saw_negative = False
    for _ in range(200):
        src.read(64)
        freq_hz = src.get_tones()[0]["freq_hz"]
        # A properly signed report never jumps to the near-Nyquist
        # alias (e.g. ~0.95 * fs) that the unwrapped `% 1.0` bug
        # produced for the negative half of the sweep.
        assert abs(freq_hz) <= 0.5 * fs
        if freq_hz < 0.0:
            saw_negative = True
    assert saw_negative
