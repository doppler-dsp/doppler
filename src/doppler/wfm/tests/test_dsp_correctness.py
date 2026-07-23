"""Analytic DSP-correctness validation for the wfm synthesis engine.

Unlike :mod:`test_wfm_synth` (which asserts *structural* properties — shapes,
reset reproducibility, step/steps parity), this module checks every generator
against **closed-form ground truth**: a tone's FFT peak lands in the exact bin
with the right power, AWGN matches ``sigma = sqrt(1 / (2 * 10**(snr/10)))`` per
component and passes a normality test, a PN m-sequence has period
``2**n - 1`` with the ideal two-level autocorrelation, QPSK sits on the
``+/-1/sqrt(2)`` constellation, a chirp's instantaneous frequency is linear,
and the RRC taps satisfy the Nyquist no-ISI criterion.

The reference math is taken from the C core:

- ``native/src/wfm_synth/wfm_synth_core.c`` — per-type kernels
- ``native/src/wfm/wfm_awgn_amplitude.c`` — ``amp = sqrt(P / (2 * snr_lin))``
- ``native/src/wfm/wfm_ebno_to_snr_db.c`` — Eb/No -> SNR(fs)
- ``native/src/wfm/wfm_dsp.c`` — RRC taps (``2*span*sps + 1``, unit energy)
- ``native/inc/lo/lo_core.h`` — 16-bit-LUT NCO (sets the achievable SFDR)

Any check that fails against current behaviour is marked ``xfail`` with a
pointer into ``docs/dev/wfm-validation-findings.md`` (see that file for the
expected-vs-observed write-up); ``strict=True`` makes a future fix that starts
passing fail CI until the marker is removed.
"""

from __future__ import annotations

import numpy as np
import pytest

import doppler.wfm as w

scipy_stats = pytest.importorskip("scipy.stats")
scipy_signal = pytest.importorskip("scipy.signal")


# --------------------------------------------------------------------------- #
# Tone — y[n] = exp(j 2 pi (freq/fs) n)
# --------------------------------------------------------------------------- #
class TestTone:
    def test_unit_magnitude(self) -> None:
        s = w.Synth(type="tone", fs=1e6, freq=123_456.0, snr=100.0)
        x = s.steps(4096)
        # Clean tone is a pure phasor: |y| == 1 to within cf32 precision.
        assert np.allclose(np.abs(x), 1.0, atol=1e-4)

    def test_fft_peak_lands_in_exact_bin(self) -> None:
        # Pick an integer-bin frequency so the tone is a single FFT line.
        n, fs, k = 4096, 1e6, 137
        freq = fs * k / n
        x = w.Synth(type="tone", fs=fs, freq=freq, snr=100.0).steps(n)
        spec = np.abs(np.fft.fft(x.astype(np.complex128)))
        peak = int(np.argmax(spec))
        assert peak == k
        # The line holds essentially all the energy.
        total = float(spec.sum())
        assert spec[peak] / total > 0.99

    def test_phase_increment_matches_frequency(self) -> None:
        fs, freq = 1e6, 100_000.0
        x = w.Synth(type="tone", fs=fs, freq=freq, snr=100.0).steps(256)
        dphi = np.angle(x[1:] * np.conj(x[:-1]))
        expected = 2.0 * np.pi * freq / fs
        assert np.allclose(dphi, expected, atol=1e-3)

    def test_negative_frequency(self) -> None:
        n, fs, k = 2048, 1e6, -211
        freq = fs * k / n
        x = w.Synth(type="tone", fs=fs, freq=freq, snr=100.0).steps(n)
        spec = np.abs(np.fft.fft(x.astype(np.complex128)))
        assert int(np.argmax(spec)) == n + k  # negative bin wraps

    def test_sfdr_from_16bit_lut(self) -> None:
        # The NCO uses a 16-bit phase->amplitude LUT; SFDR clears ~84 dBc.
        n, fs, k = 8192, 1e6, 301
        x = w.Synth(type="tone", fs=fs, freq=fs * k / n, snr=100.0).steps(n)
        spec = np.abs(np.fft.fft(x.astype(np.complex128))) ** 2
        peak = spec[k]
        spec[k] = 0.0
        spur = spec.max()
        sfdr_db = 10.0 * np.log10(peak / spur)
        assert sfdr_db > 84.0


