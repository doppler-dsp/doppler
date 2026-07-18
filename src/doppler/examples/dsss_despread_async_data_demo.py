"""dsss_despread_async_data_demo.py -- Acquisition -> Dll hand-off, against
a continuous Gold code carrying asynchronous BPSK data modulation.

Stage 2 of the same multi-part story as ``dsss_acq_async_data_demo.py``
("Acquisition -> Dll(segments) -> MpskReceiver", told one validated stage
at a time instead of as a single end-of-run BER number). Stage 1 proved
:class:`~doppler.dsss.Acquisition` lands on the exact right code phase and
Doppler bin even under continuous, asynchronous data modulation -- and, as
of the ``symbol_rate=`` sizing fix, can be configured to do so robustly by
default. This page asks the next question: does that hit correctly seed
:class:`~doppler.track.Dll` (the code-tracking despreader), and does
``Dll`` -- a **carrier-blind**, closed-loop early/late discriminator, not
an open-loop search -- have its own version of Stage 1's self-cancellation
failure mode, or is a closed loop structurally different?

Four panels, at a link margin (75 dB-Hz) chosen specifically to make the
``segments=1`` vs ``segments=4`` difference visible (a stronger margin
hides it -- both track perfectly; see "Why segments=1 doesn't mislock the
way Acquisition does" below for why weak-margin closed-loop degradation
looks nothing like Stage 1's discrete mislocks):

1. **Hand-off zoom**: ``Dll.code_phase`` for the first epochs right after
   ``Acquisition``'s hit, against the true instantaneous code phase --
   the seed lands correctly and stays there, not just close.
2. **Code-phase tracking error vs. epoch, full run**, ``segments=4``
   (non-coherent partials, immune to a single data-transition-corrupted
   look) overlaid with ``segments=1`` (a coherent full-epoch dump, the
   structurally vulnerable mode) on the same axes.
3. **Lock statistic vs. epoch, ``segments=4``**, against the always-on
   lock detector's threshold -- resolves a caveat from the older
   ``async_dsss_receiver_demo.py`` full-chain example (``Dll.locked``
   never latching there): that was a large-``segments`` artifact tuned for
   a downstream matched filter this page doesn't need, not a fundamental
   problem -- see ``docs/design/async-symbol-despreader.md``.
4. **Lock statistic vs. epoch, ``segments=1``**, same axes as panel 3, for
   a direct visual contrast.

**Why the loop bandwidth matters more than it looks:** the constructor's
own default ``bn`` (0.01) is tuned for a much lower-rate loop update than
one code period every ``TE`` samples here and goes unstable over a long
run (verified: code phase drifts by hundreds of chips and lock never
latches). Every validated example in this codebase uses ``bn=0.002``
explicitly for this class of geometry -- this page does too, and states it
plainly rather than silently inheriting a default that doesn't fit.

**Why ``segments=1`` doesn't mislock the way Acquisition does:**
``Acquisition``'s search is open-loop -- one dump, one argmax over every
code-phase/Doppler hypothesis, no memory between epochs, so a single
transition-corrupted epoch can hand the peak to a wrong candidate outright,
an instant relocation to a completely unrelated phase (Stage 1's mislock).
``Dll`` is closed-loop: its NCO integrates a loop-filtered error across
many epochs, so a bad epoch perturbs the tracked phase rather than
replacing it outright. But "closed-loop" isn't "immune" -- ``segments=1``'s
coherent full-epoch dump has less per-look SNR margin against a
transition-corrupted look than ``segments=4``'s non-coherent partials, and
over an extended run that shows up as measurably higher tracking-error
variance and periodic, brief unlock (panel 4), where ``segments=4`` stays
locked essentially the whole time (panel 3). The failure MODE is
different from Stage 1's -- gradual drift and intermittent unlock, not an
instant jump to an unrelated wrong phase -- but the vulnerability is real:
run this page's signal far enough (well past the window plotted here) and
``segments=1`` eventually loses lock outright and drifts without bound,
while ``segments=4`` degrades far more gracefully at the same point.

Carrier and symbol-timing recovery (``MpskReceiver``) are Stage 3, see
``async_dsss_receiver_demo.py`` -- this page is deliberately
despread-only.

Run:  python -m doppler.examples.dsss_despread_async_data_demo  [out.png]
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

DOPPLER_HZ = 50.0  # residual carrier -- never removed; Dll is carrier-blind
N_SYM = 6000
PRE_SILENCE = TE * 20 + 737  # deliberately not a whole number of epochs

# CCSDS defaults: known 3-valued correlation sidelobes {-1, -65, 63}, a
# real, cross-checked reference code instead of an arbitrary MLS choice.
CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
_CSIGN = np.where(CODE & 1, -1.0, 1.0)


def make_signal(cn0_dbhz: float, seed: int):
    """A continuous, asynchronous-symbol-clock DSSS capture at real units.

    Identical construction to Stage 1's ``make_signal`` -- silence, then
    the code-spread BPSK stream (independent symbol clock, residual
    carrier), with AWGN injected via the same C/N0 -> per-sample
    amplitude-SNR relationship ``Acquisition`` itself uses for sizing, so
    ``cn0_dbhz`` is directly the injected C/N0 in dB-Hz. The residual
    carrier is never derotated anywhere downstream on this page (unlike
    Stage 1's own chip-zoom panel) -- proving ``Dll`` tracks correctly
    with zero upstream carrier correction is the point.

    Returns
    -------
    x : NDArray[np.complex64]
        The full capture (silence + signal + noise).
    data : NDArray[np.float64]
        The transmitted BPSK symbols (+/-1), ground truth for validation.
    si, cph : NDArray[np.intp]
        Per-sample symbol index and chip-phase index used to build ``x``,
        relative to the start of ``sig`` (i.e. *excluding* the prepended
        silence) -- ground truth for the tracking-error checks.
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
    return x, data, si, cph


# --8<-- [end:signal]

from doppler.detection import det_threshold_noncoherent  # noqa: E402
from doppler.dsss.handoff import dll_init_chip_from_acq  # noqa: E402
from doppler.track import Dll  # noqa: E402

SEED = 6
# A weaker margin than Stage 1's deliberately-strong 97 dB-Hz -- chosen
# empirically (see the module docstring) so segments=1 vs segments=4
# actually differ. Above ~80 dB-Hz both track essentially perfectly;
# below ~60 dB-Hz both struggle and the comparison gets noisy.
DLL_CN0_DBHZ = 75.0
N_EPOCHS_TRACK = 2000
BN = 0.002  # every validated Dll example in this codebase uses this, not
# the constructor's own default (0.01, unstable at this geometry -- see
# the module docstring).
PFA_LOCK = 1e-3
N_LOOKS_LOCK = 20  # Dll's own constructor default lock-detector config


# --8<-- [start:acq_symbol_rate]
def _new_acq() -> Acquisition:
    """The first gallery use of ``symbol_rate=`` against a real continuous
    signal -- mirrors the guide's "robust default" recommendation."""
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        acq = Acquisition(
            CODE,
            spc=SPC,
            chip_rate=CHIP_RATE,
            cn0_dbhz=55.0,
            doppler_uncertainty=100.0,
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
def _new_dll(chip_phase: float, segments: int) -> Dll:
    """Seed a Dll from an Acquisition hit's converted chip_phase (see
    dll_init_chip_from_acq) and tune its always-on lock detector.

    bn=0.002, not the constructor's own default (0.01) -- see the module
    docstring's "A gotcha: the loop bandwidth" section.
    """
    dll = Dll(CODE, SPC, chip_phase, BN, 0.707, 0.5, segments=segments)
    dll.configure_lock(PFA_LOCK, N_LOOKS_LOCK)
    return dll


# --8<-- [end:handoff]


def _track(x, n_samples, s0, chip_phase, segments: int, n_epochs: int):
    """Stream ``n_epochs`` epochs of ``x`` (starting at sample ``s0``)
    through ``dll.steps()``, one epoch per call, recording the tracked
    code phase / lock statistic / lock decision and the circular tracking
    error against the true instantaneous phase after each call.

    True phase is computed continuously (``ridx / SPC``), not by indexing
    ``make_signal``'s ``cph`` array -- that array is intentionally floored
    to a whole-chip grid (it indexes the discrete code lookup table for
    signal synthesis), so comparing a continuously-tracked NCO phase
    against it would show a spurious ~0.5-chip bias with spc=2, not a real
    tracking error.

    A ``dll.steps()`` call operates on one code epoch's worth of raw
    samples (``TE``) regardless of ``segments`` -- ``segments`` controls
    how that epoch is internally split into non-coherently combined
    partials, not the driving cadence here, so the two configurations'
    traces line up epoch-for-epoch.
    """
    dll = _new_dll(chip_phase, segments)
    err = np.full(n_epochs, np.nan)
    lock_stat = np.full(n_epochs, np.nan)
    locked = np.zeros(n_epochs, dtype=bool)
    pos = s0
    for i in range(n_epochs):
        if pos + TE > n_samples:
            break
        dll.steps(x[pos : pos + TE])
        pos += TE
        # dll.code_phase reflects the phase at the next unconsumed sample.
        ridx = pos - int(PRE_SILENCE)
        true_phase = (ridx / SPC) % SF
        err[i] = ((dll.code_phase - true_phase + SF / 2) % SF) - SF / 2
        lock_stat[i] = dll.lock_stat
        locked[i] = dll.locked
    return err, lock_stat, locked


def main(out_path: str = "dsss_despread_async_data_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x, _data, _si, _cph = make_signal(DLL_CN0_DBHZ, SEED)

    # --- stage 1: acquisition ------------------------------------------
    hit, hitpos, acq = _acquire(x)
    assert hit is not None, "acquisition failed to find the continuous code"
    dop_bin, code_phase, _pk, _n, test_stat, cn0_dbhz_est, *_rest = hit
    print(
        f"acquired: hitpos={hitpos} doppler_bin={dop_bin} "
        f"code_phase={code_phase} test_stat={test_stat:.1f} "
        f"cn0_dbhz_est={cn0_dbhz_est:.1f} (injected {DLL_CN0_DBHZ:.1f})"
    )

    # --- stage 2: hand-off ----------------------------------------------
    chip_phase = dll_init_chip_from_acq(code_phase, SPC, SF)
    s0 = hitpos + acq.code_bins * acq.doppler_bins
    ridx0 = s0 - int(PRE_SILENCE)
    true_phase0 = (ridx0 / SPC) % SF  # continuous -- see _track's docstring
    handoff_err = ((chip_phase - true_phase0 + SF / 2) % SF) - SF / 2
    print(
        f"hand-off: chip_phase={chip_phase:.3f} true={true_phase0:.3f} "
        f"error={handoff_err:.3f} chips"
    )
    # A wrong sign inversion in dll_init_chip_from_acq would miss by
    # ~SF/2, not a fraction of a chip.
    assert abs(handoff_err) < 0.1, (
        f"hand-off phase disagrees with the true code phase by "
        f"{handoff_err:.2f} chips -- should be exact"
    )

    n_epochs = min(N_EPOCHS_TRACK, (len(x) - s0) // TE)
    err4, lock_stat4, locked4 = _track(
        x, len(x), s0, chip_phase, segments=4, n_epochs=n_epochs
    )
    err1, lock_stat1, locked1 = _track(
        x, len(x), s0, chip_phase, segments=1, n_epochs=n_epochs
    )
    threshold = det_threshold_noncoherent(PFA_LOCK, N_LOOKS_LOCK)

    # Steady-state window: the last 500 epochs. Not "back half" -- the
    # divergence this page investigates builds up slowly (verified: it
    # takes >1000 epochs to show up at all at this margin), so a window
    # anchored to n_epochs//2 would silently miss it for a shorter run.
    back = slice(max(0, n_epochs - 500), n_epochs)
    rms4 = float(np.sqrt(np.nanmean(err4[back] ** 2)))
    rms1 = float(np.sqrt(np.nanmean(err1[back] ** 2)))
    locked_frac4 = float(np.mean(locked4[back]))
    locked_frac1 = float(np.mean(locked1[back]))
    print(
        f"steady-state (last {back.stop - back.start} epochs): "
        f"segments=4 rms_err={rms4:.3f} chips locked={locked_frac4:.3f} | "
        f"segments=1 rms_err={rms1:.3f} chips locked={locked_frac1:.3f}"
    )

    assert rms4 < 1.0, (
        f"expected segments=4 steady-state tracking error to be sub-chip "
        f"(rms={rms4:.3f})"
    )
    assert rms1 > rms4, (
        "expected segments=1 (coherent full-epoch, vulnerable to a "
        f"transition-corrupted look) to track worse than segments=4 "
        f"(rms1={rms1:.3f}, rms4={rms4:.3f})"
    )
    assert locked_frac4 > 0.95, (
        "expected segments=4 to stay locked essentially throughout -- "
        f"resolving the older full-chain demo's caveat (locked_frac="
        f"{locked_frac4:.3f})"
    )
    assert locked_frac1 < locked_frac4, (
        "expected segments=1 to lock less reliably than segments=4 over "
        f"the same run (locked1={locked_frac1:.3f}, "
        f"locked4={locked_frac4:.3f})"
    )
    # The closed-loop contrast with Stage 1's open-loop mislocks: within
    # this page's run length, segments=1's error grows but stays within a
    # few dozen chips -- gradual drift, not an instant hundreds-of-chips
    # relocation to an unrelated wrong phase the way Acquisition's argmax
    # search can jump in a single epoch (Stage 1). A longer run than
    # plotted here does eventually see segments=1 lose lock outright and
    # drift without bound -- see the module docstring.
    assert np.nanmax(np.abs(err1)) < 50.0, (
        "expected segments=1's worst-case error within this run to stay "
        f"well short of Stage-1-scale mislocks; got "
        f"{np.nanmax(np.abs(err1)):.1f} chips"
    )

    # --- plot -------------------------------------------------------------
    fig, ((a, b), (c, d)) = plt.subplots(2, 2, figsize=(11, 8.5))

    n_zoom = 30
    ep_zoom = np.arange(n_zoom)
    true_zoom = np.array(
        [((ridx0 + (i + 1) * TE) / SPC) % SF for i in range(n_zoom)]
    )
    a.plot(
        ep_zoom,
        true_zoom,
        label="true code phase",
        lw=1.4,
        ls="--",
        color="k",
    )
    a.plot(
        ep_zoom,
        true_zoom + err4[:n_zoom],
        label="Dll.code_phase (segments=4)",
        lw=1.2,
        color="#1f77b4",
        marker=".",
        ms=4,
    )
    a.set_title("Hand-off zoom\n(seed lands correctly, stays there)")
    a.set_xlabel("epoch since hand-off")
    a.set_ylabel("code phase (chips)")
    a.legend(fontsize=7)

    ep = np.arange(n_epochs)
    b.plot(ep, err4, lw=0.8, color="#1f77b4", label="segments=4", alpha=0.85)
    b.plot(ep, err1, lw=0.8, color="#d62728", label="segments=1", alpha=0.7)
    b.axhline(0, color="k", lw=0.6, ls="--")
    b.set_title(
        f"Code-phase tracking error vs. epoch\n(steady-state rms: "
        f"seg4={rms4:.2f}, seg1={rms1:.2f} chips)",
        fontsize=9,
    )
    b.set_xlabel("epoch")
    b.set_ylabel("error (chips)")
    b.legend(fontsize=7)

    for ax, ls, lockst, label in (
        (c, locked4, lock_stat4, "segments=4"),
        (d, locked1, lock_stat1, "segments=1"),
    ):
        ax.plot(ep, lockst, lw=0.9, color="#2ca02c")
        ax.axhline(
            threshold, color="#d62728", lw=1.1, ls="--", label="threshold"
        )
        unlocked = ~ls
        if np.any(unlocked):
            ax.fill_between(
                ep,
                0,
                1,
                where=unlocked,
                transform=ax.get_xaxis_transform(),
                color="grey",
                alpha=0.15,
                label="unlocked",
            )
        ax.set_title(
            f"Lock statistic vs. epoch ({label})\n"
            f"locked {np.mean(ls):.1%} of the run",
            fontsize=9,
        )
        ax.set_xlabel("epoch")
        ax.set_ylabel("lock statistic R")
        ax.legend(fontsize=7)

    fig.suptitle(
        f"Acquisition -> Dll hand-off -- CCSDS Gold code (SF={SF}), "
        f"continuous asynchronous BPSK data ({SYM_RATE:.0f} sym/s), "
        f"C/N0={DLL_CN0_DBHZ:.0f} dB-Hz",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.subplots_adjust(hspace=0.5, wspace=0.3)
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(
        sys.argv[1]
        if len(sys.argv) > 1
        else "dsss_despread_async_data_demo.png"
    )
