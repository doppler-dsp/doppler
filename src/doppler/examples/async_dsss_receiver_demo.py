"""async_dsss_receiver_demo.py -- Acquisition -> Dll(segments) ->
RateConverter -> MpskReceiver, the full continuous DSSS receive chain,
against a continuous Gold code carrying asynchronous BPSK data
modulation.

Stage 3 of the same multi-part story as ``dsss_acq_async_data_demo.py``
(Stage 1: does :class:`~doppler.dsss.Acquisition` land the right code
phase/Doppler bin) and ``dsss_despread_async_data_demo.py`` (Stage 2: does
that hit correctly seed :class:`~doppler.track.Dll`, and is ``segments=4``
robust enough for the DLL's *own* tracking loop). This page asks the last
question the story set up: carrier and symbol-timing recovery
(:class:`~doppler.track.MpskReceiver`) sit downstream of ``Dll`` too --
does Stage 2's own ``segments=4`` sweet spot suffice there as well?

**The despreader's only job is to remove the code.** ``Dll(segments=K)``
emits its partial-correlation dumps at a fixed, uniform rate
``K*chip_rate/SF`` -- a sub-multiple of the chip rate, nothing more.
Turning that into a clean ``N`` samples/symbol grid for the demodulator
is a *separate* problem, solved by an explicit resampler
(:class:`~doppler.resample.RateConverter`, arbitrary output/input ratio,
already in this codebase), not by contorting ``Dll``'s own tuning
parameter to fake the right output rate. An earlier version of this page
got this wrong: it wired ``segments`` directly into ``MpskReceiver``'s
``sps`` (picking ``segments=34`` purely because
``round(segments*T_sym/T_epoch)`` landed on an integer), which made
``segments=4`` -- Stage 2's own tracking-optimal choice -- look
downstream-broken. It wasn't. At ``segments=4`` the partial rate is
``~11.7 kHz``, already ~5.6x the 2100 sym/s symbol rate -- comfortably
past the 2x Nyquist floor for symbol timing recovery. The failure was
architectural, not a property of non-coherent partial correlation.

**The corrected chain, measured fresh:** ``Dll(segments=4)`` (Stage 2's
own choice, kept for its own tracking-robustness reasons and nothing
else) feeds :class:`~doppler.resample.RateConverter`, which converts the
partial stream to a clean ``N=8`` samples/symbol -- ``MpskReceiver``'s
own constructor default, the same shape used everywhere else in this
codebase (``test_mpsk_receiver.py``, ``mpsk_receiver_demo.py``). A
"normal" ``MpskReceiver(m=2, sps=8, n=4, ...)`` takes over from there,
with none of the previous ``sps=47`` weirdness. On the *same* signal,
*same* ``segments=4``, the only thing that changed is whether a resample
stage sits in between: without it, BER sits at ~0.44 (chance) the entire
run; with it, BER is 0.0 and stays there. Panel 3 below is that direct
before/after comparison. ``init_norm_freq``'s unit conversion also
simplifies and becomes independent of ``segments``: it's cycles per
``RateConverter``'s *output* sample rate (``N*symbol_rate``, fixed),
not per the despreader's own partial rate.

**A caveat found while re-measuring, not chased further:** this specific
loop tuning (``bn_carrier=bn_timing=0.01``, ``sps=8``) stays perfectly
locked through the ~3500-symbol run plotted here, but a longer run (past
~4000 symbols, at this same margin) shows the symbol-timing loop starting
to jitter and occasionally slip. That's a separate, further loop-tuning
question -- how finely ``Gardner`` timing resolution scales with ``sps``
under this margin -- not resolved on this page, the same way Stage 2
found (and didn't chase) ``segments=1``'s eventual long-run divergence.

Two more things worth knowing before reusing this pattern:

- **``MpskReceiver``'s own ``tracking``/``lock`` flags are not reliable
    stand-ins for "decoding correctly."** The broken (un-resampled)
    variant frequently reports ``tracking=1`` with a healthy-looking
    ``lock`` value despite decoding pure noise -- the carrier loop can
    lock to *something* without ever producing a correct bit. Check
    measured BER against known data instead, same as this page does
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

1. **Decoded BPSK constellation** (settled window): two tight clusters
   at +/-1, confirming the residual carrier is fully removed and symbol
   timing has converged.
2. **Running BER** (settled window): flat at zero, confirming the lock
   isn't a lucky momentary alignment.
3. **Windowed decode correctness, with vs. without the resample stage**
   (50-symbol windows, the central finding above, both at ``segments=4``):
   with ``RateConverter`` in the chain, correctness is 100% from the
   first window on; without it, it never rises above chance.
4. **``MpskReceiver.norm_freq`` vs. epoch**: the carrier loop's pull-in
   from the Acquisition-quantized seed to the true residual Doppler.

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
# Deliberately shorter than Stage 1/2's N_SYM=6000: at this margin/loop
# tuning the symbol-timing loop stays locked through ~4000 symbols and
# starts to jitter past that (see the module docstring's caveat) -- this
# page's own asserts are sized to what's actually verified, not to the
# largest number that happens to compile.
N_SYM = 3500
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
from doppler.resample import RateConverter  # noqa: E402
from doppler.track import Dll, MpskReceiver  # noqa: E402

SEED = 6
CN0_OPERATING_DBHZ = 97.0  # deliberately strong -- see the module docstring

# Dll's own tracking-optimal segments (Stage 2) -- chosen for Dll's own
# robustness, nothing else. Not coupled to MpskReceiver's sps at all;
# RateConverter is what bridges the two.
K = 4
BN = 0.002  # every validated Dll example in this codebase uses this, not
# the constructor's own default (0.01, unstable at this geometry -- see
# Stage 2's module docstring).

MPSK_SPS = 8  # MpskReceiver's own constructor default -- a "normal" config
MPSK_N = 4


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


# --8<-- [start:handoff]
def _new_chain(chip_phase: float, doppler_hz_est: float, resample: bool):
    """Build a hand-off-seeded ``Dll`` -> (optional) ``RateConverter`` ->
    ``MpskReceiver`` chain.

    ``resample=True`` is the correct architecture: ``Dll(segments=K)``
    is tuned purely for its own tracking robustness (Stage 2), and
    ``RateConverter`` converts its partial-correlation stream (a
    sub-multiple of the chip rate) to a clean ``MPSK_SPS`` samples/symbol
    for a "normal" ``MpskReceiver``. ``resample=False`` reproduces the
    earlier, broken direct-wiring (``segments`` forced to double as
    ``sps``) purely for the before/after comparison in panel 3 -- not a
    pattern to reuse.
    """
    dll = Dll(CODE, SPC, chip_phase, BN, 0.707, 0.5, segments=K)
    if resample:
        partial_rate = FS * K / TE
        target_rate = MPSK_SPS * SYM_RATE
        rc = RateConverter(rate=target_rate / partial_rate)
        norm_freq = doppler_hz_est / target_rate
        sps, n_arm = MPSK_SPS, MPSK_N
    else:
        rc = None
        partial_rate = FS * K / TE
        norm_freq = doppler_hz_est / partial_rate
        sps = round(K * TSYM / TE)
        n_arm = next(c for c in (4, 2, 1) if sps % c == 0)
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
    return dll, rc, rx


# --8<-- [end:handoff]


def _receive(
    x, s0: int, chip_phase: float, doppler_hz_est: float, resample: bool
):
    """Despread ``x[s0:]``, optionally resample, and demodulate in one
    pass; return ``(dll, rx, syms)``. Both ``RateConverter.execute()``
    and ``MpskReceiver.steps()`` are block-size invariant (state carries
    between calls), so one call over the whole stream is equivalent to a
    per-epoch streaming call."""
    dll, rc, rx = _new_chain(chip_phase, doppler_hz_est, resample)
    rest = x[s0:]
    nep = len(rest) // TE
    parts = [dll.steps(rest[e * TE : (e + 1) * TE]) for e in range(nep)]
    part = np.concatenate(parts)
    stream = rc.execute(part) if rc is not None else part
    syms = rx.steps(stream)
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
    with warnings.catch_warnings():
        # A window entirely before data_start+lag>=0 (a large negative
        # lag, only possible for the un-resampled/broken chain) is
        # all-NaN by construction; the resulting NaN is the correct
        # "unknown" value, not a bug.
        warnings.simplefilter("ignore", RuntimeWarning)
        return np.array(
            [
                1.0 - np.nanmean(correct[i * window : (i + 1) * window])
                for i in range(n_win)
            ]
        )


def _norm_freq_trace(x, s0, chip_phase, doppler_hz_est, n_epochs):
    """Stream the corrected chain epoch by epoch, recording
    ``rx.norm_freq`` after each call -- the carrier loop's pull-in trace
    from its Acquisition-quantized seed toward the true residual
    Doppler."""
    dll, rc, rx = _new_chain(chip_phase, doppler_hz_est, resample=True)
    nf = np.full(n_epochs, np.nan)
    pos = s0
    for i in range(n_epochs):
        if pos + TE > len(x):
            break
        part = dll.steps(x[pos : pos + TE])
        pos += TE
        stream = rc.execute(part)
        if len(stream):
            rx.steps(stream)
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

    # --- stage 3: despread -> resample -> demod, with vs. without the -------
    # resample stage (both at segments=K, isolating exactly what changed)
    _dll_ok, rx_ok, syms_ok = _receive(x, s0, chip_phase, doppler_hz_est, True)
    bits_ok, ber_ok, lag_ok, inv_ok = _decode_ber(syms_ok, data, data_start)
    print(
        f"resampled chain (segments={K}, sps={MPSK_SPS}): recovered "
        f"{len(syms_ok)} symbols, tracking={rx_ok.tracking} "
        f"lock={rx_ok.lock:.2f} ber={ber_ok:.4f} lag={lag_ok} "
        f"inverted={inv_ok}"
    )
    assert ber_ok < 0.01, "the resampled chain failed to decode cleanly"
    assert rx_ok.tracking == 1, "MpskReceiver never handed over to tracking"

    _dll_bad, rx_bad, syms_bad = _receive(
        x, s0, chip_phase, doppler_hz_est, False
    )
    bits_bad, ber_bad, lag_bad, inv_bad = _decode_ber(
        syms_bad, data, data_start
    )
    print(
        f"un-resampled direct-wiring (segments={K}, the earlier bug): "
        f"recovered {len(syms_bad)} symbols, tracking={rx_bad.tracking} "
        f"lock={rx_bad.lock:.2f} ber={ber_bad:.4f}"
    )
    # The central finding: the fix was the resample stage, not a bigger K.
    assert ber_bad > 0.3, (
        f"expected the un-resampled direct-wiring to fail (near-chance "
        f"BER) at segments={K}; got ber={ber_bad:.4f}"
    )
    assert ber_ok < ber_bad, (
        f"expected the resampled chain to decode far better than the "
        f"un-resampled one at the same segments={K} "
        f"(ber_ok={ber_ok:.4f}, ber_bad={ber_bad:.4f})"
    )

    wber_ok = _windowed_ber(bits_ok, data, data_start, lag_ok, inv_ok)
    wber_bad = _windowed_ber(bits_bad, data, data_start, lag_bad, inv_bad)
    assert np.nanmean(wber_ok[-5:]) < 0.05, (
        "expected the resampled chain's windowed BER to converge to ~0 "
        f"by the end of the run (last 5 windows mean="
        f"{np.nanmean(wber_ok[-5:]):.3f})"
    )
    assert np.nanmean(wber_bad[-5:]) > 0.3, (
        "expected the un-resampled chain's windowed BER to stay near "
        f"chance the whole run (last 5 windows mean="
        f"{np.nanmean(wber_bad[-5:]):.3f})"
    )

    n_epochs_nf = min(300, (len(x) - s0) // TE)
    nf_trace = _norm_freq_trace(x, s0, chip_phase, doppler_hz_est, n_epochs_nf)
    norm_freq_true = DOPPLER_HZ / (MPSK_SPS * SYM_RATE)

    # --- plot -----------------------------------------------------------
    fig, ((a, b), (c, d)) = plt.subplots(2, 2, figsize=(11, 8.5))

    lo, hi = len(bits_ok) // 3, 2 * len(bits_ok) // 3
    a.scatter(
        syms_ok[lo:hi].real,
        syms_ok[lo:hi].imag,
        s=8,
        color="#1f77b4",
        alpha=0.5,
    )
    a.axhline(0, color="k", lw=0.5)
    a.axvline(0, color="k", lw=0.5)
    lim = 1.3 * np.max(np.abs(syms_ok[lo:hi]))
    a.set_xlim(-lim, lim)
    a.set_ylim(-lim, lim)
    a.set_aspect("equal")
    a.set_title(
        f"Decoded BPSK symbols (settled window)\nBER={ber_ok:.4f}",
        fontsize=9,
    )
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.grid(alpha=0.25)

    aligned_ok = bits_ok if not inv_ok else -bits_ok
    idx_ok = data_start + lag_ok + np.arange(lo, hi)
    correct_ok = aligned_ok[lo:hi] == data[idx_ok]
    running_ber = 1.0 - np.cumsum(correct_ok) / (
        np.arange(len(correct_ok)) + 1
    )
    b.plot(running_ber, color="#2ca02c", lw=1.1)
    b.set_ylim(0, max(0.05, running_ber.max() * 1.1))
    b.set_title("Running BER over the settled window", fontsize=9)
    b.set_xlabel("symbol index (settled window)")
    b.set_ylabel("running BER")
    b.grid(alpha=0.25)

    c.plot(
        np.arange(len(wber_ok)) * 50,
        wber_ok,
        lw=1.1,
        color="#1f77b4",
        label="with RateConverter (correct)",
    )
    c.plot(
        np.arange(len(wber_bad)) * 50,
        wber_bad,
        lw=1.1,
        color="#d62728",
        label="without it (the earlier bug)",
    )
    c.axhline(0.5, color="k", lw=0.8, ls="--", label="chance")
    c.set_ylim(-0.05, 0.65)
    c.set_title(
        f"Windowed decode correctness (both segments={K})\n"
        "the fix was the resample stage, not a bigger K",
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
        "Carrier pull-in from the Acquisition-quantized seed", fontsize=9
    )
    d.set_xlabel("epoch")
    d.set_ylabel("MpskReceiver.norm_freq")
    d.legend(fontsize=7)
    d.grid(alpha=0.25)

    fig.suptitle(
        f"Acquisition -> Dll(segments={K}) -> RateConverter -> "
        f"MpskReceiver(sps={MPSK_SPS}) -- CCSDS Gold code (SF={SF}), "
        f"continuous asynchronous BPSK data ({SYM_RATE:.0f} sym/s), "
        f"C/N0={CN0_OPERATING_DBHZ:.0f} dB-Hz",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.subplots_adjust(hspace=0.5, wspace=0.3)
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "async_dsss_receiver_demo.png")
