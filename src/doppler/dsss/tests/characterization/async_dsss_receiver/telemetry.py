#!/usr/bin/env python3
"""Time, and watch, one AsyncDsssReceiver and one AsyncDsssPool on the same
capture -- the instrument for the pool's speed-up.

Every piece is a doppler tool, not a second copy of one:

- **The code** is one wfm Field, ``gold:LEN:REG:TA:SA:TB:SB``, through
  :func:`~doppler.wfm.field_bits` -- the text the CLI and a scene take. Its
  default is :class:`~doppler.wfm.Gold`'s own (CCSDS registers, 1023 chips).
- **The capture** is :class:`~doppler.wfm.Synth`, the continuous-DSSS mode
  (``symbol_rate`` set) with ``dsss_code_only=1``: the pure code, no data, at
  a carrier offset, with the C/N0 set through the library's own
  ``snr_mode="fs"`` (SNR over the sample rate, C/N0 - 10 log10 fs).
- **The telemetry** is filed :class:`~doppler.telemetry.Capture`\\ s, one per
  object, lossless by construction: ``rx.tlm`` and ``pool.tlm``, each with its
  ``-meta`` sidecar -- every loop's discriminator and filter output, the
  searcher's dwells, the pool's table.
- **The timing** is a characterization, not a benchmark: it shows how the
  cost is DISTRIBUTED over pushes (a dwell push against an ordinary one),
  which a min-over-rounds bench cannot. The published cost is
  ``make bench``'s (``bench_async_dsss_pool_core``,
  ``bench_async_dsss_receiver_core``). It is a third capture,
  ``timing.tlm``: each push's wall time
  as a probe (``timing.rx_push_us``, ``timing.pool_push_us``), so it is read
  and plotted by the same tools as the loops.
- **The figures** are ``scripts/plot_capture.py --time``, the one plotter for
  any filed capture.

Each object makes two passes over the one capture, one push per code epoch:
first TIMED with its telemetry detached (the shipped cost), then again with
it attached. The second must return the same symbols on every push as the
first, or the run is refused: what is plotted is what was timed. The pool's
pushes that decided a dwell come from ``pool.tlm`` (the searcher's records
carry the push's sample index), which splits its cost in ``run.json``.

Writes into ``--out``, beside this script unless pointed elsewhere
(``make pool-telemetry`` points it at ``build/pool-telemetry/``):
``run.json`` (parameters, versions, timing summary), ``pool.events`` (the
pool's event log), the three captures and their sidecars, and
``timing.png``, ``rx.png``, ``pool.png``, ``pool-s<i>.png``.

It sits beside ``characterize.py`` in this subject because it is the same
kind of thing: a deliberate run over the receiver's operating envelope, not a
per-push test. Its fast twin (``test_async_dsss_pool.py``) runs it for a
twentieth of a second on every push, so it cannot rot between deliberate
runs.

Run:  make pool-telemetry [POOL_TLM_ARGS='--cn0 45 --seconds 2']
      (or: uv run python <this file> --out DIR)
"""

from __future__ import annotations

import argparse
import json
import platform
import subprocess
import sys
import time
from pathlib import Path
from typing import TYPE_CHECKING, Any

import numpy as np
import numpy.typing as npt

import doppler
from doppler.dsss import AsyncDsssPool, AsyncDsssReceiver
from doppler.telemetry import Capture, EventLog, Telemetry
from doppler.tests._validation_common import read_capture
from doppler.util import fromto
from doppler.wfm import SampleClock, Synth, field_bits

HERE = Path(__file__).resolve().parent

if TYPE_CHECKING:
    from collections.abc import Callable