# --------------------------------------------------------------------------- #
# Noise — n_I, n_Q ~ N(0, sigma**2), sigma = sqrt(1 / (2 * 10**(snr/10)))
# --------------------------------------------------------------------------- #
class TestNoise:
    def test_bare_synth_is_unit_power(self) -> None:
        # A standalone noise *Synth* is the raw kernel: unit complex power
        # (sigma**2 = 1/2 per quadrature). level/snr are *composition-time*
        # gains applied by Composer, NOT by Synth.steps() — see
        # docs/dev/wfm-validation-findings.md (noise-scaling behaviour note).
        x = w.Synth(type="noise", seed=7).steps(200_000)
        assert np.var(x.real) == pytest.approx(0.5, rel=0.03)
        assert np.var(x.imag) == pytest.approx(0.5, rel=0.03)
        assert float(np.mean(np.abs(x) ** 2)) == pytest.approx(1.0, rel=0.03)

    @pytest.mark.parametrize("snr_db", [-20.0, 0.0, 20.0])
    def test_bare_synth_ignores_snr(self, snr_db: float) -> None:
        # Documented behaviour: snr has no signal to reference on a bare noise
        # source, so the kernel always emits unit-power noise.
        x = w.Synth(type="noise", snr=snr_db, snr_mode="fs", seed=7).steps(
            100_000
        )
        assert float(np.mean(np.abs(x) ** 2)) == pytest.approx(1.0, rel=0.03)

    @pytest.mark.parametrize("level_db", [0.0, -6.0, -20.0])
    def test_composer_applies_level_gain(self, level_db: float) -> None:
        # The Composer applies the per-segment level (dBFS) gain.
        c = w.Composer(
            [w.Segment("noise", level=level_db, num_samples=100_000, seed=7)]
        )
        x = c.compose()
        assert float(np.mean(np.abs(x) ** 2)) == pytest.approx(
            10.0 ** (level_db / 10.0), rel=0.03
        )

    def test_iq_independence(self) -> None:
        x = w.Synth(type="noise", snr=0.0, snr_mode="fs", seed=11).steps(
            200_000
        )
        r = np.corrcoef(x.real, x.imag)[0, 1]
        assert abs(r) < 0.02

    def test_gaussianity(self) -> None:
        x = w.Synth(type="noise", snr=0.0, snr_mode="fs", seed=5).steps(50_000)
        # D'Agostino normality test on each quadrature; expect a high p-value.
        _, p_re = scipy_stats.normaltest(x.real)
        _, p_im = scipy_stats.normaltest(x.imag)
        assert p_re > 1e-3
        assert p_im > 1e-3

    def test_whiteness_flat_psd(self) -> None:
        x = w.Synth(type="noise", snr=0.0, snr_mode="fs", seed=9).steps(65536)
        _f, pxx = scipy_signal.welch(x, nperseg=1024, return_onesided=False)
        # Flat spectrum: max bin within a few dB of the mean.
        ratio_db = 10.0 * np.log10(pxx.max() / pxx.mean())
        assert ratio_db < 6.0


# --------------------------------------------------------------------------- #
# PN / BPSK — maximal-length LFSR sequence
# --------------------------------------------------------------------------- #
class TestPN:
    @pytest.mark.parametrize("length", [5, 7, 9])
    def test_period_and_balance(self, length: int) -> None:
        period = (1 << length) - 1
        # Pass an explicit primitive polynomial (the omitted-poly default is
        # degenerate — see test_default_poly_is_maximal_length).
        p = w.PN(poly=w.mls_poly(length), seed=1, length=length)
        chips = p.generate(period).copy()  # zero-copy buffer: copy!
        ones = int(chips.sum())
        # m-sequence balance: exactly 2**(n-1) ones, 2**(n-1)-1 zeros.
        assert ones == (1 << (length - 1))
        assert period - ones == ones - 1

    def test_two_level_autocorrelation(self) -> None:
        length = 7
        period = (1 << length) - 1
        chips = (
            w.PN(poly=w.mls_poly(length), seed=1, length=length)
            .generate(period)
            .copy()
        )
        bp = 1.0 - 2.0 * chips.astype(np.float64)  # {0,1} -> {+1,-1}
        ac = np.array([np.dot(bp, np.roll(bp, k)) for k in range(period)])
        assert ac[0] == period
        # Ideal m-sequence: every non-zero lag is exactly -1.
        assert np.all(ac[1:] == -1.0)

    def test_galois_fibonacci_same_period(self) -> None:
        length = 7
        period = (1 << length) - 1
        poly = w.mls_poly(length)
        g = (
            w.PN(poly=poly, seed=1, length=length, lfsr="galois")
            .generate(period)
            .copy()
        )
        f = (
            w.PN(poly=poly, seed=1, length=length, lfsr="fibonacci")
            .generate(period)
            .copy()
        )
        # Both are maximal-length: same balance, both full-period.
        assert int(g.sum()) == (1 << (length - 1))
        assert int(f.sum()) == (1 << (length - 1))

    def test_default_poly_is_maximal_length(self) -> None:
        # With poly omitted (or 0) PN now auto-selects the MLS primitive
        # polynomial, matching Synth(pn_poly=0) (#191): length-7 -> 64-ones
        # maximal-length period.
        chips = w.PN(seed=1, length=7).generate(127).copy()
        assert int(chips.sum()) == 64

    def test_bpsk_symbols_are_pm1(self) -> None:
        # sps=1 so each sample is a symbol; clean so no noise.
        x = w.Synth(
            type="bpsk", sps=1, snr=100.0, freq=0.0, pn_length=7
        ).steps(127)
        # Baseband BPSK symbols live on +/-1 (real axis).
        assert np.allclose(np.abs(x.real), 1.0, atol=1e-4)
        assert np.allclose(x.imag, 0.0, atol=1e-4)


