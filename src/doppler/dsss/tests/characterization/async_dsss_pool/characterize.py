"""Characterization: what a receiver costs, searching and tracking, and
what the pool as built costs: the runs behind
`docs/design/async-dsss-parallel.md`.

Each section below prints one entry of that page's record,
`docs/design/async-dsss-parallel-measurements.md`, under the same number, so
a number on either page names the function that made it.

- **3.1** how symbol lock rides out a dropout, and how soon it falls when a
  signal leaves for good: the hysteresis the re-acquire rule rests on.
- **5.1** how long one receiver takes from a signal's arrival to tracking,
  by Es/N0: the make-before-break budget.
- **6.1** what one tracking receiver costs per push, data on and code only.
- **6.2** what one receiver costs while it searches, at one tuned offset and
  over the whole carrier range (+/-1/T_E), by coherent depth: does it fit
  one CPU?
- **7.1** the pool as built against one receiver and against its own
  searcher alone, on one capture: where its cost goes.
- **7.2** that searcher at the design point it was sized for (+/-50 kHz,
  the Doppler-rate depth), across thread counts.

Everything is wall-clock time per push, one push per code epoch (2,046
samples, 333.3 us at 6.138 MSa/s), taken around the Python call on whatever
machine runs it, and expressed as a share of one CPU: the push time over
the 333.3 us of signal it covers. A rerun moves the timings, which is why
the record stamps each entry with date, machine and commit; what a rerun
must not move is which state costs what, and what fits one CPU.

The signal is the shipped `Synth`'s continuous DSSS: Gold-1023 at 3.069
Mcps (so 1/T_E is 3 kHz), 2 samples a chip, 2,700 sym/s asynchronous
BPSK, 1,500 Hz, with C/N0 set from Es/N0 through `snr_mode="fs"`. The
search-only rows use noise, because what they time is the search, not a
detection.

This is a characterization, not an example: it is run deliberately, and
its wall time is printed at the end. Its fast twin
(`test_async_dsss_pool_cost.py`) runs every section on a few pushes.

Run:  make characterize CHARACTERIZATIONS=<this file>
"""

from __future__ import annotations

import sys
import time
from typing import TYPE_CHECKING, Any

import numpy as np

from doppler.acquire import Acquisition
from doppler.dsss import AsyncDsssPool, AsyncDsssReceiver
from doppler.wfm import Gold, Synth, wfm_awgn_amplitude

if TYPE_CHECKING:
    from collections.abc import Callable

CHIP_RATE, SPC, SYM_RATE = 3.069e6, 2, 2700.0
FS = CHIP_RATE * SPC
GOLD = np.asarray(Gold().generate(1023), np.uint8)
TE = len(GOLD) * SPC  # one code epoch, the push: 2,046 samples
EPOCH_US = TE / FS * 1e6  # 333.3 us of signal a push
SPAN = FS / TE  # 1/T_E: 3,000 Hz

#: One strong emitter, searched over the page's carrier bound (+/-1/T_E),
#: with a 31-epoch data-free window: the pool-as-built comparison's run.
CN0, DOPPLER, PFA, DU, CODE_ONLY = 47.0, 1500.0, 1e-3, SPAN, 31
#: The design point of the pool as built (async-dsss-receiver.md 6.1):
#: +/-50 kHz, SPEC's 450 code-only symbols (500 epochs at 3.069 Mcps), and
#: the 500 Hz/s Doppler rate that bounds the coherent depth (its 2.3).
DESIGN_DU, DESIGN_CODE_ONLY, DESIGN_RATE = 50000.0, 500, 500.0


#: Dropouts a tracked signal is given, ms; None is a loss that never ends.
FADES_MS: tuple[float | None, ...] = (
    5.0,
    10.0,
    20.0,
    30.0,
    50.0,
    100.0,
    200.0,
    None,
)


def cn0_of(esn0_db: float) -> float:
    """C/N0, dB-Hz, of an Es/N0 at the symbol rate."""
    return esn0_db + 10.0 * np.log10(SYM_RATE)


def capture(
    seconds: float = 1.0,
    cn0_dbhz: float = CN0,
    code_only: bool = True,
    seed: int = 7,
) -> np.ndarray:
    """One signal, a whole number of epochs: code only, or data on."""
    n = int(seconds * FS) // TE * TE
    syn = Synth(
        type="dsss",
        fs=FS,
        sps=SPC,
        data_code=GOLD.tobytes(),
        symbol_rate=SYM_RATE,
        dsss_code_only=int(code_only),
        freq=DOPPLER,
        snr=cn0_dbhz - 10.0 * np.log10(FS),
        snr_mode="fs",
        seed=seed,
    )
    return np.asarray(syn.steps(n), np.complex64)