#: Gold()'s code as a Field: registers A (taps 934, seed 350) and B (taps
#: 567, seed 73), 10 bits, 1023 chips.
GOLD_1023 = "gold:1023:10:934:350:567:73"


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    """The capture's and the objects' parameters, defaulting to the pool
    bench's operating point (1,023 chips at 5 Mcps, 2 samples a chip)."""
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--code", default=GOLD_1023, help="a wfm Field")
    ap.add_argument("--chip-rate", type=float, default=5.0e6)
    ap.add_argument("--spc", type=int, default=2, help="samples per chip")
    ap.add_argument("--symbol-rate", type=float, default=2700.0)
    ap.add_argument("--doppler", type=float, default=1500.0, help="Hz")
    ap.add_argument("--cn0", type=float, default=47.0, help="dB-Hz")
    ap.add_argument("--seconds", type=float, default=1.0)
    ap.add_argument("--n-slots", type=int, default=4)
    ap.add_argument("--doppler-uncertainty", type=float, default=6000.0)
    ap.add_argument("--code-only-epochs", type=int, default=31)
    ap.add_argument("--pfa", type=float, default=1e-3)
    ap.add_argument("--decim", type=int, default=1, help="telemetry decim")
    ap.add_argument("--seed", type=int, default=7, help="the synth's seed")
    ap.add_argument(
        "--out",
        type=lambda s: Path(s).resolve(),
        default=None,
        help="where the run is written (default: beside this script)",
    )
    ap.add_argument(
        "--no-figures",
        action="store_true",
        help="the captures and run.json only (the fast twin's run)",
    )
    return ap.parse_args(argv)


def synth_kwargs(a: argparse.Namespace, code: npt.NDArray[np.uint8]) -> dict:
    """The Synth's parameters: the run's stimulus, stated once (and
    recorded in run.json, so the capture can be made again)."""
    fs = a.chip_rate * a.spc
    return {
        "type": "dsss",
        "fs": fs,
        "sps": a.spc,  # samples per CHIP
        "data_code": code.tobytes(),
        "symbol_rate": a.symbol_rate,  # > 0: the continuous mode
        "dsss_code_only": 1,  # the pure code, no data
        "freq": a.doppler,
        "snr": a.cn0 - 10.0 * np.log10(fs),
        "snr_mode": "fs",
        "seed": a.seed,
    }


def make_rx(a: argparse.Namespace, code: npt.NDArray[np.uint8]) -> Any:
    """The single receiver: the searching flavor, its own search."""
    return AsyncDsssReceiver(
        code,
        chip_rate=a.chip_rate,
        symbol_rate=a.symbol_rate,
        spc=a.spc,
        cn0_dbhz=a.cn0,
        pfa=a.pfa,
        doppler_uncertainty=a.doppler_uncertainty,
    )


def make_pool(a: argparse.Namespace, code: npt.NDArray[np.uint8]) -> Any:
    """The pool, on one thread: a telemetry context has one producer."""
    return AsyncDsssPool(
        code,
        chip_rate=a.chip_rate,
        symbol_rate=a.symbol_rate,
        spc=a.spc,
        cn0_dbhz=a.cn0,
        pfa=a.pfa,
        doppler_uncertainty=a.doppler_uncertainty,
        code_only_epochs=a.code_only_epochs,
        n_slots=a.n_slots,
        threads=1,
    )


def rx_step(rx: Any) -> Callable[[npt.NDArray[np.complex64]], int]:
    """One push of the single receiver: its symbol count."""
    return lambda blk: len(rx.steps(blk))


def pool_step(pool: Any) -> Callable[[npt.NDArray[np.complex64]], int]:
    """One push of the pool: every slot's symbols this push."""

    def step(blk: npt.NDArray[np.complex64]) -> int:
        pool.push(blk)
        return sum(len(pool.symbols(i)) for i in range(pool.n_slots))

    return step


