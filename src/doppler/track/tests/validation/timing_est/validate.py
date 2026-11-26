"""SymbolTimingEstimator — certification evidence.

Run directly to regenerate `results.md`:

    uv run python \
        src/doppler/track/tests/validation/timing_est/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by
`src/doppler/track/tests/test_validation_limits.py`, which runs this same
`build(write=False)`.

The order is the campaign's, not this file's:
`native/inc/doppler/timing_est/timing_est_core.h` is the SSOT,
`native/tests/timing_est/test_timing_est_core.c` certifies it in C, and
`native/validation/timing_est_noise.c` measures what a caller meets under
noise, clock offset and Doppler. C measures; Python renders and asserts.
This file takes no measurement of its own: the gated numbers are the
harness's `--check --emit` run (the regression subset `ctest` also makes),
and the tables and figures marked as the record are its full sweep, kept
as `data/*.csv`.

Regenerate the record with::

    build/native/validation/validate_timing_est_noise \\
        --csv src/doppler/track/tests/validation/timing_est/data
"""

from __future__ import annotations

import csv
import math
import sys
from pathlib import Path
from types import SimpleNamespace

from doppler.tests._repo import build_dir, exe, repo_root
from doppler.tests._validation_common import Report, cli, harness_blocks

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"
ROOT = repo_root(__file__)
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_timing_est_noise"
)

R = Report()

# The harness's own figures: a --check row is held within this many of its
# standard errors, and a detection probability within this much beyond its
# interval (`g_z` and `PD_ALLOW` in timing_est_noise.c).
Z = 4.0
PD_ALLOW = 0.03
PFA = 1e-2


def record(name: str) -> list[dict[str, float | str]]:
    """One block of the full sweep's record, `data/<name>.csv`."""
    with (DATA / f"{name}.csv").open(encoding="utf-8") as fh:
        rows = []
        for r in csv.DictReader(fh):
            row: dict[str, float | str] = {}
            for k, v in r.items():
                try:
                    row[k] = float(v)
                except ValueError:
                    row[k] = v
            rows.append(row)
        return rows


def within(meas: float, pred: float, se: float) -> bool:
    """The harness's comparison: inside Z standard errors."""
    return abs(meas - pred) <= Z * se


def rms(r: dict, key: str = "var") -> float:
    """A row's standard deviation from its variance column."""
    return math.sqrt(max(float(r[key]), 0.0))


def pm(value: float, se: float, fmt: str = "+.5f") -> str:
    """`value ± se`, both in one format."""
    return f"{value:{fmt}} ± {se:{fmt.lstrip('+')}}"


# ── 1. the object ────────────────────────────────────────────────────
def section_object() -> None:
    R.md("## 1. The object — design and expectations")
    R.md()
    R.md(
        "A feedforward estimate of symbol timing: a block of "
        "matched-filtered samples in, the eye center, the clock's rate "
        "error and a quality figure out, with no loop and no pull-in. The "
        "design, the three-term variance it predicts, the bounds a clock "
        "offset and a Doppler put on the block, and the unknowns this "
        "report was asked to settle are "
        "[`docs/design/timing-estimator.md`]"
        "(../../../../../../../docs/design/timing-estimator.md); the "
        "contract is `native/inc/doppler/timing_est/timing_est_core.h`."
    )
    R.md()
    R.md(
        "The header's claims, the section of "
        "`native/tests/timing_est/test_timing_est_core.c` that pins each, "
        "and where this report measures it over the envelope:"
    )
    R.md()
    R.table(
        ["header claim", "C test", "here"],
        [
            [
                "create refuses a rate the line cannot carry (2 or less, "
                "not finite)",
                "§1",
                "—",
            ],
            [
                "`tau` is the eye center on the symbol clock and `quality` "
                "the line's share of the power, at whole and fractional "
                "samples a symbol",
                "§2",
                "§2.1, §2.5, §2.7",
            ],
            [
                "the carrier does not enter",
                "§3",
                "every row carries one; §2.3",
            ],
            [
                "one clock across calls, and the rate from two blocks "
                "while the timing moves under half a symbol between them",
                "§4",
                "§2.2, §2.3, §2.4",
            ],
            [
                "estimate is accumulate then finish; a block with no "
                "power is not kept",
                "§5",
                "—",
            ],
            ["reset returns the object to its post-create state", "§6", "—"],
            [
                "the state resumes bit for bit and a forged blob is refused",
                "§7",
                "—",
            ],
            [
                "`tau` on a pulse that is not band limited, read at a "
                "whole number of samples a symbol, is off the eye center "
                "by where the samples fall in the symbol",
                "§8",
                "§2.7",
            ],
            [
                "nothing is decided: `quality` is reported and the caller "
                "judges it",
                "§2",
                "§2.5, §2.6 (what a threshold on it delivers)",
            ],
        ],
    )
    R.md(
        "Every number below comes from the C harness, on one chain: the "
        "library's generator (`wfm_synth`, symbols from its 31-bit PRBS), "
        "`doppler_channel` where the time base is dilated, the samples "
        "the estimator's rate keeps, noise from `awgn`, the shipped "
        "matched filter (`RateConverter`, matched, rate 1), then the "
        "estimator. The rectangle reaches the matched filter as the mean "
        "over each kept sample's own interval, which is the design "
        "condition `RateSync`'s bank was given; the root-raised cosine "
        "(roll-off 0.35) is sampled. **A carrier offset is on every "
        "row**, drawn between 0.01 and 0.05 of the symbol rate unless "
        "the row sets it, with a starting phase drawn each trial."
    )
    R.md()
    R.md(
        "The truth and the prediction both come from one measurement "
        "that does not involve the estimator: a lone symbol through the "
        "same chain, read on the generator's grid. Its energy centroid "
        "is the eye center. Its shape, the matched filter's impulse "
        "response and the block's geometry give the mean, the variance "
        "and the line's size by the formulas in the harness's header "
        "comment. N is samples a symbol, L symbols a block, and a "
        "timing is in symbols."
    )
    R.md()


