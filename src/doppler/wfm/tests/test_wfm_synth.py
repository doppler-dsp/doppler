"""Integration tests for the Synth waveform engine.

Covers the six waveform types, smart defaults (bare construct → clean tone),
the string ``type``/``snr_mode`` choices, MLS PN behaviour, the LFM chirp
sweep, and the auto-resolved SNR (Es/No for the modulated types).
"""

import numpy as np
import pytest

from doppler.mpsk import mpsk_map
from doppler.snr import snr_data_aided_db
from doppler.wfm import PN, Synth, bits, chirp, mls_poly, rrc_taps


def _inst_freq(x, fs):
    """Per-sample instantaneous frequency (Hz) from the phase increment."""
    return np.angle(x[1:] * np.conj(x[:-1])) / (2 * np.pi) * fs


def _power(x):
    return float(np.mean(np.abs(x) ** 2))


def test_default_is_clean_tone():
    """Bare construct → a clean baseband tone (smart defaults)."""
    x = Synth().steps(4096)
    assert x.dtype == np.complex64
    assert np.isclose(_power(x), 1.0, atol=0.05)  # unit-power tone, ~no noise


def test_tone_freq_peak():
    x = Synth(type="tone", fs=1e6, freq=100_000, snr=100).steps(4096)
    peak = np.argmax(np.abs(np.fft.fft(x * np.hanning(len(x))))) / len(x)
    assert abs(peak - 0.1) < 0.01


def test_noise_unit_power():
    n = Synth(type="noise", seed=7).steps(8192)
    assert np.isclose(_power(n), 1.0, atol=0.1)


def test_pn_is_maximal_length():
    """Length-7 PN with sps=1 is a balanced, period-127 m-sequence."""
    p = Synth(type="pn", pn_length=7, sps=1, snr=100).steps(127 * 2)
    chips = np.sign(p.real).astype(int)
    assert np.array_equal(chips[:127], chips[127:254])  # period 127
    assert int(np.sum(chips[:127] == -1)) == 64  # 64 ones / 63 zeros


@pytest.mark.parametrize("length,seed", [(7, 1), (7, 42), (9, 7), (11, 3)])
def test_synth_pn_matches_standalone_pn(length: int, seed: int) -> None:
    """A ``type="pn"`` synth emits exactly the bits ``doppler.wfm.PN`` would.

    This is the contract a receiver-side BER scorer depends on: the
    transmitted data is a pure function of ``(pn_poly, seed, pn_length,
    lfsr)``, so a receiver regenerates the identical truth with
    ``PN(...).generate(n)`` rather than being handed a truth array. The
    continuous-async DSSS ``prbs`` data mode draws its data bits from this same
    LFSR, seeded with the source's own ``seed`` -- so if this invariant ever
    breaks, every PRBS-mode BER measurement silently mis-scores. It was
    untested until now.

    The synth maps bit ``b`` to ``1 - 2b`` (0 -> +1, 1 -> -1); ``PN.generate``
    returns the raw 0/1 bits, and ``poly=0`` auto-selects the same
    ``mls_poly(length)`` the synth uses.
    """
    n = 300
    syn = np.asarray(
        Synth(type="pn", pn_length=length, seed=seed, sps=1, snr=100).steps(n)
    )
    # Both spellings of the code a receiver might use.
    for poly in (0, mls_poly(length)):
        truth = np.asarray(PN(poly=poly, seed=seed, length=length).generate(n))
        assert np.array_equal(
            np.sign(syn.real).astype(int), 1 - 2 * truth.astype(int)
        ), f"synth PN diverged from PN(poly={poly})"


def test_pn_fibonacci_differs_from_galois():
    """--lfsr selects the realization: same MLS period, different chips."""
    g = Synth(type="pn", pn_length=9, sps=1, snr=100, lfsr="galois")
    f = Synth(type="pn", pn_length=9, sps=1, snr=100, lfsr="fibonacci")
    gc = np.sign(g.steps(511 * 2).real).astype(int)
    fc = np.sign(f.steps(511 * 2).real).astype(int)
    assert np.array_equal(fc[:511], fc[511:1022])  # fibonacci also maximal
    assert int(np.sum(fc[:511] == -1)) == 256  # balanced (2**8)
    assert not np.array_equal(gc, fc)  # distinct ordering


def test_pn_64bit_length():
    """pn_length > 32 drives the 64-bit LFSR + auto-MLS table (n=2..64)."""
    x = Synth(type="pn", pn_length=40, sps=1, snr=100).steps(20000)
    chips = np.sign(x.real).astype(int)
    assert set(chips.tolist()) <= {-1, 1}  # clean ±1 chips
    assert abs(chips.mean()) < 0.05  # balanced over a 2**40-1 sequence


def test_bpsk_constellation():
    b = Synth(type="bpsk", sps=8, snr=100, freq=0).steps(8 * 64)
    centers = b[4::8]
    assert set(np.sign(centers.real).astype(int)) == {-1, 1}
    assert np.allclose(centers.imag, 0, atol=1e-3)


