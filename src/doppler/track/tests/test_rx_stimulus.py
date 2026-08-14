"""The receiver stimulus itself, measured before any receiver sees it.

Every conclusion the M-PSK receiver tests draw is a comparison against a
theoretical curve at a stated Es/N0 -- the coherent SER bound, the `-Es/N0` EVM
bound, the settling budget. All of them are void if the stimulus is not at the
Es/N0 it says it is, and none of them FAIL informatively when it isn't: a
stimulus 3 dB hot reads as a receiver with 3 dB of implementation loss, which
is a number a person will happily go and try to explain. That happened here
(issue #754), so the stimulus is now measured directly.

`make_signal` delegates the waveform to `wfm.Synth`, whose `snr_mode="esno"` is
checked at the generator in `wfm/tests/test_wfm_synth.py`. What is checked HERE
is the part the harness still owns: the level convention, the projection onto
the real path, and the 3 dB that separates the two -- see
`_mpsk_rx_harness.REAL_ESNO_OFFSET_DB`.
"""

from __future__ import annotations

import numpy as np
import pytest

from ._mpsk_rx_harness import IF_FS4, make_signal

ORDERS = (2, 4, 8)
PATHS = (False, True)
PATH_ID = {False: "complex", True: "real"}


def decompose(sps, nsym, *, real, m, esn0_db, seed=3):
    """Split a stimulus into `(signal, noise)` using the noiseless twin.

    The noiseless call gives the signal component exactly -- same symbols, same
    carrier, same generator -- but not at the same LEVEL, since `agc()`
    normalises the noisy waveform's TOTAL power to unity and the clean one's
    signal power to unity. So the amplitude is recovered by projection
    (`a = <c,y>/<c,c>`, the least-squares fit of the clean waveform to the
    noisy one) and everything orthogonal to it is noise. No estimator is
    involved and no distributional assumption is made; this is the definition
    of the two components, applied to a signal we happen to know exactly.
    """
    clean, _ = make_signal(
        sps, nsym, real=real, m=m, fc=IF_FS4, esn0_db=None, seed=seed
    )
    noisy, _ = make_signal(
        sps, nsym, real=real, m=m, fc=IF_FS4, esn0_db=esn0_db, seed=seed
    )
    c = clean.astype(np.complex128)
    y = noisy.astype(np.complex128)
    a = np.vdot(c, y) / np.vdot(c, c)
    return a * c, y - a * c


def _corr(a, b):
    """Normalised correlation of two real series."""
    a, b = np.real(a), np.real(b)
    return float(np.dot(a, b) / np.sqrt(np.dot(a, a) * np.dot(b, b)))


def measured_esn0_db(sps, nsym, *, real, m, esn0_db, seed=3):
    """Read back the Es/N0 a `make_signal` stimulus actually carries.

    The convention is the one `make_signal` documents: `Es = P_sig * sps` on
    the complex path, halved on the real path, both against the variance
    measured in that path's own samples. See `decompose` for how the two
    components are separated.
    """
    sig, noise = decompose(
        sps, nsym, real=real, m=m, esn0_db=esn0_db, seed=seed
    )
    es = float(np.mean(np.abs(sig) ** 2)) * sps / (2.0 if real else 1.0)
    return 10.0 * np.log10(es / float(np.mean(np.abs(noise) ** 2)))


@pytest.mark.parametrize("real", PATHS, ids=lambda r: PATH_ID[r])
@pytest.mark.parametrize("esn0", [0.0, 6.0, 12.0, 20.0])
@pytest.mark.parametrize("sps", [8, 24])
def test_make_signal_delivers_the_requested_esn0(sps, esn0, real):
    """Both paths must carry the Es/N0 they were asked for.

    The real path is the one this is really about. It is generated 3 dB hot on
    purpose so that `Re{}` lands on the real convention (`Es = A^2*sps/2`,
    `var = Es/(2*Es/N0)`); drop that offset and every real-path measurement
    moves by 3 dB while still looking entirely reasonable -- an EVM of -9 dB
    against a -12 dB bound is exactly what a receiver with a real bug produces.
    """
    got = measured_esn0_db(sps, 20000, real=real, m=4, esn0_db=esn0)
    assert abs(got - esn0) < 0.2, (
        f"{PATH_ID[real]} sps={sps}: asked for Es/N0 {esn0:.1f} dB, "
        f"the stimulus carries {got:.2f} dB"
    )


