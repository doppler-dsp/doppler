"""dsss_receiver_demo.py -- the single-object form of the chain Stage 1-3
validated by hand: ``Acquisition -> Dll(segments) -> RateConverter ->
MpskReceiver``, composed into one C object, :class:`~doppler.dsss.
DsssReceiver`.

Same CCSDS Gold-code, continuous-asynchronous-BPSK-data signal and
operating point (CN0=97 dB-Hz, SEED=6) as
``async_dsss_receiver_demo.py`` (Stage 3) -- this page is not a new
finding, it's the payoff: everything that page hand-composed across
four objects (``_new_acq``/``_new_chain``/``_receive``, the phase-
inversion hand-off, the ``RateConverter`` bridge) is one object and one
``steps()`` call here. Only ``code``/``chip_rate``/``symbol_rate`` are
required; ``segments``/``sps`` default to Stage 2/3's own validated
values (4 and 8) and can be overridden, and ``configure_search_raw``/
``configure_lock_raw``/``configure_chain_raw`` are the escape hatches
for a power user who wants to pin the acquisition grid, code-lock
detector, or despread/resample/demod grid directly -- the same "easy
path derives, raw path pins" shape ``Acquisition``/``Dll`` already use.

Run:  python -m doppler.examples.dsss_receiver_demo  [out.png]
"""

from __future__ import annotations

import sys
import warnings

# --8<-- [start:signal]
import numpy as np

from doppler.wfm import Gold

SF = 1023  # 2**10 - 1: the CCSDS 415.0-G-1 command-link Gold code period
CHIP_RATE = 3.0e6  # Hz
SYM_RATE = 2100.0  # Hz -- chips/symbol = 1428.6, non-integer (asynchronous)
SPC = 2  # samples/chip (front-end oversample)
FS = CHIP_RATE * SPC
TE = SF * SPC  # samples per code epoch
TSYM = FS / SYM_RATE  # samples per symbol ~= 1.4 code epochs

DOPPLER_HZ = 50.0
N_SYM = 3500
PRE_SILENCE = TE * 20 + 737  # deliberately not a whole number of epochs

CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
_CSIGN = np.where(CODE & 1, -1.0, 1.0)


def make_signal(cn0_dbhz: float, seed: int):
    """Identical construction to Stage 1-3's ``make_signal``."""
    rng = np.random.default_rng(seed)
    n = int(N_SYM * TSYM) + 2 * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, N_SYM + 4) * 2 - 1).astype(float)
    si = np.clip(np.floor(idx / TSYM).astype(int), 0, len(data) - 1)
    cph = (idx / SPC).astype(int) % SF
    sig = data[si] * _CSIGN[cph] * np.exp(2j * np.pi * (DOPPLER_HZ / FS) * idx)

    amp_snr = np.sqrt(10.0 ** (cn0_dbhz / 10.0) / FS)
    sigma = 1.0 / amp_snr
    total_n = int(PRE_SILENCE) + n
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(total_n) + 1j * rng.standard_normal(total_n)
    )
    x = np.concatenate([np.zeros(int(PRE_SILENCE)), sig]).astype(
        np.complex64
    ) + noise.astype(np.complex64)
    return x, data


# --8<-- [end:signal]

from doppler.dsss import DsssReceiver  # noqa: E402

SEED = 6
CN0_OPERATING_DBHZ = 97.0


# --8<-- [start:receiver]
def _new_receiver() -> DsssReceiver:
    """The "just works" call: only the signal's own physical parameters
    are required. segments/sps default to Stage 2/3's own validated
    values (4, 8) -- this page passes them explicitly for clarity, not
    because they're required."""
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        return DsssReceiver(
            CODE,
            chip_rate=CHIP_RATE,
            symbol_rate=SYM_RATE,
            cn0_dbhz=55.0,
            doppler_uncertainty=100.0,
            reps=16,
            max_noncoh=8,
            segments=4,
            sps=8,
        )


# --8<-- [end:receiver]


# --8<-- [start:stream]
def _stream(rx: DsssReceiver, x: np.ndarray):
    """Feed ``x`` through ``rx`` one code epoch at a time, collecting every
    emitted symbol and a per-epoch ``norm_freq`` trace."""
    syms_parts = []
    nf_trace = []
    for pos in range(0, len(x) - TE, TE):
        out = rx.steps(x[pos : pos + TE])
        if len(out):
            syms_parts.append(out)
        nf_trace.append(rx.norm_freq)
    syms = np.concatenate(syms_parts)
    return syms, nf_trace


# --8<-- [end:stream]