def test_qpsk_constellation():
    q = Synth(type="qpsk", sps=8, snr=100, freq=0).steps(8 * 64)
    c = q[4::8]
    assert np.isclose(np.mean(np.abs(c)), 1.0, atol=0.02)
    quad = {(int(np.sign(s.real)), int(np.sign(s.imag))) for s in c}
    assert len(quad) == 4


def test_bpsk_esno_auto_snr():
    """--snr on bpsk auto-resolves to Es/No over the data symbols."""
    s = Synth(type="bpsk", sps=8, snr=10, seed=3).steps(1 << 16)
    snr_fs = 10 - 10 * np.log10(8)  # Es/No 10 dB spread over sps=8 samples
    expected_total = 1 + 1 / (10 ** (snr_fs / 10))
    assert np.isclose(_power(s), expected_total, atol=0.05)


#: Symbols per case in the Es/N0 read-back below. 20 000 puts the estimator's
#: own standard error near 0.03 dB, so the 0.25 dB tolerance is about the
#: generator, not about how long the test ran.
_ESNO_NSYM = 20000


@pytest.mark.parametrize("sps", [1, 4, 8, 16])
@pytest.mark.parametrize("esno", [0.0, 6.0, 12.0, 20.0])
def test_esno_mode_delivers_esno_at_the_matched_filter(sps, esno):
    """``snr_mode="esno"`` must deliver the Es/N0 it claims, where it claims.

    The claim is a RATIO OF ENERGIES at the symbol decision, not a per-sample
    power ratio, so it is only readable after a matched filter: an
    integrate-and-dump over ``sps`` samples turns the requested `Es = A^2*sps`
    into the soft symbol's power and the per-sample `N0` into `sps*N0`, and
    the ratio survives. Reading the raw sample stream instead measures
    `Es/N0 - 10log10(sps)` and reads 9 dB low at sps=8, which is what makes a
    wrong `esno` look plausible.

    Read back with `snr.snr_data_aided_db` -- the library's own data-aided
    estimator, against the known transmitted signs -- so a shared error would
    have to live in both the generator and the estimator to hide here. Nothing
    checked this before: `test_bpsk_esno_auto_snr` above pins the total power
    the resolved noise amplitude produces, which is the same number for a
    correct Es/N0 and for one referenced to the wrong node.
    """
    rng = np.random.default_rng(5)
    sent = rng.integers(0, 2, _ESNO_NSYM).astype(np.uint8)
    syms = np.where(sent, -1.0, 1.0).astype(np.complex64)
    x = Synth(
        type="symbols",
        symbols=syms,
        sps=sps,
        fs=1.0,
        freq=0.0,
        snr=esno,
        snr_mode="esno",
        seed=7,
    ).steps(sps * _ESNO_NSYM)
    soft = x.reshape(_ESNO_NSYM, sps).sum(axis=1).astype(np.complex64)
    got = float(snr_data_aided_db(np.ascontiguousarray(soft), sent))
    assert abs(got - esno) < 0.25, (
        f"sps={sps}: asked for Es/N0 {esno:.1f} dB, the matched-filter "
        f"output reads {got:.2f} dB"
    )


def test_esno_holds_on_a_carrier():
    """The LO must not change the delivered Es/N0 -- it is a rotation.

    fs/4 is the placement the real receiver path is designed around, and a
    carrier is the one thing between the noise injection and the measurement
    that could plausibly rescale it.
    """
    rng = np.random.default_rng(5)
    sent = rng.integers(0, 2, _ESNO_NSYM).astype(np.uint8)
    syms = np.where(sent, -1.0, 1.0).astype(np.complex64)
    sps, esno, fc = 8, 12.0, 0.25
    x = Synth(
        type="symbols",
        symbols=syms,
        sps=sps,
        fs=1.0,
        freq=fc,
        snr=esno,
        snr_mode="esno",
        seed=7,
    ).steps(sps * _ESNO_NSYM)
    z = x * np.exp(-2j * np.pi * fc * np.arange(x.size))
    soft = z.reshape(_ESNO_NSYM, sps).sum(axis=1).astype(np.complex64)
    got = float(snr_data_aided_db(np.ascontiguousarray(soft), sent))
    assert abs(got - esno) < 0.25, f"on an fs/4 carrier: {got:.2f} dB"


def test_bpsk_payload_regenerates_from_its_descriptor():
    """The PN-sourced bits of a `type="bpsk"` waveform are reproducible.

    A stimulus whose payload cannot be regenerated outside the generator can
    only be scored against itself, which is how a receiver test ends up
    measuring its own demodulator. `PN(poly=0, seed, length)` -- the same three
    numbers the waveform descriptor carries -- must reproduce the transmitted
    bits exactly, so a BER can be scored against the descriptor alone.
    """
    n, sps, seed, length = 4000, 4, 7, 16
    x = Synth(
        type="bpsk",
        sps=sps,
        fs=1.0,
        freq=0.0,
        snr=100.0,
        seed=seed,
        pn_length=length,
    ).steps(sps * n)
    sent = (x[::sps].real < 0).astype(np.uint8)
    assert np.array_equal(
        PN(poly=0, seed=seed, length=length).generate(n), sent
    )


