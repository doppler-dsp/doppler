"""Tests for doppler.detection — Marcum Q and optimal detector parameters.

Covers correctness at the Python API layer: return types, special cases,
numerical accuracy, monotonicity, roundtrips, and equivalence between the
envelope and power detector variants.

The C-level unit tests in native/tests/test_detection_core.c already check
many numerical values; these tests extend coverage to the Python binding,
to a wider range of pfa / dwell / snr combinations, and to properties that
are best expressed as parametrized invariants.
"""

import math

import numpy as np
import pytest

from doppler.detection import (
    det_dwell,
    det_dwell_power,
    det_n_noncoh,
    det_pd,
    det_pd_noncoherent,
    det_pd_power,
    det_snr,
    det_snr_power,
    det_threshold,
    det_threshold_noncoherent,
    det_threshold_power,
    marcum_q,
)

# ── Shared fixtures ──────────────────────────────────────────────────────────

PFA_VALUES = [1e-2, 1e-3, 1e-4, 1e-6, 1e-8, 1e-10]

# ── marcum_q ─────────────────────────────────────────────────────────────────


class TestMarcumQ:
    def test_b_nonpositive_returns_one(self):
        assert marcum_q(1, 0.0, 0.0) == 1.0
        assert marcum_q(1, 3.0, 0.0) == 1.0
        assert marcum_q(3, 2.0, -1.0) == 1.0
        assert marcum_q(5, 10.0, 0.0) == 1.0

    @pytest.mark.parametrize("b", [0.5, 1.0, 2.0, 3.0, 5.0])
    def test_a_zero_m1_exact(self, b):
        # Q_1(0, b) = exp(-b^2/2)
        assert marcum_q(1, 0.0, b) == pytest.approx(
            math.exp(-(b**2) / 2), rel=1e-12
        )

    @pytest.mark.parametrize("b", [1.0, 2.0, 3.0])
    def test_a_zero_m2_exact(self, b):
        # Q_2(0, b) = exp(-v) * (1 + v),  v = b^2/2
        v = b**2 / 2
        assert marcum_q(2, 0.0, b) == pytest.approx(
            math.exp(-v) * (1 + v), rel=1e-12
        )

    @pytest.mark.parametrize("b", [1.0, 2.0, 3.0])
    def test_a_zero_m3_exact(self, b):
        # Q_3(0, b) = exp(-v) * (1 + v + v^2/2),  v = b^2/2
        v = b**2 / 2
        assert marcum_q(3, 0.0, b) == pytest.approx(
            math.exp(-v) * (1 + v + v**2 / 2), rel=1e-12
        )

    @pytest.mark.parametrize(
        "m, a, b, expected",
        [
            (1, 2.0, 1.0, 0.9181076963694063),
            (1, 1.0, 2.0, 0.2690120600359100),
            (2, 1.0, 1.0, 0.9407902191465286),
            (1, 3.0, 2.0, 0.8867207544023923),
            (1, 0.5, 0.5, 0.8955085810698598),
            (4, 2.0, 3.0, 0.6639534637953503),
        ],
    )
    def test_known_values(self, m, a, b, expected):
        # rel=1e-8: tighter than libm worst-case (1-2 ULP ≈ 4e-16 near 0.9),
        # looser than the series accumulation error floor (~2e-14), so this
        # survives non-correctly-rounded exp() across platforms.
        assert marcum_q(m, a, b) == pytest.approx(expected, rel=1e-8)

    def test_monotone_increasing_in_a(self):
        # Stronger signal → higher Q_M.
        for m in [1, 2, 4]:
            for b in [1.0, 2.5]:
                assert marcum_q(m, 1.0, b) < marcum_q(m, 2.0, b)
                assert marcum_q(m, 2.0, b) < marcum_q(m, 4.0, b)

    def test_monotone_decreasing_in_b(self):
        # Higher threshold → lower probability.
        for m in [1, 2]:
            for a in [1.0, 3.0]:
                assert marcum_q(m, a, 3.0) < marcum_q(m, a, 1.5)

    def test_monotone_increasing_in_m(self):
        # More coherent integrations → higher Q_M at same (a, b).
        for a, b in [(1.5, 1.5), (1.0, 2.0), (2.0, 3.0)]:
            assert marcum_q(1, a, b) < marcum_q(2, a, b)
            assert marcum_q(2, a, b) < marcum_q(4, a, b)

    @pytest.mark.parametrize(
        "m,a,b",
        [
            (1, 0.0, 1.0),
            (2, 1.0, 2.0),
            (4, 3.0, 3.0),
            (8, 2.0, 1.0),
        ],
    )
    def test_bounded_in_0_1(self, m, a, b):
        q = marcum_q(m, a, b)
        assert 0.0 <= q <= 1.0

    def test_large_a_erfc_fast_path(self):
        # a >> b: erfc fast path must return a value indistinguishable
        # from 1.0 in double precision (the series would underflow to 0).
        assert marcum_q(1, 100.0, 1.0) == pytest.approx(1.0, abs=1e-14)
        assert marcum_q(1, 50.0, 10.0) == pytest.approx(1.0, abs=1e-10)

    def test_returns_float(self):
        assert isinstance(marcum_q(1, 1.0, 1.0), float)

    def test_wrong_arg_count_raises(self):
        with pytest.raises(TypeError):
            marcum_q(1, 1.0)