def noise(epochs: int = 64, seed: int = 3) -> np.ndarray:
    """Unit-power complex noise, `epochs` code epochs, cycled by the caller."""
    rng = np.random.default_rng(seed)
    z = rng.standard_normal((2, TE * epochs))
    return ((z[0] + 1j * z[1]) / np.sqrt(2)).astype(np.complex64)


def make_receiver(cn0_dbhz: float = CN0, du: float = DU) -> Any:
    """The searching receiver, its own search over `du`."""
    return AsyncDsssReceiver(
        GOLD,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        spc=SPC,
        cn0_dbhz=cn0_dbhz,
        pfa=PFA,
        doppler_uncertainty=du,
    )


def make_pool(n_slots: int = 4, threads: int = 1) -> Any:
    """The pool as built, at the one-emitter run's settings."""
    return AsyncDsssPool(
        GOLD,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        spc=SPC,
        cn0_dbhz=CN0,
        pfa=PFA,
        doppler_uncertainty=DU,
        code_only_epochs=CODE_ONLY,
        n_slots=n_slots,
        threads=threads,
    )


def make_searcher(
    du: float = DU,
    code_only: int = CODE_ONLY,
    rate: float = 0.0,
    threads: int = 1,
) -> Any:
    """A continuous Acquisition built as the pool builds its searcher."""
    a = Acquisition(
        GOLD,
        spc=SPC,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        cn0_dbhz=CN0,
        doppler_uncertainty=du,
        pfa=PFA,
        pd=0.9,
        noise_mode="mean",
        code_only_epochs=code_only,
        doppler_rate=rate,
    )
    a.set_max_peaks(16)
    a.set_threads(threads)
    return a


def per_push(
    pushers: list[Callable[[np.ndarray], object]],
    x: np.ndarray,
    pushes: int,
) -> np.ndarray:
    """Time each pusher on the same block in turn, `pushes` blocks, in us.

    Interleaved, so whatever else the machine is doing lands on every
    object alike and the ratios hold. `x` is cycled when it is shorter.
    """
    t = np.empty((len(pushers), pushes))
    blocks = x.size // TE
    for i in range(pushes):
        blk = x[(i % blocks) * TE : (i % blocks + 1) * TE]
        for j, push in enumerate(pushers):
            t0 = time.perf_counter_ns()
            push(blk)
            t[j, i] = time.perf_counter_ns() - t0
    return t / 1e3


