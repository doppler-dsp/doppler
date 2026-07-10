import numpy as np
import pytest

from doppler.agc import AGC

# (0.6 + 0.8j) has unit magnitude, so scaling it by A yields an input of
# magnitude A that exercises both the real and imaginary paths.
DIR = 0.6 + 0.8j


def _const(scale, n, dtype=np.complex64):
    return np.full(n, DIR * scale, dtype=dtype)


def test_create():
    obj = AGC(0.0, 0.0025, 0.05)
    assert obj is not None


def test_step_runs():
    obj = AGC(0.0, 0.0025, 0.05)
    y = obj.step(1.0 + 0.0j)
    assert isinstance(y, complex)


def test_steps_shape_dtype():
    obj = AGC(0.0, 0.0025, 0.05)
    x = np.ones(64, dtype=np.complex64)
    y = obj.steps(x)
    assert y.shape == (64,)
    assert y.dtype == np.complex64


def test_steps_out_param():
    x = np.ones(64, dtype=np.complex64)
    buf = np.zeros(64, dtype=np.complex64)
    obj1 = AGC(0.0, 0.0025, 0.05)
    ret = obj1.steps(x, buf)
    assert ret is buf
    obj2 = AGC(0.0, 0.0025, 0.05)
    np.testing.assert_array_equal(ret, obj2.steps(x))


def test_create_seeds_loop_state():
    # gain_db starts at unity (0 dB); p_avg is seeded with the reference
    # power so the loop starts settled.
    obj = AGC(0.0, 0.0025, 0.05)
    assert obj.gain_db == pytest.approx(0.0, abs=1e-9)


def test_convergence_to_reference():
    # A loud input is driven down until its output power reaches ref_db.
    obj = AGC(0.0, 0.0025, 0.05)
    y = obj.steps(_const(10.0, 4000))
    out_db = 10.0 * np.log10(abs(y[-1]) ** 2)
    assert out_db == pytest.approx(0.0, abs=0.5)
    # gain settles to -20 dB (undoing the 20 dB-loud input).
    assert obj.gain_db == pytest.approx(-20.0, abs=0.5)


def test_linear_in_db():
    # A quiet (-40 dB) and a loud (+40 dB) input settle to the same level
    # within the same sample budget — the defining linear-in-dB property.
    lo = AGC(0.0, 0.0025, 0.05)
    hi = AGC(0.0, 0.0025, 0.05)
    lo_db = 10.0 * np.log10(abs(lo.steps(_const(0.01, 4000))[-1]) ** 2)
    hi_db = 10.0 * np.log10(abs(hi.steps(_const(100.0, 4000))[-1]) ** 2)
    assert lo_db == pytest.approx(0.0, abs=0.5)
    assert hi_db == pytest.approx(0.0, abs=0.5)
    assert lo_db == pytest.approx(hi_db, abs=0.5)


def test_nonzero_reference():
    obj = AGC(-6.0, 0.0025, 0.05)
    y = obj.steps(_const(3.0, 4000))
    out_db = 10.0 * np.log10(abs(y[-1]) ** 2)
    assert out_db == pytest.approx(-6.0, abs=0.5)


def test_steps_converges_like_step():
    # steps() decimates the control loop (gain held over AGC_DECIM-sample
    # chunks), so it is not bit-identical to a per-sample step() loop --
    # but it reaches the same steady-state gain.
    a = AGC(0.0, 0.005, 0.1)
    b = AGC(0.0, 0.005, 0.1)
    x = _const(4.0, 4000)
    a.steps(x)
    for v in x:
        b.step(complex(v))
    assert a.gain_db == pytest.approx(b.gain_db, abs=0.3)


