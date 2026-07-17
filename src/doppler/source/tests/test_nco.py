"""Tests for the NCO Python extension.

Mirrors test_nco_core.c: lifecycle, zero-freq, quarter-rate sequence,
phase continuity, nmax scaling, overflow carry flag, property accessors,
ctrl-port FM shift.
"""

import resource

import numpy as np

from doppler.source import NCO

# ── Lifecycle ────────────────────────────────────────────────────────


def test_create_defaults():
    obj = NCO()
    assert obj is not None


def test_create_params():
    obj = NCO(norm_freq=0.25, nmax=0)
    assert obj is not None


def test_context_manager():
    with NCO(0.0, 0):
        pass


def test_destroy():
    obj = NCO(0.0, 0)
    obj.destroy()


# ── Zero frequency ───────────────────────────────────────────────────


def test_zero_freq_all_zero():
    """phase_inc = 0 → accumulator never moves → all outputs are 0."""
    nco = NCO(0.0, 0)
    out = nco.steps_u32(8)
    assert out.dtype == np.uint32
    assert np.all(out == 0)


# ── Quarter-rate sequence ────────────────────────────────────────────


def test_quarter_rate_sequence():
    """norm_freq = 0.25 → phase_inc = 0x40000000.

    Four samples emitted before-increment:
      out[0] = 0x00000000, out[1] = 0x40000000,
      out[2] = 0x80000000, out[3] = 0xC0000000.
    """
    nco = NCO(0.25, 0)
    assert nco.phase_inc == 0x40000000
    out = nco.steps_u32(4)
    expected = np.array(
        [0x00000000, 0x40000000, 0x80000000, 0xC0000000], dtype=np.uint32
    )
    np.testing.assert_array_equal(out, expected)


# ── Phase continuity ─────────────────────────────────────────────────


def test_phase_continuity():
    """Two consecutive blocks of n must equal one block of 2n."""
    a = NCO(0.1, 0)
    b = NCO(0.1, 0)
    ref = a.steps_u32(16).copy()
    blk0 = b.steps_u32(8).copy()
    blk1 = b.steps_u32(8).copy()
    np.testing.assert_array_equal(blk0, ref[:8])
    np.testing.assert_array_equal(blk1, ref[8:])


# ── nmax scaling ─────────────────────────────────────────────────────


def test_nmax_scaling():
    """At quarter-rate with nmax=4, values cycle 0,1,2,3,0,..."""
    nco = NCO(0.25, 4)
    out = nco.steps_u32_scaled(5)
    np.testing.assert_array_equal(out, [0, 1, 2, 3, 0])


def test_nmax_zero_raw():
    """nmax=0 falls back to raw accumulator, same as steps_u32."""
    a = NCO(0.3, 0)
    b = NCO(0.3, 0)
    raw = a.steps_u32(8).copy()
    scaled = b.steps_u32_scaled(8).copy()
    np.testing.assert_array_equal(scaled, raw)


# ── Overflow carry flag ───────────────────────────────────────────────


def test_ovf_carry_flag():
    """At norm_freq=0.25, carry fires every 4th sample (on the wrap).

    phase: 0 -> 0x40000000 -> 0x80000000 -> 0xC0000000 -> [wrap] -> 0 ...
    carry: 0    0             0             1              ...
    """
    nco = NCO(0.25, 0)
    ph, ov = nco.steps_u32_ovf(8)
    expected_ph = np.array(
        [
            0x00000000,
            0x40000000,
            0x80000000,
            0xC0000000,
            0x00000000,
            0x40000000,
            0x80000000,
            0xC0000000,
        ],
        dtype=np.uint32,
    )
    expected_ov = np.array([0, 0, 0, 1, 0, 0, 0, 1], dtype=np.uint8)
    np.testing.assert_array_equal(ph, expected_ph)
    np.testing.assert_array_equal(ov, expected_ov)


def test_ovf_return_types():
    nco = NCO(0.25, 0)
    ph, ov = nco.steps_u32_ovf(4)
    assert ph.dtype == np.uint32
    assert ov.dtype == np.uint8


# ── Property accessors ────────────────────────────────────────────────


def test_property_norm_freq():
    nco = NCO(0.25, 0)
    assert nco.norm_freq == 0.25


def test_property_phase_inc():
    nco = NCO(0.25, 0)
    assert nco.phase_inc == 0x40000000


