import numpy as np
import pytest

from doppler.spectral import (
    FFT,
    FFT2D,
    find_peaks_f32,
    hann_window,
    kaiser_enbw,
    kaiser_window,
    magnitude_db_cf32,
    magnitude_db_cf64,
)

# ── FFT ──────────────────────────────────────────────────────────────────────


def test_fft_create():
    obj = FFT(1024, -1, 1)
    assert obj.n == 1024
    assert obj.sign == -1


def test_fft_round_trip_cf64():
    N = 64
    fwd = FFT(N, -1, 1)
    inv = FFT(N, +1, 1)
    x = np.arange(N, dtype=np.complex128)
    spec = fwd.execute_cf64(x)
    rec = inv.execute_cf64(spec)
    np.testing.assert_allclose(rec, N * x, rtol=1e-9, atol=1e-3)


def test_fft_round_trip_cf32():
    N = 64
    fwd = FFT(N, -1, 1)
    inv = FFT(N, +1, 1)
    x = np.arange(N, dtype=np.complex64)
    spec = fwd.execute_cf32(x)
    rec = inv.execute_cf32(spec)
    np.testing.assert_allclose(rec, N * x, rtol=1e-4, atol=1e-3)


def test_fft_dc_tone_cf64():
    N = 32
    obj = FFT(N, -1, 1)
    x = np.ones(N, dtype=np.complex128)
    out = obj.execute_cf64(x)
    assert abs(out[0] - N) < 1e-9
    np.testing.assert_allclose(out[1:], 0.0, atol=1e-9)


def test_fft_inplace_cf64_matches_oop():
    N = 32
    obj = FFT(N, -1, 1)
    x = np.random.default_rng(0).standard_normal(N).astype(np.complex128)
    oop = obj.execute_cf64(x)
    ip = obj.execute_inplace_cf64(x)
    np.testing.assert_allclose(ip, oop, rtol=1e-12)


def test_fft_inplace_cf32_matches_oop():
    N = 32
    obj = FFT(N, -1, 1)
    x = np.random.default_rng(0).standard_normal(N).astype(np.complex64)
    oop = obj.execute_cf32(x)
    ip = obj.execute_inplace_cf32(x)
    np.testing.assert_allclose(ip, oop, rtol=1e-5)


def test_fft_output_dtype_cf64():
    obj = FFT(16, -1, 1)
    out = obj.execute_cf64(np.ones(16, dtype=np.complex128))
    assert out.dtype == np.complex128


def test_fft_output_dtype_cf32():
    obj = FFT(16, -1, 1)
    out = obj.execute_cf32(np.ones(16, dtype=np.complex64))
    assert out.dtype == np.complex64


def test_fft_context_manager():
    with FFT(16, -1, 1) as obj:
        out = obj.execute_cf64(np.ones(16, dtype=np.complex128))
        assert len(out) == 16


def test_fft_destroy():
    obj = FFT(16, -1, 1)
    obj.destroy()


# ── FFT2D ────────────────────────────────────────────────────────────────────


def test_fft2d_create():
    obj = FFT2D(8, 16, -1, 1)
    assert obj.ny == 8
    assert obj.nx == 16
    assert obj.sign == -1


def test_fft2d_round_trip_cf64():
    NY, NX = 8, 8
    fwd = FFT2D(NY, NX, -1, 1)
    inv = FFT2D(NY, NX, +1, 1)
    x = np.arange(NY * NX, dtype=np.complex128)
    spec = fwd.execute_cf64(x)
    rec = inv.execute_cf64(spec)
    np.testing.assert_allclose(rec, NY * NX * x, rtol=1e-9)


def test_fft2d_round_trip_cf32():
    NY, NX = 8, 8
    fwd = FFT2D(NY, NX, -1, 1)
    inv = FFT2D(NY, NX, +1, 1)
    x = np.arange(NY * NX, dtype=np.complex64)
    spec = fwd.execute_cf32(x)
    rec = inv.execute_cf32(spec)
    np.testing.assert_allclose(rec, NY * NX * x, rtol=1e-4)


def test_fft2d_inplace_cf64_matches_oop():
    NY, NX = 8, 8
    obj = FFT2D(NY, NX, -1, 1)
    x = np.random.default_rng(0).standard_normal(NY * NX).astype(np.complex128)
    oop = obj.execute_cf64(x)
    ip = obj.execute_inplace_cf64(x)
    np.testing.assert_allclose(ip, oop, rtol=1e-12)


def test_fft2d_destroy():
    obj = FFT2D(4, 4, -1, 1)
    obj.destroy()


# ── Module-level functions ───────────────────────────────────────────────────


def test_kaiser_enbw():
    w = np.ones(64, dtype=np.float32)
    # All-ones window: ENBW = 1.0
    enbw = kaiser_enbw(w)
    assert abs(enbw - 1.0) < 1e-5


def test_kaiser_window_fills_in_place():
    w = np.zeros(64, dtype=np.float32)
    kaiser_window(w, 6.0)
    assert w[32] > 0.9  # peak is near 1.0
    assert w[0] < 0.05  # endpoints are near 0


def test_kaiser_window_enbw_range():
    w = np.zeros(256, dtype=np.float32)
    kaiser_window(w, 6.0)
    enbw = kaiser_enbw(w)
    assert 1.3 < enbw < 2.0


def test_hann_window_symmetry():
    w = np.zeros(64, dtype=np.float32)
    hann_window(w)
    np.testing.assert_allclose(w, w[::-1], atol=1e-6)
    assert abs(w[0]) < 1e-6
    assert abs(w[-1]) < 1e-6


