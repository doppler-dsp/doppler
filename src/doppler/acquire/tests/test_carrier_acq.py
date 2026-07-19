import numpy as np
import pytest

from doppler.acquire import CarrierAcquisition

SAMPLE_RATE_HZ = 8000.0
SYMBOL_RATE_HZ = 1000.0
TONE_HZ = 123.0
SPS = 8
N_SYMBOLS = 4000
NO_TEMPLATE = np.array([], dtype=np.float32)


def _make_signal(n_symbols=N_SYMBOLS, tone_hz=TONE_HZ, seed=12345):
    rng = np.random.default_rng(seed)
    bits = np.where(rng.integers(0, 2, n_symbols), 1.0, -1.0)
    data = np.repeat(bits, SPS)
    t = np.arange(len(data))
    tone = np.exp(2j * np.pi * tone_hz * t / SAMPLE_RATE_HZ)
    return (data * tone).astype(np.complex64)


def test_create():
    ca = CarrierAcquisition(NO_TEMPLATE, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ)
    assert ca.ready is False
    assert ca.n_blocks == 0
    assert ca.nfft >= 1
    assert ca.dwell_target >= 1


@pytest.mark.parametrize("sequential", [True, False])
def test_detects_tone(sequential):
    x = _make_signal()
    ca = CarrierAcquisition(
        NO_TEMPLATE, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, sequential=sequential
    )
    ca.steps(x)
    assert ca.ready
    assert abs(ca.residual_hz - TONE_HZ) < 5.0


def test_carry_buffer_split_calls():
    x = _make_signal()
    ca = CarrierAcquisition(NO_TEMPLATE, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ)
    chunk = 3  # deliberately not a multiple of n_fft or sps
    for off in range(0, len(x), chunk):
        if ca.ready:
            break
        ca.steps(x[off : off + chunk])
    assert ca.ready
    assert abs(ca.residual_hz - TONE_HZ) < 5.0


def test_reset():
    x = _make_signal()
    ca = CarrierAcquisition(NO_TEMPLATE, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ)
    ca.steps(x)
    assert ca.ready
    ca.reset()
    assert ca.ready is False
    assert ca.n_blocks == 0
    ca.steps(x)
    assert ca.ready
    assert abs(ca.residual_hz - TONE_HZ) < 5.0


def test_template_override_runs_to_completion():
    probe = CarrierAcquisition(NO_TEMPLATE, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ)
    nfft = probe.nfft
    tmpl = np.zeros(nfft, dtype=np.float32)
    tmpl[nfft // 2] = 1.0  # a DC spike, deliberately not the default sinc^2

    x = _make_signal()
    ca = CarrierAcquisition(tmpl, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ)
    assert ca.nfft == nfft
    ca.steps(x)
    assert ca.n_blocks > 0


def test_template_wrong_length_rejected():
    with pytest.raises(MemoryError):
        CarrierAcquisition(
            np.zeros(3, dtype=np.float32), SAMPLE_RATE_HZ, SYMBOL_RATE_HZ
        )


def test_invalid_sample_rate_rejected():
    with pytest.raises(MemoryError):
        CarrierAcquisition(NO_TEMPLATE, 0.0, SYMBOL_RATE_HZ)


def test_context_manager():
    with CarrierAcquisition(NO_TEMPLATE, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ) as ca:
        ca.steps(_make_signal())
        assert ca.ready


def test_destroy():
    ca = CarrierAcquisition(NO_TEMPLATE, SAMPLE_RATE_HZ, SYMBOL_RATE_HZ)
    ca.destroy()
