"""Integration tests for doppler.track.CarrierNda (NDA M-th-power loop).

The non-data-aided carrier-recovery loop: per-sample integer-NCO wipe-off, an
I/Q arm integrate-and-dump at ``n`` dumps per symbol, and an M-th-power phase
discriminator that strips the M-PSK data — so it acquires the carrier with **no
symbol timing and no data present** (a bare carrier, or modulated data before
timing lock). It locks to one of ``m`` phases (M-fold ambiguity, resolved
downstream). ``steps`` returns the de-rotated sample stream; ``norm_freq`` is
the tracked carrier and ``lock`` the carrier lock metric.
"""

import numpy as np
import pytest

from doppler.mpsk import mpsk_map
from doppler.track import CarrierNda

SPS, N = 8, 4


def _unmod(f0, nsamp, seed=0, sigma=0.05):
    """An unmodulated (data-free) carrier at f0 cycles/sample, plus AWGN."""
    rng = np.random.default_rng(seed)
    k = np.arange(nsamp)
    rx = np.exp(2j * np.pi * f0 * k)
    rx = rx + sigma * (
        rng.standard_normal(nsamp) + 1j * rng.standard_normal(nsamp)
    )
    return rx.astype(np.complex64)


def _moddata(m, f0, nsym, seed=0, sigma=0.1):
    """Random M-PSK at SPS samples/symbol, carrier f0 — no timing align."""
    rng = np.random.default_rng(seed)
    labels = rng.integers(0, m, nsym).astype(np.uint8)
    sig = np.repeat(mpsk_map(labels, m), SPS).astype(np.complex64)
    k = np.arange(sig.size)
    rx = sig * np.exp(2j * np.pi * f0 * k)
    rx = rx + sigma * (
        rng.standard_normal(rx.size) + 1j * rng.standard_normal(rx.size)
    )
    return rx.astype(np.complex64)


# -------------------------------------------------------------------- #
# Lifecycle / API                                                      #
# -------------------------------------------------------------------- #
def test_create_defaults_and_kwargs():
    assert CarrierNda() is not None
    c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.002, sps=8, n=4, m=4)
    assert c.m == 4 and c.n == 4 and c.sps == 8
    assert c.norm_freq == pytest.approx(0.002)
    assert c.bn == pytest.approx(0.01)


def test_context_manager_and_destroy():
    with CarrierNda(m=4):
        pass
    CarrierNda(m=8).destroy()


@pytest.mark.parametrize("m", [2, 4, 8])
def test_valid_orders(m):
    assert CarrierNda(m=m).m == m


@pytest.mark.parametrize("m", [0, 1, 3, 5, 16])
def test_invalid_order_raises(m):
    with pytest.raises(MemoryError):
        CarrierNda(m=m)


@pytest.mark.parametrize("sps,n", [(8, 3), (8, 0), (0, 4), (10, 4)])
def test_invalid_arm_geometry_raises(sps, n):
    # sps must be a positive whole multiple of n (arm_len = sps/n)
    with pytest.raises(MemoryError):
        CarrierNda(sps=sps, n=n, m=4)


def test_setters():
    c = CarrierNda(m=4)
    c.bn = 0.02
    assert c.bn == pytest.approx(0.02)
    c.norm_freq = 0.003
    assert c.norm_freq == pytest.approx(0.003)


def test_steps_returns_derotated_same_length():
    c = CarrierNda(m=4)
    x = _unmod(0.001, 1000)
    y = c.steps(x)
    assert y.shape == x.shape and y.dtype == np.complex64


# -------------------------------------------------------------------- #
# The headline: lock with no data, and with no timing                  #
# -------------------------------------------------------------------- #
@pytest.mark.parametrize("m", [2, 4, 8])
def test_cold_start_unmodulated_carrier(m):
    # a bare carrier (no data at all) is acquired by the M-th-power loop
    c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0, sps=SPS, n=N, m=m)
    c.steps(_unmod(0.001, 40000))
    assert c.norm_freq == pytest.approx(0.001, abs=5e-4)
    # lock peaks at the per-M lock_scale (1 / 0.619 / 0.412)
    peak = {2: 1.0, 4: 0.619, 8: 0.412}[m]
    assert c.lock > 0.5 * peak


