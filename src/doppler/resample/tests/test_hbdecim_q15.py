"""Integration tests for HalfbandDecimatorQ15 — Q15 halfband 2:1 decimator.

Frequency-domain and SNR tests use scipy to generate proper halfband
coefficients; structural tests replicate the C-level checks at the
Python boundary.
"""

import numpy as np
import pytest

from doppler.resample import HalfbandDecimatorQ15

# ── shared coefficient fixture ─────────────────────────────────────────────


@pytest.fixture(scope="module")
def h51():
    """Compact 19-tap FIR branch for a 60 dB halfband decimator.

    Uses _halfband_bank to get the non-zero-only FIR polyphase branch
    (same format that HalfbandDecimator and hbdecim_create expect).
    sum(h[:K]) ≈ 0.5 so that FIR + delay-branch together give unity DC gain.
    """
    from doppler.resample import _halfband_bank

    bank = _halfband_bank(60.0, 0.4, 0.6)
    centre = bank.shape[1] // 2
    fir_row = (
        0 if abs(float(bank[0, centre])) < abs(float(bank[1, centre])) else 1
    )
    return np.ascontiguousarray(bank[fir_row]).astype(np.float32)


@pytest.fixture
def dec51(h51):
    d = HalfbandDecimatorQ15(h51)
    yield d
    d.destroy()


# ── helpers ────────────────────────────────────────────────────────────────


def _iq_tone(n, f0, amplitude=20000):
    """Interleaved int16 IQ tone at normalised frequency f0."""
    t = np.arange(n, dtype=np.float64)
    i = (amplitude * np.cos(2 * np.pi * f0 * t)).astype(np.int16)
    q = (amplitude * np.sin(2 * np.pi * f0 * t)).astype(np.int16)
    return np.stack([i, q], axis=1).ravel()


def _complex_from_iq(y_iq, scale=1.0):
    return (
        y_iq[0::2].astype(np.float64) + 1j * y_iq[1::2].astype(np.float64)
    ) / scale


def _windowed_amplitude(y_c):
    """Hann-windowed peak amplitude — avoids spectral-leakage bias.

    For a complex signal y[n] = A*exp(2πjf₀n) the full amplitude sits in
    ONE FFT bin (positive or negative frequency).  Normalization is n*cg,
    NOT n*cg/2 which would be the one-sided real-signal convention.
    """
    n = len(y_c)
    w = np.hanning(n)
    cg = w.mean()
    S = np.abs(np.fft.fft(y_c * w))
    return np.max(S) / (n * cg)


# ── lifecycle ──────────────────────────────────────────────────────────────


def test_create_returns_object(h51):
    dec = HalfbandDecimatorQ15(h51)
    assert dec is not None
    dec.destroy()


def test_num_taps(h51):
    dec = HalfbandDecimatorQ15(h51)
    assert dec.num_taps == len(h51)
    dec.destroy()


def test_rate(h51):
    dec = HalfbandDecimatorQ15(h51)
    assert abs(dec.rate - 0.5) < 1e-9
    dec.destroy()


def test_destroy_then_raises(h51):
    dec = HalfbandDecimatorQ15(h51)
    dec.destroy()
    with pytest.raises(RuntimeError, match="destroyed"):
        dec.execute(np.zeros(8, dtype=np.int16))


def test_context_manager(h51):
    with HalfbandDecimatorQ15(h51) as dec:
        y = dec.execute(np.zeros(16, dtype=np.int16))
    assert y.dtype == np.int16


# ── output type and shape ──────────────────────────────────────────────────


def test_output_dtype(dec51):
    y = dec51.execute(np.zeros(256, dtype=np.int16))
    assert y.dtype == np.int16


def test_decimation_ratio(dec51):
    n_complex = 1024
    y = dec51.execute(np.zeros(2 * n_complex, dtype=np.int16))
    assert len(y) == n_complex  # 2·n_complex int16 in → n_complex int16 out


def test_odd_block_buffering(dec51):
    """Trailing even IQ pair is buffered and consumed on next call."""
    y1 = dec51.execute(np.zeros(6, dtype=np.int16))  # 3 complex in
    assert len(y1) == 2  # 1 complex out
    y2 = dec51.execute(np.zeros(2, dtype=np.int16))  # 1 complex in
    assert len(y2) == 2  # pending + 1 → 1 out


# ── zero input ─────────────────────────────────────────────────────────────


def test_zero_input_zero_output(dec51):
    y = dec51.execute(np.zeros(512, dtype=np.int16))
    np.testing.assert_array_equal(y, 0)


