"""Step 2's figure pages (async-dsss-receiver.md §4.11), from the captures.

Per captured draw, the page set: each loop on a whole-draw page and a
hand-over-to-pull-in page, each detector on a whole-draw page and a zoom
at its flag's first change after it sustained. Every page is captioned
from the draw's own row and ``rules.csv``. ``scripts/plot_capture.py``
draws; this script only reads the row, the rules and the capture's flag
records to place the zoom windows, and builds the command lines.

Run from the tree the captures were taken on (``integ/async-rx``)::

    python figs2.py RUN_DIR OUT_DIR DRAW [DRAW ...]

``RUN_DIR/cap/<DRAW>/`` holds a capture written by
``validate_async_dsss_receiver_pullin --telemetry``, and
``RUN_DIR/cap/<DRAW>.csv`` holds its row. ``DRAW`` is a name such as
``G-works-2.2-4-9``: arm, category, carrier GHz, Es/N0 dB and seed.
"""

from __future__ import annotations

import statistics
import subprocess
import sys
from pathlib import Path

ROOT = Path.cwd()
sys.path.insert(0, str(ROOT / "src"))
from doppler.tests._validation_common import read_capture  # noqa: E402

PY = str(ROOT / ".venv/bin/python")
PLOT = str(ROOT / "scripts/plot_capture.py")
#: The receiver's front-end rate at 415.1-B-1, chip_rate * spc, Sa/s.
FS = 11e6
#: MpskReceiver's input rate: sps 8 at 3,000 symbols a second.
DEMOD = 8 * 3000.0
#: Chips a code period.
SF = 1023
#: The pages' panels that say when the loops are held.
HELD = ["rx.coast", "rx.hold"]

LOOPS: dict[str, tuple[list[str], list[str], str]] = {
    "carrier": (
        [
            "rx.car.e",
            "rx.car.freq",
            "rx.car.nco",
            "truth.hz",
            "err.car_unwrapped",
        ],
        [
            "--mean",
            "rx.car.e=64",
            "--scale",
            f"rx.car.freq={FS}:Hz",
            "--scale",
            f"rx.car.nco={FS}:Hz",
            "--scale",
            "truth.hz=1:Hz",
            "--scale",
            "err.car_unwrapped=1:deg",
            "--scale",
            "rx.car.e=1:sin of phase error",
        ],
        "The pre-despread carrier loop: e is the discriminator, sin of the "
        "phase error; freq its integrator and nco its control, in Hz at 11 "
        "MSa/s, beside the truth's Doppler; err.car_unwrapped the truth's "
        "phase error, unwrapped, in degrees; kp*e is nco - freq.",
    ),
    "code": (
        [
            "rx.code.steer.e",
            "rx.code.steer.prop",
            "rx.code.steer.integ",
            "rx.code.steer.ctrl",
            "rx.code.steer.aid",
            "err.code_chip",
        ],
        [
            "--mean",
            "rx.code.steer.e=64",
            "--mean",
            "rx.code.steer.prop=64",
            "--scale",
            "rx.code.steer.e=1:E-L / P",
            "--scale",
            "rx.code.steer.prop=1:filter units",
            "--scale",
            "rx.code.steer.integ=1:filter units",
            "--scale",
            f"rx.code.steer.ctrl={FS * SF}:chips/s",
            "--scale",
            "err.code_chip=1:chip",
        ],
        "The code loop at each steer of the symbol aid: e its discriminator, "
        "prop kp*e and integ its integrator in the filter's own units, ctrl "
        "the rate it adds in chips/s, aid the carrier's rate aid in chips/s; "
        "err.code_chip the truth's code error.",
    ),
    "demod-carrier": (
        [
            "rx.demod.car.e",
            "rx.demod.car.freq",
            "rx.demod.car.nco",
            "err.demod_car_deg",
        ],
        [
            "--mean",
            "rx.demod.car.e=64",
            "--scale",
            "rx.demod.car.e=1:rad",
            "--scale",
            f"rx.demod.car.freq={DEMOD}:Hz",
            "--scale",
            f"rx.demod.car.nco={DEMOD}:Hz",
            "--scale",
            "err.demod_car_deg=1:deg",
        ],
        "The demodulator's carrier loop, once a symbol: e in radians, freq "
        "its integrator and nco its control in Hz at its 24 kSa/s input, "
        "beside the truth's phase error at the decisions in degrees.",
    ),
    "timing": (
        [
            "rx.demod.sync.e",
            "rx.demod.sync.prop",
            "rx.demod.sync.integ",
            "rx.demod.sync.ctrl",
            "rx.demod.sync.mu",
        ],
        [
            "--mean",
            "rx.demod.sync.e=64",
            "--mean",
            "rx.demod.sync.prop=64",
            "--scale",
            "rx.demod.sync.e=1:TED units",
            "--scale",
            "rx.demod.sync.prop=1:filter units",
            "--scale",
            "rx.demod.sync.integ=1:filter units",
            "--scale",
            "rx.demod.sync.ctrl=1:rate dev. / input",
            "--scale",
            "rx.demod.sync.mu=1:sample fraction",
        ],
        "The timing loop (RateSync), once a symbol: e the TED output, prop "
        "and integ its filter terms, ctrl the rate deviation it commands per "
        "terminal input, mu the resampler's accumulator.",
    ),
}