def test_reset_reproduces():
    obj = Synth(type="qpsk", sps=4, seed=11)
    a = obj.steps(512)
    obj.reset()
    assert np.array_equal(a, obj.steps(512))


def test_bad_type_rejected():
    with pytest.raises((ValueError, TypeError)):
        Synth(type="not-a-waveform")


# ── RRC pulse shaping ────────────────────────────────────────────────────────


def _occupied_bw(x):
    """Fraction of FFT bins within 30 dB of the peak (a crude bandwidth)."""
    p = np.abs(np.fft.fft(x * np.hanning(len(x)))) ** 2
    p /= p.max()
    return float((p > 1e-3).mean())


def test_rrc_default_rect_unchanged():
    """pulse='rect' (the default) is byte-identical to omitting it."""
    a = Synth(type="qpsk", sps=8, snr=100, seed=3).steps(4096)
    b = Synth(type="qpsk", sps=8, snr=100, seed=3, pulse="rect").steps(4096)
    assert np.array_equal(a, b)


def test_rrc_is_band_limited():
    """RRC shaping narrows the occupied bandwidth vs rectangular hold."""
    rect = Synth(type="qpsk", sps=8, snr=100, seed=3).steps(8192)
    rrc = Synth(
        type="qpsk", sps=8, snr=100, seed=3, pulse="rrc", rrc_beta=0.22
    ).steps(8192)
    assert _occupied_bw(rrc) < 0.5 * _occupied_bw(rect)
    assert not np.array_equal(rrc, rect)


def test_rrc_unit_power():
    """The sqrt(sps) tap scaling keeps RRC output at ~unit average power."""
    rrc = Synth(
        type="qpsk", sps=8, snr=100, seed=1, pulse="rrc", rrc_beta=0.35
    ).steps(1 << 15)
    assert np.isclose(np.mean(np.abs(rrc) ** 2), 1.0, atol=0.1)


def test_rrc_reset_reproduces():
    s = Synth(
        type="bpsk", sps=4, seed=7, pulse="rrc", rrc_beta=0.3, rrc_span=6
    )
    a = s.steps(2048)
    s.reset()
    assert np.array_equal(a, s.steps(2048))


def test_rrc_only_modulated():
    """RRC is a no-op for tone/noise — they carry no symbol stream to shape."""
    a = Synth(type="tone", freq=1e5, fs=1e6).steps(1024)
    b = Synth(type="tone", freq=1e5, fs=1e6, pulse="rrc").steps(1024)
    assert np.array_equal(a, b)


def test_rrc_shapes_bits():
    """pulse='rrc' on a user bit pattern is NOT a no-op: it band-limits the
    stream just like the pn/bpsk/qpsk path. Guards the silent-ignore bug where
    --type bits accepted --pulse rrc but emitted rectangular pulses anyway."""
    pat = "1011001010110100"
    rect = bits(pattern=pat, sps=8, modulation="bpsk").steps(8192)
    rrc = bits(
        pattern=pat, sps=8, modulation="bpsk", pulse="rrc", rrc_beta=0.22
    ).steps(8192)
    assert not np.array_equal(rrc, rect)
    assert _occupied_bw(rrc) < 0.5 * _occupied_bw(rect)


@pytest.mark.parametrize("sps", [4, 3])
def test_rrc_bits_matches_matched_filter(sps):
    """Definitive check: the bits RRC output equals the symbol-rate impulse
    train convolved with the sqrt(sps)-scaled taps — for both bpsk and qpsk.

    ``sps`` selects the shaping implementation, and that is why it is
    parametrized rather than fixed: ``wfm_synth_set_rrc`` shapes a
    **power-of-two** ``sps`` with a polyphase ``resamp`` bank and falls back
    to a **dense FIR** for anything else. Same convolution, two separate
    block loops in ``wfm_synth_steps()`` — and the dense-FIR bits loop had no
    test on any path, so the four-copy bits→symbol map it carried could have
    been fixed in one branch and not the other with nothing to say so.
    """
    span, beta, n = 8, 0.35, 200
    pat = np.array([1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0], np.uint8)
    taps = rrc_taps(beta, sps, span)

    # The reference symbols come from the library's own bits->symbol map
    # (mpsk_map, the inverse dp_ber_score() applies), NOT from a formula
    # rewritten here: a hand-built map is a second copy that can — and did —
    # disagree with the canonical label assignment while both sides looked
    # self-consistent. One symbol's bits are read MSB-first into a Gray label.
    syms = mpsk_map(pat, 2)  # bpsk: 1 bit/symbol, the bit IS the label
    nsym = n // sps + span + 4
    imp = np.zeros(nsym * sps, dtype=np.complex64)
    imp[::sps] = syms[np.arange(nsym) % len(syms)]
    ref = np.convolve(imp, taps * np.sqrt(sps))[:n]
    got = bits(
        pattern=pat,
        sps=sps,
        modulation="bpsk",
        pulse="rrc",
        rrc_beta=beta,
        rrc_span=span,
    ).steps(n)
    assert np.allclose(got, ref, atol=1e-5)

    # qpsk: 2 bits/symbol, MSB first -> the same Gray label mpsk_map takes
    pairs = pat.reshape(-1, 2)
    qsym = mpsk_map(((pairs[:, 0] << 1) | pairs[:, 1]).astype(np.uint8), 4)
    impq = np.zeros(nsym * sps, dtype=np.complex64)
    impq[::sps] = qsym[np.arange(nsym) % len(qsym)]
    refq = np.convolve(impq, taps * np.sqrt(sps))[:n]
    gotq = bits(
        pattern=pat,
        sps=sps,
        modulation="qpsk",
        pulse="rrc",
        rrc_beta=beta,
        rrc_span=span,
    ).steps(n)
    assert np.allclose(gotq, refq, atol=1e-5)


