"""async_dsss_receiver_demo.py -- Acquisition -> Dll(segments) -> MpskReceiver,
the full continuous DSSS receive chain, against a continuous Gold code
carrying asynchronous BPSK data modulation.

Stage 3 of the same multi-part story as ``dsss_acq_async_data_demo.py``
(Stage 1: does :class:`~doppler.dsss.Acquisition` land the right code
phase/Doppler bin) and ``dsss_despread_async_data_demo.py`` (Stage 2: does
that hit correctly seed :class:`~doppler.track.Dll`, and is ``segments=4``
robust enough for the DLL's *own* tracking loop). This page asks the last
question the story set up: carrier and symbol-timing recovery
(:class:`~doppler.track.MpskReceiver`) sit downstream of ``Dll`` too --
does Stage 2's own ``segments=4`` sweet spot suffice there as well, or
does a coherent-combining matched filter need something ``Dll``'s own
loop never needed?

``MpskReceiver``'s own docstring states the intended composition: *"A
DSSS-MPSK receiver is ``Dll(segments) -> MpskReceiver``: despread to
symbol-rate soft chips, then this modem."* ``Dll.steps()``'s partial
stream feeds ``MpskReceiver.steps()`` directly -- no intermediate carrier
wipe or matched filter, since ``MpskReceiver`` owns both internally
(NDA carrier acquisition, then Gardner+Farrow symbol timing, then --
once locked -- decision-directed tracking).

**The central finding, measured fresh on this page's signal (not assumed
from an older run): ``segments=4`` never decodes.** Streamed through
``MpskReceiver`` unchanged, its BER sits at ~0.47 -- indistinguishable
from a coin flip -- for the entire run. ``segments=34`` (chosen so
``round(segments * T_sym/T_epoch)`` lands on an ``MpskReceiver``-friendly
``sps`` with a small divisor for the carrier-arm count ``n``) decodes
perfectly, converging to 100% symbol correctness within about 150
symbols and staying there. The reason ``segments=4`` fails here even
though it's ``Dll``'s own robust choice (Stage 2): each partial is
``segments``-times weaker than a full coherent code epoch, and while
that's fine for the DLL's own non-coherent early/late discriminator, it
starves ``MpskReceiver``'s coherent matched filter (length ``sps``) of
the samples-per-symbol it needs to rebuild real coherent gain before its
carrier/timing loops can converge at all.

Two more things worth knowing before reusing this pattern:

- **``MpskReceiver``'s own ``tracking``/``lock`` flags are not reliable
    stand-ins for "decoding correctly" at the failing ``segments``.**
    ``segments=4`` frequently reports ``tracking=1`` with a healthy-looking
    ``lock`` value despite decoding pure noise -- the carrier loop can
    lock to *something* (a wrong absolute phase, or a timing point that
    isn't actually sampling the symbol) without ever producing a correct
    bit. Don't gate success on these flags for an unfamiliar ``segments``;
    check measured BER against known data instead, same as this page does
    (extending Stage 2's own caution about ``Dll.locked`` at large
    ``segments`` -- the analogous flag one layer up shows the same
    pattern).
- **``init_norm_freq`` starts from a coarse, quantized estimate, not the
    true residual carrier.** ``Acquisition``'s Doppler bins are sized
    wide (this page's config resolves to a single ~kHz-scale bin at
    Doppler=0), so the seed handed to ``MpskReceiver`` can be off by the
    full within-bin residual -- here, the entire 50 Hz injected Doppler.
    The NDA carrier loop pulls this in over the first ~100 epochs (panel
    4 below); nothing downstream needs the Acquisition estimate to be
    exact, only close enough for the carrier loop's own capture range.

The phase-inversion hand-off (``Acquisition.code_phase`` -> ``Dll``'s
``init_chip``) is Stage 2's own finding, reused verbatim here via
:func:`doppler.dsss.handoff.dll_init_chip_from_acq` -- not re-litigated
on this page.

Four panels, all at this page's one operating point (CN0=97 dB-Hz,
chosen -- as in the pre-story version of this demo -- to unambiguously
validate the pipeline mechanics rather than run a margin sensitivity
study; see Stage 2 for a page that studies margin sensitivity instead):

1. **Decoded BPSK constellation** (settled window, ``segments=34``): two
   tight clusters at +/-1, confirming the residual carrier is fully
   removed and symbol timing has converged.
2. **Running BER** (settled window, ``segments=34``): flat at zero,
   confirming the lock isn't a lucky momentary alignment.
3. **Windowed decode correctness, ``segments=4`` vs. ``segments=34``**
   (50-symbol windows, the central finding above): ``segments=34``
   converges to 100% correct almost immediately; ``segments=4`` never
   rises above chance for the entire run.
4. **``MpskReceiver.norm_freq`` vs. epoch** (``segments=34``): the
   carrier loop's pull-in from the Acquisition-quantized seed to the true
   residual Doppler.

Run:  python -m doppler.examples.async_dsss_receiver_demo  [out.png]
"""