@pytest.mark.parametrize("m", [2, 4, 8])
def test_cold_start_modulated_no_timing(m):
    # modulated random M-PSK, NO symbol-timing alignment — still locks
    c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0, sps=SPS, n=N, m=m)
    c.steps(_moddata(m, 0.001, 6000))
    assert c.norm_freq == pytest.approx(0.001, abs=5e-4)
    peak = {2: 1.0, 4: 0.619, 8: 0.412}[m]
    assert c.lock > 0.4 * peak


def test_reset_reproducible():
    c = CarrierNda(m=4)
    x = _unmod(0.0012, 8000, seed=3)
    c.steps(x)
    f1, l1 = c.norm_freq, c.lock
    c.reset()
    c.steps(x)
    assert c.norm_freq == f1 and c.lock == l1


def test_empty_input():
    c = CarrierNda(m=4)
    y = c.steps(np.zeros(0, dtype=np.complex64))
    assert y.shape == (0,)


# -------------------------------------------------------------------- #
# Amplitude invariance, streaming, and the gh-219 output guarantee     #
# -------------------------------------------------------------------- #
@pytest.mark.parametrize("scale", [0.01, 0.1, 1.0])
def test_amplitude_invariance(scale):
    # The arm AGC drives the phase detector to unit power, so the loop gain is
    # independent of input scale: an input at or below the unit reference
    # converges to the same carrier regardless of amplitude. Cold input well
    # above unit is out of spec — the slow AGC cannot normalize it before the
    # discriminator reacts; the front end is expected to deliver roughly
    # unit-scaled samples.
    base = _unmod(0.001, 40000, seed=5)
    ref = CarrierNda(bn=0.01, sps=SPS, n=N, m=4)
    ref.steps(base)
    c = CarrierNda(bn=0.01, sps=SPS, n=N, m=4)
    c.steps((scale * base).astype(np.complex64))
    assert c.norm_freq == pytest.approx(0.001, abs=5e-4)
    assert c.norm_freq == pytest.approx(ref.norm_freq, abs=1e-4)


def test_block_size_invariance():
    # Streaming the input in chunks equals a single-block run (loop state
    # carries across calls). Each chunk is copied because steps() returns a
    # view into a reused buffer (see test_output_survives_buffer_grow).
    x = _moddata(4, 0.001, 4000, seed=8)
    single = CarrierNda(bn=0.01, sps=SPS, n=N, m=4).steps(x)
    c = CarrierNda(bn=0.01, sps=SPS, n=N, m=4)
    chunks = [c.steps(x[i : i + 777]).copy() for i in range(0, x.size, 777)]
    assert np.array_equal(np.concatenate(chunks), single)


def test_output_survives_buffer_grow():
    # gh-219: a returned array must survive a later buffer GROW — the old
    # buffer is retired (kept alive), not freed. Mirrors ddc's regression test.
    c = CarrierNda(m=4)
    x = _unmod(0.001, 6000)
    y1 = c.steps(x[:1000])
    snapshot = y1.copy()
    c.steps(x[:5000])  # forces the output buffer to grow
    assert np.array_equal(y1, snapshot)


def test_steps_out_writes_into_callers_buffer():
    c = CarrierNda(m=4)
    x = _unmod(0.001, 1000)
    out = np.zeros(max(c.steps_max_out(), len(x)), dtype=np.complex64)
    y = c.steps(x, out=out)
    assert np.shares_memory(y, out)


def test_steps_out_undersized_raises():
    c = CarrierNda(m=4)
    with pytest.raises(ValueError):
        c.steps(_unmod(0.001, 1000), out=np.zeros(1, dtype=np.complex64))