def test_magnitude_db_cf32():
    x = np.ones(8, dtype=np.complex64)
    db = magnitude_db_cf32(x, 1e-12, 0.0)
    assert db.dtype == np.float32
    np.testing.assert_allclose(db, 0.0, atol=1e-5)


def test_magnitude_db_cf64():
    x = np.ones(8, dtype=np.complex128)
    db = magnitude_db_cf64(x, 1e-12, 0.0)
    assert db.dtype == np.float32
    np.testing.assert_allclose(db, 0.0, atol=1e-5)


def test_magnitude_db_floor():
    x = np.zeros(4, dtype=np.complex64)
    db = magnitude_db_cf32(x, 1e-6, 0.0)
    # All bins should be 20*log10(1e-6) = -120 dB
    np.testing.assert_allclose(db, -120.0, atol=0.01)


def test_find_peaks_f32_single_peak():
    db = np.full(64, -80.0, dtype=np.float32)
    db[16] = -10.0
    db[15] = -20.0
    db[17] = -20.0
    peaks = find_peaks_f32(db, 1, -60.0)
    assert len(peaks) == 1
    freq_norm, amp_db = peaks[0]
    assert abs(freq_norm - (16 - 32) / 64) < 0.02
    assert amp_db > -15.0


def test_find_peaks_f32_no_peaks_above_threshold():
    db = np.full(64, -80.0, dtype=np.float32)
    peaks = find_peaks_f32(db, 4, -60.0)
    assert len(peaks) == 0


def test_find_peaks_f32_sorted_by_amplitude():
    db = np.full(128, -80.0, dtype=np.float32)
    db[30] = -5.0
    db[29] = -15.0
    db[31] = -15.0
    db[60] = -20.0
    db[59] = -30.0
    db[61] = -30.0
    peaks = find_peaks_f32(db, 2, -60.0)
    assert len(peaks) == 2
    assert peaks[0][1] > peaks[1][1]  # sorted descending


# ── integer-IQ FFT (execute_ci16 / execute_ci8) ──────────────────────────────


def _ref_fft_from_int(iq, scale):
    z = (
        iq[0::2].astype(np.float64) + 1j * iq[1::2].astype(np.float64)
    ) / scale
    return np.fft.fft(z)


def test_fft_execute_ci16_matches_numpy():
    rng = np.random.default_rng(0)
    for n in (1024, 4096, 2046):  # PFFFT sizes + a non-5-smooth fallback size
        iq = rng.integers(-30000, 30000, size=2 * n).astype(np.int16)
        y = FFT(n, -1, 1).execute_ci16(iq)
        assert y.dtype == np.complex64
        ref = _ref_fft_from_int(iq, 32768.0)
        assert np.max(np.abs(y - ref)) / np.max(np.abs(ref)) < 1e-5


def test_fft_execute_ci8_matches_numpy():
    rng = np.random.default_rng(1)
    n = 2048
    iq = rng.integers(-120, 120, size=2 * n).astype(np.int8)
    y = FFT(n, -1, 1).execute_ci8(iq)
    assert y.dtype == np.complex64
    ref = _ref_fft_from_int(iq, 128.0)
    assert np.max(np.abs(y - ref)) / np.max(np.abs(ref)) < 1e-5


def test_fft_execute_ci16_identical_to_cvt_then_cf32():
    # The fused integer path must equal i16_to_f32 (v/32768) then execute_cf32.
    rng = np.random.default_rng(2)
    n = 4096
    iq = rng.integers(-30000, 30000, size=2 * n).astype(np.int16)
    fused = FFT(n, -1, 1).execute_ci16(iq)
    cf = (iq.astype(np.float32) / 32768.0).view(np.complex64)
    twostep = FFT(n, -1, 1).execute_cf32(cf)
    assert np.array_equal(fused, twostep)


def test_execute_cf32_out_writes_into_callers_buffer():
    n = 64
    obj = FFT(n, -1, 1)
    x = np.ones(n, dtype=np.complex64)
    out = np.zeros(max(obj.execute_cf32_max_out(), n), dtype=np.complex64)
    y = obj.execute_cf32(x, out=out)
    assert np.shares_memory(y, out)


def test_execute_cf32_out_undersized_raises():
    n = 64
    obj = FFT(n, -1, 1)
    with pytest.raises(ValueError):
        obj.execute_cf32(
            np.ones(n, dtype=np.complex64), out=np.zeros(1, dtype=np.complex64)
        )


def test_execute_cf64_out_writes_into_callers_buffer():
    n = 64
    obj = FFT(n, -1, 1)
    x = np.ones(n, dtype=np.complex128)
    out = np.zeros(max(obj.execute_cf64_max_out(), n), dtype=np.complex128)
    y = obj.execute_cf64(x, out=out)
    assert np.shares_memory(y, out)


def test_execute_inplace_cf32_out_writes_into_callers_buffer():
    n = 64
    obj = FFT(n, -1, 1)
    x = np.ones(n, dtype=np.complex64)
    out = np.zeros(
        max(obj.execute_inplace_cf32_max_out(), n), dtype=np.complex64
    )
    y = obj.execute_inplace_cf32(x, out=out)
    assert np.shares_memory(y, out)


def test_execute_inplace_cf64_out_writes_into_callers_buffer():
    n = 64
    obj = FFT(n, -1, 1)
    x = np.ones(n, dtype=np.complex128)
    out = np.zeros(
        max(obj.execute_inplace_cf64_max_out(), n), dtype=np.complex128
    )
    y = obj.execute_inplace_cf64(x, out=out)
    assert np.shares_memory(y, out)