from __future__ import annotations

import sys
import warnings

# --8<-- [start:signal]
import numpy as np

from doppler.dsss import Acquisition
from doppler.wfm import Gold

SF = 1023  # 2**10 - 1: the CCSDS 415.0-G-1 command-link Gold code period
CHIP_RATE = 3.0e6  # Hz
SYM_RATE = 2100.0  # Hz -- chips/symbol = 1428.6, non-integer (asynchronous)
SPC = 2  # samples/chip (front-end oversample)
FS = CHIP_RATE * SPC
TE = SF * SPC  # samples per code epoch
TSYM = FS / SYM_RATE  # samples per symbol ~= 1.4 code epochs

DOPPLER_HZ = 50.0  # residual carrier -- never removed upstream of MpskReceiver
N_SYM = 6000
PRE_SILENCE = TE * 20 + 737  # deliberately not a whole number of epochs

# CCSDS defaults: known 3-valued correlation sidelobes {-1, -65, 63}, a
# real, cross-checked reference code instead of an arbitrary MLS choice.
CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
_CSIGN = np.where(CODE & 1, -1.0, 1.0)


def make_signal(cn0_dbhz: float, seed: int):
    """A continuous, asynchronous-symbol-clock DSSS capture at real units.

    Identical construction to Stage 1/2's ``make_signal`` -- silence,
    then the code-spread BPSK stream (independent symbol clock, residual
    carrier), with AWGN injected via the same C/N0 -> per-sample
    amplitude-SNR relationship ``Acquisition`` itself uses for sizing, so
    ``cn0_dbhz`` is directly the injected C/N0 in dB-Hz.

    Returns
    -------
    x : NDArray[np.complex64]
        The full capture (silence + signal + noise).
    data : NDArray[np.float64]
        The transmitted BPSK symbols (+/-1), ground truth for validation.
    """
    rng = np.random.default_rng(seed)
    n = int(N_SYM * TSYM) + 2 * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, N_SYM + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    cph = (idx / SPC).astype(int) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (DOPPLER_HZ / FS) * idx)

    amp_snr = np.sqrt(10.0 ** (cn0_dbhz / 10.0) / FS)  # per-sample amplitude
    sigma = 1.0 / amp_snr  # total complex noise RMS, unit chip amplitude
    total_n = int(PRE_SILENCE) + n
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(total_n) + 1j * rng.standard_normal(total_n)
    )
    x = np.concatenate([np.zeros(int(PRE_SILENCE)), sig]).astype(
        np.complex64
    ) + noise.astype(np.complex64)
    return x, data


# --8<-- [end:signal]

from doppler.dsss.handoff import dll_init_chip_from_acq  # noqa: E402
from doppler.track import Dll, MpskReceiver  # noqa: E402

SEED = 6
CN0_OPERATING_DBHZ = 97.0  # deliberately strong -- see the module docstring

# Segments per code epoch that MpskReceiver actually needs (measured on
# this page's signal, not assumed from the older PN-code run) versus
# Dll's own tracking sweet spot from Stage 2, kept for direct contrast.
K_WORKING = 34
K_DLL_SWEETSPOT = 4
BN = 0.002  # every validated Dll example in this codebase uses this, not
# the constructor's own default (0.01, unstable at this geometry -- see
# Stage 2's module docstring).


# --8<-- [start:acq_symbol_rate]
def _new_acq() -> Acquisition:
    """Symbol-rate-aware sizing, identical to Stage 2's ``_new_acq``."""
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        acq = Acquisition(
            CODE,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
            doppler_uncertainty=100.0,
            reps=16,
            max_noncoh=8,
            symbol_rate=SYM_RATE,
            pfa=1e-3,
            pd=0.9,
        )
    assert acq.pd_predicted >= acq.pd
    return acq