def run_pass(
    step: Callable[[npt.NDArray[np.complex64]], int],
    x: npt.NDArray[np.complex64],
    te: int,
    tlm: Any,
    emit: Callable[[int, float], None] | None = None,
) -> tuple[npt.NDArray[np.int64], npt.NDArray[np.int64]]:
    """Push `x` an epoch at a time. `tlm.set_now()` stamps each push's
    boundary (and drains its capture) outside the clock; `emit(k, us)`, if
    given, files the push's wall time after it. Returns the per-push
    nanoseconds and the symbols each push returned."""
    n = len(x) // te
    ns = np.empty(n, np.int64)
    out = np.empty(n, np.int64)
    clock = time.perf_counter_ns
    for k in range(n):
        blk = x[k * te : (k + 1) * te]
        tlm.set_now(k * te)
        t0 = clock()
        out[k] = step(blk)
        ns[k] = clock() - t0
        if emit is not None:
            emit(k, ns[k] * 1e-3)
    tlm.set_now(n * te)
    return ns, out


def summary(ns: npt.NDArray[np.int64], te: int, fs: float) -> dict:
    """A pass's cost: per push and per sample, and against real time."""
    total = float(ns.sum())
    return {
        "pushes": len(ns),
        "ns_per_sample_mean": total / (len(ns) * te),
        "push_us_median": float(np.median(ns)) * 1e-3,
        "push_us_p99": float(np.percentile(ns, 99)) * 1e-3,
        "push_us_max": float(ns.max()) * 1e-3,
        "real_time_factor": (len(ns) * te / fs) / (total * 1e-9),
    }


def git_head() -> str:
    """The checkout's commit, for the record; 'unknown' outside a repo."""
    try:
        return subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            check=True,
            cwd=HERE,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def figures(out: Path, slots: list[str]) -> None:
    """Every figure through the one plotter, on a time axis.

    The plotter is ``scripts/plot_capture.py``, outside the package, so it
    is imported here and not at the top: the fast twin imports this module
    wherever the package is, and only a run that draws needs the checkout.
    This subject is ``src/doppler/dsss/tests/characterization/
    async_dsss_receiver/``, six directories below the repo root.
    """
    sys.path.insert(0, str(HERE.parents[5] / "scripts"))
    import plot_capture

    plot_capture.main(
        [
            str(out / "timing.tlm"),
            "--time",
            "-o",
            str(out / "timing.png"),
            "timing.rx_push_us",
            "timing.pool_push_us",
        ]
    )
    plot_capture.main(
        [str(out / "rx.tlm"), "--time", "-o", str(out / "rx.png")]
        + [
            f"rx.{p}"
            for p in (
                "car.freq",
                "car.e",
                "car.lock",
                "code.rate",
                "code.e",
                "code.lock",
                "code.locked",
                "demod.car.freq",
                "demod.sync.rate",
                "demod.sync.e",
                "demod.lock",
                "acq.hit",
            )
        ]
    )
    plot_capture.main(
        [str(out / "pool.tlm"), "--time", "-o", str(out / "pool.png")]
        + [
            f"pool.{p}"
            for p in (
                "assigned",
                "events",
                "dropped",
                "acq.stat",
                "acq.gate",
                "acq.n_peaks",
                "acq.n_held",
                "acq.conc",
                "acq.hit",
            )
        ]
    )
    for s in slots:
        plot_capture.main(
            [str(out / "pool.tlm"), "--time", "-o", str(out / f"pool-{s}.png")]
            + [
                f"pool.{s}.{p}"
                for p in (
                    "car.freq",
                    "car.e",
                    "code.rate",
                    "code.e",
                    "code.locked",
                    "demod.sync.rate",
                    "demod.lock",
                )
            ]
        )