# ── 2. characterisation ──────────────────────────────────────────────
def characterise() -> SimpleNamespace:
    d = SimpleNamespace()
    # The report's harness call is the C regression subset ctest also makes
    # (`--check --emit`): the limits below read this run, so the evidence
    # and the gate cannot disagree. A failed assertion or a control that
    # passes makes the harness exit non-zero, which raises here.
    B = harness_blocks(
        HARNESS, "SymbolTimingEstimator", ROOT, args=("--check", "--emit")
    )
    d.B = B
    R.md("## 2. Characterisation — what the object delivers")
    R.md()
    R.md(
        "Each part opens with the rows the gate runs (the harness's "
        "`--check`, seeded, held within four standard errors of their "
        "own trials) and then shows the record: the same harness's full "
        "sweep, kept as `data/*.csv` and refreshed on purpose. `±` is "
        "one standard error."
    )
    R.md()

    # 2.1 noise --------------------------------------------------------
    R.md("### 2.1 Noise: the mean and the variance against the pulse's")
    R.md()
    R.md(
        "The variance is `(k_ss + k_sn / g + k_nn / g^2) / L` to first "
        "order, `g` the Es/N0, with the three constants computed from "
        "the pulse; the prediction held here carries it one order "
        "further. The mean error is against the eye center, and its "
        "prediction is the bias the pulse and the sampling give (§2.7)."
    )
    R.md()
    d.noise = B["noise"]
    R.table(
        [
            "pulse",
            "N",
            "L",
            "Es/N0 dB",
            "carrier",
            "mean error",
            "predicted",
            "rms",
            "predicted",
            "line energy / predicted",
        ],
        [
            [
                str(r["pulse"]),
                f"{r['sps']:.3g}",
                f"{r['L']:.0f}",
                f"{r['esn0_db']:.0f}",
                f"{r['carrier']:+.3f}",
                pm(r["err_mean"], r["err_se"]),
                f"{r['bias_pred']:+.5f}",
                pm(rms(r), r["var_se"] / (2.0 * rms(r)), ".4f"),
                f"{rms(r, 'var2'):.4f}",
                f"{r['c2'] / r['c2_pred']:.3f}",
            ]
            for r in d.noise
        ],
    )
    rec = record("noise")
    d.rec_noise = rec
    held = [r for r in rec if r["held"] == 1.0]
    d.line_rect = sorted(r["quality"] for r in rec if r["pulse"] == "rect")
    d.line_rrc = sorted(r["quality"] for r in rec if r["pulse"] == "rrc")
    at4 = [r for r in rec if r["sps"] == 4.0 and r["esn0_db"] == 4.0]
    d.rect_128 = sorted(
        rms(r) for r in at4 if r["pulse"] == "rect" and r["L"] == 128
    )
    d.rrc_512 = [rms(r) for r in at4 if r["pulse"] == "rrc" and r["L"] == 512]
    d.rrc_128_corr = sorted(
        r["corr"] for r in at4 if r["pulse"] == "rrc" and r["L"] == 128
    )
    not_held = [r for r in rec if r["held"] != 1.0]
    d.noise_ratio_held = [r["var"] / r["var2"] for r in held]
    d.noise_ratio_all = [r["var"] / r["var2"] for r in rec]
    R.md(
        f"The record (`data/noise.csv`, {len(rec)} rows: Es/N0 0 to 14 "
        "dB, L 32 to 512, N 3 to 8 whole and not, both pulses). On the "
        f"{len(held)} rows where the expansion is a prediction (its "
        "second-order term under a quarter of the first) the measured "
        "variance is "
        f"{min(d.noise_ratio_held):.2f} to {max(d.noise_ratio_held):.2f} "
        f"of the predicted one. On the other {len(not_held)}, all short "
        "blocks or low Es/N0, it is "
        f"{min(r['var'] / r['var2'] for r in not_held):.2f} to "
        f"{max(r['var'] / r['var2'] for r in not_held):.2f}: the "
        "estimate is past its threshold there and the form no longer "
        "predicts it."
    )
    R.md()
    R.md("![rms timing error against Es/N0 and block length](noise.png)")
    R.md()
    R.md(
        "The constants, at L 128 and the carrier and starting offset "
        "each row drew (`data/constants.csv`). `line` is the quality "
        "the pulse gives at Es/N0 4 dB, and `bias` the mean error it "
        "predicts:"
    )
    R.md()
    const = record("constants")
    d.const = const
    R.table(
        ["pulse", "N", "k_ss", "k_sn", "k_nn", "line", "bias"],
        [
            [
                str(r["pulse"]),
                f"{r['sps']:.3f}",
                f"{r['k_ss']:.4f}",
                f"{r['k_sn']:.3f}",
                f"{r['k_nn']:.3f}",
                f"{r['line']:.3f}",
                f"{r['bias']:+.4f}",
            ]
            for r in sorted(const, key=lambda r: (str(r["pulse"]), r["sps"]))
        ],
    )

    # 2.2 clock offset -------------------------------------------------
    R.md("### 2.2 Clock offset: the rate, and where the one bin ends")
    R.md()
    R.md(
        "`doppler_channel` dilates the time base with no carrier of its "
        "own; `x` is how many cycles the power's line turns in one "
        "block, so the bin's edge is `x = 1/2`. Two blocks a trial give "
        "the rate. The line's energy is shown over the same block with "
        "no offset, beside the prediction and the design page's "
        "`sinc(x)^2`."
    )
    R.md()
    d.clock = B["clock"]
    R.table(
        [
            "pulse",
            "N",
            "L",
            "x",
            "rate",
            "predicted",
            "sent",
            "line energy",
            "predicted",
            "sinc^2",
        ],
        [
            [
                str(r["pulse"]),
                f"{r['sps']:.3g}",
                f"{r['L']:.0f}",
                f"{r['x']:+.2f}",
                pm(1e6 * r["rate"], 1e6 * r["rate_se"], "+.1f") + " ppm",
                f"{1e6 * r['rate_pred']:+.1f}",
                f"{1e6 * r['rate_true']:+.1f}",
                f"{r['c2'] / r['c2_flat']:.3f}",
                f"{r['c2_pred'] / r['c2_flat']:.3f}",
                f"{r['sinc'] ** 2:.3f}",
            ]
            for r in d.clock
        ],
    )
    rc = record("clock")
    d.rec_clock = rc
    inside = [
        r
        for r in rc
        if abs(r["x"]) < 0.45 and r["pulse"] == "rect" and r["sps"] == 4.0
    ]
    d.clock_rate_err = max(
        abs(r["rate"] - r["rate_true"]) * r["L"] for r in inside
    )
    d.line_at_04 = sorted(
        r["c2"] / r["c2_flat"]
        for r in rc
        if abs(abs(r["x"]) - 0.4) < 1e-9 and r["pulse"] == "rect"
    )
    R.md(
        "The record (`data/clock.csv`): `x` from 0 to 1.25, both signs. "
        "Inside the bin the rate is the offset sent to within "
        f"{d.clock_rate_err:.3f} cycles a block on the rectangle at N 4 "
        "(the sampling-phase bias of §2.7 is what is left), and on the "
        "root-raised cosine to within its standard error. At `x = 1/2` "
        "the two signs are indistinguishable and the mean reads 0. Past "
        "it the rate reads `(x - round(x)) / L`: the wrong sign at 0.6 "
        "and 0.75, nothing at 1."
    )
    R.md()
    R.md("![the rate read and the line's energy against x](clock.png)")
    R.md()

    # 2.3 Doppler ------------------------------------------------------
    R.md("### 2.3 Doppler: the carrier and the time base together")
    R.md()
    R.md(
        "`doppler_channel` applies both from one figure: a carrier 5000 "
        "symbol rates up, so 20 ppm is a tenth of the symbol rate off "
        "and a dilation of 20 ppm. `carrier` is the same offset from "
        "the generator with no dilation; `clock` (record only) is the "
        "dilation with no carrier, the control. N 4, L 512, Es/N0 10 dB."
    )
    R.md()
    d.dop = B["doppler"]

    def dop_rows(rows):
        return [
            [
                str(r["pulse"]),
                str(r["kind"]),
                f"{r['ppm']:+.0f}",
                f"{r['carrier']:+.3f}",
                pm(1e6 * r["rate"], 1e6 * r["rate_se"], "+.2f"),
                f"{1e6 * r['rate_pred']:+.2f}",
                f"{1e6 * r['rate_sent']:+.0f}",
                pm(rms(r), r["var_se"] / (2.0 * rms(r)), ".5f"),
                f"{rms(r, 'var2'):.5f}",
            ]
            for r in rows
        ]

    dop_head = [
        "pulse",
        "kind",
        "ppm",
        "carrier",
        "rate ppm",
        "predicted",
        "sent",
        "rms",
        "predicted",
    ]
    R.table(dop_head, dop_rows(d.dop))
    rd = record("doppler")
    d.rec_dop = rd
    R.md("The record (`data/doppler.csv`):")
    R.md()
    R.table(dop_head, dop_rows(rd))
    carrier_only = [r for r in rd if r["kind"] == "carrier"]
    d.carrier_rate_z = max(abs(r["rate"]) / r["rate_se"] for r in carrier_only)
    rect_both = [
        r for r in rd if r["kind"] != "carrier" and r["pulse"] == "rect"
    ]
    d.rect_gain = [r["rate"] / r["rate_sent"] for r in rect_both]
    rrc_both = [
        r for r in rd if r["kind"] != "carrier" and r["pulse"] == "rrc"
    ]
    d.rrc_gain = [r["rate"] / r["rate_sent"] for r in rrc_both]
    R.md(
        "The carrier alone reads no rate (at most "
        f"{d.carrier_rate_z:.1f} standard errors from 0 over "
        f"{len(carrier_only)} rows, to a tenth of the symbol rate), and "
        "the variance is the pulse's at that carrier. With the "
        "dilation, the root-raised cosine reads "
        f"{min(d.rrc_gain):.2f} to {max(d.rrc_gain):.2f} of the rate "
        "sent. The rectangle at N 4 reads "
        f"{min(d.rect_gain):.2f} to {max(d.rect_gain):.2f} of it: a "
        "drift of a hundredth of a symbol over the block is read "
        "through the slope of the sampling-phase bias (§2.7), and the "
        "prediction, which carries that bias, is what the rows are held "
        "to."
    )
    R.md()

    # 2.4 Doppler rate -------------------------------------------------
    R.md("### 2.4 A Doppler rate: the line chirps across the block")
    R.md()
    R.md(
        "`q` is the change in clock offset across a block times L, with "
        "the offset zero at the block's middle. The design page left "
        "its cost underived. For a line whose size does not depend on "
        "the sampling phase it is the integral of `exp(j pi q u^2)` "
        "over the block: the line's energy falls as its squared "
        "magnitude and the timing moves by its argument over `2 pi` "
        "(`page` below)."
    )
    R.md()
    d.chirp = B["doppler_rate"]

    def chirp_rows(rows):
        return [
            [
                str(r["pulse"]),
                f"{r['L']:.0f}",
                f"{r['q']:+.2f}",
                pm(r["c2"] / r["c2_flat"], r["c2_se"] / r["c2_flat"], ".3f"),
                f"{r['c2_pred'] / r['c2_flat']:.3f}",
                f"{r['c2_page'] / r['c2_flat']:.3f}",
                pm(r["err_mean"], r["err_se"], "+.4f"),
                f"{r['err_pred']:+.4f}",
                f"{r['err_page']:+.4f}",
            ]
            for r in rows
        ]

    chirp_head = [
        "pulse",
        "L",
        "q",
        "line energy",
        "predicted",
        "page",
        "mean error",
        "predicted",
        "page",
    ]
    R.table(chirp_head, chirp_rows(d.chirp))
    rq = record("doppler_rate")
    d.rec_chirp = rq
    R.md("The record (`data/doppler_rate.csv`):")
    R.md()
    R.table(chirp_head, chirp_rows(rq))
    small = min(rq, key=lambda r: abs(r["q"]))
    d.chirp_small = small
    R.md(
        "The integral holds on the root-raised cosine. On the rectangle "
        "at N 4 the prediction on the block's own geometry holds and "
        "the integral alone does not, for the reason of §2.7. At "
        f"`q = {small['q']:g}` nothing is lost: a physical Doppler rate "
        "is orders of magnitude under that."
    )
    R.md()

    # 2.5 quality, noise alone -----------------------------------------
    R.md("### 2.5 The quality figure with noise alone")
    R.md()
    R.md(
        "With no signal `c` is circular Gaussian, and the mean quality "
        "is the Rayleigh mean of `|c|` over the mean power. A threshold "
        "for a false-alarm probability is bracketed by two computed "
        "ones: the Rayleigh threshold, which leaves out the spread of "
        "the block's power, and the one that averages the tail over "
        "that spread as if it were independent of `|c|`. Each "
        "probability carries its Wilson 95% interval. `independent` is "
        "the threshold samples taken as independent would call for."
    )
    R.md()
    d.h0 = B["quality_h0"]

    def h0_rows(rows):
        return [
            [
                str(r["pulse"]),
                f"{r['sps']:.3g}",
                f"{r['L']:.0f}",
                f"{r['pfa']:g}",
                f"{r['threshold']:.4f}",
                f"{r['p']:.5f} [{r['lo']:.5f}, {r['hi']:.5f}]",
                f"{r['ray_threshold']:.4f}",
                f"{r['ray_p']:.5f} [{r['ray_lo']:.5f}, {r['ray_hi']:.5f}]",
                f"{r['white_threshold']:.4f}",
                f"{r['white_hits'] / r['trials']:.5f}",
                pm(r["q_mean"], r["q_se"], ".5f"),
                f"{r['q_pred']:.5f}",
            ]
            for r in rows
        ]

    h0_head = [
        "pulse",
        "N",
        "L",
        "target",
        "with spread",
        "P",
        "Rayleigh",
        "P",
        "independent",
        "P",
        "mean quality",
        "predicted",
    ]
    R.table(h0_head, h0_rows(d.h0))
    r0 = record("quality_h0")
    d.rec_h0 = r0
    R.md(
        f"The record (`data/quality_h0.csv`, {r0[0]['trials']:.0f} "
        "trials a row):"
    )
    R.md()
    R.table(h0_head, h0_rows(r0))
    d.spread_ratio = [r["p"] / r["pfa"] for r in r0]
    d.ray_ratio = [r["ray_p"] / r["pfa"] for r in r0]
    R.md(
        "The threshold with the spread gives "
        f"{min(d.spread_ratio):.2f} to {max(d.spread_ratio):.2f} of the "
        "target and the Rayleigh one "
        f"{min(d.ray_ratio):.2f} to {max(d.ray_ratio):.2f}: `|c|` and "
        "the block's power rise and fall together, so the first "
        "over-corrects. It is the conservative one of the two."
    )
    R.md()

    # 2.6 quality, with signal -----------------------------------------
    R.md("### 2.6 The quality figure with signal, and with no transitions")
    R.md()
    R.md(
        "At the conservative threshold for a false-alarm probability of "
        f"{PFA:g}: the probability the quality exceeds it, with its "
        "Wilson 95% interval, beside the normal form computed from the "
        "pulse. `constant` is the same stream with every symbol the "
        "same, which has no transitions and so no line."
    )
    R.md()
    d.h1 = B["quality_h1"]

    def h1_rows(rows):
        return [
            [
                str(r["pulse"]),
                f"{r['L']:.0f}",
                f"{r['esn0_db']:.0f}",
                str(r["symbols"]),
                f"{r['pd']:.4f} [{r['lo']:.4f}, {r['hi']:.4f}]",
                "—" if math.isnan(r["pd_pred"]) else f"{r['pd_pred']:.4f}",
                f"{r['quality']:.4f}",
            ]
            for r in rows
        ]

    h1_head = [
        "pulse",
        "L",
        "Es/N0 dB",
        "symbols",
        "P(over threshold)",
        "predicted",
        "mean quality",
    ]
    R.table(h1_head, h1_rows(d.h1))
    r1 = record("quality_h1")
    d.rec_h1 = r1
    data_rows = [r for r in r1 if r["symbols"] == "data"]
    d.pd_gap = max(
        max(r["lo"] - r["pd_pred"], r["pd_pred"] - r["hi"], 0.0)
        for r in data_rows
    )
    d.const_hits = sum(r["hits"] for r in r1 if r["symbols"] == "constant")
    d.const_trials = sum(r["trials"] for r in r1 if r["symbols"] == "constant")
    R.md(
        "The record (`data/quality_h1.csv`): the rectangle from 128 "
        "symbols and the root-raised cosine from 512, Es/N0 0 to 14 "
        "dB. The normal form is inside the interval or within "
        f"{d.pd_gap:.3f} of it on every row. A stream with no "
        f"transitions crossed the threshold {d.const_hits:.0f} times in "
        f"{d.const_trials:.0f} blocks."
    )
    R.md()
    R.md("![detection probability against Es/N0](detection.png)")
    R.md()

    # 2.7 starting offset ----------------------------------------------
    R.md("### 2.7 Starting offset, and the sampling-phase bias")
    R.md()
    R.md(
        "64 starting offsets across one symbol, at one carrier. `z` is "
        "the mean error less its prediction over its standard error, at "
        "each offset; the test is the largest `z` and their mean "
        "square, whose bound is `1 + 4 sqrt(2 / 64)`. `fixed eye` is "
        "the same statistic with the eye center taken as offset 0's at "
        "every offset, which is the control."
    )
    R.md()
    d.off = B["offset_summary"]

    def off_rows(rows):
        return [
            [
                str(r["pulse"]),
                f"{r['sps']:.3g}",
                f"{r['L']:.0f}",
                f"{r['trials']:.0f}",
                f"{r['z_max']:.2f}",
                f"{r['chi2']:.3f}",
                f"{r['bound']:.3f}",
                f"{r['chi2_fixed']:.0f}",
                f"{r['bias_lo']:+.4f} to {r['bias_hi']:+.4f}",
                f"{r['line_lo']:.3f} to {r['line_hi']:.3f}",
            ]
            for r in rows
        ]

    off_head = [
        "pulse",
        "N",
        "L",
        "trials an offset",
        "largest z",
        "mean z^2",
        "bound",
        "fixed eye",
        "predicted bias",
        "predicted line",
    ]
    R.table(off_head, off_rows(d.off))
    ro = record("offset_summary")
    d.rec_off = ro
    R.md(
        "The record (`data/offset_summary.csv`, every offset in "
        "`data/offset.csv`; its bound is for five standard errors):"
    )
    R.md()
    R.table(off_head, off_rows(ro))
    by = {(str(r["pulse"]), round(r["sps"], 3)): r for r in ro}
    d.rect4 = by[("rect", 4.0)]
    d.rect32 = by[("rect", 3.2)]
    d.rrc4 = by[("rrc", 4.0)]
    d.point4 = by[("rect_point", 4.0)]
    R.md(
        "The mean error is the predicted bias at every offset, so the "
        "estimate does not depend on where the block starts beyond what "
        "the pulse and the sampling put there. What they put there is "
        "not zero. For the rectangle at a whole N the power's third "
        "harmonic folds onto the line, and the line's phase and size "
        "then depend on where the samples fall in the symbol: at N 4 "
        f"the bias runs {d.rect4['bias_lo']:+.4f} to "
        f"{d.rect4['bias_hi']:+.4f} symbols and the line's share of the "
        f"power {d.rect4['line_lo']:.3f} to {d.rect4['line_hi']:.3f}. "
        "At N 3.2 the sampling phase runs through every value inside "
        "the block and the bias is "
        f"{d.rect32['bias_lo']:+.4f} to {d.rect32['bias_hi']:+.4f}, "
        "nearly all of it the leak of a block that is not a whole "
        "number of symbols (410 samples). The root-raised cosine is "
        f"band limited: {abs(d.rrc4['bias_hi']):.5f} at most. The "
        "rectangle merely sampled, `rect_point`, passes the same test "
        "with a bias that steps through "
        f"{d.point4['bias_lo']:+.3f} to {d.point4['bias_hi']:+.3f}: "
        "half a sample, because 16 generator offsets give it the same "
        "samples."
    )
    R.md()
    R.md(
        "![mean error and predicted bias against starting offset](offset.png)"
    )
    R.md()
    return d