def test_rrc_bits_reset_reproduces():
    s = bits(
        pattern="11010010",
        sps=4,
        modulation="bpsk",
        seed=5,
        pulse="rrc",
        rrc_beta=0.3,
        rrc_span=6,
    )
    a = s.steps(2048)
    s.reset()
    assert np.array_equal(a, s.steps(2048))


def test_rrc_bits_carrier_and_noise():
    """The RRC bits block path mixes the shaped baseband with the LO carrier
    and AWGN. Exercise every mix tail: LO-only, AWGN-only, and LO+AWGN — plus
    the unmodulated (modulation='none') latch under the FIR."""
    pat = "1011001010110100"
    base = {
        "pattern": pat,
        "sps": 8,
        "modulation": "bpsk",
        "pulse": "rrc",
        "seed": 3,
    }

    # LO carrier only (freq != 0, clean): shaped baseband rides the LO.
    lo = bits(freq=1e5, fs=1e6, snr=100, **base).steps(4096)
    assert np.all(np.isfinite(lo.view(np.float32)))
    # A carrier offset puts spectral energy off DC (vs the baseband-only case).
    assert np.argmax(np.abs(np.fft.fft(lo))) != 0

    # AWGN only (freq 0, low SNR): noise added to the shaped baseband.
    nz = bits(freq=0.0, fs=1e6, snr=5, **base).steps(1 << 15)
    assert np.mean(np.abs(nz) ** 2) > 1.0  # signal + noise power

    # LO carrier AND AWGN together (the fused tail).
    both = bits(freq=1e5, fs=1e6, snr=5, **base).steps(4096)
    assert np.all(np.isfinite(both.view(np.float32)))

    # Unmodulated bits (0/1 amplitude) shaped by the RRC FIR.
    none = bits(pattern=pat, sps=8, modulation="none", pulse="rrc").steps(4096)
    rect = bits(pattern=pat, sps=8, modulation="none").steps(4096)
    assert not np.array_equal(none, rect)


def test_rrc_bits_step_matches_steps():
    """Drive the per-sample step() bits-RRC path directly (the high-level
    Synth uses the block steps()): step() must match steps() bit-for-bit."""
    from doppler.wfm import _SynthEngine

    pat = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.uint8)
    taps = rrc_taps(0.35, 4, 6)

    def mk():
        e = _SynthEngine(type="bits", fs=1e6, freq=0.0, snr=100.0, sps=4)
        e.set_bits(pat, 1)
        e.set_rrc(taps)
        return e

    block = mk().steps(128)
    eng = mk()
    one_by_one = np.array([eng.step() for _ in range(128)], dtype=np.complex64)
    assert np.array_equal(block, one_by_one)


# ── bits (user pattern) ──────────────────────────────────────────────────────


def test_bits_bpsk_mapping():
    """bpsk: bit 0 -> +1, bit 1 -> -1; each bit held sps samples."""
    s = bits(pattern="10110101", sps=4, modulation="bpsk")
    y = s.steps(32)  # 8 bits * 4 sps = 32 samples / pass
    centers = y[2::4].real.round().astype(int).tolist()
    assert centers == [-1, 1, -1, -1, 1, -1, 1, -1]  # 1->-1, 0->+1


def test_bits_none_amplitude():
    """none: bit 0 -> 0, bit 1 -> 1 amplitude."""
    y = bits(pattern="1100", sps=1, modulation="none").steps(4)
    assert y.real.round().astype(int).tolist() == [1, 1, 0, 0]


def test_bits_qpsk_four_points():
    """qpsk consumes 2 bits/symbol → 4 Gray-mapped constellation points."""
    q = bits(pattern=[0, 0, 0, 1, 1, 0, 1, 1], sps=1, modulation="qpsk").steps(
        4
    )
    assert np.allclose(np.abs(q), 1.0, atol=1e-3)  # unit power
    quad = {(int(np.sign(c.real)), int(np.sign(c.imag))) for c in q}
    assert len(quad) == 4