# --------------------------------------------------------------------------- #
# QPSK — Gray-coded +/-1/sqrt(2) +/- j/sqrt(2)
# --------------------------------------------------------------------------- #
class TestQPSK:
    def test_constellation_points(self) -> None:
        x = w.Synth(
            type="qpsk", sps=1, snr=100.0, freq=0.0, pn_length=9
        ).steps(511)
        r = 1.0 / np.sqrt(2.0)
        # Unit symbol energy.
        assert np.allclose(np.abs(x), 1.0, atol=1e-4)
        # Every sample sits on one of the four Gray points.
        for s in x:
            assert np.isclose(abs(s.real), r, atol=1e-4)
            assert np.isclose(abs(s.imag), r, atol=1e-4)

    def test_four_distinct_points_min_distance(self) -> None:
        x = w.Synth(
            type="qpsk", sps=1, snr=100.0, freq=0.0, pn_length=9
        ).steps(511)
        pts = np.unique(np.round(x, 4))
        assert len(pts) == 4
        dmin = min(abs(a - b) for i, a in enumerate(pts) for b in pts[i + 1 :])
        assert dmin == pytest.approx(np.sqrt(2.0), rel=1e-3)


# --------------------------------------------------------------------------- #
# Chirp — instantaneous frequency f(n) = f0 + (f1-f0)*min(n,span)/span
# --------------------------------------------------------------------------- #
class TestChirp:
    def test_linear_instantaneous_frequency(self) -> None:
        fs, f0, f1, n = 1e6, -200_000.0, 200_000.0, 4096
        x = w.Synth(type="chirp", fs=fs, freq=f0, f_end=f1, snr=100.0).steps(n)
        # Unwrapped phase derivative -> instantaneous freq.
        phase = np.unwrap(np.angle(x.astype(np.complex128)))
        inst = np.diff(phase) / (2.0 * np.pi) * fs
        # Least-squares slope of f(n) over the sweep.
        idx = np.arange(len(inst))
        slope, intercept = np.polyfit(idx, inst, 1)
        assert intercept == pytest.approx(f0, abs=fs * 0.01)
        assert slope == pytest.approx((f1 - f0) / n, rel=0.05)

    def test_phase_continuity(self) -> None:
        x = w.Synth(
            type="chirp", fs=1e6, freq=0.0, f_end=300_000.0, snr=100.0
        ).steps(2048)
        dphi = np.angle(x[1:] * np.conj(x[:-1]))
        # No 2*pi jumps: per-step phase step stays well inside (-pi, pi).
        assert np.all(np.abs(dphi) < np.pi)