# ── det_threshold ────────────────────────────────────────────────────────────


class TestDetThreshold:
    @pytest.mark.parametrize("pfa", PFA_VALUES)
    def test_roundtrip(self, pfa):
        eta = det_threshold(pfa)
        assert math.exp(-0.5 * eta * eta) == pytest.approx(pfa, rel=1e-12)

    def test_known_value(self):
        assert det_threshold(1e-6) == pytest.approx(
            5.256521769756932, rel=1e-10
        )

    @pytest.mark.parametrize(
        "lo, hi",
        [
            (1e-6, 1e-4),
            (1e-10, 1e-6),
            (1e-4, 1e-2),
        ],
    )
    def test_monotone_decreasing_pfa(self, lo, hi):
        # Stricter Pfa requires a higher amplitude threshold.
        assert det_threshold(lo) > det_threshold(hi)

    def test_returns_float(self):
        assert isinstance(det_threshold(1e-6), float)


# ── det_pd ───────────────────────────────────────────────────────────────────


class TestDetPd:
    @pytest.mark.parametrize("m", [1, 2, 4, 8, 16, 64])
    @pytest.mark.parametrize("pfa", [1e-2, 1e-4, 1e-6])
    def test_snr_zero_equals_pfa(self, m, pfa):
        # Noise-only regime: Pd must equal Pfa regardless of dwell order.
        eta = det_threshold(pfa)
        assert det_pd(0.0, m, eta) == pytest.approx(pfa, rel=1e-12)

    def test_monotone_in_snr(self):
        eta = det_threshold(1e-6)
        for m in [1, 4, 16]:
            assert det_pd(0.5, m, eta) < det_pd(1.0, m, eta)
            assert det_pd(1.0, m, eta) < det_pd(2.0, m, eta)

    def test_monotone_in_dwell(self):
        eta = det_threshold(1e-6)
        for snr in [0.5, 1.0, 2.0]:
            assert det_pd(snr, 1, eta) < det_pd(snr, 4, eta)
            assert det_pd(snr, 4, eta) < det_pd(snr, 16, eta)

    def test_bounded_in_0_1(self):
        eta = det_threshold(1e-6)
        assert det_pd(10.0, 64, eta) <= 1.0
        assert det_pd(0.0, 1, eta) >= 0.0

    def test_high_snr_approaches_one(self):
        eta = det_threshold(1e-6)
        assert det_pd(20.0, 1, eta) == pytest.approx(1.0, abs=1e-10)

    def test_returns_float(self):
        assert isinstance(det_pd(1.0, 4, det_threshold(1e-6)), float)


# ── det_dwell ────────────────────────────────────────────────────────────────


class TestDetDwell:
    def test_high_snr_returns_one(self):
        assert det_dwell(100.0, 0.9, 1e-6, 256) == 1

    def test_infeasible_returns_minus_one(self):
        assert det_dwell(0.001, 0.9, 1e-6, 10) == -1

    @pytest.mark.parametrize(
        "snr,pd_min,pfa",
        [
            (0.5, 0.9, 1e-6),
            (0.3, 0.8, 1e-4),
            (1.0, 0.99, 1e-6),
            (0.7, 0.95, 1e-8),
        ],
    )
    def test_minimum_dwell_property(self, snr, pd_min, pfa):
        m = det_dwell(snr, pd_min, pfa, 2048)
        assert m > 0, f"infeasible for snr={snr}, pd_min={pd_min}, pfa={pfa}"
        eta = det_threshold(pfa)
        assert det_pd(snr, m, eta) >= pd_min - 1e-12
        if m > 1:
            assert det_pd(snr, m - 1, eta) < pd_min

    def test_returns_int(self):
        assert isinstance(det_dwell(1.0, 0.9, 1e-6, 256), int)