def test_bits_hex_pattern():
    """A 0x.. hex string expands MSB-first to bits."""
    y = bits(pattern="0xA5", sps=1, modulation="none").steps(8)  # 1010 0101
    assert y.real.astype(int).tolist() == [1, 0, 1, 0, 0, 1, 0, 1]


def test_bits_cycles_to_fill():
    """The pattern repeats to fill a request longer than one pass."""
    s = bits(pattern="101", sps=2, modulation="none")
    period = 3 * 2  # 3 bits * 2 sps = 6 samples / pass
    one = s.steps(period)
    s.reset()
    two = s.steps(2 * period)
    assert np.array_equal(two, np.tile(one, 2))


def test_bits_reset_reproduces():
    s = bits(pattern="11010010", sps=3, modulation="bpsk", seed=5)
    period = 8 * 3  # 8 bits * 3 sps
    a = s.steps(period)
    s.reset()
    assert np.array_equal(a, s.steps(period))


def test_bits_array_input():
    """A numpy 0/1 array is accepted directly."""
    arr = np.array([1, 0, 1, 1], dtype=np.uint8)
    y = bits(pattern=arr, sps=1, modulation="bpsk").steps(4)
    assert y.real.round().astype(int).tolist() == [-1, 1, -1, -1]


def test_carrier_freq_truncates_toward_zero():
    """The carrier's actual frequency is the *truncated* 32-bit phase-word
    approximation of the requested frequency (native/inc/nco/nco_core.h's
    nco_norm_freq_to_inc, the one shared LO/NCO primitive), rounded toward
    zero
    -- not to nearest. freq=51 Hz at fs=21e6 is a case where truncation and
    round-to-nearest differ (freq=50 Hz at the same fs does not -- see
    native/tests/test_lo_core.c's own cases for both): the truncated
    increment leaves the carrier low by ~0.63 of a quantization step,
    past the half-step a round would guarantee -- and strictly never high.
    Truncation is deliberate: a bare C99 float->uint cast is bit-identical
    on every host, where round-to-nearest is FP-sensitive at a boundary and
    made a closed-loop DLL diverge on arm64 (the rationale is documented in
    nco_core.h). Measured directly via mean instantaneous frequency over a
    modest block -- a fixed-point phase increment is exactly constant per
    sample by construction, so this needs no long floating-point simulation
    (a double loses precision at large sample counts too; the exact 32-bit
    modular accumulator is the only long-run guarantee this design makes)."""
    fs = 21e6
    freq = 51.0
    step_hz = fs / 4294967296.0
    # The truncating (toward zero) phase-word approximation the NCO realises.
    inc = int(freq / fs * 4294967296.0)  # C99 float->uint cast == floor here
    realised = inc / 4294967296.0 * fs
    # A large-enough block for the per-sample estimator to average out the
    # LUT's own coarser (2^16) quantization noise at this slow a relative
    # rate; deterministic (snr=100 disables AWGN), so this is not flaky.
    x = Synth(type="tone", fs=fs, freq=freq, snr=100.0).steps(1_000_000)
    measured = float(np.mean(_inst_freq(x, fs)))
    # Realised == the truncated approximation (not the round-to-nearest one).
    assert measured == pytest.approx(realised, abs=1e-4), (
        f"measured {measured} vs truncated-realised {realised}"
    )
    # ... which is LOW by up to one full step, and strictly never high.
    assert freq - measured <= step_hz + 1e-4
    assert measured <= freq + 1e-4


def test_step_matches_steps_bit_exact():
    """Synth.step() (inline per-sample) must reproduce Synth.steps()
    (block) bit-exactly -- the same guarantee this project's C-level
    LO/NCO test suite already locks in (native/tests/test_lo_core.c)."""
    a = Synth(type="tone", fs=1e6, freq=12345.6, snr=100.0)
    b = Synth(type="tone", fs=1e6, freq=12345.6, snr=100.0)
    ref = a.steps(257)
    got = np.array([b.step() for _ in range(257)], dtype=np.complex64)
    assert np.array_equal(got, ref)


def test_bits_needs_pattern():
    # A pattern-less bits Synth has nothing to transmit. Standalone generation
    # is lazy, so the guard (in the C bridge) surfaces at first steps(): the
    # generated ensure_gen raises when wfm_source_to_synth returns NULL.
    with pytest.raises(RuntimeError):
        Synth(type="bits").steps(4)


def test_bits_bad_string_rejected():
    with pytest.raises(ValueError):
        bits(pattern="10201").steps(4)  # '2' is not a bit


# ── symbols (user complex constellation) ─────────────────────────────────────


def _symbols_engine(syms, sps=4, **kw):
    from doppler.wfm import _SynthEngine

    kw.setdefault("fs", 1.0)
    kw.setdefault("freq", 0.0)
    kw.setdefault("snr", 100.0)
    e = _SynthEngine(type="symbols", sps=sps, **kw)
    e.set_symbols(np.asarray(syms, np.complex64))
    return e


