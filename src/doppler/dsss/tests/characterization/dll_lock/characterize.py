"""Characterization of the `Dll` code-lock detector at the continuous
async-DSSS operating point: default per-partial looks, sized per-partial
looks, and the symbol-period-aided looks, on one signal, with telemetry.

This is the run behind `docs/design/async-dsss-receiver.md` §12.4: the
DLL the packaged `AsyncDsssReceiver` builds (`bn 0.002, segments 4`), fed
the shipped synth's continuous DSSS (Gold-1023 at 5 Mcps, 2 samples per
chip, 2700 sym/s asynchronous BPSK) with a `Telemetry` context attached,
so ``code.lock`` (the statistic), ``code.locked``, ``code.e`` and
``code.rate`` are read back on one timeline and plotted. It answers two
questions the release harness (`native/validation/
async_dsss_receiver_release.c`) raised and could not: whether the code
loop is disturbed when its flag chatters (it is not — the tracked rate
stays at 1.000000 within 3 ppm in every run), and what the flag needs
(looks sized for the C/N0, and coherent over a symbol once the period is
known — `Dll.set_symbol_period`).

This is a **characterization**, not an example: 2 s of signal per run,
six runs, and it is run deliberately by `make characterize`. See
`doppler.dsss.tests.characterization` for what the per-push fast twin
(`test_dll_lock_twin.py`) does and does not cover.

Run:  python -m doppler.dsss.tests.characterization.dll_lock.characterize
"""

from __future__ import annotations

from pathlib import Path

import numpy as np

from doppler.detection import det_n_noncoh, det_threshold_noncoherent
from doppler.telemetry import Telemetry
from doppler.track import Dll
from doppler.wfm import Gold, Synth

HERE = Path(__file__).resolve().parent

SF, SPC, CHIP_RATE, SYM_RATE, K = 1023, 2, 5e6, 2700.0, 4
FS = CHIP_RATE * SPC
TE = SF * SPC
PARTIALS_PER_SYMBOL = K * CHIP_RATE / (SF * SYM_RATE)  # 7.24
CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
LOCK_PFA = 1e-3
LOCK_PD = 0.99


def make_signal(cn0_dbhz: float, seed: int, seconds: float) -> np.ndarray:
    """The operating-point waveform from the shipped synth, noise included.

    The synth takes an Es/N0 referred to the data symbol; a C/N0 converts
    to it as ``cn0 - 10 log10(symbol_rate)``, the same relation the
    receiver examples use.
    """
    esno = cn0_dbhz - 10 * np.log10(SYM_RATE)
    syn = Synth(
        type="dsss",
        data_code=bytes(CODE.tolist()),
        symbol_rate=SYM_RATE,
        sps=SPC,
        snr=esno,
        snr_mode="esno",
        fs=FS,
        seed=seed,
    )
    return syn.steps(int(seconds * FS)).astype(np.complex64)