# ── det_snr ──────────────────────────────────────────────────────────────────


class TestDetSnr:
    @pytest.mark.parametrize("dwell", [1, 2, 4, 8, 16, 32, 64])
    def test_roundtrip(self, dwell):
        pfa = 1e-6
        pd_min = 0.9
        snr = det_snr(dwell, pd_min, pfa)
        eta = det_threshold(pfa)
        assert det_pd(snr, dwell, eta) >= pd_min - 1e-12

    def test_coherent_gain(self):
        # More dwell → less required SNR (coherent processing gain).
        snr1 = det_snr(1, 0.9, 1e-6)
        snr4 = det_snr(4, 0.9, 1e-6)
        snr16 = det_snr(16, 0.9, 1e-6)
        assert snr16 < snr4 < snr1

    @pytest.mark.parametrize("pd_min", [0.5, 0.8, 0.9, 0.99])
    def test_higher_pd_requires_more_snr(self, pd_min):
        if pd_min < 0.99:
            assert det_snr(4, pd_min, 1e-6) < det_snr(4, pd_min + 0.09, 1e-6)

    def test_nonnegative(self):
        for dwell in [1, 4, 16]:
            assert det_snr(dwell, 0.9, 1e-6) >= 0.0

    def test_returns_float(self):
        assert isinstance(det_snr(4, 0.9, 1e-6), float)


# ── det_threshold_power ──────────────────────────────────────────────────────


class TestDetThresholdPower:
    @pytest.mark.parametrize("pfa", PFA_VALUES)
    def test_roundtrip(self, pfa):
        p = det_threshold_power(pfa)
        assert math.exp(-p) == pytest.approx(pfa, rel=1e-14)

    def test_known_value(self):
        # -ln(1e-6) = 6·ln(10)
        assert det_threshold_power(1e-6) == pytest.approx(
            6.0 * math.log(10.0), rel=1e-12
        )

    def test_relationship_to_amplitude_threshold(self):
        # Power threshold = eta^2 / 2
        for pfa in PFA_VALUES:
            eta = det_threshold(pfa)
            p = det_threshold_power(pfa)
            assert p == pytest.approx(0.5 * eta * eta, rel=1e-12)

    @pytest.mark.parametrize("lo, hi", [(1e-6, 1e-4), (1e-10, 1e-6)])
    def test_monotone_decreasing_pfa(self, lo, hi):
        assert det_threshold_power(lo) > det_threshold_power(hi)

    def test_returns_float(self):
        assert isinstance(det_threshold_power(1e-6), float)


# ── det_pd_power ─────────────────────────────────────────────────────────────


class TestDetPdPower:
    @pytest.mark.parametrize("m", [1, 2, 4, 8, 16])
    @pytest.mark.parametrize("pfa", [1e-2, 1e-4, 1e-6])
    def test_snr_power_zero_equals_pfa(self, m, pfa):
        p = det_threshold_power(pfa)
        assert det_pd_power(0.0, m, p) == pytest.approx(pfa, rel=1e-12)

    @pytest.mark.parametrize(
        "snr,m",
        [
            (2.0, 1),
            (1.0, 4),
            (0.5, 8),
            (3.0, 2),
        ],
    )
    def test_equivalence_with_envelope_pd(self, snr, m):
        # det_pd_power(snr^2, M, p) == det_pd(snr, M, eta) exactly.
        pfa = 1e-6
        eta = det_threshold(pfa)
        p = det_threshold_power(pfa)
        assert det_pd_power(snr**2, m, p) == pytest.approx(
            det_pd(snr, m, eta), rel=1e-12
        )

    def test_monotone_in_snr_power(self):
        p = det_threshold_power(1e-6)
        for m in [1, 4, 16]:
            assert det_pd_power(0.25, m, p) < det_pd_power(1.0, m, p)

    def test_monotone_in_dwell(self):
        p = det_threshold_power(1e-6)
        assert det_pd_power(1.0, 1, p) < det_pd_power(1.0, 4, p)
        assert det_pd_power(1.0, 4, p) < det_pd_power(1.0, 16, p)

    def test_bounded_in_0_1(self):
        p = det_threshold_power(1e-6)
        assert 0.0 <= det_pd_power(0.0, 1, p) <= 1.0
        assert det_pd_power(100.0, 8, p) <= 1.0

    def test_returns_float(self):
        assert isinstance(
            det_pd_power(1.0, 4, det_threshold_power(1e-6)), float
        )