# ── 3. review ────────────────────────────────────────────────────────
def review(d: SimpleNamespace) -> None:
    R.md("## 3. Review — findings, with verdicts")
    R.md()
    edge = {length: 0.4 / length * 1e6 for length in (128, 512, 2048)}
    d.edge = edge
    R.find(
        "one_rate_bin_holds_to_half_cycle",
        "BY DESIGN",
        "**One rate bin, and it ends where the page said: half a cycle "
        "of the line a block.** Inside it the rate is read; at 0.4 of a "
        "cycle the rectangle's line still carries "
        f"{d.line_at_04[0]:.2f} to {d.line_at_04[-1]:.2f} of its energy "
        "and the rate is right (§2.2). Past it the rate is aliased "
        "with the wrong sign. The usable clock offset is therefore "
        f"about `0.4 / L`: {edge[128]:.0f} ppm at 128 symbols, "
        f"{edge[512]:.0f} ppm at 512 and {edge[2048]:.0f} ppm at 2048. "
        "The noise asks for 128 to 512 symbols to reach 0.01 to 0.02 "
        "symbols rms (§2.1), and at those lengths one bin holds any "
        f"offset under {edge[512]:.0f} ppm. A set of rate hypotheses "
        "is needed only by a caller who wants a block of thousands of "
        "symbols and an offset of hundreds of ppm together.",
    )
    R.find(
        "whole_n_rect_sampling_phase",
        "BY DESIGN",
        "**On the rectangle at a whole number of samples a symbol the "
        "estimate depends on where the samples fall in the symbol.** "
        f"The bias is up to {d.rect4['bias_hi']:.3f} symbols at N 4 and "
        "its slope reads a slow drift with a gain of "
        f"{min(d.rect_gain):.2f} to {max(d.rect_gain):.2f} (§2.3, "
        "§2.7). It is the pulse's, not the estimator's arithmetic: the "
        "prediction from the pulse gives every one of those rows. A "
        "caller who needs better than a hundredth of a symbol from a "
        "rectangle picks an N that is not whole, where the bias "
        "averages out inside the block, or reads the rate over a drift "
        "of a sample or more, where it averages out between blocks "
        "(§2.2).",
    )
    R.find(
        "rrc_line_is_weak",
        "BY DESIGN",
        "**The root-raised cosine's line is weak.** At roll-off 0.35 "
        f"the quality is {d.line_rrc[0]:.3f} to {d.line_rrc[-1]:.3f} "
        f"over the record against {d.line_rect[0]:.3f} to "
        f"{d.line_rect[-1]:.3f} for the rectangle, so the same error "
        "takes about four times the symbols: at Es/N0 4 dB, "
        f"{d.rrc_512[0]:.3f} rms from 512 against {d.rect_128[0]:.3f} "
        f"to {d.rect_128[-1]:.3f} from 128 for the rectangle. From 128 "
        "symbols at 4 dB the expansion's second-order term is already "
        f"{d.rrc_128_corr[0]:.2f} of the first and the estimate is "
        "nearing its threshold (§2.1). The line grows with the "
        "roll-off; that is the pulse's excess bandwidth and no setting "
        "of this object.",
    )
    R.find(
        "sampled_rect_has_sample_timing",
        "BY DESIGN",
        "**A rectangle merely sampled carries no timing finer than a "
        "sample.** At a whole N every starting offset inside one "
        "sample interval gives the same samples, so the estimate is "
        f"the same and its error steps through ±{d.point4['bias_hi']:.3f} "
        "symbols at N 4 (§2.7). The estimator reports what the samples "
        "hold. The front end that averages over each sample's interval "
        "is what gives the stream a timing between samples.",
    )
    R.find(
        "pfa_threshold_is_a_bracket",
        "GAP",
        "**The threshold for a stated false-alarm probability is "
        "bracketed, not derived.** The Rayleigh threshold gives "
        f"{min(d.ray_ratio):.2f} to {max(d.ray_ratio):.2f} of the "
        "target and the one that allows for the power's spread "
        f"{min(d.spread_ratio):.2f} to {max(d.spread_ratio):.2f} "
        "(§2.5). The second is safe to use; the exact one needs the "
        "joint behavior of `|c|` and the block's power, tracked in "
        "[#3367](https://github.com/doppler-dsp/doppler/issues/3367).",
    )
    R.find(
        "cascades_not_measured",
        "GAP",
        "**The rectangle is measured behind one front end.** The "
        "design page asks for the constants behind each planned "
        "cascade; this report has the sample that is the mean over its "
        "own interval, and the finding "
        f"{R.ref('whole_n_rect_sampling_phase')} says the cascade's "
        "own response decides the numbers. `RateSync`'s cascades are "
        "not run here: "
        "[#3367](https://github.com/doppler-dsp/doppler/issues/3367).",
    )


