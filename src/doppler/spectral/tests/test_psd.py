import numpy as np
import pytest

from doppler.spectral import PSD, find_peaks_f32


def _tone(n, k):
    return np.exp(2j * np.pi * k * np.arange(n) / n).astype(np.complex64)


def test_create_props():
    w = PSD(n=1024, fs=1e6, window="kaiser", beta=8.0, mode="mean")
    assert w.n == 1024
    assert w.fs == 1e6
    assert w.enbw > 1.0  # any non-rectangular window widens the bin
    assert abs(w.rbw - w.enbw * w.fs / w.n) < 1e-3
    assert w.count == 0
    assert w.psd_db() is None


def test_dc_tone_centre():
    n = 64
    w = PSD(n=n, fs=1.0, window="hann", mode="mean")
    w.accumulate(np.ones(n, dtype=np.complex64))
    psd = w.psd_db()
    assert psd.shape == (n,)
    assert psd.dtype == np.float32
    assert int(np.argmax(psd)) == n // 2  # DC centred by fftshift


def test_tone_bin_and_count():
    n, k = 64, 8
    w = PSD(n=n, fs=1.0, window="hann", mode="mean")
    for _ in range(3):
        w.accumulate(_tone(n, k))
    assert w.count == 3
    assert int(np.argmax(w.psd_db())) == n // 2 + k


def test_psd_dbhz_constant_offset():
    n = 32
    w = PSD(n=n, fs=2.0, window="hann", mode="mean")
    w.accumulate(_tone(n, 5))
    a = w.psd_db()
    b = w.psd_dbhz()
    assert np.allclose(a - b, (a - b)[0], atol=1e-3)


def test_band_power_partition():
    n = 64
    w = PSD(n=n, fs=1.0, window="hann", mode="mean")
    for _ in range(4):
        w.accumulate(_tone(n, 10))
    bands = np.array([-0.5, 0.0, 0.0, 0.5])
    per = w.band_power(bands)
    assert per.shape == (2,)
    total = w.total_band_power(bands)
    lin = 10 ** (per[0] / 10) + 10 ** (per[1] / 10)
    assert abs(10 * np.log10(lin) - total) < 1e-2


@pytest.mark.parametrize("window", ["hann", "kaiser", "blackman-harris"])
@pytest.mark.parametrize("pad", [1, 4])
def test_band_power_absolute_is_window_and_pad_invariant(window, pad):
    """Integrated band power must equal the TRUE power, independent of the
    window and zero-padding.

    Regression for the ENBW bug: band power normalised by coherent gain cg^2
    (the tone-peak reference) instead of the noise-power gain nfft*s2
    over-counts every band by the window's equivalent noise bandwidth — 1.5x
    for Hann, 2.0x for Blackman-Harris — and by a further nfft/n when padded
    (6x, 8x). The correct normalisation is window- and pad-invariant.

    Two absolute references, both = 1.0:
      * white complex noise of unit variance -> total power 1.0;
      * a unit-amplitude tone -> power 1.0 (integrated across its leakage).
    """
    n = 2048
    rng = np.random.default_rng(0)

    # White noise, unit total power (variance 0.5 per component).
    wn = PSD(n=n, fs=1.0, window=window, beta=8.0, pad=pad, mode="mean")
    for _ in range(200):
        x = (rng.standard_normal(n) + 1j * rng.standard_normal(n)) / np.sqrt(2)
        wn.accumulate(x.astype(np.complex64))
    noise_lin = 10 ** (wn.total_band_power(np.array([-0.5, 0.5])) / 10.0)
    assert noise_lin == pytest.approx(1.0, abs=0.05), (
        f"{window} pad={pad}: noise band power {noise_lin:.3f} != 1.0 "
        "(ENBW normalisation missing?)"
    )

    # Unit-amplitude tone: integrated power is 1.0 regardless of leakage.
    wt = PSD(n=n, fs=1.0, window=window, beta=8.0, pad=pad, mode="mean")
    for _ in range(30):
        wt.accumulate(_tone(n, 205))  # off-bin so the window actually leaks
    tone_lin = 10 ** (wt.total_band_power(np.array([-0.5, 0.5])) / 10.0)
    assert tone_lin == pytest.approx(1.0, abs=0.02), (
        f"{window} pad={pad}: tone band power {tone_lin:.3f} != 1.0"
    )