# ── det_dwell_power ──────────────────────────────────────────────────────────


class TestDetDwellPower:
    @pytest.mark.parametrize(
        "snr,pd_min,pfa",
        [
            (0.5, 0.9, 1e-6),
            (0.3, 0.8, 1e-4),
            (1.0, 0.99, 1e-6),
            (0.7, 0.95, 1e-8),
        ],
    )
    def test_equivalence_with_envelope_dwell(self, snr, pd_min, pfa):
        m_amp = det_dwell(snr, pd_min, pfa, 2048)
        m_pow = det_dwell_power(snr**2, pd_min, pfa, 2048)
        assert m_amp == m_pow

    def test_high_snr_returns_one(self):
        assert det_dwell_power(10000.0, 0.9, 1e-6, 256) == 1

    def test_infeasible_returns_minus_one(self):
        assert det_dwell_power(1e-6, 0.9, 1e-6, 10) == -1

    def test_minimum_dwell_property(self):
        snr_power = 0.25
        m = det_dwell_power(snr_power, 0.9, 1e-6, 2048)
        assert m > 0
        p = det_threshold_power(1e-6)
        assert det_pd_power(snr_power, m, p) >= 0.9 - 1e-12
        if m > 1:
            assert det_pd_power(snr_power, m - 1, p) < 0.9

    def test_returns_int(self):
        assert isinstance(det_dwell_power(1.0, 0.9, 1e-6, 256), int)


# ── det_snr_power ────────────────────────────────────────────────────────────


class TestDetSnrPower:
    @pytest.mark.parametrize("dwell", [1, 2, 4, 8, 16, 32, 64])
    def test_roundtrip(self, dwell):
        pfa = 1e-6
        pd_min = 0.9
        sp = det_snr_power(dwell, pd_min, pfa)
        p = det_threshold_power(pfa)
        assert det_pd_power(sp, dwell, p) >= pd_min - 1e-12

    @pytest.mark.parametrize("dwell", [1, 2, 4, 8, 16, 32, 64])
    def test_equals_amplitude_snr_squared(self, dwell):
        sa = det_snr(dwell, 0.9, 1e-6)
        sp = det_snr_power(dwell, 0.9, 1e-6)
        assert sp == pytest.approx(sa * sa, rel=1e-8)

    def test_coherent_gain(self):
        sp1 = det_snr_power(1, 0.9, 1e-6)
        sp4 = det_snr_power(4, 0.9, 1e-6)
        sp16 = det_snr_power(16, 0.9, 1e-6)
        assert sp16 < sp4 < sp1

    def test_nonnegative(self):
        assert det_snr_power(4, 0.9, 1e-6) >= 0.0

    def test_returns_float(self):
        assert isinstance(det_snr_power(4, 0.9, 1e-6), float)


