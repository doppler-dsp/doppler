"""Integration tests for the specan DSP engine against the demo source.

These exercise the *whole* analysis chain end to end — demo signal
generation → DDC mix/resample → Kaiser window → FFT → magnitude/dBm →
peak detection — through the real ``doppler.ddc`` / ``doppler.spectral``
C extensions.

Why this exists: the engine consumes several extension APIs (``DDC``,
``FFT.execute_cf32``, ``kaiser_window``/``kaiser_enbw``, ``find_peaks_f32``).
Those APIs were renamed/reshaped during the jm migration, and because no
test drove the engine, the breakage shipped silently — ``doppler-specan
--source demo`` hung on "Waiting for signal..." (every per-block exception
was swallowed by the display's DSP loop). A single frame-producing
assertion catches the entire class of regression.
"""

import numpy as np
import pytest

from doppler.specan.config import SpecanConfig
from doppler.specan.engine import SpecanEngine
from doppler.specan.source import make_source


def _first_frame(cfg, max_reads: int = 300):
    """Drive the demo source through the engine until a frame appears.

    Returns ``(engine, frame)``. Asserts a frame is produced — a ``None``
    return forever is exactly the "Waiting for signal..." failure mode.
    """
    src = make_source(cfg)
    eng = SpecanEngine(cfg)
    try:
        for _ in range(max_reads):
            iq, fs, cf = src.read(max(eng.block_size, 4096))
            frame = eng.process(iq, fs, cf)
            if frame is not None:
                return eng, frame
    finally:
        src.close()
    pytest.fail("engine never produced a frame from the demo source")


def _bin_freqs_hz(frame):
    """Reconstruct the per-bin absolute frequency axis of a frame.

    ``frame.db`` is the central passband of a ``data_size * 2`` zero-padded
    FFT, DC-centred, so bin ``i`` maps to
    ``(i - len/2) * fs_out / nfft + center_freq``.
    """
    n = len(frame.db)
    nfft = frame.data_size * 2
    return (np.arange(n) - n // 2) * frame.fs_out / nfft + frame.center_freq


def test_demo_produces_a_frame():
    """The default demo invocation must yield a spectrum frame (not hang)."""
    cfg = SpecanConfig(source="demo", span=200e3)
    _eng, frame = _first_frame(cfg)
    assert frame is not None
    assert len(frame.db) > 0
    assert frame.fs_out > 0.0
    assert frame.span == pytest.approx(200e3, rel=0.2)


def test_demo_tone_lands_at_configured_offset():
    """An interior demo tone must appear at its offset, at roughly its power.

    100 kHz (the CLI default) sits at the +edge of a 200 kHz span where the
    resampler rolls off, so use an interior tone for a clean amplitude/peak
    check. Edge behaviour is covered separately below.
    """
    cfg = SpecanConfig(source="demo", span=200e3)
    cfg.demo.tone_freq = 50e3
    cfg.demo.tone_power = -20.0
    _eng, frame = _first_frame(cfg)

    freqs = _bin_freqs_hz(frame)
    db = np.asarray(frame.db, dtype=float)
    peak_hz = freqs[int(np.argmax(db))]

    # Frequency placement is exact to within one display bin.
    assert peak_hz == pytest.approx(50e3, abs=frame.fs_out / len(db))
    # Amplitude within a few dB of the tone power (window scalloping /
    # coherent-gain calibration accounts for the small offset).
    assert db.max() == pytest.approx(-20.0, abs=4.0)
    # The tone must clear the noise floor by a wide margin.
    assert db.max() - float(np.median(db)) > 40.0


def test_demo_tone_is_detected_as_a_peak():
    """find_peaks_f32 must surface the interior tone as the top peak."""
    cfg = SpecanConfig(source="demo", span=200e3)
    cfg.demo.tone_freq = 50e3
    _eng, frame = _first_frame(cfg)

    assert frame.peaks, "no peaks detected for a clear interior tone"
    top = max(frame.peaks, key=lambda p: p.db)
    assert top.freq_hz == pytest.approx(50e3, abs=frame.rbw * 4)


def test_retune_uses_the_norm_freq_setter():
    """retune() must move the DDC mix frequency without raising.

    Guards the ``set_freq`` → ``norm_freq`` property migration.
    """
    cfg = SpecanConfig(source="demo", span=200e3)
    eng, _frame = _first_frame(cfg)
    eng.retune(25e3)  # would AttributeError on the old set_freq() call
    assert eng._ddc.norm_freq == pytest.approx(
        (25e3 - eng._center_freq) / eng._fs_in
    )