def test_property_phase_get_set():
    nco = NCO(0.25, 0)
    assert nco.phase == 0
    nco.phase = 0x80000000
    assert nco.phase == 0x80000000


def test_set_norm_freq():
    nco = NCO(0.25, 0)
    nco.norm_freq = 0.5
    assert nco.phase_inc == 0x80000000
    # phase is unchanged by set_norm_freq
    assert nco.phase == 0


def test_reset_zeroes_phase():
    nco = NCO(0.5, 0)
    _ = nco.steps_u32(3)  # advance phase
    nco.reset()
    assert nco.phase == 0
    assert nco.phase_inc == 0x80000000  # norm_freq unchanged


# ── Large single-call output (#116 regression) ───────────────────────


def test_steps_u32_large_n_no_overflow():
    """All NCO steps_* methods size their output buffer to n, not a fixed cap;
    a large single call must not overflow (regression for #116)."""
    n = 393_216  # > NCO_MAX_OUT (65536), the crash size
    assert NCO(norm_freq=0.1).steps_u32(n).shape == (n,)
    assert NCO(norm_freq=0.1).steps_u32_scaled(n).shape == (n,)
    ph, ovf = NCO(norm_freq=0.1).steps_u32_ovf(n)
    assert ph.shape == (n,) and ovf.shape == (n,)


def test_steps_u32_large_matches_chunked():
    """A large steps_u32 equals the same span pulled in chunks."""
    n = 200_000
    big = NCO(norm_freq=0.1).steps_u32(n)
    nco = NCO(norm_freq=0.1)
    chunked = np.concatenate(
        [nco.steps_u32(50_000), nco.steps_u32(n - 50_000)]
    )
    assert np.array_equal(big, chunked)


# ── ctrl-port FM shift ────────────────────────────────────────────────


def test_steps_u32_ctrl_constant_shift():
    """Constant ctrl=0.25 with base norm_freq=0 equals NCO at 0.25."""
    nco_ctrl = NCO(norm_freq=0.0, nmax=0)
    nco_ref = NCO(norm_freq=0.25, nmax=0)
    ctrl = np.full(8, 0.25, dtype=np.float32)
    out_ctrl = nco_ctrl.steps_u32_ctrl(ctrl)
    out_ref = nco_ref.steps_u32(8)
    np.testing.assert_array_equal(out_ctrl, out_ref)


def test_steps_u32_ctrl_no_base_mutation():
    """steps_u32_ctrl must not modify the base norm_freq/phase_inc."""
    nco = NCO(norm_freq=0.0, nmax=0)
    ctrl = np.full(8, 0.25, dtype=np.float32)
    nco.steps_u32_ctrl(ctrl)
    assert nco.norm_freq == 0.0
    assert nco.phase_inc == 0


def test_steps_u32_ctrl_output_length():
    nco = NCO(norm_freq=0.1, nmax=0)
    ctrl = np.zeros(16, dtype=np.float32)
    out = nco.steps_u32_ctrl(ctrl)
    assert out.shape == (16,)
    assert out.dtype == np.uint32


def test_steps_u32_ctrl_no_leak_in_tight_loop():
    """Regression: jm's default cached-buffer + gh-437 weakref-gated
    retire template leaks unboundedly under `x = obj.method(...)` in a
    loop (the previous return value is still referenced at the moment
    the next call runs, since Python only drops it after the RHS
    finishes evaluating -- see source_ext_nco.c's steps_u32_ctrl
    comment). This method deliberately allocates a fresh NumPy array
    per call instead, matching steps_u32/steps_u32_scaled/steps_u32_ovf.
    Confirmed leaking ~12 KB/call before the fix; assert well under
    that over a loop long enough to make a regression obvious."""
    nco = NCO(norm_freq=1.0 / 2046, nmax=0)
    ctrl = np.full(2046, 1e-7, dtype=np.float32)
    for _ in range(2000):
        _ = nco.steps_u32_ctrl(ctrl)
    start_kb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    for _ in range(50_000):
        _ = nco.steps_u32_ctrl(ctrl)
    end_kb = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    # The old cached-buffer scheme leaked ~12 KB/call -> ~600 MB over
    # 50,000 calls; a generous 20 MB ceiling catches any reintroduction
    # while tolerating normal allocator/interpreter noise.
    assert (end_kb - start_kb) < 20_000