# ── 4. limits ────────────────────────────────────────────────────────
def limits(d: SimpleNamespace) -> None:
    R.md("## 4. Limits — the certified envelope")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, not "
        "a new finding. Every one is asserted by "
        "`src/doppler/track/tests/test_validation_limits.py`, on the rows "
        "of §2 that the harness's `--check` run produced."
    )
    R.md()
    R.md(
        "The last three columns are the evidence that a claim can fail. "
        "*Checked by* names the block of the harness's `--check` run "
        "(`native/validation/timing_est_noise.c`, the run `ctest` also "
        "makes). *Control* is the named wrong case the harness evaluates "
        "the same assertion on and requires to fail. *Sabotage* is a "
        "one-line break of `native/src/timing_est/timing_est_core.c` (or "
        "the header's inline accumulate), built and run against that "
        "`--check`, and what went red; every row that names one was "
        "observed red, and each break was reverted. They are recorded by "
        "hand: whether a record like this is generated is "
        "[#847](https://github.com/doppler-dsp/doppler/issues/847)."
    )
    R.md()
    R.md(
        "The limits marked *pin* are numbers set from a measurement, not "
        "claims the page makes, so a change that moves one is for a "
        "person to judge, not necessarily a defect."
    )
    R.md()

    n = d.noise
    R.limit(
        all(within(r["err_mean"], r["bias_pred"], r["err_se"]) for r in n),
        "under noise the mean timing error is the bias the pulse and the "
        "sampling predict, within four standard errors, on the rectangle "
        "at N 4 and N 3.2 (Es/N0 4 dB, 128 symbols) and the root-raised "
        "cosine (10 dB, 512 symbols)",
        by="harness `noise` block",
        control="the eye center one generator sample, 1/64 symbol, away",
        red="`tau`'s sign flipped → all three rows",
    )
    R.limit(
        all(
            r["held"] == 1.0 and within(r["var"], r["var2"], r["var_se"])
            for r in n
        ),
        "its variance is the one computed from the pulse (`k_ss`, `k_sn`, "
        "`k_nn`), within four standard errors, on those rows",
        by="harness `noise` block",
        control="the same rows predicted at 1 dB more Es/N0 than was sent",
        red="`|x|` accumulated in place of `|x|^2` → all three rows",
    )
    R.limit(
        all(
            within(r["c2"], r["c2_pred"], r["c2_se"])
            and within(r["power"], r["power_pred"], r["power_se"])
            for r in n
        ),
        "the line's energy `E|c|^2` and the mean power are the pulse's on "
        "those rows",
        by="harness `noise` block",
        control="the line a clock offset of a quarter cycle a block would "
        "leave; the power at 1 dB more Es/N0",
        red="`quality` × 0.9 → the line's energy; `power` / 1.05 → both",
    )
    R.limit(
        rms(n[0]) < 0.025,
        f"*pin*: the rectangle at N 4, 128 symbols, Es/N0 4 dB: "
        f"{rms(n[0]):.4f} symbols rms, under 0.025",
        by="harness `noise` block, first row",
    )
    c = d.clock
    inside = [r for r in c if abs(r["x"]) < 0.5]
    past = [r for r in c if abs(r["x"]) > 0.5]
    R.limit(
        all(within(r["rate"], r["rate_pred"], r["rate_se"]) for r in inside)
        and all(
            not within(r["rate"], r["rate_true"], r["rate_se"]) for r in past
        ),
        "inside half a cycle of the line a block the rate from two blocks "
        "is the predicted one (a quarter cycle: rectangle from 128 "
        "symbols, root-raised cosine from 512), and at three quarters it "
        "is not the offset sent",
        by="harness `clock` block",
        control="the row at three quarters of a cycle, held to the offset "
        "sent",
        red="the rate's divisor × 1.5 → both rows inside the bin",
    )
    R.limit(
        all(
            within(r["c2"], r["c2_pred"], r["c2_se"])
            and within(r["err_mean"], r["bias_pred"], r["err_se"])
            for r in inside
        ),
        "under that offset the line's energy and the mean error at the "
        "block's middle are the predicted ones",
        by="harness `clock` block",
        control="for the rectangle, the page's `sinc^2` on the line the "
        "block's own sampling phase would have; for the root-raised "
        "cosine, a line the offset costs nothing; the eye center one "
        "generator sample away",
        red="the clock's step off by half a cycle a block → the line's "
        "energy on both rows and the mean on the rectangle's",
    )
    rrc_clock = [r for r in inside if r["pulse"] == "rrc"]
    R.limit(
        bool(rrc_clock)
        and all(within(r["c2"], r["c2_page"], r["c2_se"]) for r in rrc_clock),
        "on the band-limited pulse the line's energy under a clock offset "
        "is the design page's `sinc(x)^2` of the line with no offset",
        by="harness `clock` block, root-raised cosine row",
        control="a line the offset costs nothing",
        red="`quality` × 0.9 → this row",
    )
    R.limit(
        all(
            r["held"] == 1.0
            and abs(r["rate_var"] - r["rate_var_pred"])
            <= Z * r["rate_var_se"] + 4.0 / r["L"] * r["rate_var_pred"]
            for r in inside
        ),
        "the rate's variance is the two blocks' timing variances over "
        "`L^2`, within four standard errors and the `4 / L` allowed for "
        "the symbols the blocks share",
        by="harness `clock` block",
        control="a quarter of that variance (blocks twice as long)",
        red="the rate's divisor × 1.5 → both rows",
    )
    both = [r for r in d.dop if r["kind"] == "both"]
    alone = [r for r in d.dop if r["kind"] == "carrier"]
    R.limit(
        all(within(r["rate"], r["rate_pred"], r["rate_se"]) for r in d.dop)
        and all(
            not within(a["rate"], b["rate_pred"], a["rate_se"])
            for a in alone
            for b in both
        ),
        "with carrier and time base dilated together by `doppler_channel` "
        "(20 ppm, the carrier a tenth of the symbol rate off) the rate is "
        "the predicted one, and the carrier alone reads none",
        by="harness `doppler` block",
        control="the carrier alone, held to the rate the dilation gives",
        red="`tau`'s sign flipped → the row with both",
    )
    R.limit(
        all(
            within(r["err_mean"], r["bias_pred"], r["err_se"])
            and r["held"] == 1.0
            and within(r["var"], r["var2"], r["var_se"])
            and within(r["c2"], r["c2_pred"], r["c2_se"])
            for r in d.dop
        ),
        "a carrier a tenth of the symbol rate off, from the channel or "
        "from the generator, leaves the mean, the variance and the line's "
        "energy at what the pulse gives at that carrier",
        by="harness `doppler` block",
        control="the eye center one generator sample away; half the variance",
        red="a constant added to the correlation kernel → every column "
        "of both rows",
    )
    q = d.chirp
    R.limit(
        all(
            within(r["c2"], r["c2_pred"], r["c2_se"])
            and within(r["err_mean"], r["err_pred"], r["err_se"])
            and not within(r["c2"], r["c2_flat"], r["c2_se"])
            for r in q
        ),
        "under a Doppler rate (`q` = 2) the line's energy and the "
        "timing's shift are the predicted ones, and the line is not the "
        "one of a block the rate does not touch",
        by="harness `doppler_rate` block",
        control="a block the Doppler rate does not touch",
        red="the clock's step off by half a cycle a block → both columns",
    )
    h = d.h0
    R.limit(
        all(r["lo"] <= r["pfa"] <= r["ray_hi"] for r in h)
        and all(r["white_hi"] < r["pfa"] for r in h),
        "noise alone: the threshold that allows for the power's spread "
        "gives the false-alarm target or less and the Rayleigh threshold "
        "the target or more (Wilson 95%), at a target of 0.01 from 512 "
        "samples",
        by="harness `quality_h0` block",
        control="the threshold for ten times the target; the threshold "
        "samples taken as independent call for",
        red="a constant added to the correlation kernel → the first; "
        "`quality` × 0.9 → the second",
    )
    R.limit(
        all(within(r["q_mean"], r["q_pred"], r["q_se"]) for r in h),
        "noise alone: the mean quality is the Rayleigh mean computed from "
        "the matched filter's response",
        by="harness `quality_h0` block",
        control="the mean samples taken as independent would give",
        red="`quality` × 0.9 → this row",
    )
    data0 = [r for r in d.h1 if r["symbols"] == "data" and r["esn0_db"] == 0.0]
    data10 = [
        r for r in d.h1 if r["symbols"] == "data" and r["esn0_db"] == 10.0
    ]
    const = [r for r in d.h1 if r["symbols"] == "constant"]
    R.limit(
        bool(data0)
        and all(
            r["lo"] - PD_ALLOW <= r["pd_pred"] <= r["hi"] + PD_ALLOW
            for r in data0
        ),
        "the detection probability at that threshold is the normal form's "
        "within 0.03 beyond its interval (rectangle, 128 symbols, Es/N0 "
        "0 dB)",
        by="harness `quality_h1` block",
        control="noise alone at the same threshold",
        red="the clock's step off by half a cycle a block → this row",
    )
    R.limit(
        bool(data0)
        and bool(data10)
        and data0[0]["lo"] > 0.55
        and data10[0]["lo"] > 0.99,
        f"*pin*: the rectangle from 128 symbols is detected "
        f"{data0[0]['pd']:.2f} of the time at Es/N0 0 dB (interval above "
        f"0.55) and {data10[0]['pd']:.3f} at 10 dB (above 0.99)",
        by="harness `quality_h1` block",
    )
    R.limit(
        bool(const)
        and all(r["lo"] <= PFA for r in const)
        and all(r["lo"] > PFA for r in data10),
        "a stream with no transitions crosses the threshold no more often "
        "than the false-alarm target, where the same stream with its "
        "transitions is detected",
        by="harness `quality_h1` block",
        control="the same stream with its transitions",
        red="a constant added to the correlation kernel → this row",
    )
    o = d.off
    R.limit(
        all(
            r["z_max"] <= Z
            and r["chi2"] <= r["bound"]
            and r["chi2_fixed"] > r["bound"]
            for r in o
        ),
        "over 64 starting offsets across a symbol the mean error is the "
        "predicted bias at each one (largest normalized error under 4, "
        "their mean square under its bound)",
        by="harness `offset_summary` block",
        control="the eye center taken as offset 0's at every offset",
        red="`tau`'s sign flipped → this row",
    )
    R.limit(
        all(
            max(abs(r["bias_lo"]), abs(r["bias_hi"])) < 0.012
            for r in o
            if r["pulse"] == "rect" and r["sps"] == 4.0
        ),
        "*pin*: the rectangle's sampling-phase bias at N 4 is under 0.012 "
        "symbols at every starting offset",
        by="harness `offset_summary` block",
    )