def test_reset_clears_state(dec51):
    dec51.execute(_iq_tone(256, 0.05))
    dec51.reset()
    y = dec51.execute(np.zeros(256, dtype=np.int16))
    np.testing.assert_array_equal(y, 0)


# ── passband gain ──────────────────────────────────────────────────────────


@pytest.mark.parametrize("f0", [0.02, 0.05, 0.08, 0.10])
def test_passband_amplitude(h51, f0):
    """Passband tones should pass with near-unity gain (±10%)."""
    dec = HalfbandDecimatorQ15(h51)
    amplitude = 20000
    x = _iq_tone(4096, f0, amplitude)
    y = dec.execute(x)
    settle = dec.num_taps
    y_c = _complex_from_iq(y[settle * 2 :], scale=amplitude)
    amp = _windowed_amplitude(y_c)
    dec.destroy()
    assert 0.90 <= amp <= 1.10, f"f0={f0}: amplitude={amp:.4f}"


# ── stopband rejection ─────────────────────────────────────────────────────


@pytest.mark.parametrize("f0", [0.30, 0.35, 0.40, 0.45])
def test_stopband_rejection(h51, f0):
    """Stopband tones should be attenuated by ≥ 34 dB.

    Q15 coefficient quantization costs a few dB versus the float design
    (the _halfband_bank filter targets 60 dB, Q15 delivers ≥ 34 dB).
    """
    dec = HalfbandDecimatorQ15(h51)
    amplitude = 20000
    x = _iq_tone(4096, f0, amplitude)
    y = dec.execute(x)
    settle = dec.num_taps
    y_c = _complex_from_iq(y[settle * 2 :], scale=amplitude)
    amp = _windowed_amplitude(y_c)
    dec.destroy()
    assert amp < 0.02, f"f0={f0}: amplitude={amp:.4f} (expected < −34 dB)"


# ── SNR vs float reference ─────────────────────────────────────────────────


def test_snr_vs_float_reference(h51):
    """Q15 output SNR vs the same filter run in float64 should exceed 30 dB."""
    amplitude = 20000
    f0 = 0.05
    dec = HalfbandDecimatorQ15(h51)
    x_iq = _iq_tone(8192, f0, amplitude)
    y_q15 = dec.execute(x_iq)
    settle = dec.num_taps
    dec.destroy()

    # Float64 reference using the SAME FIR branch (not a different remez
    # design).
    from doppler.resample import HalfbandDecimator

    dec_f = HalfbandDecimator(h51)
    t = np.arange(8192, dtype=np.float64)
    x_c = (amplitude * np.exp(2j * np.pi * f0 * t)).astype(np.complex64)
    y_ref = dec_f.execute(x_c).astype(np.complex128)

    y_c = _complex_from_iq(y_q15[settle * 2 :], scale=amplitude)
    n = min(len(y_c), len(y_ref) - settle)
    err = y_c[:n] - y_ref[settle : settle + n] / amplitude
    snr = 10 * np.log10(
        np.mean(np.abs(y_c[:n]) ** 2) / (np.mean(np.abs(err) ** 2) + 1e-300)
    )
    assert snr > 30, f"SNR={snr:.1f} dB (expected > 30 dB)"


# ── multi-block streaming ──────────────────────────────────────────────────


def test_streaming_matches_single_block(h51):
    """Feeding the same samples in many small blocks = one big block.

    execute() returns a zero-copy view into the object's internal buffer;
    the view is overwritten by the next call.  Each chunk result must be
    copied before the next execute() call.  This test verifies that the
    polyphase delay-line state accumulates correctly across calls so that
    streaming output is bit-exact with the single-block output.
    """
    n = 512
    f0 = 0.05
    amplitude = 20000
    x = _iq_tone(n, f0, amplitude)

    # Single block reference.
    d_ref = HalfbandDecimatorQ15(h51)
    y_ref = d_ref.execute(x).copy()
    d_ref.destroy()

    # Small chunks of 8 IQ pairs (16 int16 elements each).
    chunk = 16
    d_stream = HalfbandDecimatorQ15(h51)
    parts = []
    for i in range(0, len(x), chunk):
        parts.append(d_stream.execute(x[i : i + chunk]).copy())
    y_stream = np.concatenate(parts)
    d_stream.destroy()

    assert len(y_stream) == len(y_ref)
    np.testing.assert_array_equal(
        y_stream,
        y_ref,
        err_msg="streaming output differs from single-block output",
    )


# ── extreme amplitudes ─────────────────────────────────────────────────────