#: name, rule row, statistic, flag, extra panels, the row's sustained time.
DETECTORS = [
    (
        "symbol",
        "sym",
        "rx.sym.lock",
        "rx.sym.locked",
        ["rx.sym.num", "rx.sym.den"],
        "t_sym_s",
    ),
    ("carrier", "car", "rx.car.lock", "rx.car.locked", [], "t_car_s"),
    (
        "code",
        "code",
        "rx.code.lock",
        "rx.code.locked",
        ["rx.code.prompt_amp", "rx.code.offset_pwr"],
        "t_code_s",
    ),
    (
        "demod-carrier",
        "demod",
        "rx.demod.lock",
        "rx.demod.car.locked",
        [],
        "t_demod_s",
    ),
    (
        "sync",
        "demod_sync",
        "rx.demod.sync.lock",
        "rx.demod.sync.locked",
        [],
        "t_sync_s",
    ),
]


def row_of(csv: Path) -> dict[str, str]:
    """The draw's row: the header line and the line after it."""
    lines = csv.read_text(encoding="utf-8").splitlines()
    i = next(k for k, line in enumerate(lines) if line.startswith("fc_ghz"))
    return dict(zip(lines[i].split(","), lines[i + 1].split(",")))


def rules_of(path: Path) -> dict[str, dict[str, str]]:
    """The draw's lock rules, by detector."""
    lines = path.read_text(encoding="utf-8").splitlines()
    head = lines[0].split(",")
    return {
        line.split(",")[0]: dict(zip(head, line.split(",")))
        for line in lines[1:]
    }


def when(x: str) -> str:
    """A row's time, or "never" for NaN."""
    return "never" if x == "nan" else f"{float(x):.3f}"


def caption(r: dict[str, str], draw: str, note: str) -> str:
    """The caption from the row: the meter's state first, then the draw."""
    arm, cat = draw.split("-")[:2]
    armname = "guideline arm (D3)" if arm == "G" else "comparison arm (0.04)"
    ho, bound = float(r["ho_err_hz"]), float(r["base_pullin_hz"])
    side = "outside" if abs(ho) > bound else "inside"
    first = (
        "The error meter never aligned: scored 0. "
        if r["scored"] == "0"
        else ""
    )
    return (
        f"{first}{r['fc_ghz']} GHz {float(r['esn0_db']):.0f} dB static, "
        f"{armname}, seed {r['seed']}, chosen as '{cat}'. Hand-over error "
        f"{ho:+.1f} Hz against the bound ASYNC_DSSS_RX_CARRIER_PULLIN_HZ "
        f"{bound:.1f} Hz ({side}); pulled in at t_pull_s "
        f"{when(r['t_pull_s'])} s, carrier flag sustained at t_car_s "
        f"{when(r['t_car_s'])} s. Carrier slips {r['ho_slips']} in the "
        f"pull-in transient, {r['car_slips']} steady; car_sd "
        f"{float(r['car_sd_deg']):.1f} deg. Symbol errors {r['errors']} of "
        f"{r['scored']} scored; works {r['works']}. rx.hold reads 1 while "
        "the code and symbol flags are both up (the hold snapshot "
        f"refreshed), not a held loop (§4.11 C18). {note}"
    )


def draw_page(
    cap: Path, out: Path, probes: list[str], opts: list[str], text: Path
) -> None:
    """One page through plot_capture.py, its caption from ``text``."""
    cmd = [PY, PLOT, str(cap), *probes, "--time", "--mark", "rx.state"]
    cmd += ["--caption-file", str(text), "-o", str(out), *opts]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)