# --------------------------------------------------------------------------- #
# Bits — user pattern, oversampled and cycled
# --------------------------------------------------------------------------- #
class TestBits:
    def test_bpsk_pattern_fidelity(self) -> None:
        pat = bytes([1, 0, 1, 1, 0, 0, 1, 0])
        x = w.Synth(
            type="bits",
            bits=pat,
            modulation="bpsk",
            sps=1,
            snr=100.0,
            freq=0.0,
        ).steps(len(pat))
        expected = np.where(np.frombuffer(pat, np.uint8) == 1, -1.0, 1.0)
        assert np.allclose(x.real, expected, atol=1e-4)
        assert np.allclose(x.imag, 0.0, atol=1e-4)

    def test_pattern_cycles(self) -> None:
        pat = bytes([1, 0])
        x = w.Synth(
            type="bits",
            bits=pat,
            modulation="bpsk",
            sps=1,
            snr=100.0,
            freq=0.0,
        ).steps(6)
        expected = np.array([-1, 1, -1, 1, -1, 1], dtype=np.float64)
        assert np.allclose(x.real, expected, atol=1e-4)


# --------------------------------------------------------------------------- #
# RRC pulse-shaping taps
# --------------------------------------------------------------------------- #
class TestRRC:
    @pytest.mark.parametrize("beta", [0.0, 0.25, 0.5, 1.0])
    def test_tap_count_energy_symmetry(self, beta: float) -> None:
        sps, span = 8, 8
        h = w.rrc_taps(beta, sps, span)
        assert len(h) == 2 * span * sps + 1
        # Unit energy.
        assert float((h.astype(np.float64) ** 2).sum()) == pytest.approx(
            1.0, rel=1e-4
        )
        # Symmetric about the centre, which is the peak.
        assert np.allclose(h, h[::-1], atol=1e-5)
        assert int(np.argmax(h)) == span * sps
        assert np.all(np.isfinite(h))

    def test_nyquist_no_isi(self) -> None:
        # RRC (x) RRC = raised cosine -> zero ISI at non-zero symbol instants.
        sps, span, beta = 8, 10, 0.35
        h = w.rrc_taps(beta, sps, span).astype(np.float64)
        rc = np.convolve(h, h)
        centre = len(rc) // 2
        rc /= rc[centre]
        for k in range(1, span):
            assert abs(rc[centre + k * sps]) < 1e-2

    def test_singularity_points_finite(self) -> None:
        # beta>0 has a removable 0/0 at t = +/- 1/(4 beta); must stay finite.
        h = w.rrc_taps(0.25, 4, 8)
        assert np.all(np.isfinite(h))


# --------------------------------------------------------------------------- #
# DSSS spreading / despreading
# --------------------------------------------------------------------------- #
class TestDSSS:
    def test_spread_despread_recovers_symbols(self) -> None:
        rng = np.random.default_rng(0)
        syms = (1.0 - 2.0 * rng.integers(0, 2, 16)).astype(np.complex64)
        code = rng.integers(0, 2, 7).astype(np.uint8)
        sf = len(code)
        chips = np.asarray(w.dsss_spread(syms, code, sf))
        assert len(chips) == len(syms) * sf
        # Re-spreading by the same code despreads (code is its own inverse).
        bp = 1.0 - 2.0 * code.astype(np.float64)  # {0,1} -> {+1,-1}
        per_sym = chips.reshape(len(syms), sf)
        recovered = (per_sym * bp).mean(axis=1)
        assert np.allclose(recovered, syms, atol=1e-4)


# --------------------------------------------------------------------------- #
# SNR-mode conversions
# --------------------------------------------------------------------------- #
class TestSNRModes:
    def test_awgn_amplitude_formula(self) -> None:
        # amp = sqrt(P / (2 * 10**(snr/10)))
        for snr_db in (-3.0, 0.0, 10.0):
            amp = w.wfm_awgn_amplitude(snr_db, 1.0)
            expected = np.sqrt(1.0 / (2.0 * 10.0 ** (snr_db / 10.0)))
            assert amp == pytest.approx(expected, rel=1e-5)

    def test_awgn_amplitude_scales_with_signal_power(self) -> None:
        a1 = w.wfm_awgn_amplitude(10.0, 1.0)
        a4 = w.wfm_awgn_amplitude(10.0, 4.0)
        # Doubling amplitude (4x power) doubles the noise amplitude.
        assert a4 == pytest.approx(2.0 * a1, rel=1e-5)

    def test_ebno_to_snr_fs(self) -> None:
        # snr_fs = ebno + 10log10(bps) - 10log10(sps)
        for ebno, bps, sps in [(10.0, 1, 8.0), (6.0, 2, 4.0)]:
            got = w.wfm_ebno_to_snr_db(ebno, bps, sps)
            expected = ebno + 10.0 * np.log10(bps) - 10.0 * np.log10(sps)
            assert got == pytest.approx(expected, abs=1e-4)

    def test_realized_snr_fs_mode(self) -> None:
        # Generate signal+noise at a known fs-band SNR; measure it back.
        snr_db = 10.0
        clean = w.Synth(type="tone", fs=1e6, freq=100_000.0, snr=100.0).steps(
            200_000
        )
        noisy = w.Synth(
            type="tone",
            fs=1e6,
            freq=100_000.0,
            snr=snr_db,
            snr_mode="fs",
            seed=1,
        ).steps(200_000)
        noise = noisy - clean
        sig_p = float(np.mean(np.abs(clean) ** 2))
        noi_p = float(np.mean(np.abs(noise) ** 2))
        measured = 10.0 * np.log10(sig_p / noi_p)
        assert measured == pytest.approx(snr_db, abs=0.5)