def sized_looks(cn0_dbhz: float, window_partials: int) -> int:
    """``det_n_noncoh`` over a look of ``window_partials`` partials."""
    amp = np.sqrt(10 ** (cn0_dbhz / 10) / FS)  # per-sample amplitude SNR
    return int(
        det_n_noncoh(amp, window_partials * (TE // K), LOCK_PD, LOCK_PFA, 4000)
    )


def run_trial(
    x: np.ndarray,
    cn0_dbhz: float,
    *,
    aided: bool,
    size_looks: bool,
    block: int = 4 * TE,
) -> dict:
    """One DLL over ``x`` with telemetry; the flag's statistics and traces.

    ``aided`` turns on ``set_symbol_period``; ``size_looks`` sizes
    ``n_looks`` with ``det_n_noncoh`` for the look in force (a partial, or
    the aided window). Neither → the DLL's default 20-partial detector.
    """
    tlm = Telemetry(1 << 20)
    d = Dll(CODE, SPC, 0.0, 0.002, 0.707, 0.5, segments=K)
    if aided:
        d.set_symbol_period(PARTIALS_PER_SYMBOL)
    window = d.symbol_window or 1
    n_looks = sized_looks(cn0_dbhz, window) if size_looks else 20
    if size_looks:
        d.configure_lock(LOCK_PFA, n_looks)
    d.set_telemetry(tlm, "code")
    for i in range(0, x.size, block):
        tlm.set_now(i)
        d.steps(x[i : i + block])
    recs = tlm.read()
    assert tlm.dropped == 0
    traces = {}
    for p in ("lock", "locked", "e", "rate"):
        r = recs[recs["probe"] == tlm.probe_id(f"code.{p}")]
        traces[p] = (r["n"] / FS, r["value"].astype(float))
    t, locked = traces["locked"]
    settle = len(locked) // 4  # let the loop and the reference settle
    eta = det_threshold_noncoherent(LOCK_PFA, n_looks)
    stat = traces["lock"][1][settle:]
    return {
        "n_looks": n_looks,
        "window": window,
        "eta": eta,
        "off_fraction": float(1.0 - locked[settle:].mean()),
        "drops_per_s": float(np.sum(np.diff(locked[settle:]) < 0))
        / float(t[-1] - t[settle] + 1e-9),
        "miss_per_decision": float(np.mean(stat < eta)),
        "code_rate": float(d.code_rate),
        "locked": bool(d.locked),
        "traces": traces,
    }


DETECTORS = (
    ("20 partials (default)", False, False),
    ("partials, sized", False, True),
    ("symbol-aided, sized", True, True),
)


def main(seconds: float = 2.0, out_path: Path | None = None) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    out_path = HERE / "dll_lock.png" if out_path is None else out_path
    cn0s = (45.0, 40.0)
    fig, axes = plt.subplots(4, len(cn0s), figsize=(13, 10), sharex="col")
    for col, cn0 in enumerate(cn0s):
        x = make_signal(cn0, 3, seconds)
        results = []
        for label, aided, size in DETECTORS:
            r = run_trial(x, cn0, aided=aided, size_looks=size)
            results.append((label, r))
            print(
                f"cn0={cn0:.0f}  {label:24s} N={r['n_looks']:4d}  "
                f"miss/decision={r['miss_per_decision']:.3f}  "
                f"off={100 * r['off_fraction']:5.1f}%  "
                f"drops/s={r['drops_per_s']:5.2f}  "
                f"code_rate={r['code_rate']:.6f}"
            )
        ax = axes[0, col]
        for (label, r), c in zip(results, ("C0", "C1", "C2")):
            ax.plot(
                *r["traces"]["lock"],
                lw=0.6,
                color=c,
                label=f"{label}, N={r['n_looks']}",
            )
            ax.axhline(r["eta"], color=c, ls="--", lw=0.8)
        ax.set_ylabel("code.lock R (dashed: threshold)")
        ax.legend(fontsize=7, loc="upper right")
        ax.set_title(
            f"C/N0 {cn0:.0f} dB-Hz "
            f"(Es/N0 {cn0 - 10 * np.log10(SYM_RATE):.1f} dB)"
        )
        ax = axes[1, col]
        colours = ("C0", "C1", "C2")
        for i, ((_label, r), c) in enumerate(zip(results, colours)):
            t, v = r["traces"]["locked"]
            ax.step(t, v + 1.15 * i, where="post", color=c, lw=0.8)
        ax.set_ylabel("code.locked (stacked)")
        ax.set_yticks([])
        r0 = results[0][1]
        axes[2, col].plot(*r0["traces"]["e"], lw=0.4, color="C2")
        axes[2, col].set_ylabel("code.e (E−L discriminator)")
        axes[3, col].plot(*r0["traces"]["rate"], lw=0.6, color="C3")
        axes[3, col].set_ylabel("code.rate")
        axes[3, col].set_xlabel("time, s")
        for a in axes[:, col]:
            a.grid(alpha=0.25)
    fig.suptitle(
        "The receiver's DLL alone: Gold-1023 at 5 Mcps, spc 2, "
        "segments 4, bn 0.002",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main()