def test_writable_properties():
    obj = AGC(0.0, 0.0025, 0.05)
    assert obj.ref_db == pytest.approx(0.0)
    assert obj.loop_bw == pytest.approx(0.0025)
    assert obj.alpha == pytest.approx(0.05)
    assert obj.decim == 8  # default decimation factor
    assert obj.clip_db == pytest.approx(120.0)  # default — effectively off
    assert obj.gain_update_period == 1  # default — exact per-sample step()
    obj.ref_db = -3.0
    obj.loop_bw = 0.005
    obj.alpha = 0.1
    obj.decim = 16
    obj.clip_db = 3.0
    obj.gain_update_period = 8
    assert obj.ref_db == pytest.approx(-3.0)
    assert obj.loop_bw == pytest.approx(0.005)
    assert obj.alpha == pytest.approx(0.1)
    assert obj.decim == 16
    assert obj.clip_db == pytest.approx(3.0)
    assert obj.gain_update_period == 8


def test_gain_update_period_converges_like_per_sample():
    # gain_update_period > 1 amortises the loop-filter command over P samples
    # (a zero-order hold on the gain); the detector and gain-apply still run
    # every sample, so it is not bit-identical to the per-sample loop but
    # reaches the same steady-state gain.
    base = AGC(0.0, 0.005, 0.1)
    dec = AGC(0.0, 0.005, 0.1)
    dec.gain_update_period = 8
    x = _const(4.0, 4000)
    for v in x:
        base.step(complex(v))
        dec.step(complex(v))
    assert dec.gain_db == pytest.approx(base.gain_db, abs=0.3)
    # output power converged to the 0 dB reference (|y| ~ 1)
    assert abs(dec.step(complex(DIR * 4.0))) == pytest.approx(1.0, abs=0.1)


def test_decimation_factor():
    # decim is tunable to 8 / 16 / 32; every setting still converges the
    # output power to the reference.
    for d in (8, 16, 32):
        agc = AGC(0.0, 0.002, 0.05)
        agc.decim = d
        assert agc.decim == d
        y = agc.steps(_const(8.0, 4000))
        out_db = 10.0 * np.log10(abs(y[-1]) ** 2)
        assert out_db == pytest.approx(0.0, abs=0.5)


def test_gain_db_read_only():
    obj = AGC(0.0, 0.0025, 0.05)
    with pytest.raises(AttributeError):
        obj.gain_db = 5.0


def test_applied_gain_db():
    # applied_gain_db reports the gain the signal last saw: unity (0 dB)
    # at create, and the commanded gain_db once the loop has converged.
    obj = AGC(0.0, 0.0025, 0.05)
    assert obj.applied_gain_db == pytest.approx(0.0, abs=1e-9)
    obj.steps(_const(10.0, 4000))
    assert obj.applied_gain_db == pytest.approx(obj.gain_db, abs=0.5)
    assert obj.applied_gain_db == pytest.approx(-20.0, abs=0.5)


def test_applied_gain_db_read_only():
    obj = AGC(0.0, 0.0025, 0.05)
    with pytest.raises(AttributeError):
        obj.applied_gain_db = 1.0


def test_square_clip():
    # Square clip limits I and Q independently. The first sample has
    # unity gain, so output = clip(input): re clamps to L while im (below
    # L) passes through unchanged -- circular clipping would scale both.
    obj = AGC(0.0, 0.0025, 0.05)
    obj.clip_db = 6.0  # L = 10**(6/20) ~ 1.995
    L = 10.0 ** (6.0 / 20.0)
    y = obj.step(5.0 + 1.0j)
    assert y.real == pytest.approx(L, abs=0.02)
    assert y.imag == pytest.approx(1.0, abs=1e-6)


def test_clip_does_not_affect_convergence():
    # The detector measures the unclipped signal, so a clip leaves the
    # loop untouched -- gain converges identically with or without it.
    free = AGC(0.0, 0.0025, 0.05)
    clipped = AGC(0.0, 0.0025, 0.05)
    clipped.clip_db = -3.0
    x = _const(10.0, 4000)
    free.steps(x)
    clipped.steps(x)
    assert clipped.gain_db == pytest.approx(free.gain_db, abs=1e-9)


def test_reset():
    obj = AGC(0.0, 0.0025, 0.05)
    obj.steps(_const(50.0, 2000))  # perturb the loop
    assert abs(obj.gain_db) > 1.0
    obj.reset()
    assert obj.gain_db == pytest.approx(0.0, abs=1e-9)
    # configuration survives reset
    assert obj.ref_db == pytest.approx(0.0)
    assert obj.loop_bw == pytest.approx(0.0025)
    assert obj.alpha == pytest.approx(0.05)


