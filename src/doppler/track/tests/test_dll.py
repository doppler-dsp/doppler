"""Integration tests for doppler.track.Dll (code-tracking loop)."""

import numpy as np
import pytest

from doppler.track import Dll

SF, SPS = 63, 4


def _code(seed=1):
    return np.random.default_rng(seed).integers(0, 2, SF).astype(np.uint8)


def _signal(code, delta, nper, seed=9, const_data=False):
    """Carrier-free spread signal at code rate (1+delta), BPSK data/period.

    With ``const_data`` the per-period BPSK symbol is held at +1, isolating
    code tracking from the data-symbol vs code-period timing (a separate
    symbol-sync concern): under a code-rate offset the code period slowly
    slides against the data-symbol clock, so a per-period data flip can split
    a prompt integration across two opposite symbols and cut the despread.
    """
    rng = np.random.default_rng(seed)
    n = SF * SPS * nper
    rx = np.empty(n, np.complex64)
    cph = 0.0
    for p in range(nper):
        data = 1 if const_data else (1 if rng.integers(0, 2) else -1)
        for i in range(SF * SPS):
            idx = int(cph % SF)
            rx[p * SF * SPS + i] = data * (-1.0 if code[idx] & 1 else 1.0)
            cph += (1 + delta) / SPS
    return rx


def test_create_and_guard():
    assert Dll(_code(), SPS, 0.0, 0.01, 0.707, 0.5) is not None


def test_context_manager_and_destroy():
    with Dll(_code(), SPS, 0.0, 0.01, 0.707, 0.5):
        pass
    d = Dll(_code(), SPS, 0.0, 0.01, 0.707, 0.5)
    d.destroy()


def test_properties():
    d = Dll(_code(), SPS, 0.0, bn=0.01, zeta=0.707, spacing=0.5)
    assert d.bn == pytest.approx(0.01)
    assert d.code_rate == pytest.approx(1.0)
    d.bn = 0.005
    assert d.bn == pytest.approx(0.005)


def test_one_prompt_per_period():
    code = _code()
    rx = _signal(code, 0.0, 100)
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)
    sym = d.steps(rx)
    assert sym.dtype == np.complex64
    assert len(sym) == 100