def test_symbols_are_the_constellation():
    """The symbol IS the output point — no bit→symbol mapping, just hold."""
    syms = np.array([1 + 0j, 1j, -1 + 0j, -1j], np.complex64)
    y = _symbols_engine(syms, sps=4).steps(16)
    assert np.allclose(y[::4], syms, atol=1e-5)  # symbol centres


def test_symbols_cycles_to_fill():
    syms = np.array([1 + 1j, -1 - 1j], np.complex64)
    e = _symbols_engine(syms, sps=2)
    one = e.steps(4)  # 2 syms * 2 sps
    e.reset()
    assert np.array_equal(e.steps(8), np.tile(one, 2))


def test_symbols_step_matches_steps():
    syms = np.array([1 + 1j, 1j, -1, 0.5 - 0.5j], np.complex64)
    a = _symbols_engine(syms, sps=4)
    b = _symbols_engine(syms, sps=4)
    ys = a.steps(64)
    yp = np.array([b.step() for _ in range(64)], np.complex64)
    assert np.array_equal(ys, yp)


def test_symbols_pi4_qpsk():
    """pi/4-QPSK as 'compute the symbols, pass them' — rotate every other
    QPSK symbol by pi/4. This is the generalisation the feature is for."""
    base = np.array([1, 1j, -1, -1j, 1, 1j, -1, -1j], np.complex64)
    syms = base.copy()
    syms[1::2] *= np.exp(1j * np.pi / 4)
    syms = syms.astype(np.complex64)
    y = _symbols_engine(syms, sps=4).steps(len(syms) * 4)
    assert np.allclose(y[::4], syms, atol=1e-5)
    # even symbols sit on a cardinal axis; the pi/4-rotated odd symbols are
    # off both axes (the alternating-constellation signature of pi/4-QPSK).
    assert np.allclose(syms[0::2].imag, 0, atol=1e-5)
    assert np.all(np.abs(syms[1::2].real) > 0.5)
    assert np.all(np.abs(syms[1::2].imag) > 0.5)


def test_symbols_rrc_matches_matched_filter():
    """RRC-shaped symbols == symbol-rate impulse train ⊛ (taps·√sps)."""
    sps, span, beta, n = 4, 6, 0.35, 120
    syms = np.array([1 + 1j, -1 + 1j, 1 - 1j, -1 - 1j, 1 + 0j], np.complex64)
    taps = rrc_taps(beta, sps, span)
    e = _symbols_engine(syms, sps=sps)
    e.set_rrc(taps)
    got = e.steps(n)
    nsym = n // sps + span + 4
    imp = np.zeros(nsym * sps, np.complex64)
    imp[::sps] = syms[np.arange(nsym) % len(syms)]
    ref = np.convolve(imp, taps * np.sqrt(sps))[:n]
    assert np.allclose(got, ref, atol=1e-5)
    assert not np.array_equal(got, _symbols_engine(syms, sps=sps).steps(n))


def test_symbols_carrier_and_noise():
    """Shaped symbols ride the LO and pick up AWGN (the mix tails)."""
    syms = np.array([1 + 1j, -1 - 1j], np.complex64)
    lo = _symbols_engine(syms, sps=8, freq=1e5).steps(4096)
    assert np.all(np.isfinite(lo.view(np.float32)))
    assert np.argmax(np.abs(np.fft.fft(lo))) != 0  # energy off DC
    nz = _symbols_engine(syms, sps=8, snr=5).steps(1 << 15)
    assert np.mean(np.abs(nz) ** 2) > 1.0  # signal + noise


def test_symbols_rrc_carrier_and_noise():
    """RRC-shaped symbols riding the LO with AWGN — exercises the FIR-path mix
    tails (has_lo && has_awgn, has_lo, has_awgn) in the block generator."""
    syms = np.array([1 + 1j, -1 - 1j, 1 - 1j], np.complex64)
    taps = rrc_taps(0.35, 8, 6)
    # LO + AWGN together (fused tail)
    e = _symbols_engine(syms, sps=8, freq=1e5, snr=5)
    e.set_rrc(taps)
    both = e.steps(4096)
    assert np.all(np.isfinite(both.view(np.float32)))
    # LO only (clean) and AWGN only (freq 0) with shaping
    lo = _symbols_engine(syms, sps=8, freq=1e5)
    lo.set_rrc(taps)
    assert np.argmax(np.abs(np.fft.fft(lo.steps(4096)))) != 0
    nz = _symbols_engine(syms, sps=8, snr=5)
    nz.set_rrc(taps)
    assert np.mean(np.abs(nz.steps(1 << 15)) ** 2) > 0


def test_symbols_reset_reproduces():
    syms = np.array([1 + 1j, 1j, -1, -1j, 0.3 + 0.7j], np.complex64)
    e = _symbols_engine(syms, sps=3)
    a = e.steps(150)
    e.reset()
    assert np.array_equal(a, e.steps(150))