def main(out_path: str = "dsss_receiver_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x, data = make_signal(CN0_OPERATING_DBHZ, SEED)
    rx = _new_receiver()
    syms, nf_trace = _stream(rx, x)

    print(
        f"tracking={rx.tracking} segments={rx.segments} sps={rx.sps} "
        f"n={rx.n} cn0_dbhz_est={rx.cn0_dbhz_est:.1f} "
        f"(injected {CN0_OPERATING_DBHZ:.1f}) recovered {len(syms)} symbols"
    )
    assert rx.tracking == 1, "DsssReceiver never locked"

    bits = np.where(syms.real > 0, 1.0, -1.0)
    lo, hi = len(bits) // 3, 2 * len(bits) // 3
    best_ber, best_lag, best_inv = 1.0, 0, False
    for lag in range(-100, 101):
        ti = lag + np.arange(lo, hi)
        if ti.min() < 0 or ti.max() >= len(data):
            continue
        truth = data[ti]
        e_same = float(np.mean(bits[lo:hi] != truth))
        e_inv = float(np.mean(bits[lo:hi] != -truth))
        if e_same < best_ber:
            best_ber, best_lag, best_inv = e_same, lag, False
        if e_inv < best_ber:
            best_ber, best_lag, best_inv = e_inv, lag, True
    print(f"decoded BER={best_ber:.4f} at lag={best_lag} inverted={best_inv}")
    assert best_ber < 0.01, "failed to decode cleanly"

    aligned = bits if not best_inv else -bits
    idx = best_lag + np.arange(len(bits))
    valid = (idx >= 0) & (idx < len(data))
    correct = np.full(len(bits), np.nan)
    correct[valid] = (aligned[valid] == data[idx[valid]]).astype(float)
    window = 50
    n_win = len(correct) // window
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", RuntimeWarning)
        wber = np.array(
            [
                1.0 - np.nanmean(correct[i * window : (i + 1) * window])
                for i in range(n_win)
            ]
        )
    assert np.nanmean(wber[-5:]) < 0.05, "expected sustained clean decode"

    # --- plot -----------------------------------------------------------
    fig, ((a, b), (c, d)) = plt.subplots(2, 2, figsize=(11, 8.5))

    a.scatter(
        syms[lo:hi].real, syms[lo:hi].imag, s=8, color="#1f77b4", alpha=0.5
    )
    a.axhline(0, color="k", lw=0.5)
    a.axvline(0, color="k", lw=0.5)
    lim = 1.3 * np.max(np.abs(syms[lo:hi]))
    a.set_xlim(-lim, lim)
    a.set_ylim(-lim, lim)
    a.set_aspect("equal")
    a.set_title(
        f"Decoded BPSK symbols (settled window)\nBER={best_ber:.4f}",
        fontsize=9,
    )
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.grid(alpha=0.25)

    correct_settled = aligned[lo:hi] == data[idx[lo:hi]]
    running_ber = 1.0 - np.cumsum(correct_settled) / (
        np.arange(len(correct_settled)) + 1
    )
    b.plot(running_ber, color="#2ca02c", lw=1.1)
    b.set_ylim(0, max(0.05, running_ber.max() * 1.1))
    b.set_title("Running BER over the settled window", fontsize=9)
    b.set_xlabel("symbol index (settled window)")
    b.set_ylabel("running BER")
    b.grid(alpha=0.25)

    c.plot(np.arange(len(wber)) * window, wber, lw=1.1, color="#1f77b4")
    c.axhline(0.5, color="k", lw=0.8, ls="--", label="chance")
    c.set_ylim(-0.05, 0.65)
    c.set_title(
        "Windowed decode correctness, full run\n(50-symbol windows)",
        fontsize=9,
    )
    c.set_xlabel("symbol index")
    c.set_ylabel("windowed BER")
    c.legend(fontsize=7)
    c.grid(alpha=0.25)

    ep = np.arange(len(nf_trace))
    d.plot(ep, nf_trace, lw=1.1, color="#1f77b4")
    d.set_title("DsssReceiver.norm_freq vs. epoch", fontsize=9)
    d.set_xlabel("epoch")
    d.set_ylabel("norm_freq")
    d.grid(alpha=0.25)

    fig.suptitle(
        f"DsssReceiver -- CCSDS Gold code (SF={SF}), continuous "
        f"asynchronous BPSK data ({SYM_RATE:.0f} sym/s), "
        f"C/N0={CN0_OPERATING_DBHZ:.0f} dB-Hz",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.subplots_adjust(hspace=0.5, wspace=0.3)
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "dsss_receiver_demo.png")