def test_tracks_code_doppler():
    code = _code(11)
    delta = 5e-4
    # const_data isolates code-Doppler tracking from data-symbol/code-period
    # async; the replica speeds up to match the incoming chip rate and the
    # fractional-boundary discriminator holds sub-chip lock (clean despread).
    rx = _signal(code, delta, 1500, seed=9, const_data=True)
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)
    sym = d.steps(rx)
    assert d.code_rate == pytest.approx(1.0 + delta, abs=1e-4)
    # 0.85, not 0.9: the point-sample 2x-oversampled replica (dll_replica)
    # has a small, expected SNR cost versus the old dwell-integrated exact
    # matched filter (measured ~0.865 here; same class of retune as
    # test_dll_core.c's Test 3).
    assert np.mean(np.abs(sym[len(sym) // 2 :].real)) > 0.85


def test_pulls_in_static_offset():
    code = _code(13)
    rx = _signal(code, 0.0, 800, seed=17)
    d = Dll(code, SPS, 0.4, 0.005, 0.707, 0.5)  # 0.4-chip offset
    d.steps(rx[: SF * SPS * 3])
    assert abs(d.last_error) > 0.05  # starts misaligned
    d.steps(rx[SF * SPS * 3 :])
    assert abs(d.last_error) < 0.05  # pulled in


def test_reset_reproducible():
    code = _code(21)
    rx = _signal(code, 3e-4, 300, seed=5)
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)
    d.steps(rx)
    r1, e1 = d.code_rate, d.last_error
    d.reset()
    d.steps(rx)
    assert (r1, e1) == (d.code_rate, d.last_error)


# --- segments > 1: sub-epoch partial despreading for an async symbol clock ---
TE = SF * SPS


def _async_signal(code, nsym, dsym, phi, seed=0):
    """Carrier-free DSSS, code-aligned, data on an independent clock
    Tsym=TE*(1+dsym), phase phi (samples)."""
    rng = np.random.default_rng(seed)
    csign = np.where(code & 1, -1.0, 1.0)
    chip = np.repeat(csign, SPS)
    tsym = TE * (1.0 + dsym)
    n = int(nsym * tsym) + 2 * TE
    data = (rng.integers(0, 2, nsym + 6) * 2 - 1).astype(float)
    idx = np.arange(n)
    si = np.clip(np.floor((idx - phi) / tsym).astype(int), 0, len(data) - 1)
    return (data[si] * chip[idx % TE]).astype(np.complex64), data, tsym


def test_segments_default_is_one():
    assert Dll(_code(), SPS, 0.0, 0.005, 0.707, 0.5).segments == 1


def test_segments_emits_partials_per_epoch():
    code = _code(11)
    rx, _, _ = _async_signal(code, 400, 0.0, 0.0)
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    part = d.steps(rx)
    nep = len(rx) // TE
    assert d.segments == 4
    assert (nep - 1) * 4 <= len(part) <= (nep + 1) * 4


def test_segments_recover_async_data():
    # the partials carry an async symbol clock that a coherent full-epoch
    # despread would collapse; a known-timing symbol despread recovers it
    code = _code(11)
    phi = 0.37 * TE
    rx, data, tsym = _async_signal(code, 2000, 3e-3, phi)
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    part = d.steps(rx)
    acc = np.zeros(2000 + 8)
    for pp in range(len(part)):
        s = int(np.floor((TE * (pp + 0.5) / 4 - phi) / tsym))
        if 0 <= s < 2000:
            acc[s] += part[pp].real
    dec = np.where(acc[2:1998] >= 0, 1, -1)
    assert min(np.mean(dec != data[2:1998]), np.mean(dec == data[2:1998])) == 0


# --- carrier-present: the despreader removes only the code; the non-coherent
# code loop is carrier-blind and carrier recovery is a downstream stage. ---


def _carrier_async_signal(
    code, nsym, dsym, phi, f0, dcode=0.0, const_data=False, seed=0
):
    """Async-data DSSS with a residual CARRIER (``f0`` cyc/sample) on it.

    Models the despreader's realistic input: acquisition has removed the bulk
    Doppler, leaving a small residual carrier (``f0`` ≲ ½ an epoch FFT bin),
    plus a code-rate offset ``dcode`` and a data clock ``Tsym=TE*(1+dsym)`` at
    phase ``phi``.  Carrier recovery is a *downstream* problem, so the carrier
    is left on the samples here.
    """
    rng = np.random.default_rng(seed)
    csign = np.where(code & 1, -1.0, 1.0)
    tsym = TE * (1.0 + dsym)
    n = int(nsym * tsym) + 2 * TE
    idx = np.arange(n)
    if const_data:
        data = np.ones(nsym + 6)
    else:
        data = (rng.integers(0, 2, nsym + 6) * 2 - 1).astype(float)
    si = np.clip(np.floor((idx - phi) / tsym).astype(int), 0, len(data) - 1)
    cph = (idx * (1.0 + dcode) / SPS).astype(int) % SF
    carrier = np.exp(2j * np.pi * f0 * idx)
    rx = (data[si] * csign[cph] * carrier).astype(np.complex64)
    return rx, data, tsym


def test_segments_streaming_block_invariant():
    # a streaming despreader feeds blocks and keeps every result, so steps()
    # must be block-size invariant and return independent arrays. (Regression:
    # the binding returned a view into a reused scratch buffer -> successive
    # blocks aliased and the output depended on the chunking.)
    code = _code(11)
    rx, _, _ = _carrier_async_signal(
        code, 300, 3e-3, 0.37 * TE, f0=2e-3, seed=4
    )
    full = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4).steps(rx)
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    blocks = [d.steps(rx[i : i + TE]) for i in range(0, len(rx), TE)]
    chunked = np.concatenate(blocks)
    n = min(len(full), len(chunked))
    assert np.allclose(full[:n], chunked[:n], atol=1e-6)
    assert not np.shares_memory(blocks[0], blocks[1])  # independent arrays


def test_segments_carrier_present_holds_code_lock():
    # the non-coherent (|E|-|L|) discriminator is carrier-blind, so the loop
    # tracks a code-rate offset with a residual carrier still on the samples
    # (const data isolates code tracking from the async-symbol straddle).
    code = _code(11)
    dcode = 3e-4
    rx, _, _ = _carrier_async_signal(
        code, 1500, 0.0, 0.0, f0=1e-3, dcode=dcode, const_data=True, seed=7
    )
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5, segments=4)
    d.steps(rx)
    assert d.code_rate == pytest.approx(1.0 + dcode, abs=1e-4)