def split(t: np.ndarray, d: int) -> tuple[float, float, float]:
    """(ordinary median, dwell mean, mean) of one object's push times.

    A dwell decides once every `d` pushes, so the slowest 1-in-`d` are the
    dwell pushes; everything else is ordinary.
    """
    k = max(t.size // max(d, 1), 1)
    dwell = np.argsort(t)[-k:]
    other = np.setdiff1d(np.arange(t.size), dwell)
    return float(np.median(t[other])), float(t[dwell].mean()), float(t.mean())


def cpu(mean_us: float) -> float:
    """Mean push time over the signal time it covers: share of one CPU."""
    return mean_us / EPOCH_US


def receiver_run(x: np.ndarray, cn0_dbhz: float) -> tuple[np.ndarray, ...]:
    """One receiver over `x`: each push's state when it began, and its time."""
    rx = make_receiver(cn0_dbhz)
    state, times = [], []
    for i in range(x.size // TE):
        state.append(
            "tracking"
            if rx.tracking
            else "refining"
            if rx.refining
            else "searching"
        )
        t0 = time.perf_counter_ns()
        rx.steps(x[i * TE : (i + 1) * TE])
        times.append((time.perf_counter_ns() - t0) / 1e3)
    return np.asarray(state), np.asarray(times)


def faded(
    esn0_db: float,
    fade_ms: float | None,
    settle_s: float = 0.6,
    after_s: float = 0.5,
    seed: int = 7,
) -> tuple[np.ndarray, int, int | None]:
    """A data-on signal whose SIGNAL is cut for `fade_ms`, noise running on.

    Returns the capture and the epochs the cut starts and ends at (`None`
    for a cut that never ends). The noise is the library's own relation
    (`wfm_awgn_amplitude`, per-component, over a unit-noise `Synth`), so
    the signal can be switched off without touching the noise floor.
    """
    epoch_s = TE / FS
    a = round(settle_s / epoch_s)
    b = None if fade_ms is None else a + round(fade_ms * 1e-3 / epoch_s)
    total = a + round(after_s / epoch_s) + (0 if b is None else b - a)
    n = total * TE
    sig = np.asarray(
        Synth(
            type="dsss",
            fs=FS,
            sps=SPC,
            data_code=GOLD.tobytes(),
            symbol_rate=SYM_RATE,
            freq=DOPPLER,
            seed=seed,
        ).steps(n),
        np.complex64,
    )
    sig[a * TE : n if b is None else b * TE] = 0.0
    amp = float(
        wfm_awgn_amplitude(esn0_db - 10.0 * np.log10(FS / SYM_RATE), 1.0)
    )
    unit = np.asarray(Synth(type="noise", fs=FS, seed=seed + 1).steps(n))
    return (sig + amp * np.sqrt(2.0) * unit).astype(np.complex64), a, b


def s3_1_lock_hysteresis(
    esn0s: tuple[float, ...] = (10.0, 5.0),
    fades: tuple[float | None, ...] = FADES_MS,
    settle_s: float = 0.6,
    after_s: float = 0.5,
) -> list[dict]:
    """3.1: symbol lock through a dropout of each length, per Es/N0.

    Per epoch, the receiver's `locked` (symbol lock). A dropout it rides
    out leaves `locked` up throughout; otherwise the row gives when it fell
    after the cut began, and when it came back after the signal did.
    """
    epoch_ms = TE / FS * 1e3
    rows = []
    for esn0 in esn0s:
        for fade in fades:
            x, a, b = faded(esn0, fade, settle_s, after_s)
            rx = make_receiver(cn0_of(esn0), du=SPAN)
            locked = np.empty(x.size // TE, dtype=bool)
            for i in range(locked.size):
                rx.steps(x[i * TE : (i + 1) * TE])
                locked[i] = bool(rx.locked)
            down = np.flatnonzero(~locked[a:])
            drop = None if down.size == 0 else a + int(down[0])
            relock = None
            if drop is not None and b is not None:
                up = np.flatnonzero(locked[max(drop, b) :])
                relock = None if up.size == 0 else max(drop, b) + int(up[0])
            rows.append(
                {
                    "esn0_db": esn0,
                    "fade_ms": "never ends" if fade is None else fade,
                    "locked_before": bool(locked[a - 1]),
                    "held": drop is None,
                    "drop_ms": float("nan")
                    if drop is None
                    else (drop - a) * epoch_ms,
                    "relock_ms": float("nan")
                    if relock is None
                    else (relock - b) * epoch_ms,
                }
            )
    return rows


def s5_1_acquisition(
    esn0s: tuple[float, ...] = (10.0, 5.0), seconds: float = 1.0
) -> list[dict]:
    """5.1: arrival to tracking, data on, the receiver's own search
    over +/-1/T_E."""
    rows = []
    for esn0 in esn0s:
        s, _ = receiver_run(
            capture(seconds, cn0_of(esn0), False), cn0_of(esn0)
        )
        search = int((s == "searching").sum())
        refine = int((s == "refining").sum())
        rows.append(
            {
                "esn0_db": esn0,
                "searching": search,
                "refining": refine,
                "to_tracking_ms": (search + refine) * EPOCH_US / 1e3,
                "tracked": bool((s == "tracking").any()),
            }
        )
    return rows


def s6_1_tracking(
    esn0s: tuple[float, ...] = (10.0, 5.0), seconds: float = 2.0
) -> list[dict]:
    """6.1: one tracking receiver per push, data on and code only."""
    rows = []
    for esn0, code_only in [(e, False) for e in esn0s] + [(esn0s[0], True)]:
        s, t = receiver_run(
            capture(seconds, cn0_of(esn0), code_only), cn0_of(esn0)
        )
        trk = t[s == "tracking"]
        rows.append(
            {
                "esn0_db": esn0,
                "signal": "code only" if code_only else "data on",
                "pushes": int(trk.size),
                "median_us": float(np.median(trk)),
                "cpu": cpu(float(trk.mean())),
            }
        )
    return rows


def s6_2_search_depth(
    blocks: int = 4,
    code_only_deep: int = DESIGN_CODE_ONLY,
    min_pushes: int = 1024,
) -> list[dict]:
    """6.2: one receiver searching, on noise, by range and by depth.

    Two ranges: one tile (+/- just under 1/(2 T_E), the offset the
    controller tunes a receiver to) and the whole carrier range (+/-1/T_E).
    The receiver's own search is one-epoch looks; the slow-time rows are
    the same continuous engine with a coherent block of D epochs, as a
    receiver would carry it on command. The first block of each run is
    warm-up and is dropped; at least `min_pushes` pushes and `blocks`
    dwells are timed after it.
    """
    rows = []
    for rng_name, du in (("one tile", 0.49 * SPAN), ("+/-1/T_E", SPAN)):
        rx = make_receiver(du=du)
        t = per_push([rx.steps], noise(), 16 + max(64, min_pushes))[0][16:]
        rows.append(
            {
                "range": rng_name,
                "search": "the receiver's own",
                "depth": 1,
                "dwell_ms": float("nan"),
                "cpu": cpu(float(t.mean())),
                "still_searching": not (rx.tracking or rx.refining),
            }
        )
        for label, code_only, rate in (
            ("slow-time, rate-free", CODE_ONLY, 0.0),
            ("slow-time, rate-bound", code_only_deep, DESIGN_RATE),
        ):
            a = make_searcher(du=du, code_only=code_only, rate=rate)
            d = a.coherent_bins
            n = d + max(blocks * d, min_pushes)
            t = per_push([a.push], noise(), n)[0][d:]
            _, w, m = split(t, d)
            rows.append(
                {
                    "range": rng_name,
                    "search": label,
                    "depth": d,
                    "dwell_ms": w / 1e3,
                    "cpu": cpu(m),
                    "still_searching": True,
                }
            )
    return rows


def s7_1_split(x: np.ndarray) -> list[dict]:
    """7.1: receiver, pool and the pool's searcher alone, interleaved."""
    rx, pool, acq = make_receiver(), make_pool(), make_searcher()
    t = per_push([rx.steps, pool.push, acq.push], x, x.size // TE)
    d = acq.coherent_bins
    rows = []
    for name, row in zip(("receiver", "pool", "searcher alone"), t):
        o, w, m = split(row, d)
        rows.append(
            {"object": name, "ordinary_us": o, "dwell_us": w, "cpu": cpu(m)}
        )
    return rows


def s7_2_design_point(
    blocks: int = 4,
    counts: tuple[int, ...] = (1, 4, 8),
    code_only: int = DESIGN_CODE_ONLY,
) -> list[dict]:
    """7.2: the pool's searcher at +/-50 kHz and the rate-bound depth."""
    rows = []
    for c in counts:
        a = make_searcher(
            du=DESIGN_DU, code_only=code_only, rate=DESIGN_RATE, threads=c
        )
        d = a.coherent_bins
        t = per_push([a.push], noise(), (blocks + 1) * d)[0][d:]
        o, w, m = split(t, d)
        rows.append(
            {
                "threads": c,
                "depth": d,
                "ordinary_us": o,
                "dwell_ms": w / 1e3,
                "cpu": cpu(m),
            }
        )
    return rows


def _table(title: str, rows: list[dict]) -> None:
    print(f"\n{title}")
    keys = list(rows[0])
    print("  " + "  ".join(f"{k:>16}" for k in keys))
    for r in rows:
        cells = (
            f"{v:16.3f}" if isinstance(v, float) else f"{v!s:>16}"
            for v in r.values()
        )
        print("  " + "  ".join(cells))


def main() -> int:
    t0 = time.perf_counter()
    print(
        f"one push = one code epoch: {TE} samples, {EPOCH_US:.1f} us; "
        f"1/T_E = {SPAN:.1f} Hz"
    )
    _table("3.1 symbol lock through a dropout", s3_1_lock_hysteresis())
    _table("5.1 arrival to tracking (data on)", s5_1_acquisition())
    _table("6.1 one tracking receiver", s6_1_tracking())
    _table("6.2 one receiver searching (noise)", s6_2_search_depth())
    _table("7.1 the pool as built: where a push goes", s7_1_split(capture()))
    _table("7.2 its searcher at the design point (noise)", s7_2_design_point())
    print(f"\nwall time {time.perf_counter() - t0:.0f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