def main(argv: list[str] | None = None) -> int:
    a = parse_args(argv)
    a.out = fromto(a.out)
    a.out.mkdir(parents=True, exist_ok=True)
    fs = a.chip_rate * a.spc
    code = field_bits(a.code)
    te = len(code) * a.spc  # one code epoch, the push
    n = int(a.seconds * fs) // te * te
    skw = synth_kwargs(a, code)
    x = np.asarray(Synth(**skw).steps(n), np.complex64)
    print(f"capture: {a.code}, {n} samples, {n // te} epochs, {n / fs:g} s")

    # Pass 1: timed, telemetry detached. Each push's wall time is filed as a
    # probe of its own context, so it is read and plotted like the loops.
    timing = Telemetry(1 << 12)
    ids = {k: timing.probe(f"timing.{k}_push_us") for k in ("rx", "pool")}
    ns, out = {}, {}
    with Capture(timing, te, str(a.out / "timing.tlm"), SampleClock(fs)):
        for name, make, stepper in (
            ("rx", make_rx, rx_step),
            ("pool", make_pool, pool_step),
        ):
            pid = ids[name]
            ns[name], out[name] = run_pass(
                stepper(make(a, code)),
                x,
                te,
                timing,
                emit=lambda k, us, pid=pid: timing.emit(pid, us),
            )

    # Pass 2: the same capture through fresh objects, telemetry attached.
    probes = {}
    for name, make, stepper in (
        ("rx", make_rx, rx_step),
        ("pool", make_pool, pool_step),
    ):
        obj = make(a, code)
        tlm = Telemetry(1 << 12)
        obj.set_telemetry(tlm, name, a.decim)
        log = None
        if name == "pool":
            log = EventLog(str(a.out / "pool.events"))
            obj.set_event_log(log)
        probes[name] = int(tlm.probe_count)
        with Capture(tlm, te, str(a.out / f"{name}.tlm"), SampleClock(fs)):
            _, got = run_pass(stepper(obj), x, te, tlm)
        if log is not None:
            obj.set_event_log(None)
            log.close()
        if not np.array_equal(got, out[name]):
            k = int(np.flatnonzero(got != out[name])[0])
            print(
                f"{name}: push {k} returned {got[k]} symbols with telemetry "
                f"attached and {out[name][k]} without -- not the same run",
                file=sys.stderr,
            )
            return 1

    # The pool's dwell pushes, read back from its filed capture: the
    # searcher's records carry the boundary of the push that decided them.
    pool = read_capture(a.out / "pool.tlm", index=True)
    dwell = np.zeros(len(ns["pool"]), bool)
    dwell[np.unique(pool["pool.acq.stat"][0] // te).astype(np.int64)] = True
    slots = sorted(
        {
            k.split(".")[1]
            for k, (m, _) in pool.items()
            if k.startswith("pool.s") and len(m)
        }
    )

    timing_sum = {k: summary(v, te, fs) for k, v in ns.items()}
    p = ns["pool"]
    timing_sum["pool"].update(
        dwell_pushes=int(dwell.sum()),
        dwell_push_us_mean=float(p[dwell].mean()) * 1e-3 if dwell.any() else 0,
        other_push_us_mean=float(p[~dwell].mean()) * 1e-3,
        dwell_share_of_time=float(p[dwell].sum() / p.sum()),
    )
    record = {
        "synth": {k: v for k, v in skw.items() if k != "data_code"},
        "code": a.code,
        "params": {
            k: str(v) if isinstance(v, Path) else v for k, v in vars(a).items()
        },
        "doppler": doppler.__version__,
        "git": git_head(),
        "python": platform.python_version(),
        "machine": platform.node(),
        "epoch_samples": te,
        "timing": timing_sum,
        "probes": probes,
        "slots_reporting": slots,
        "symbols": {k: int(v.sum()) for k, v in out.items()},
    }
    (a.out / "run.json").write_text(
        json.dumps(record, indent=2) + "\n", encoding="utf-8"
    )

    if not a.no_figures:
        figures(a.out, slots)

    for name, s in timing_sum.items():
        print(
            f"{name}: {s['ns_per_sample_mean']:.1f} ns/sample mean, push "
            f"median {s['push_us_median']:.1f} us, p99 "
            f"{s['push_us_p99']:.1f} us, {s['real_time_factor']:.2f}x real "
            "time"
        )
    s = timing_sum["pool"]
    print(
        f"pool: {s['dwell_pushes']} dwell pushes at "
        f"{s['dwell_push_us_mean']:.0f} us against "
        f"{s['other_push_us_mean']:.0f} us, "
        f"{100 * s['dwell_share_of_time']:.0f}% of the time"
    )
    print(f"wrote {a.out}/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