class TestNonCoherent:
    """Non-coherent integration: order-N_nc Marcum-Q helpers.

    These package the generalized Marcum-Q for N_nc magnitude-summed looks and
    must reduce exactly to the coherent (order-1) helpers at n_noncoh == 1.
    """

    # ── reduce-to-coherent (n_noncoh == 1) ──────────────────────────────────
    @pytest.mark.parametrize("pfa", [1e-2, 1e-3, 1e-6, 1e-9])
    def test_threshold_reduces_to_coherent(self, pfa):
        assert det_threshold_noncoherent(pfa, 1) == det_threshold(pfa)

    @pytest.mark.parametrize("snr,n_coh", [(0.0, 8), (0.5, 8), (1.6, 16)])
    def test_pd_reduces_to_coherent(self, snr, n_coh):
        eta = det_threshold(1e-6)
        assert det_pd_noncoherent(snr, n_coh, 1, eta) == det_pd(
            snr, n_coh, eta
        )

    # ── threshold defining property + monotonicity ──────────────────────────
    @pytest.mark.parametrize("n_noncoh", [1, 2, 4, 8, 16])
    def test_threshold_solves_pfa(self, n_noncoh):
        pfa = 1e-3
        eta = det_threshold_noncoherent(pfa, n_noncoh)
        # eta is the b solving marcum_q(n_noncoh, 0, b) = pfa
        assert marcum_q(n_noncoh, 0.0, eta) == pytest.approx(pfa, rel=1e-6)

    def test_threshold_increases_with_looks(self):
        pfa = 1e-3
        etas = [det_threshold_noncoherent(pfa, m) for m in (1, 2, 4, 8, 16)]
        assert all(a < b for a, b in zip(etas, etas[1:]))

    # ── Pd properties ───────────────────────────────────────────────────────
    @pytest.mark.parametrize("n_noncoh", [2, 4, 8])
    def test_pd_snr_zero_equals_pfa(self, n_noncoh):
        pfa = 1e-3
        eta = det_threshold_noncoherent(pfa, n_noncoh)
        assert det_pd_noncoherent(0.0, 16, n_noncoh, eta) == pytest.approx(
            pfa, rel=1e-6
        )

    def test_pd_monotone_in_snr(self):
        eta = det_threshold_noncoherent(1e-3, 4)
        vals = [
            det_pd_noncoherent(s, 16, 4, eta) for s in (0.0, 0.2, 0.4, 0.8)
        ]
        assert all(a < b for a, b in zip(vals, vals[1:]))

    def test_pd_bounded(self):
        eta = det_threshold_noncoherent(1e-3, 4)
        for s in (0.0, 0.5, 5.0):
            assert 0.0 <= det_pd_noncoherent(s, 16, 4, eta) <= 1.0

    # ── det_n_noncoh (the look-count inverse) ───────────────────────────────
    def test_n_noncoh_strong_signal_one_look(self):
        assert det_n_noncoh(2.0, 16, 0.9, 1e-3, 64) == 1

    def test_n_noncoh_weaker_needs_more(self):
        n_strong = det_n_noncoh(0.4, 16, 0.9, 1e-3, 256)
        n_weak = det_n_noncoh(0.25, 16, 0.9, 1e-3, 256)
        assert 1 < n_strong < n_weak

    def test_n_noncoh_meets_pd(self):
        snr, n_coh, pd_min, pfa = 0.25, 16, 0.9, 1e-3
        k = det_n_noncoh(snr, n_coh, pd_min, pfa, 256)
        eta = det_threshold_noncoherent(pfa, k)
        assert det_pd_noncoherent(snr, n_coh, k, eta) >= pd_min
        # one fewer look must fall short (it is the *minimum*)
        eta_m1 = det_threshold_noncoherent(pfa, k - 1)
        assert det_pd_noncoherent(snr, n_coh, k - 1, eta_m1) < pd_min

    def test_n_noncoh_infeasible_returns_minus_one(self):
        assert det_n_noncoh(1e-4, 1, 0.99, 1e-9, 4) == -1

    def test_return_types(self):
        assert isinstance(det_threshold_noncoherent(1e-3, 4), float)
        assert isinstance(det_pd_noncoherent(0.3, 16, 4, 5.0), float)
        assert isinstance(det_n_noncoh(0.4, 16, 0.9, 1e-3, 64), int)

    # ── Monte-Carlo validation (P1 acceptance: match Q_{N_nc} < 1%) ──────────
    @pytest.mark.parametrize(
        "snr,n_coh,n_noncoh", [(0.0, 16, 4), (0.3, 16, 4), (0.25, 16, 8)]
    )
    def test_pd_matches_monte_carlo(self, snr, n_coh, n_noncoh):
        pfa = 1e-3
        eta = det_threshold_noncoherent(pfa, n_noncoh)
        theory = det_pd_noncoherent(snr, n_coh, n_noncoh, eta)
        # Simulate the normalized statistic R = sqrt(sum_looks |z|^2): each
        # look is unit-variance complex, non-centrality a = sqrt(2*n_coh)*snr.
        rng = np.random.default_rng(0)
        trials = 200_000
        a = math.sqrt(2.0 * n_coh) * snr
        x = rng.standard_normal((trials, n_noncoh)) + a
        y = rng.standard_normal((trials, n_noncoh))
        r = np.sqrt((x * x + y * y).sum(axis=1))
        mc = float((r > eta).mean())
        # absolute agreement to <1% (and within MC sampling error)
        assert abs(theory - mc) < 0.01
