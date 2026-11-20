"""Ofdm -- certification evidence: the C pins, the float32 error, the binding.

Run directly to regenerate `results.md` and `data/fft_error_sweep.csv`:

    python src/doppler/ofdm/tests/validation/ofdm/validate.py

`--check` re-renders in memory and compares with the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit is
asserted by `src/doppler/ofdm/tests/test_validation_limits.py`, which runs
this same `build(write=False)`.

The order is the campaign's (`docs/dev/contributing/validation.md`):
`native/inc/doppler/ofdm/ofdm_core.h` is the SSOT, its claims are pinned in
`native/tests/ofdm/test_ofdm_core.c` (section 11 holds what the inventory
found unasserted), and `native/validation/ofdm_fft_error.c` measures the one
question the header does not answer: how far a float32 render sits from the
exact sum, per transform length. Its `--check --emit` subset is this report's
live measurement, the same run ctest makes; the full sweep is regenerated
only when this file writes, and read back from `data/` otherwise.
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path

import numpy as np

from doppler.ofdm import Ofdm
from doppler.tests._repo import build_dir, exe, repo_root
from doppler.tests._validation_common import Report, cli, harness_blocks

HERE = Path(__file__).resolve().parent
ROOT = repo_root(__file__)
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_ofdm_fft_error"
)
SWEEP = HERE / "data" / "fft_error_sweep.csv"

R = Report()


def _sweep(write: bool) -> list[dict]:
    """The full sweep: run and saved when writing, read back otherwise."""
    if write:
        rows = harness_blocks(HARNESS, "ofdm", ROOT)["fft_error"]
        SWEEP.parent.mkdir(exist_ok=True)
        with SWEEP.open("w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(
                f, fieldnames=list(rows[0]), lineterminator="\n"
            )
            w.writeheader()
            w.writerows(rows)
        return rows
    with SWEEP.open(encoding="utf-8") as f:
        return [
            {k: (v if k == "reference" else float(v)) for k, v in r.items()}
            for r in csv.DictReader(f)
        ]


def _binding_error(bound: float) -> tuple[float, float]:
    """A 64-point render through the Python face, against numpy in double.

    The same stimulus as the harness's (every subcarrier a unit-power QPSK
    point, scale 1/8, no prefix, no window); the reference is the header's
    sum written with numpy.fft in double, sharing no code with the C. The
    control moves one grid value by 1e-3 and must exceed the bound.
    """
    rng = np.random.default_rng(64)
    n = 64
    bits = rng.integers(0, 2, size=(2, n))
    pts = ((2 * bits[0] - 1) + 1j * (2 * bits[1] - 1)) / np.sqrt(2.0)
    pts = pts.astype(np.complex64)
    o = Ofdm()
    o.add_class(n, np.ones(n, np.uint8), scale=1.0 / 8.0)
    o.add_run(0, 1)
    x = o.modulate(pts).astype(np.complex128)
    # index k + n/2 holds subcarrier k: move it to transform order
    grid = np.fft.ifftshift(pts.astype(np.complex128))
    y = np.fft.ifft(grid) * n / 8.0
    err = float(np.linalg.norm(x - y) / np.linalg.norm(y))
    bad = grid.copy()
    bad[5] += 1e-3
    yb = np.fft.ifft(bad) * n / 8.0
    ctl = float(np.linalg.norm(x - yb) / np.linalg.norm(yb))
    return err, ctl


def characterise(live: list[dict], sweep: list[dict], binding) -> None:
    R.md("## 2. Characterisation")
    R.md()
    R.md(
        "### 2.1 The float32 error of a render, per transform length "
        "(`ofdm_fft_error.c`; C section 11)"
    )
    R.md()
    R.md(
        "One class of `n_fft` points, every subcarrier a seeded unit-power "
        "QPSK point, scale `1/sqrt(n_fft)`, no prefix and no window, so a "
        "body is exactly the header's sum. The error is the relative l2 "
        "distance of one body from that sum in double (written out up to "
        "1024 points, the library's cf64 transform above), the worst over "
        "the symbols. The bound is derived for a power of two from the "
        "transform that runs (section 11 of the C test): "
        "`L eta / (1 - L eta) + 2u`, `eta = mu + gamma_4 (sqrt 2 + mu)`, "
        "`mu < 20u`, `L = log2 n_fft`, `u = 2^-24`. The twiddles are formed "
        "in float (pffft.c:1141, 1157, 1161, 1264), so `mu` is 20u and not "
        "u: the inventory's first figure, 2.4e-6 at N = 64, assumed "
        "twiddles rounded from double and is replaced by 9.3e-6 here "
        f"({R.ref('inventory_bound_corrected')})."
    )
    R.md()
    R.md(
        "The full sweep (`make validate`; its rows are "
        "`data/fft_error_sweep.csv`):"
    )
    R.md()
    R.table(
        ["n_fft", "symbols", "reference", "rms", "worst", "bound"],
        [
            [
                f"{int(r['n_fft'])}",
                f"{int(r['symbols'])}",
                str(r["reference"]),
                f"{r['rel_rms']:.2e}",
                f"{r['rel_max']:.2e}",
                "--" if np.isnan(r["bound"]) else f"{r['bound']:.2e}",
            ]
            for r in sweep
        ],
    )
    R.md()
    R.md(
        "The live subset this report and ctest both run "
        "(`--check --emit`): one length per path."
    )
    R.md()
    R.table(
        ["n_fft", "path", "worst", "bound"],
        [
            [
                f"{int(r['n_fft'])}",
                "pffft (float)"
                if int(r["n_fft"]) == 64
                else "double, rounded",
                f"{r['rel_max']:.2e}",
                f"{r['bound']:.2e}",
            ]
            for r in live
        ],
    )
    R.md()
    R.md("### 2.2 The same render through the binding")
    R.md()
    R.md(
        "`doppler.ofdm.Ofdm`, 64 points, the same stimulus shape, against "
        "the header's sum written with numpy in double: worst relative error "
        f"**{binding[0]:.2e}**; with one grid value moved by 1e-3 (the "
        f"control), **{binding[1]:.2e}**."
    )
    R.md()


def review() -> None:
    R.md("## 3. Review")
    R.md()
    R.find(
        "ofdm_inventory_pins_added",
        "FIXED",
        "Reading `ofdm_core.h` against `test_ofdm_core.c` found 41 claims: "
        "28 pinned, 5 pinned only at literals (a gain on a fixed entry, three "
        "of five builders re-opening a sealed layout, reset of an unsealed "
        "layout, the accepted edges `cp == useful` and `window == symbol`) "
        "and 8 absent (the `OFDM_N_FFT_MAX`, `OFDM_N_PHASE_MAX` and "
        "`OFDM_COUNT_MAX` edges, `state_bytes` over a render, one plan per "
        "length, `modulate_max_out` past the end and for no points, `finish` "
        "before a render, a fixed entry on a null subcarrier). Section 11 "
        "pins each beside its control, and each went red under its own "
        "sabotage. One half of one claim has no sabotage: "
        "`modulate_max_out (0)` is 0 when the next symbol needs points "
        "because `walk()` returns 0 there by construction, and no mutant "
        "short of an absurd one reaches it.",
    )
    R.find(
        "ofdm_nan_scale_guard_removed",
        "FIXED",
        "`dp_ofdm_add_class` refused a NaN `scale`, which its header did not "
        "say. Under the library's rule a NaN parameter is garbage in "
        "(#2103), and the scale reaches only the samples and the layout "
        "hash: no size, index or abort. Nothing refuses it, and no test "
        "pins a refusal.",
    )
    R.find(
        "inventory_bound_corrected",
        "FIXED",
        "The claim inventory first gave the 64-point bound as 2.4e-6, "
        "taking the twiddles as accurate to u. pffft forms each twiddle's "
        "angle in float with three roundings, so the error is under 20u and "
        "the bound 9.3e-6. The test derives it from those lines, and says "
        "the condition under which it stops applying: the plan leaving "
        "pffft (pocketfft.c:71's gate) or the twiddle formation changing.",
    )
    R.find(
        "ofdm_error_tracks_the_path",
        "BY DESIGN",
        "The error depends on which transform the length takes, not on "
        "its size alone. A multiple of 16 that is 5-smooth runs pffft in "
        "float: about 1e-7 to 2.3e-7, rising as `sqrt (log2 N)` and at "
        "least 39 times under the derived bound at every power of two. "
        "Any other length runs pocketfft in double and rounds once: 448, "
        "704 and 832 read about 4e-8, under u. The pffft lengths that are "
        "not powers of two (192, 320, 576, 960) read 1.4e-7 to 3.1e-7, "
        "and no bound is derived for their radix-3 and radix-5 passes. The "
        "header makes no accuracy claim and the design page asked for the "
        "measurement; this is it, and nothing is asserted where nothing is "
        "derived.",
    )
    R.md()


def limits(live: list[dict], binding) -> None:
    R.md("## 4. Limits")
    R.md()
    by64 = next(r for r in live if int(r["n_fft"]) == 64)
    by704 = next(r for r in live if int(r["n_fft"]) == 704)
    R.limit(
        by64["rel_max"] <= by64["bound"],
        f"a 64-point float32 render is within its derived bound "
        f"({by64['bound']:.1e})",
        by="test_ofdm_core.c section 11; ofdm_fft_error --check",
        control="one grid value moved by 1e-3 exceeds it",
        red="scale x 1.0001: red",
    )
    R.limit(
        by704["rel_max"] <= by704["bound"],
        f"a 704-point render (double path) is within 3u plus the double "
        f"transform's bound ({by704['bound']:.1e})",
        by="ofdm_fft_error --check",
        control="the two paths' bounds differ (asserted)",
    )
    R.limit(
        binding[0] <= by64["bound"] < binding[1],
        "the binding's 64-point render is within the same bound, and its "
        "control is not",
        by="this report, section 2.2",
        control="one grid value moved by 1e-3",
    )
    R.md()


def build(write: bool = True) -> Report:
    live = harness_blocks(HARNESS, "ofdm", ROOT, ("--check", "--emit"))[
        "fft_error"
    ]
    sweep = _sweep(write)
    by64 = next(r for r in live if int(r["n_fft"]) == 64)
    binding = _binding_error(by64["bound"])

    R.md("# Ofdm -- certification evidence")
    R.md()
    R.md("## 1. The object")
    R.md()
    R.md(
        "A general OFDM modulator: constellation points and a layout in, "
        "baseband samples out. Design: [docs/design/ofdm.md]"
        "(../../../../../../docs/design/ofdm.md). Contract: "
        "`native/inc/doppler/ofdm/ofdm_core.h`. Pins: "
        "`native/tests/ofdm/test_ofdm_core.c`. The standard presets built on "
        "it are certified in the wlan report."
    )
    R.md()
    R.md("### Claim coverage")
    R.md()
    R.md(
        "Every prose claim of the header was mapped onto the C test before "
        f"anything was measured ({R.ref('ofdm_inventory_pins_added')}). "
        "Where the claims are pinned:"
    )
    R.md()
    R.table(
        ["claims", "where pinned", "proof"],
        [
            [
                "the grid, the transform's sign and scale, the cyclic body "
                "and prefix, the window formula and its boundary length",
                "C sections 2-5, 8",
                "forward sum returns the grid",
            ],
            [
                "builder refusals and edges, re-opening, reset, the size "
                "limits",
                "C sections 1, 1b, 11",
                "minimal pair per refusal; sabotage per edge",
            ],
            [
                "modulate's refusals change nothing; max_out and finish",
                "C sections 6, 6b, 11",
                "poisoned output and blob compare",
            ],
            [
                "the state: a split resumes bit for bit; set_state rejects",
                "C sections 9, 10, the round trip",
                "DP_STATE_ROUNDTRIP_TEST; forged fields",
            ],
            [
                "the float32 error (a design-page unknown, not a header "
                "claim)",
                "section 2.1 here; C section 11",
                "derived bound at N = 64",
            ],
        ],
    )
    R.md()

    characterise(live, sweep, binding)
    review()
    limits(live, binding)

    R.executive(
        "Ofdm",
        source=(
            "Generated by `validate.py` in this folder. Section 2.1 is "
            "measured in C by `native/validation/ofdm_fft_error.c`, section "
            "2.2 through the binding; the bound beside them is derived from "
            "the transform's code, not fitted. Re-run to regenerate."
        ),
        takeaways=[
            "**Every header claim is pinned in C beside a control** "
            f"({R.ref('ofdm_inventory_pins_added')}).",
            "**A float32 render sits about 1e-7 to 2.3e-7 from the exact "
            "sum at every power of two to 65536**, at least 39 times under a "
            "bound derived from the transform that runs (§2.1).",
            "**The error follows the transform's path, not the length** "
            f"({R.ref('ofdm_error_tracks_the_path')}): lengths outside "
            "pffft's 5-smooth set are rendered in double and read under u.",
            "**A NaN scale is not refused**: NaN in, NaN samples out "
            f"({R.ref('ofdm_nan_scale_guard_removed')}).",
        ],
    )
    R.summary(
        "\n- The runs behind §2 are `native/validation/ofdm_fft_error.c` "
        "(`--check --emit` live, the full sweep in `data/`); the pins are "
        "`native/tests/ofdm/test_ofdm_core.c`."
    )
    if write:
        R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