# ── plots ────────────────────────────────────────────────────────────
def plots(d: SimpleNamespace) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    colour = {"rect": "C0", "rrc": "C1", "rect_point": "C3"}

    # noise: rms against Es/N0 at L 128, and against L at 4 and 10 dB
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(13, 4.8))
    for pulse in ("rect", "rrc"):
        rows = [
            r
            for r in d.rec_noise
            if r["pulse"] == pulse and r["sps"] == 4.0 and r["L"] == 128
        ]
        rows.sort(key=lambda r: r["esn0_db"])
        a1.plot(
            [r["esn0_db"] for r in rows],
            [rms(r) for r in rows],
            "o",
            color=colour[pulse],
            label=f"{pulse}, measured",
        )
        a1.plot(
            [r["esn0_db"] for r in rows],
            [rms(r, "var2") for r in rows],
            "x",
            color=colour[pulse],
            label=f"{pulse}, predicted",
        )
        for esn0, style in ((4.0, "-"), (10.0, "--")):
            rows = [
                r
                for r in d.rec_noise
                if r["pulse"] == pulse
                and r["sps"] == 4.0
                and r["esn0_db"] == esn0
            ]
            by_l: dict[float, list] = {}
            for r in rows:
                by_l.setdefault(r["L"], []).append(r)
            ls = sorted(by_l)
            a2.plot(
                ls,
                [
                    math.sqrt(sum(r["var"] for r in by_l[k]) / len(by_l[k]))
                    for k in ls
                ],
                "o" + style,
                color=colour[pulse],
                label=f"{pulse}, {esn0:.0f} dB, measured",
            )
            a2.plot(
                ls,
                [
                    math.sqrt(sum(r["var2"] for r in by_l[k]) / len(by_l[k]))
                    for k in ls
                ],
                "x",
                color="k",
            )
    a1.set_yscale("log")
    a1.set_xlabel("Es/N0 (dB)")
    a1.set_ylabel("rms timing error (symbols)")
    a1.set_title("N 4, L 128: one point a row, each at its own offset")
    a1.grid(True, which="both", alpha=0.3)
    a1.legend(fontsize=8)
    a2.set_xscale("log", base=2)
    a2.set_yscale("log")
    a2.set_xlabel("block length L (symbols)")
    a2.set_ylabel("rms timing error (symbols)")
    a2.set_title("N 4: against L (black crosses: predicted)")
    a2.grid(True, which="both", alpha=0.3)
    a2.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "noise.png", dpi=110)
    plt.close(fig)

    # clock: cycles a block read against x, and the line's energy
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(13, 4.8))
    for pulse, sps, length in (("rect", 4.0, 128), ("rrc", 4.0, 512)):
        rows = [
            r
            for r in d.rec_clock
            if r["pulse"] == pulse and r["sps"] == sps and r["L"] == length
        ]
        rows.sort(key=lambda r: r["x"])
        a1.errorbar(
            [r["x"] for r in rows],
            [r["rate"] * r["L"] for r in rows],
            yerr=[r["rate_se"] * r["L"] for r in rows],
            fmt="o",
            color=colour[pulse],
            label=f"{pulse}, L {length}",
        )
        a2.plot(
            [r["x"] for r in rows],
            [r["c2"] / r["c2_flat"] for r in rows],
            "o",
            color=colour[pulse],
            label=f"{pulse}, measured",
        )
        a2.plot(
            [r["x"] for r in rows],
            [r["c2_pred"] / r["c2_flat"] for r in rows],
            "x",
            color=colour[pulse],
            label=f"{pulse}, predicted",
        )
    xs = [i / 100.0 for i in range(-130, 131)]
    a1.plot(xs, xs, ":", color="gray", label="the offset sent")
    a1.plot(
        xs,
        [x - round(x) for x in xs],
        "-",
        color="gray",
        lw=0.8,
        label="x - round(x)",
    )
    a1.set_xlabel("x: cycles of the line a block")
    a1.set_ylabel("rate read, cycles a block")
    a1.set_title("the rate read")
    a1.grid(True, alpha=0.3)
    a1.legend(fontsize=8)
    a2.plot(
        xs,
        [
            1.0 if x == 0 else (math.sin(math.pi * x) / (math.pi * x)) ** 2
            for x in xs
        ],
        "-",
        color="gray",
        lw=0.8,
        label="sinc^2",
    )
    a2.set_xlabel("x: cycles of the line a block")
    a2.set_ylabel("line energy over the same block with no offset")
    a2.set_title("the line's energy")
    a2.grid(True, alpha=0.3)
    a2.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "clock.png", dpi=110)
    plt.close(fig)

    # detection
    fig, ax = plt.subplots(figsize=(9, 4.8))
    for pulse, length in (("rect", 128), ("rrc", 512)):
        rows = [
            r
            for r in d.rec_h1
            if r["pulse"] == pulse
            and r["L"] == length
            and r["symbols"] == "data"
        ]
        rows.sort(key=lambda r: r["esn0_db"])
        ax.errorbar(
            [r["esn0_db"] for r in rows],
            [r["pd"] for r in rows],
            yerr=[
                [r["pd"] - r["lo"] for r in rows],
                [r["hi"] - r["pd"] for r in rows],
            ],
            fmt="o",
            color=colour[pulse],
            capsize=3,
            label=f"{pulse}, L {length}, measured (Wilson 95%)",
        )
        ax.plot(
            [r["esn0_db"] for r in rows],
            [r["pd_pred"] for r in rows],
            "x--",
            color=colour[pulse],
            label=f"{pulse}, normal form",
        )
    ax.set_xlabel("Es/N0 (dB)")
    ax.set_ylabel("P(quality over the 0.01 false-alarm threshold)")
    ax.set_title("detection at the conservative threshold")
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(HERE / "detection.png", dpi=110)
    plt.close(fig)

    # offsets
    off = record("offset")
    fig, axes = plt.subplots(2, 2, figsize=(13, 8), sharex=True)
    panels = (
        ("rect", 4.0, "rectangle, N 4"),
        ("rect", 3.2, "rectangle, N 3.2"),
        ("rrc", 4.0, "root-raised cosine, N 4"),
        ("rect_point", 4.0, "rectangle merely sampled, N 4"),
    )
    for ax, (pulse, sps, title) in zip(axes.flat, panels):
        rows = [
            r for r in off if r["pulse"] == pulse and round(r["sps"], 3) == sps
        ]
        rows.sort(key=lambda r: r["offset"])
        ax.errorbar(
            [r["offset"] for r in rows],
            [r["err_mean"] for r in rows],
            yerr=[r["err_se"] for r in rows],
            fmt="o",
            ms=3,
            color=colour[pulse],
            label="mean error ± se",
        )
        ax.plot(
            [r["offset"] for r in rows],
            [r["bias_pred"] for r in rows],
            "-",
            color="k",
            lw=0.8,
            label="predicted bias",
        )
        ax.set_title(title)
        ax.set_ylabel("symbols")
        ax.grid(True, alpha=0.3)
        ax.legend(fontsize=8)
    for ax in axes[1]:
        ax.set_xlabel("starting offset (1/64 symbol)")
    fig.tight_layout()
    fig.savefig(HERE / "offset.png", dpi=110)
    plt.close(fig)