def test_segments_carrier_present_output_recoverable():
    # despread partials carry the async BPSK with the residual carrier still on
    # them; a downstream carrier wipe (genie) + known-timing symbol despread
    # recovers every bit -> the despreader removed the code losslessly.
    code = _code(11)
    phi, f0 = 0.37 * TE, 1e-3
    rx, data, tsym = _carrier_async_signal(
        code, 2000, 3e-3, phi, f0=f0, seed=3
    )
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    part = d.steps(rx)
    acc = np.zeros(2000 + 8, dtype=complex)
    for pp in range(len(part)):
        t = TE * (pp + 0.5) / 4
        s = int(np.floor((t - phi) / tsym))
        if 0 <= s < 2000:
            acc[s] += part[pp] * np.exp(
                -2j * np.pi * f0 * t
            )  # downstream wipe
    # a constant integration phase remains; the data is on the dominant axis
    use = (
        acc.real
        if np.sum(np.abs(acc.real)) >= np.sum(np.abs(acc.imag))
        else acc.imag
    )
    dec = np.where(use[2:1998] >= 0, 1, -1)
    assert min(np.mean(dec != data[2:1998]), np.mean(dec == data[2:1998])) == 0


# --- always-on code-lock detector (reuses acquisition's non-coherent test) ---


def _noise(n, seed):
    rng = np.random.default_rng(seed)
    return (
        (rng.standard_normal(n) + 1j * rng.standard_normal(n)) / np.sqrt(2)
    ).astype(np.complex64)


def test_lock_defaults_unlocked():
    # a fresh loop reports a clean, unlocked detector before any input
    d = Dll(_code(), SPS, 0.0, 0.005, 0.707, 0.5, segments=4)
    assert d.locked is False
    assert d.lock_stat == 0.0
    assert d.noise_est == 0.0


def test_lock_acquires_on_signal():
    # a despread signal (carrier and async data present) drives the
    # non-coherent statistic well above the default CFAR threshold
    code = _code(11)
    rx, _, _ = _carrier_async_signal(
        code, 1500, 3e-3, 0.37 * TE, f0=1e-3, seed=3
    )
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    d.steps(rx)
    assert d.locked is True
    assert d.lock_stat > 8.5  # > det_threshold_noncoherent(1e-3, 20)


def test_lock_acquires_segments_one():
    # the detector also runs in the coherent full-epoch (segments=1) mode
    code = _code(11)
    rx = _signal(code, 0.0, 1500, const_data=True)
    d = Dll(code, SPS, 0.0, 0.005, 0.707, 0.5)  # segments=1
    d.steps(rx)
    assert d.locked is True


def test_lock_stays_low_on_noise():
    # noise only: prompt power matches the off-peak (noise) reference, so the
    # statistic sits near sqrt(2*N)~6.3 under H0 and stays below threshold
    code = _code(11)
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    d.steps(_noise(1500 * TE, seed=2))
    assert d.locked is False
    assert d.lock_stat < 8.5
    assert d.lock_stat == pytest.approx(np.sqrt(2 * 20), rel=0.4)


