"""Benchmarks for doppler.detection scalar functions.

Run: pytest src/doppler/detection/benchmarks/bench_detection.py --benchmark-only

All functions are pure scalar computation (no array allocation), so the
dominant cost is C function overhead + Python call overhead.  The results
show how many calls per second are achievable from Python, which matters
when these functions are called in tight planning loops or parameter sweeps.
"""

from doppler.detection import (
    det_dwell,
    det_dwell_power,
    det_pd,
    det_pd_power,
    det_snr,
    det_snr_power,
    det_threshold,
    det_threshold_power,
    marcum_q,
)

# Pre-computed thresholds so benchmark loops don't include threshold overhead.
ETA = det_threshold(1e-6)
P = det_threshold_power(1e-6)


# ── marcum_q ──────────────────────────────────────────────────────────────────


def test_bench_marcum_q_m1(benchmark):
    """Q_1(2.0, 1.0) — typical single-pulse case."""
    benchmark(marcum_q, 1, 2.0, 1.0)


def test_bench_marcum_q_m4(benchmark):
    """Q_4(2.0, 3.0) — 4-pulse integration; more Poisson terms."""
    benchmark(marcum_q, 4, 2.0, 3.0)


def test_bench_marcum_q_large_a(benchmark):
    """Q_1(50.0, 1.0) — erfc fast path (large signal, avoids series)."""
    benchmark(marcum_q, 1, 50.0, 1.0)


# ── det_threshold / det_threshold_power ──────────────────────────────────────


def test_bench_det_threshold(benchmark):
    """Amplitude threshold: √(−2·ln pfa)."""
    benchmark(det_threshold, 1e-6)


def test_bench_det_threshold_power(benchmark):
    """Power threshold: −ln pfa."""
    benchmark(det_threshold_power, 1e-6)


# ── det_pd / det_pd_power ─────────────────────────────────────────────────────


def test_bench_det_pd_m1(benchmark):
    """Pd for single-pulse envelope detector."""
    benchmark(det_pd, 1.0, 1, ETA)


def test_bench_det_pd_m4(benchmark):
    """Pd for 4-pulse envelope detector."""
    benchmark(det_pd, 1.0, 4, ETA)


def test_bench_det_pd_m16(benchmark):
    """Pd for 16-pulse envelope detector."""
    benchmark(det_pd, 1.0, 16, ETA)


def test_bench_det_pd_power_m4(benchmark):
    """Pd for 4-pulse power detector."""
    benchmark(det_pd_power, 1.0, 4, P)


# ── det_dwell / det_dwell_power ───────────────────────────────────────────────


def test_bench_det_dwell(benchmark):
    """Minimum dwell search: snr=0.5, pd=0.9, pfa=1e-6 (moderate search)."""
    benchmark(det_dwell, 0.5, 0.9, 1e-6, 512)


def test_bench_det_dwell_power(benchmark):
    """Minimum dwell search (power variant)."""
    benchmark(det_dwell_power, 0.25, 0.9, 1e-6, 512)


# ── det_snr / det_snr_power ───────────────────────────────────────────────────


def test_bench_det_snr_m4(benchmark):
    """SNR requirement for 4-pulse dwell: bisection over snr."""
    benchmark(det_snr, 4, 0.9, 1e-6)


def test_bench_det_snr_m16(benchmark):
    """SNR requirement for 16-pulse dwell."""
    benchmark(det_snr, 16, 0.9, 1e-6)


def test_bench_det_snr_power_m4(benchmark):
    """SNR requirement (power) for 4-pulse dwell."""
    benchmark(det_snr_power, 4, 0.9, 1e-6)