def test_symbols_empty_rejected():
    from doppler.wfm import _SynthEngine

    e = _SynthEngine(type="symbols", fs=1.0, freq=0.0, snr=100.0, sps=4)
    with pytest.raises(ValueError):
        e.set_symbols(np.array([], np.complex64))


# ── chirp (LFM) ──────────────────────────────────────────────────────────────


def test_chirp_unit_envelope():
    """A pure FM sweep has constant (unit) magnitude everywhere."""
    x = chirp(f_start=1e5, f_end=3e5, fs=1e6).steps(4096)
    assert np.allclose(np.abs(x), 1.0, atol=1e-4)


def test_chirp_up_sweep_linear():
    """Instantaneous frequency rises linearly from f_start to f_end."""
    fs, f0, f1, n = 1e6, 1e5, 3e5, 8192
    x = chirp(f_start=f0, f_end=f1, fs=fs).steps(n)
    f = _inst_freq(x, fs)
    assert np.isclose(f[0], f0, atol=2e3)  # starts at f_start
    assert np.isclose(f[-1], f1, atol=2e3)  # ends at f_end
    # linear ramp: a straight-line fit explains essentially all the variance
    coeffs = np.polyfit(np.arange(len(f)), f, 1)
    resid = f - np.polyval(coeffs, np.arange(len(f)))
    assert coeffs[0] > 0 and np.std(resid) < 2e3


def test_chirp_down_sweep():
    """f_end < f_start sweeps high → low."""
    fs, f0, f1, n = 1e6, 3e5, 1e5, 8192
    f = _inst_freq(chirp(f_start=f0, f_end=f1, fs=fs).steps(n), fs)
    assert np.isclose(f[0], f0, atol=2e3)
    assert np.isclose(f[-1], f1, atol=2e3)


def test_chirp_span_is_generation_length():
    """The sweep fills exactly the requested length: f_end is hit at sample
    N."""
    fs, f0, f1 = 1e6, 1e5, 4e5
    short = _inst_freq(chirp(f_start=f0, f_end=f1, fs=fs).steps(2000), fs)
    long = _inst_freq(chirp(f_start=f0, f_end=f1, fs=fs).steps(8000), fs)
    # both reach f_end at their own end, so the short sweep ramps ~4x faster
    assert np.isclose(short[-1], f1, atol=3e3)
    assert np.isclose(long[-1], f1, atol=3e3)
    assert (short[1] - short[0]) > 3 * (long[1] - long[0])


def test_chirp_reset_reproduces():
    s = chirp(f_start=1e5, f_end=3e5, fs=1e6)
    a = s.steps(4096)
    s.reset()
    assert np.array_equal(a, s.steps(4096))


def test_chirp_respects_snr():
    """A noisy chirp adds AWGN over fs like a tone (clean chirp is unit
    power)."""
    clean = chirp(f_start=1e5, f_end=3e5, fs=1e6, snr=100).steps(1 << 16)
    noisy = chirp(f_start=1e5, f_end=3e5, fs=1e6, snr=10, seed=3).steps(
        1 << 16
    )
    assert np.isclose(_power(clean), 1.0, atol=0.02)
    # signal (1) + noise (1/10) over the band
    assert np.isclose(_power(noisy), 1 + 1 / 10, atol=0.05)


def test_chirp_freq_is_f_start_alias():
    """``freq`` and ``f_start`` are the same knob for a chirp."""
    a = Synth(type="chirp", freq=1e5, f_end=2e5, fs=1e6).steps(1024)
    b = Synth(type="chirp", f_start=1e5, f_end=2e5, fs=1e6).steps(1024)
    assert np.array_equal(a, b)


@pytest.mark.parametrize("sps", [1, 2, 4, 8])
def test_rrc_shaper_lives_at_every_power_of_two_sps(sps: int) -> None:
    """The polyphase RRC shaper must produce a real waveform at sps == 1.

    ``sps == 1`` is a power of two, so ``set_rrc`` takes the polyphase
    branch and builds the shaper at ``rate = sps = 1.0``. Two undefined
    conversions used to meet there and cancel: ``phase_inc`` came from
    ``(uint32_t)(2^32 / 1.0)`` (C99 6.3.1.4, 0 on x86) and ``get_branch``
    shifted a ``uint32_t`` by 32 (6.5.7p3). A zero increment pinned the
    phase at 0, which made the bad shift select arm 0, and the shaper
    silently emitted a DEAD waveform -- all zeros, one distinct sample --
    while every other sps was correct.

    Fixing either alone is worse than fixing neither: a live phase with
    the bad shift indexes far outside a one-arm bank and segfaults. So
    this pins the OUTPUT, not the increment, and it is parametrized
    across sps because the defect was invisible at every value but one.
    """
    s = Synth(
        type="bpsk",
        sps=sps,
        pulse="rrc",
        rrc_beta=0.35,
        rrc_span=8,
        symbols=256,
        seed=7,
    )
    y = np.asarray(s.steps(256))

    assert len(y) == 256
    assert not np.allclose(y, 0.0), f"dead waveform at sps={sps}"
    # A shaped BPSK stream is near unit power and takes many distinct
    # values; a stalled delay line yields exactly one.
    assert len(np.unique(np.round(y, 6))) > 64
    assert 0.5 < np.sqrt((abs(y) ** 2).mean()) < 1.5


