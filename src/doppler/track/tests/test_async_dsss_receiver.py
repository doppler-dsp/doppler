"""End-to-end integration: the real closed-loop async DSSS receiver.

`docs/design/async-symbol-despreader.md` validates the segmented-DLL
code path (`Dll(segments=K)`) and the always-on lock detector against
real objects, but its own BER table comes from a **numpy stand-in**
for the despread step with **no carrier at all**
(`src/doppler/examples/async_despreader_study.py`), and the gallery
page's downstream composition is an unexecuted comment. Nobody had
chained the real `Dll(segments=K)` -> `Costas` -> `SymbolSync` -> bit
decisions and shown it converges blind (no genie carrier/timing
knowledge).

These tests close that gap at a real link budget: 2.046 Mcps chip
rate, a 1023-chip Gold-length code (one epoch = 0.5 ms), 1800 bps data
(1.1111 epochs/symbol, 1136.7 chips/symbol -> ~30.6 dB processing
gain) -- symbol period slightly *longer* than one code epoch, so more
than one code period elapses per data bit and the symbol boundary
continuously slides through the epoch grid.

Composition (no new object -- three existing `doppler.track`
primitives): `Dll(..., segments=K)` splits each epoch into `K`
sub-epoch partial correlations (code tracking survives a mid-epoch
data flip because the non-coherent discriminator only degrades the
one straddling partial); `Costas(tsamps=1)` de-rotates the residual
carrier per partial (no premature block integration -- the true
symbol boundary isn't a fixed multiple of partials); a sliding
length-`K` boxcar re-forms a symbol matched filter without decimating;
`SymbolSync` recovers the (possibly drifting, independent) symbol
clock via Gardner TED + Farrow interpolation and emits one decision
per symbol.

`Costas.norm_freq` is in cycles/*partial*, not cycles/original-sample
-- the partial stream is a `TE/K`-times decimated view of the input,
so a residual carrier that's small relative to the original sample
rate can still be a large fraction of a cycle per partial. Choose the
loop bandwidth accordingly (`f0 * TE / K` must sit inside the loop's
pull-in range).
"""

import numpy as np
import pytest

from doppler.track import Costas, Dll, SymbolSync
from doppler.wfm import PN, mls_poly

CHIP_RATE = 2.046e6
SF = 1023  # a real length-10 maximal-length sequence, period 2^10 - 1
SPS = 2  # samples/chip -- kept small for a tractable simulation size
TE = SF * SPS  # samples/code-epoch
DATA_RATE = 1800.0
EPOCHS_PER_SYMBOL = (1.0 / DATA_RATE) / (SF / CHIP_RATE)  # 1.11111
K = 8  # partials/epoch -> ~K*EPOCHS_PER_SYMBOL = 8.89 partials/symbol
NOMINAL_RATE = K * EPOCHS_PER_SYMBOL  # SymbolSync's starting sps hint
F0 = 1e-4  # residual carrier after acquisition, cycles/sample


def _code(seed=1):
    # A real m-sequence, not an arbitrary random 0/1 draw: an arbitrary
    # random sequence has no guarantee on its autocorrelation sidelobes,
    # and some seeds produce a sequence bad enough that the DLL's
    # tracking loop genuinely destabilizes over a long run -- confirmed
    # by sweeping seeds at a fixed SF/bn (some clean, some not) and by
    # swapping in this real MLS at the SAME seeds/bn/SF (uniformly
    # stable). `seed` here only selects the LFSR's start phase within
    # the one guaranteed-clean period-1023 sequence, not the sequence's
    # autocorrelation quality.
    return np.asarray(
        PN(poly=mls_poly(10), seed=seed, length=10).generate(SF)
    ).astype(np.uint8)