# --------------------------------------------------------------------------- #
# Quantization round-trip (Writer -> Reader)
# --------------------------------------------------------------------------- #
class TestQuantization:
    @pytest.mark.parametrize(
        "stype,scale",
        [("ci8", 127.0), ("ci16", 32767.0), ("ci32", 2147483647.0)],
    )
    def test_integer_roundtrip_within_1lsb(
        self, tmp_path, stype: str, scale: float
    ) -> None:
        x = w.Synth(type="tone", fs=1e6, freq=50_000.0, snr=100.0).steps(4096)
        # Stay inside +/-1 full-scale (unit-magnitude tone -> ~0.7 per comp).
        p = tmp_path / f"cap_{stype}.iq"
        with w.Writer(
            str(p), file_type="raw", sample_type=stype, total=len(x)
        ) as wr:
            wr.write(x)
        with w.Reader(str(p), sample_type=stype) as r:
            y = r.read(len(x))
        assert y.shape == x.shape
        # Round-trip error bounded by one quantizer step.
        assert np.max(np.abs(y - x)) <= 1.5 / scale

    @pytest.mark.parametrize("stype", ["cf32", "cf64"])
    def test_float_roundtrip_exact(self, tmp_path, stype: str) -> None:
        x = w.Synth(type="qpsk", sps=2, snr=100.0).steps(2048)
        p = tmp_path / f"cap_{stype}.iq"
        with w.Writer(
            str(p), file_type="raw", sample_type=stype, total=len(x)
        ) as wr:
            wr.write(x)
        with w.Reader(str(p), sample_type=stype) as r:
            y = r.read(len(x))
        # Float containers are a bit-exact re-interpretation of cf32 input.
        assert np.allclose(y, x, atol=1e-6)

    def test_clip_tracking(self, tmp_path) -> None:
        # Drive samples beyond full-scale to force saturation.
        x = 3.0 * w.Synth(type="tone", freq=10_000.0, snr=100.0).steps(1024)
        p = tmp_path / "clip.iq"
        with w.Writer(
            str(p), file_type="raw", sample_type="ci16", total=len(x)
        ) as wr:
            wr.track_clipping(1)
            wr.write(x)
            assert wr.clipped is True
            assert wr.clip_fraction > 0.0
            assert wr.peak_dbfs >= 0.0  # peak exceeds full-scale


# --------------------------------------------------------------------------- #
# Determinism
# --------------------------------------------------------------------------- #
class TestDeterminism:
    def test_same_seed_bit_identical(self) -> None:
        a = w.Synth(type="noise", snr=3.0, seed=42).steps(4096)
        b = w.Synth(type="noise", snr=3.0, seed=42).steps(4096)
        assert np.array_equal(a, b)

    def test_reset_reproduces(self) -> None:
        s = w.Synth(type="qpsk", sps=4, seed=1, snr=5.0)
        a = s.steps(2048).copy()
        s.reset()
        assert np.array_equal(a, s.steps(2048))

    def test_step_steps_parity(self) -> None:
        s1 = w.Synth(type="pn", sps=2, seed=1, snr=100.0, pn_length=7)
        block = s1.steps(64)
        s2 = w.Synth(type="pn", sps=2, seed=1, snr=100.0, pn_length=7)
        one_by_one = np.array([s2.step() for _ in range(64)], np.complex64)
        assert np.array_equal(block, one_by_one)

    def test_different_seed_differs(self) -> None:
        a = w.Synth(type="noise", snr=3.0, seed=1).steps(4096)
        b = w.Synth(type="noise", snr=3.0, seed=2).steps(4096)
        assert not np.array_equal(a, b)