# --8<-- [end:acq_symbol_rate]


def _acquire(x: np.ndarray):
    """Stream ``x`` until a hit lands; return ``(hit, hitpos, acq)``."""
    acq = _new_acq()
    frame = acq.code_bins * acq.doppler_bins
    pos = 0
    while pos + frame <= len(x):
        hits = acq.push(x[pos : pos + frame])
        if hits:
            return hits[0], pos, acq
        pos += frame
    return None, None, acq


def _mpsk_dims(segments: int):
    """MpskReceiver's own sps/n for a given Dll ``segments`` count --
    ``sps`` chosen so ``round(segments*TSYM/TE)`` lands close to an
    integer partials-per-symbol count, ``n`` the largest divisor of
    ``sps`` in ``{4, 2, 1}`` (the carrier-arm count)."""
    sps = round(segments * TSYM / TE)
    n_arm = next(c for c in (4, 2, 1) if sps % c == 0)
    return sps, n_arm


# --8<-- [start:handoff]
def _new_chain(chip_phase: float, doppler_hz_est: float, segments: int):
    """Build a hand-off-seeded ``Dll``/``MpskReceiver`` pair.

    ``MpskReceiver``'s ``init_norm_freq`` is cycles per *its own input
    sample* -- the Dll partial stream, at rate ``FS*segments/TE`` -- not
    cycles per raw ADC sample. Converting by the raw sample rate instead
    would seed a wildly wrong carrier frequency.
    """
    partial_rate = FS * segments / TE
    norm_freq = doppler_hz_est / partial_rate
    sps, n_arm = _mpsk_dims(segments)
    dll = Dll(CODE, SPC, chip_phase, BN, 0.707, 0.5, segments=segments)
    rx = MpskReceiver(
        m=2,
        sps=sps,
        n=n_arm,
        pulse="iandd",
        bn_carrier=0.01,
        bn_timing=0.01,
        acq_to_track=1,
        lock_thresh=0.3,
        init_norm_freq=norm_freq,
        warmup_syms=30,
    )
    return dll, rx


# --8<-- [end:handoff]


def _receive(
    x, s0: int, chip_phase: float, doppler_hz_est: float, segments: int
):
    """Despread ``x[s0:]`` and demodulate it in one pass; return
    ``(dll, rx, syms)``. ``MpskReceiver.steps()`` is block-size invariant
    (state carries across calls), so one call over the whole despread
    stream is equivalent to a per-epoch streaming call."""
    dll, rx = _new_chain(chip_phase, doppler_hz_est, segments)
    rest = x[s0:]
    nep = len(rest) // TE
    parts = [dll.steps(rest[e * TE : (e + 1) * TE]) for e in range(nep)]
    part = np.concatenate(parts)
    syms = rx.steps(part)
    return dll, rx, syms


def _decode_ber(syms: np.ndarray, data: np.ndarray, data_start: int):
    """Hard-decide ``syms`` and lag-search against the known ``data`` for
    the best-matching (possibly phase-inverted) alignment. The lag search
    absorbs the Gardner/Farrow settling transient and the receiver's own
    pipeline delay -- the *existence* of a clean near-zero-error lag is
    the proof, not any particular lag value."""
    bits = np.where(syms.real > 0, 1.0, -1.0)
    lo, hi = len(bits) // 3, 2 * len(bits) // 3
    best_ber, best_lag, best_inv = 1.0, 0, False
    for lag in range(-100, 101):
        ti = data_start + lag + np.arange(lo, hi)
        if ti.min() < 0 or ti.max() >= len(data):
            continue
        truth = data[ti]
        e_same = float(np.mean(bits[lo:hi] != truth))
        e_inv = float(np.mean(bits[lo:hi] != -truth))
        if e_same < best_ber:
            best_ber, best_lag, best_inv = e_same, lag, False
        if e_inv < best_ber:
            best_ber, best_lag, best_inv = e_inv, lag, True
    return bits, best_ber, best_lag, best_inv