@pytest.mark.parametrize("m", ORDERS)
def test_esn0_does_not_depend_on_the_order(m):
    """Es/N0 is per SYMBOL, so it must not move with the constellation size.

    A per-BIT slip would show here as a 3 dB step from BPSK to QPSK and 4.8 to
    8PSK -- the mistake `snr_mode="ebno"` exists to make deliberately.
    """
    got = measured_esn0_db(16, 20000, real=False, m=m, esn0_db=12.0)
    assert abs(got - 12.0) < 0.2, f"m={m}: {got:.2f} dB, not 12 dB"


def test_paths_share_one_waveform_and_one_noise_draw():
    """The real stimulus must BE the complex one's real part, not a sibling.

    Two paths compared on two draws of "the same" random signal cannot be
    compared at all -- any difference between the receivers is then partly the
    draw. The generator gets the same symbols and the same seed, so the signals
    are one waveform; it gets a noise amplitude smaller by `sqrt(2)`, which
    scales the SAME Gaussian stream rather than drawing a second one. Both
    halves are checked, because the second is the one that would fail silently:
    a redrawn noise sequence still gives every test the right Es/N0 and only
    shows up as unexplained scatter between the paths.

    Note the raw waveforms correlate at ~0.988, not ~1, and that is correct
    arithmetic rather than a defect -- the real path carries half the noise
    POWER, so a whole-signal correlation is dragged down by the noise the two
    do not share. Asserting on that number is how this test first failed.
    """
    kw = {"m": 4, "esn0_db": 12.0, "seed": 11}
    _, ic = make_signal(8, 4000, real=False, fc=IF_FS4, esn0_db=12.0, seed=11)
    _, ir = make_signal(8, 4000, real=True, fc=IF_FS4, esn0_db=12.0, seed=11)
    assert np.array_equal(ic, ir), "the two paths carry different truth"

    sig_c, n_c = decompose(8, 4000, real=False, **kw)
    sig_r, n_r = decompose(8, 4000, real=True, **kw)
    assert _corr(sig_c, sig_r) > 0.9999, "the real path is a different signal"
    assert _corr(n_c, n_r) > 0.9999, "the real path redraws its noise"

    # Levels are compared as noise-to-signal within each path, because `agc()`
    # scales the two differently (it normalises TOTAL power, and the real path
    # holds less of both) -- so a raw amplitude ratio is 1.096 here and means
    # nothing on its own.
    def nsr(sig, noise):
        return float(np.std(np.real(noise)) / np.std(np.real(sig)))

    ratio = nsr(sig_r, n_r) / nsr(sig_c, n_c)
    assert abs(ratio - 2**-0.5) < 0.01, (
        f"real-path noise is {ratio:.4f} of the complex path's relative to "
        f"its own signal, not 1/sqrt(2): REAL_ESNO_OFFSET_DB is not 3 dB"
    )


@pytest.mark.parametrize("real", PATHS, ids=lambda r: PATH_ID[r])
def test_the_heavily_oversampled_standard_case(real):
    """`sps = 10 000` -- Fs = 10 MSa/s at Rs = 1 kSps -- carries its Es/N0 too.

    This ratio is the harness's standard case and it is also the one whose
    RECEIVER behaviour is still under investigation. Pinning the stimulus here
    is what takes the generator off that list: at 10 000 samples per symbol the
    per-sample SNR is -28 dB relative to Es/N0, which is exactly where an
    amplitude convention that quietly refers to samples instead of symbols
    stops being noticeable and starts being 40 dB.

    Short (400 symbols) on purpose -- 4 M samples per call is already the cost
    driver, and the quantity under test does not need many symbols.
    """
    got = measured_esn0_db(10000, 400, real=real, m=2, esn0_db=12.0)
    assert abs(got - 12.0) < 0.3, f"{PATH_ID[real]} sps=1e4: {got:.2f} dB"


def test_clean_stimulus_is_actually_clean():
    """`esn0_db=None` must add no noise at all, not merely a little.

    The lock-time and no-false-declare tests read this path, and "a bit of
    noise" there is a slow leak into every threshold derived from it.
    """
    x, idx = make_signal(8, 2000, real=False, m=4, esn0_db=None)
    step = 2.0 * np.pi / 4
    ideal = np.exp(
        1j * (step * idx + 2 * np.pi * IF_FS4 * np.arange(2000) * 8)
    )
    got = x[::8].astype(np.complex128)
    err = np.mean(np.abs(got / np.abs(got).mean() - ideal) ** 2)
    assert 10 * np.log10(err) < -50.0, f"clean EVM {10 * np.log10(err):.1f} dB"