def pages(run_dir: Path, out_dir: Path, draw: str) -> int:
    """Every page of one draw; returns how many."""
    d = run_dir / "cap" / draw
    cap = next(d.glob("*.tlm"))
    r = row_of(run_dir / "cap" / f"{draw}.csv")
    rules = rules_of(next(d.glob("*.rules.csv")))
    chip = float(r["fc_ghz"]) * 1e9 * 31 / 21216
    o = out_dir / draw
    o.mkdir(parents=True, exist_ok=True)
    ct = o / "caption.txt"

    def page(name: str, probes: list[str], opts: list[str], note: str) -> None:
        ct.write_text(caption(r, draw, note), encoding="utf-8")
        draw_page(cap, o / name, probes + HELD, opts, ct)

    ev: list[str] = []
    for key, label in (
        ("t_track_s", "hand-over"),
        ("t_pull_s", "t_pull"),
        ("t_car_s", "t_car"),
    ):
        if r[key] != "nan":
            ev += ["--vline", f"{float(r[key]):.4f}:{label}"]
    end = [float(r[k]) for k in ("t_pull_s", "t_car_s") if r[k] != "nan"]
    pull = f"0,{max(3.0, (max(end) if end else 2.0) + 1.0):.2f}"
    for i, (name, (probes, opts, note)) in enumerate(LOOPS.items(), 1):
        if name == "code":
            opts = [*opts, "--scale", f"rx.code.steer.aid={chip}:chips/s"]
        page(f"loop-{i}-{name}.png", probes, opts + ev, note + " Whole draw.")
        page(
            f"loop-{i}-{name}-pullin.png",
            probes,
            [*opts, *ev, "--xlim", pull],
            note + f" Hand-over to the end of pull-in ({pull} s).",
        )

    stamped = read_capture(cap, index=True)
    for i, (name, rule, stat, flag, extra, t_key) in enumerate(DETECTORS, 1):
        ru = rules[rule]
        lines = ["--line", f"{stat}={ru['up_thresh']}:up {ru['up_thresh']}"]
        if ru["down_thresh"] != ru["up_thresh"]:
            lines += ["--line", f"{stat}={ru['down_thresh']}:down"]
            lines[-1] += f" {ru['down_thresh']}"
        alpha = float(ru["alpha"])
        ema = (
            f"EMA alpha {alpha:g} ({1 / alpha:.0f} updates)"
            if alpha > 0
            else "no EMA (a block of looks)"
        )
        note = (
            f"The {name} detector: its statistic with its up and down "
            f"thresholds, its flag below. n_up {ru['n_up']}, n_down "
            f"{ru['n_down']}, {ema}, looks {ru['looks']}, the loop's bn "
            f"{ru['loop_bn']}."
        )
        probes = [stat, flag, *extra]
        page(f"det-{i}-{name}.png", probes, lines + ev, note + " Whole draw.")
        # The zoom: the flag's first change after it sustained (the row's
        # t_*_s), a window 300 of the statistic's own records wide.
        n_f, v_f = stamped[flag]
        n_s, _ = stamped[stat]
        tf = [n / FS for n in n_f]
        if r[t_key] == "nan":
            continue
        ts = float(r[t_key])
        ch = [
            k
            for k in range(1, len(v_f))
            if v_f[k] != v_f[k - 1] and tf[k] > ts + 1e-9
        ]
        if not ch:
            continue
        t0 = tf[ch[0]]
        dt = statistics.median(
            (n_s[k + 1] - n_s[k]) / FS for k in range(min(len(n_s) - 1, 2000))
        )
        w = 300 * dt
        page(
            f"det-{i}-{name}-zoom.png",
            probes,
            [
                *lines,
                *ev,
                "--vline",
                f"{t0:.4f}:first change after sustained",
                "--xlim",
                f"{t0 - w / 2:.4f},{t0 + w / 2:.4f}",
            ],
            note + f" Zoom: the flag's first change after it sustained at "
            f"{ts:.3f} s, at {t0:.3f} s, 300 updates wide.",
        )
    ct.unlink()
    return len(list(o.glob("*.png")))


def main() -> int:
    """Draw every named draw's pages."""
    run_dir, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    for draw in sys.argv[3:]:
        print(draw, pages(run_dir, out_dir, draw), "pages")
    return 0


if __name__ == "__main__":
    sys.exit(main())