def test_band_outside_span_is_floor():
    n = 64
    w = PSD(n=n, fs=1.0, window="hann", mode="mean")
    w.accumulate(_tone(n, 4))
    far = w.band_power(np.array([10.0, 11.0]))
    assert far[0] < -150.0


def test_band_power_out_writes_into_callers_buffer():
    n = 64
    w = PSD(n=n, fs=1.0, window="hann", mode="mean")
    w.accumulate(_tone(n, 10))
    bands = np.array([-0.5, 0.0, 0.0, 0.5])
    out = np.zeros(max(w.band_power_max_out(), len(bands)), dtype=np.float32)
    per = w.band_power(bands, out=out)
    assert np.shares_memory(per, out)


def test_band_power_out_undersized_raises():
    n = 64
    w = PSD(n=n, fs=1.0, window="hann", mode="mean")
    w.accumulate(_tone(n, 10))
    with pytest.raises(ValueError):
        w.band_power(
            np.array([-0.5, 0.0, 0.0, 0.5]), out=np.zeros(1, dtype=np.float32)
        )


def test_measurements_finite():
    n = 64
    w = PSD(n=n, fs=1.0, window="kaiser", beta=8.0, mode="mean")
    t = np.arange(n)
    x = (
        np.exp(2j * np.pi * 6 * t / n) + 0.1 * np.exp(2j * np.pi * 20 * t / n)
    ).astype(np.complex64)
    for _ in range(8):
        w.accumulate(x)
    assert np.isfinite(w.noise_floor())
    assert w.snr(0.0, 0.2) > 0.0  # carrier above the floor
    assert w.sfdr(-120.0) > 0.0  # carrier above the spur
    assert 0.0 < w.occupied_bw(0.99) < 0.5  # a tone occupies little


def test_peaks_via_free_function():
    # spectral peaks are obtained idiomatically by composing find_peaks_f32
    n, k = 64, 8
    w = PSD(n=n, fs=1.0, window="hann", mode="mean")
    for _ in range(4):
        w.accumulate(_tone(n, k))
    pk = find_peaks_f32(w.psd_db(), 4, -60.0)
    assert len(pk) >= 1
    # strongest peak sits near the tone's normalised frequency k/n
    assert abs(pk[0][0] - k / n) < 2.0 / n


def test_all_modes_and_reset():
    n = 32
    w = None
    for mode in ["mean", "exp", "maxhold", "minhold"]:
        w = PSD(n=n, fs=1.0, mode=mode)
        w.accumulate(np.ones(n, dtype=np.complex64))
        assert w.psd_db() is not None
        assert w.mode in (0, 1, 2, 3)
    w.reset()
    assert w.count == 0
    assert w.psd_db() is None


def test_bad_window_raises():
    with pytest.raises(ValueError):
        PSD(n=64, window="triangle")


def test_context_manager():
    with PSD(n=64, fs=1.0, mode="mean") as w:
        w.accumulate(np.ones(64, dtype=np.complex64))
        assert w.psd_db() is not None


def test_bits_is_the_single_dbfs_reference():
    """bits=B sets full_scale = 2**(B-1); identical to passing full_scale."""
    wb = PSD(n=256, fs=1.0, window="hann", bits=12)
    wf = PSD(n=256, fs=1.0, window="hann", full_scale=2**11)
    assert wb.full_scale == 2**11 == wf.full_scale
    assert wb.bits == 12
    x = (3.0 * np.exp(2j * np.pi * 8 * np.arange(256) / 256)).astype(
        np.complex64
    )
    wb.accumulate(x)
    wf.accumulate(x)
    assert np.allclose(wb.psd_db(), wf.psd_db())