def build(write: bool = True) -> Report:
    """Measure through the harness, review and assert; emit when asked."""
    global R
    R = Report(write=write)
    R.md("# SymbolTimingEstimator — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    n = d.noise
    R.executive(
        "SymbolTimingEstimator",
        source=(
            "Generated by `validate.py` in this folder. Every number is "
            "the C harness's, `native/validation/timing_est_noise.c`: the "
            "gated ones from its `--check --emit` run (the regression "
            "subset `ctest` also makes), which this file runs and "
            "renders, and the ones marked as the record from its full "
            "sweep (`--csv`), refreshed on purpose and not part of the "
            "gate. Nothing is modelled. Re-run to regenerate."
        ),
        takeaways=[
            "**The error is the one the pulse predicts, at any starting "
            "offset, with a carrier offset on every row.** Rectangle, 4 "
            f"samples a symbol, 128 symbols, Es/N0 4 dB: {rms(n[0]):.3f} "
            f"symbols rms against {rms(n[0], 'var2'):.3f} computed; the "
            "three constants for each pulse and rate are in §2.1.",
            "**One rate bin is enough for blocks of hundreds of "
            "symbols.** The rate is read out to about 0.4 cycles of the "
            f"line a block, {d.edge[128]:.0f} ppm at 128 symbols and "
            f"{d.edge[512]:.0f} ppm at 512; past half a cycle it aliases "
            "with the wrong sign "
            f"({R.ref('one_rate_bin_holds_to_half_cycle')}, §2.2).",
            "**On a rectangle at a whole number of samples a symbol the "
            "timing carries a sampling-phase bias** of up to "
            f"{d.rect4['bias_hi']:.3f} symbols at 4, and a slow drift is "
            "read with a gain of "
            f"{min(d.rect_gain):.2f} to {max(d.rect_gain):.2f}. A rate "
            "that is not whole, or a band-limited pulse, does not have "
            f"it ({R.ref('whole_n_rect_sampling_phase')}, §2.7).",
            "**Doppler costs nothing it should not.** A carrier a tenth "
            "of the symbol rate off changes no statistic beyond the "
            "matched filter's, the dilation reads as rate, and a "
            "physical Doppler rate is far under what costs the line "
            "anything (§2.3, §2.4).",
            "**A threshold on `quality` works as a presence test.** At "
            "the conservative threshold for a 0.01 false-alarm "
            "probability the rectangle is detected "
            f"{next(r for r in d.h1 if r['symbols'] == 'data')['pd']:.2f} "
            "of the time at 0 dB from 128 symbols and over 0.99 at 10 "
            "dB, and a stream with no transitions is refused "
            f"({R.ref('pfa_threshold_is_a_bracket')}, §2.5, §2.6).",
            "**The root-raised cosine needs about four times the "
            "symbols** for the same error: its line is a third the "
            f"rectangle's ({R.ref('rrc_line_is_weak')}, §2.1).",
        ],
    )
    if write:
        plots(d)
    R.summary(
        "\n- Raw sweeps (written by `native/validation/"
        "timing_est_noise.c --csv`): `data/noise.csv`, "
        "`data/constants.csv`, `data/clock.csv`, `data/doppler.csv`, "
        "`data/doppler_rate.csv`, `data/quality_h0.csv`, "
        "`data/quality_h1.csv`, `data/offset.csv`, "
        "`data/offset_summary.csv`"
    )
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