def _windowed_ber(bits, data, data_start, lag, inv, window=50):
    """Fraction of incorrect decisions in each non-overlapping ``window``-
    symbol block across the *entire* recovered stream (not just the
    settled third ``_decode_ber`` uses for its headline number) -- this
    is what actually shows whether/when decoding converges, since the
    receiver's own ``tracking``/``lock`` flags don't (see the module
    docstring)."""
    aligned = bits if not inv else -bits
    idx = data_start + lag + np.arange(len(bits))
    valid = (idx >= 0) & (idx < len(data))
    correct = np.full(len(bits), np.nan)
    correct[valid] = (aligned[valid] == data[idx[valid]]).astype(float)
    n_win = len(correct) // window
    return np.array(
        [
            1.0 - np.nanmean(correct[i * window : (i + 1) * window])
            for i in range(n_win)
        ]
    )


def _norm_freq_trace(x, s0, chip_phase, doppler_hz_est, segments, n_epochs):
    """Stream ``segments`` epoch by epoch, recording ``rx.norm_freq``
    after each call -- the carrier loop's pull-in trace from its
    Acquisition-quantized seed toward the true residual Doppler."""
    dll, rx = _new_chain(chip_phase, doppler_hz_est, segments)
    nf = np.full(n_epochs, np.nan)
    pos = s0
    for i in range(n_epochs):
        if pos + TE > len(x):
            break
        part = dll.steps(x[pos : pos + TE])
        pos += TE
        if len(part):
            rx.steps(part)
        nf[i] = rx.norm_freq
    return nf