def test_context_manager():
    with AGC(0.0, 0.0025, 0.05) as obj:
        y = obj.step(1.0 + 0.0j)
    assert isinstance(y, complex)


def test_destroy():
    obj = AGC(0.0, 0.0025, 0.05)
    obj.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        obj.step(1.0 + 0.0j)


# ── telemetry attach (dp_tlm; see docs/design/telemetry.md) ─────────────


def test_set_telemetry_records_gain_series():
    from doppler.telemetry import Telemetry

    tlm = Telemetry(1 << 12)
    obj = AGC(0.0, 0.0025, 0.05)
    obj.set_telemetry(tlm, "agc")  # returns None; raises on failure
    x = np.full(4096, 0.125 + 0j, dtype=np.complex64)
    obj.steps(x)

    recs = tlm.read()
    # one record per decim-chunk control update
    assert len(recs) == 4096 // obj.decim
    gains = recs["value"]
    # quiet input, 0 dB reference: the commanded gain rises monotonically
    # toward +18 dB (10*log10(1/0.125^2)) without overshooting wildly
    assert gains[-1] > gains[0]
    assert gains[-1] == pytest.approx(20.0 * np.log10(1 / 0.125), abs=3.0)
    # the last record equals the live property exactly (float32 narrow)
    assert gains[-1] == pytest.approx(obj.gain_db, abs=1e-4)


def test_set_telemetry_accepts_capsule_and_none():
    from doppler.telemetry import Telemetry

    tlm = Telemetry(1 << 10)
    obj = AGC(0.0, 0.0025, 0.05)
    # the raw capsule is the documented attach point too
    obj.set_telemetry(tlm._capsule, "agc")
    obj.steps(np.ones(64, dtype=np.complex64))
    assert len(tlm.read()) > 0

    # None detaches; further steps emit nothing
    obj.set_telemetry(None, "agc")
    obj.steps(np.ones(64, dtype=np.complex64))
    assert len(tlm.read()) == 0


def test_set_telemetry_failure_raises():
    from doppler.telemetry import Telemetry

    tlm = Telemetry(1 << 10)
    obj = AGC(0.0, 0.0025, 0.05)
    # fill the probe table; the attach must then fail whole -> ValueError
    for i in range(64):
        tlm.probe(f"p{i}")
    with pytest.raises(ValueError):
        obj.set_telemetry(tlm, "agc")
    # failed attach leaves the object detached
    obj.steps(np.ones(64, dtype=np.complex64))
    assert len(tlm.read()) == 0


def test_set_telemetry_wrong_capsule_rejected():
    from doppler.telemetry import Telemetry  # noqa: F401

    obj = AGC(0.0, 0.0025, 0.05)
    with pytest.raises((ValueError, AttributeError)):
        obj.set_telemetry(object(), "agc")


def test_state_roundtrip_with_attachment():
    from doppler.telemetry import Telemetry

    tlm = Telemetry(1 << 10)
    a = AGC(0.0, 0.0025, 0.05)
    a.set_telemetry(tlm, "agc")
    a.steps(np.full(256, 0.5 + 0j, dtype=np.complex64))

    # blob is attachment-independent: restoring into a detached twin
    # reproduces the exact gain state
    b = AGC(0.0, 0.0025, 0.05)
    b.set_state(a.get_state())
    assert b.gain_db == a.gain_db

    # a receiver with its own live attachment keeps it across restore
    tlm2 = Telemetry(1 << 10)
    c = AGC(0.0, 0.0025, 0.05)
    c.set_telemetry(tlm2, "rx.agc")
    c.set_state(a.get_state())
    assert c.gain_db == a.gain_db
    drained = tlm2.read()  # clear anything pre-restore
    del drained
    c.steps(np.full(64, 0.5 + 0j, dtype=np.complex64))
    assert len(tlm2.read()) > 0  # attachment survived the restore