def _signal(code, nsym, epochs_per_symbol, phi, f0, snr_db, seed):
    """Async-data DSSS-BPSK with a residual carrier, no genie knowledge.

    The data clock (``tsym`` samples, phase ``phi``) is an independent
    timebase from the code clock -- the symbol boundary slides through
    the code-epoch grid at the beat rate between the two.
    """
    rng = np.random.default_rng(seed)
    csign = np.where(code & 1, -1.0, 1.0)
    tsym = TE * epochs_per_symbol
    n = int(nsym * tsym) + 2 * TE
    data = (rng.integers(0, 2, nsym + 6) * 2 - 1).astype(float)
    idx = np.arange(n)
    si = np.clip(np.floor((idx - phi) / tsym).astype(int), 0, len(data) - 1)
    cph = (idx // SPS) % SF
    rx = (data[si] * csign[cph] * np.exp(2j * np.pi * f0 * idx)).astype(
        np.complex64
    )
    if snr_db is not None:
        p = np.sqrt(np.mean(np.abs(rx) ** 2))
        std = np.sqrt(10 ** (-snr_db / 10)) * p
        rx = rx + (
            rng.normal(0, std / np.sqrt(2), n)
            + 1j * rng.normal(0, std / np.sqrt(2), n)
        ).astype(np.complex64)
    return rx, data, tsym


def _recover(rx, code):
    """The real closed-loop chain -- no genie carrier or timing knowledge."""
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=K)
    part = d.steps(rx).astype(np.complex64)
    cos = Costas(bn=0.02, zeta=0.707, tsamps=1)
    wiped = cos.steps(part).astype(np.complex64)
    mf = np.convolve(wiped, np.ones(K), mode="same").astype(np.complex64)
    ss = SymbolSync(
        sps=round(NOMINAL_RATE), bn=0.02, zeta=0.707, order="cubic"
    )
    syms = ss.steps(mf)
    return syms, d, cos, ss


def _ber(dec, truth):
    dec = np.asarray(dec, float)
    c = np.correlate(dec, truth.astype(float), "full")
    k = int(np.argmax(np.abs(c)))
    lag = k - (len(truth) - 1)
    inv = np.sign(c[k])
    lo, hi = len(dec) // 4, len(dec) - len(dec) // 4
    err = cnt = 0
    for i in range(lo, hi):
        j = i - lag
        if 0 <= j < len(truth):
            err += dec[i] != inv * truth[j]
            cnt += 1
    return err / cnt if cnt else 1.0


@pytest.mark.parametrize("phi_frac", [0.1, 0.37, 0.63])
def test_recovers_bits_blind(phi_frac):
    # Es/N0 ~ 22.6 dB (SNR + the ~30.6 dB processing gain) -- comfortably
    # inside the working range found below; three code-phase offsets rule
    # out a phase-specific fluke.
    code = _code(11)
    rx, data, _ = _signal(
        code, 1200, EPOCHS_PER_SYMBOL, phi_frac * TE, F0, snr_db=-8, seed=5
    )
    syms, d, cos, ss = _recover(rx, code)
    dec = np.where(syms.real >= 0, 1, -1)
    assert _ber(dec, data) == 0.0
    assert d.code_rate == pytest.approx(1.0, abs=1e-3)
    assert cos.locked is True
    assert ss.rate == pytest.approx(NOMINAL_RATE, abs=0.05)


@pytest.mark.parametrize("drift_pct", [-1.0, -0.3, 0.3, 1.0])
def test_tracks_independent_symbol_clock_drift(drift_pct):
    # SymbolSync is only ever given the NOMINAL rate as a starting hint;
    # the true rate differs by +-1% (an independent, unknown timebase, not
    # a static offset it could get away with ignoring). If the loop merely
    # free-ran at the hint instead of tracking, `ss.rate` would sit at
    # NOMINAL_RATE regardless of drift_pct and this would still pass BER
    # (over a short enough run) but fail the rate assertion below.
    code = _code(11)
    true_ratio = EPOCHS_PER_SYMBOL * (1 + drift_pct / 100)
    rx, data, _ = _signal(
        code, 1200, true_ratio, 0.37 * TE, F0, snr_db=-8, seed=5
    )
    syms, _d, _cos, ss = _recover(rx, code)
    dec = np.where(syms.real >= 0, 1, -1)
    assert _ber(dec, data) == 0.0
    assert ss.rate == pytest.approx(K * true_ratio, abs=0.05)


def test_recovers_near_the_noise_floor():
    # Es/N0 ~ 10.6 dB -- clean recovery holds down to here; ~2 dB lower
    # (~8.6 dB) the Costas/lock-detector threshold is crossed and BER
    # floors near 0.5 (see the module docstring's link-budget note).
    code = _code(11)
    rx, data, _ = _signal(
        code, 1200, EPOCHS_PER_SYMBOL, 0.37 * TE, F0, snr_db=-20, seed=1
    )
    syms, _d, cos, _ss = _recover(rx, code)
    dec = np.where(syms.real >= 0, 1, -1)
    assert _ber(dec, data) < 0.02
    assert cos.locked is True


def test_unlocks_on_noise_only():
    # noise-only input must not report a false lock
    code = _code(11)
    rng = np.random.default_rng(2)
    rx = (
        (rng.standard_normal(1200 * TE) + 1j * rng.standard_normal(1200 * TE))
        / np.sqrt(2)
    ).astype(np.complex64)
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=K)
    d.steps(rx)
    assert d.locked is False