def test_configure_lock_changes_only_threshold():
    # configure_lock(pfa, n_looks) sets the decision threshold; the statistic
    # itself is config-independent for a given stream + n_looks, so two pfas
    # yield the same R and decisions consistent with their thresholds.
    # (locked is verify-counted — the final-comparison equivalence below
    # holds because this strong signal makes every decision agree.)
    from doppler.detection import det_threshold_noncoherent

    code = _code(11)
    rx, _, _ = _carrier_async_signal(
        code, 1500, 3e-3, 0.37 * TE, f0=1e-3, seed=3
    )
    a = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    a.configure_lock(1e-3, 20)
    a.steps(rx)
    b = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    b.configure_lock(1e-12, 20)
    b.steps(rx)
    assert b.lock_stat == pytest.approx(a.lock_stat, rel=1e-6)
    assert a.locked == (a.lock_stat > det_threshold_noncoherent(1e-3, 20))
    assert b.locked == (b.lock_stat > det_threshold_noncoherent(1e-12, 20))


def test_configure_lock_rejects_bad_pfa():
    d = Dll(_code(), SPS, 0.0, 0.005, 0.707, 0.5, segments=4)
    with pytest.raises(ValueError):
        d.configure_lock(0.0, 20)
    with pytest.raises(ValueError):
        d.configure_lock(1.0, 20)


def test_lock_reset_clears():
    code = _code(11)
    rx, _, _ = _carrier_async_signal(
        code, 1500, 3e-3, 0.37 * TE, f0=1e-3, seed=3
    )
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    d.steps(rx)
    assert d.locked is True
    d.reset()
    assert d.locked is False
    assert d.lock_stat == 0.0
    assert d.noise_est == 0.0


def test_configure_lock_raw_sets_geometry_directly():
    # configure_lock() only ever derives a symmetric threshold and fixes
    # n_down=2; configure_lock_raw() is the escape hatch -- an unreachable
    # declare threshold never locks even on a strong signal.
    code = _code(11)
    rx, _, _ = _carrier_async_signal(
        code, 1500, 3e-3, 0.37 * TE, f0=1e-3, seed=3
    )
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    d.configure_lock_raw(
        up_thresh=1e9,
        down_thresh=1e9,
        n_looks=20,
        alpha=1.0 / 1024.0,
        n_up=1,
        n_down=1,
    )
    d.steps(rx)
    assert d.locked is False


def test_configure_lock_raw_clears_statistic():
    code = _code(11)
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    d.steps(
        _carrier_async_signal(code, 1500, 3e-3, 0.37 * TE, f0=1e-3, seed=3)[0]
    )
    assert d.lock_stat > 0.0
    d.configure_lock_raw(
        up_thresh=3.0,
        down_thresh=2.5,
        n_looks=20,
        alpha=1.0 / 1024.0,
        n_up=2,
        n_down=2,
    )
    assert d.lock_stat == 0.0  # retune clears the in-flight statistic


def test_configure_lock_raw_independent_verify_counts():
    # n_up=1 declares on the very first above-threshold decision; a much
    # larger n_down means the lock survives a single noisy decision that
    # configure_lock()'s fixed n_down=2 would have dropped.
    code = _code(11)
    rx, _, _ = _carrier_async_signal(
        code, 1500, 3e-3, 0.37 * TE, f0=1e-3, seed=3
    )
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=4)
    d.configure_lock_raw(
        up_thresh=3.0,
        down_thresh=2.5,
        n_looks=20,
        alpha=1.0 / 1024.0,
        n_up=1,
        n_down=10,
    )
    d.steps(rx[: TE * 20])  # one n_looks-window's worth: a single decision
    assert d.locked is True