def main(out_path: str = "async_dsss_receiver_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x, data = make_signal(CN0_OPERATING_DBHZ, SEED)

    # --- stage 1: acquisition -- stream until a hit lands -------------------
    hit, hitpos, acq = _acquire(x)
    assert hit is not None, "acquisition failed to find the continuous code"
    dop_bin, code_phase, _pk, _noise_est, test_stat, cn0_dbhz_est = hit
    print(
        f"acquired: doppler_bin={dop_bin} code_phase={code_phase} "
        f"test_stat={test_stat:.1f} cn0_dbhz_est={cn0_dbhz_est:.1f} "
        f"(injected {CN0_OPERATING_DBHZ:.1f})"
    )

    # --- stage 2: hand-off ---------------------------------------------------
    doppler_bins = acq.doppler_bins
    k_fold = (dop_bin + doppler_bins // 2) % doppler_bins - doppler_bins // 2
    doppler_hz_est = k_fold * acq.doppler_res_hz
    chip_phase = dll_init_chip_from_acq(code_phase, SPC, SF)
    frame = acq.code_bins * acq.doppler_bins
    s0 = hitpos + frame
    data_start = round((s0 - PRE_SILENCE) / TSYM)

    # --- stage 3: despread + demod, at both segments counts -----------------
    _dll34, rx34, syms34 = _receive(
        x, s0, chip_phase, doppler_hz_est, K_WORKING
    )
    bits34, ber34, lag34, inv34 = _decode_ber(syms34, data, data_start)
    print(
        f"segments={K_WORKING}: recovered {len(syms34)} symbols, "
        f"tracking={rx34.tracking} lock={rx34.lock:.2f} ber={ber34:.4f} "
        f"lag={lag34} inverted={inv34}"
    )
    assert ber34 < 0.01, "segments=K_WORKING failed to decode cleanly"
    assert rx34.tracking == 1, "MpskReceiver never handed over to tracking"

    _dll4, rx4, syms4 = _receive(
        x, s0, chip_phase, doppler_hz_est, K_DLL_SWEETSPOT
    )
    bits4, ber4, lag4, inv4 = _decode_ber(syms4, data, data_start)
    print(
        f"segments={K_DLL_SWEETSPOT} (Dll's own Stage-2 sweet spot): "
        f"recovered {len(syms4)} symbols, tracking={rx4.tracking} "
        f"lock={rx4.lock:.2f} ber={ber4:.4f}"
    )
    # The central finding: Dll's own optimum is downstream-insufficient.
    assert ber4 > 0.3, (
        f"expected segments={K_DLL_SWEETSPOT} to fail to decode (near-"
        f"chance BER) despite being Dll's own tracking sweet spot; "
        f"got ber={ber4:.4f}"
    )
    assert ber34 < ber4, (
        f"expected segments={K_WORKING} to decode far better than "
        f"segments={K_DLL_SWEETSPOT} (ber34={ber34:.4f}, ber4={ber4:.4f})"
    )

    wber34 = _windowed_ber(bits34, data, data_start, lag34, inv34)
    wber4 = _windowed_ber(bits4, data, data_start, lag4, inv4)
    assert np.nanmean(wber34[-5:]) < 0.05, (
        "expected segments=K_WORKING's windowed BER to converge to "
        f"~0 by the end of the run (last 5 windows mean="
        f"{np.nanmean(wber34[-5:]):.3f})"
    )
    assert np.nanmean(wber4[-5:]) > 0.3, (
        "expected segments=K_DLL_SWEETSPOT's windowed BER to stay near "
        f"chance the whole run (last 5 windows mean="
        f"{np.nanmean(wber4[-5:]):.3f})"
    )

    n_epochs_nf = min(300, (len(x) - s0) // TE)
    nf_trace = _norm_freq_trace(
        x, s0, chip_phase, doppler_hz_est, K_WORKING, n_epochs_nf
    )
    partial_rate34 = FS * K_WORKING / TE
    norm_freq_true = DOPPLER_HZ / partial_rate34

    # --- plot -----------------------------------------------------------
    fig, ((a, b), (c, d)) = plt.subplots(2, 2, figsize=(11, 8.5))

    lo, hi = len(bits34) // 3, 2 * len(bits34) // 3
    a.scatter(
        syms34[lo:hi].real,
        syms34[lo:hi].imag,
        s=8,
        color="#1f77b4",
        alpha=0.5,
    )
    a.axhline(0, color="k", lw=0.5)
    a.axvline(0, color="k", lw=0.5)
    lim = 1.3 * np.max(np.abs(syms34[lo:hi]))
    a.set_xlim(-lim, lim)
    a.set_ylim(-lim, lim)
    a.set_aspect("equal")
    a.set_title(
        f"Decoded BPSK symbols (settled window, segments={K_WORKING})\n"
        f"BER={ber34:.4f}",
        fontsize=9,
    )
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.grid(alpha=0.25)

    aligned34 = bits34 if not inv34 else -bits34
    idx34 = data_start + lag34 + np.arange(lo, hi)
    correct34 = aligned34[lo:hi] == data[idx34]
    running_ber = 1.0 - np.cumsum(correct34) / (np.arange(len(correct34)) + 1)
    b.plot(running_ber, color="#2ca02c", lw=1.1)
    b.set_ylim(0, max(0.05, running_ber.max() * 1.1))
    b.set_title(
        f"Running BER over the settled window (segments={K_WORKING})",
        fontsize=9,
    )
    b.set_xlabel("symbol index (settled window)")
    b.set_ylabel("running BER")
    b.grid(alpha=0.25)

    c.plot(
        np.arange(len(wber34)) * 50,
        wber34,
        lw=1.1,
        color="#1f77b4",
        label=f"segments={K_WORKING}",
    )
    c.plot(
        np.arange(len(wber4)) * 50,
        wber4,
        lw=1.1,
        color="#d62728",
        label=f"segments={K_DLL_SWEETSPOT} (Dll's own sweet spot)",
    )
    c.axhline(0.5, color="k", lw=0.8, ls="--", label="chance")
    c.set_ylim(-0.05, 0.65)
    c.set_title(
        "Windowed decode correctness (50-symbol windows)\n"
        "Dll's own optimum is downstream-insufficient",
        fontsize=9,
    )
    c.set_xlabel("symbol index")
    c.set_ylabel("windowed BER")
    c.legend(fontsize=7)
    c.grid(alpha=0.25)

    ep = np.arange(len(nf_trace))
    d.plot(ep, nf_trace, lw=1.1, color="#1f77b4", label="rx.norm_freq")
    d.axhline(
        norm_freq_true, color="k", lw=1.0, ls="--", label="true norm_freq"
    )
    d.set_title(
        f"Carrier pull-in (segments={K_WORKING})\n"
        "from the Acquisition-quantized seed",
        fontsize=9,
    )
    d.set_xlabel("epoch")
    d.set_ylabel("MpskReceiver.norm_freq")
    d.legend(fontsize=7)
    d.grid(alpha=0.25)

    fig.suptitle(
        f"Acquisition -> Dll(segments) -> MpskReceiver -- CCSDS Gold code "
        f"(SF={SF}), continuous asynchronous BPSK data ({SYM_RATE:.0f} "
        f"sym/s), C/N0={CN0_OPERATING_DBHZ:.0f} dB-Hz",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.subplots_adjust(hspace=0.5, wspace=0.3)
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "async_dsss_receiver_demo.png")