def test_near_full_scale_no_saturation(h51):
    """Passband tone at near full-scale (amp=30000) should not hard-clip
    output.

    The two-pass AVX2 inner loop avoids folding delay-line values as int16,
    so it handles amplitudes above -6 dBFS without saturation clipping.
    The output amplitude should remain close to 30000 — within ±5% — rather
    than clipping at 32767.
    """
    amplitude = 30000
    f0 = 0.05
    n = 4096
    x = _iq_tone(n, f0, amplitude)
    dec = HalfbandDecimatorQ15(h51)
    y = dec.execute(x).copy()
    settle = dec.num_taps
    dec.destroy()

    y_c = _complex_from_iq(y[settle * 2 :], scale=amplitude)
    amp_out = _windowed_amplitude(y_c)

    # Output should be near-unity gain, not clipped at 32767/30000 ≈ 1.09.
    assert amp_out < 1.05, (
        f"output amplitude {amp_out:.4f} suggests hard clipping"
    )
    assert amp_out > 0.90, (
        f"output amplitude {amp_out:.4f} too low — unexpected signal loss"
    )


# ── cascade: two-stage 4:1 decimation ─────────────────────────────────────


def test_cascade_4x(h51):
    """Two HalfbandDecimatorQ15 stages give 4:1 decimation.

    The second stage receives the int16 IQ output of the first stage directly.
    The passband tone (at f0 = 0.05 relative to the original sample rate =
    0.10 relative to stage-1 output = 0.20 relative to stage-2 output) should
    survive with near-unity amplitude.  The 4:1 ratio is verified by the
    output length.
    """
    n = 8192
    f0 = 0.05
    amplitude = 20000
    x = _iq_tone(n, f0, amplitude)

    dec1 = HalfbandDecimatorQ15(h51)
    dec2 = HalfbandDecimatorQ15(h51)
    taps = dec1.num_taps  # read before destroy

    # Stage 1: 2:1 decimation.
    y1 = dec1.execute(x).copy()
    # Stage 2: 2:1 decimation of stage-1 output → 4:1 overall.
    y2 = dec2.execute(y1).copy()

    dec1.destroy()
    dec2.destroy()

    # Output should be n/4 complex samples (n/4 * 2 int16 elements).
    assert len(y2) == n // 2, (
        f"expected {n // 2} int16 elements, got {len(y2)}"
    )

    # The tone at f0=0.05 relative to original rate is at 0.20 relative to
    # the stage-2 output rate — still in the passband (< 0.4).
    settle = taps + taps
    y_c = _complex_from_iq(y2[settle * 2 :], scale=amplitude)
    amp_out = _windowed_amplitude(y_c)
    assert 0.85 <= amp_out <= 1.15, (
        f"cascade passband amplitude={amp_out:.4f} (expected ≈ 1.0)"
    )


# ── filter length variants ─────────────────────────────────────────────────


def _get_fir_coeffs(atten, pb, sb):
    """Return the FIR polyphase branch for given halfband design parameters.

    Selects the FIR row (not the delay row) from _halfband_bank by finding
    the row whose maximum absolute coefficient is smaller.  The delay branch
    has a single coefficient at exactly 1.0; the FIR branch has a peak of
    roughly 0.5–0.65 (halfband center coefficient).
    """
    from doppler.resample import _halfband_bank

    bank = _halfband_bank(atten, pb, sb)
    fir_row = int(np.argmin([np.max(np.abs(bank[r])) for r in range(2)]))
    return np.ascontiguousarray(bank[fir_row]).astype(np.float32)


@pytest.mark.parametrize(
    "att,pb,sb",
    [
        (40.0, 0.4, 0.6),  # short filter  (~12 taps)
        (60.0, 0.4, 0.6),  # standard      (~19 taps)
        (80.0, 0.4, 0.6),  # long filter   (~26 taps)
    ],
)
def test_various_filter_lengths(att, pb, sb):
    """HalfbandDecimatorQ15 works for different filter lengths from
    _halfband_bank.

    Verifies that a passband tone at f0=0.05 passes with near-unity gain
    (±15%) for each design, exercising different num_taps values in the
    Q15 inner loop.
    """
    h = _get_fir_coeffs(att, pb, sb)
    amplitude = 20000
    f0 = 0.05
    n = 4096
    x = _iq_tone(n, f0, amplitude)

    dec = HalfbandDecimatorQ15(h)
    y = dec.execute(x).copy()
    settle = dec.num_taps
    dec.destroy()

    y_c = _complex_from_iq(y[settle * 2 :], scale=amplitude)
    amp_out = _windowed_amplitude(y_c)
    assert 0.85 <= amp_out <= 1.15, (
        f"att={att}: passband amplitude={amp_out:.4f} (expected ≈ 1.0)"
    )