# ── The unspread frame ───────────────────────────────────────────────────────
#
# `sync` / `acq_code` / `acq_reps` / `crc` were accepted on every face, stored,
# and readable back — and applied only on `type="dsss"`. A caller who asked for
# a framed BPSK waveform got an unframed one, exit 0, no warning. What let that
# ship is that nothing asserted a frame kwarg CHANGES the waveform, so that is
# what these assert. A flag-presence check would have passed throughout.

BARKER13 = np.array([1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1], np.uint8)
ACQ8 = np.array([1, 0] * 4, np.uint8)
PAYLOAD = np.array([0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0], np.uint8)


def _framed_kwargs():
    return {"sync": BARKER13, "acq_code": ACQ8, "acq_reps": 4, "crc": "crc16"}


def test_a_frame_changes_the_waveform():
    """The assertion whose absence was the defect.

    Not "the kwargs are accepted" — they always were. The frame has to reach
    the samples, and the only way to say that is to compare against the
    unframed twin built from the same payload and seed.
    """
    common = {
        "type": "bits",
        "fs": 1e6,
        "sps": 4,
        "bits": PAYLOAD,
        "modulation": "bpsk",
    }
    plain = np.asarray(Synth(**common).steps(512))
    framed = np.asarray(Synth(**common, **_framed_kwargs()).steps(512))

    assert not np.array_equal(plain, framed)


def test_a_framed_stream_carries_the_frames_bits():
    """It is the DESCRIPTOR's bits, not merely different ones.

    `[preamble x 4 | Barker-13 | payload | CRC-16]` at one sample per symbol,
    BPSK-mapped (0 -> +1, 1 -> -1). Checking the head pins the layout and the
    order; checking the period pins that the frame is what cycles.
    """
    s = Synth(
        type="bits",
        fs=1.0,
        sps=1,
        bits=PAYLOAD,
        modulation="bpsk",
        **_framed_kwargs(),
    )
    nbits = 4 * len(ACQ8) + len(BARKER13) + len(PAYLOAD) + 16
    y = np.asarray(s.steps(2 * nbits)).real

    head = np.concatenate([np.tile(ACQ8, 4), BARKER13, PAYLOAD])
    np.testing.assert_allclose(y[: len(head)], 1.0 - 2.0 * head, atol=1e-6)
    # The 16-bit CRC trailer occupies the rest of the frame...
    assert len(y[:nbits]) == nbits
    # ...and then the whole frame repeats, which is what turns a one-frame
    # description into a multi-frame record with no repeat count in it.
    np.testing.assert_allclose(y[:nbits], y[nbits : 2 * nbits], atol=1e-6)


def test_the_frame_survives_a_reset():
    s = Synth(
        type="bits",
        fs=1.0,
        sps=1,
        bits=PAYLOAD,
        modulation="bpsk",
        **_framed_kwargs(),
    )
    first = np.asarray(s.steps(96))
    s.reset()
    np.testing.assert_array_equal(first, np.asarray(s.steps(96)))


@pytest.mark.parametrize("wtype", ["bpsk", "qpsk", "pn", "tone"])
def test_a_frame_a_waveform_cannot_carry_is_refused(wtype):
    """Refused, never accepted and dropped.

    These types source their symbols from the PN LFSR, so there is no length
    to bound a payload. Silently ignoring the request is what this whole
    change exists to stop, so the refusal is the feature — and the message
    names the replacement.
    """
    s = Synth(type=wtype, fs=1e6, sps=4, **_framed_kwargs())
    with pytest.raises((RuntimeError, ValueError)):
        s.steps(64)


def test_a_frame_with_no_payload_is_refused():
    s = Synth(
        type="bits", fs=1e6, sps=4, modulation="bpsk", **_framed_kwargs()
    )
    with pytest.raises((RuntimeError, ValueError)):
        s.steps(64)


def test_crc_alone_does_not_frame_an_unframed_pattern():
    """`crc` defaults to crc16 on EVERY source.

    So reading it as intent to frame would have appended a 16-bit trailer to
    every unframed bit pattern anyone has ever generated — a silent change to
    existing waveforms, which is the same class of failure as the one being
    fixed. A preamble or a sync word is what says "framed".
    """
    common = {
        "type": "bits",
        "fs": 1.0,
        "sps": 1,
        "bits": PAYLOAD,
        "modulation": "bpsk",
    }
    plain = np.asarray(Synth(**common).steps(64))
    crc_only = np.asarray(Synth(**common, crc="crc16").steps(64))

    np.testing.assert_array_equal(plain, crc_only)
