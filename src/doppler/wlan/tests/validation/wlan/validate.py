"""Wlan -- certification evidence: the pins, transmit quality, the binding.

Run directly to regenerate `results.md` and `data/*.csv`:

    python src/doppler/wlan/tests/validation/wlan/validate.py

`--check` re-renders in memory and compares with the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit is
asserted by `src/doppler/wlan/tests/test_validation_limits.py`, which runs
this same `build(write=False)`.

The order is the campaign's (`docs/dev/contributing/validation.md`):
`native/inc/doppler/wlan/wlan_core.h` is the SSOT, its claims are pinned in
`native/tests/wlan/test_wlan_core.c` (10e and 10f hold what the inventory
found unasserted), and `native/validation/wlan_tx_quality.c` measures what
the design page leaves open (wlan.md section 7, its fourth proof, and
section 8): the transmit mask, the error a window adds, flatness and PAPR,
against window and oversampling.
Its `--check --emit` subset is this report's live measurement, the same run
ctest makes; the full sweep is regenerated only when this file writes.
"""

from __future__ import annotations

import csv
import sys
from pathlib import Path

import numpy as np

from doppler.tests._repo import build_dir, exe, repo_root
from doppler.tests._validation_common import Report, cli, harness_blocks
from doppler.wlan import Wlan

HERE = Path(__file__).resolve().parent
ROOT = repo_root(__file__)
HARNESS = exe(
    build_dir(__file__) / "native/validation/validate_wlan_tx_quality"
)
DATA = HERE / "data"
BLOCKS = ("evm", "mask", "flatness", "papr")
MASK_DB = (-20.0, -28.0, -40.0)

R = Report()


def _num(v: str) -> float | str:
    try:
        return float(v)
    except ValueError:
        return v


def _sweep(write: bool) -> dict[str, list[dict]]:
    """The full sweep: run and saved when writing, read back otherwise."""
    if write:
        blocks = harness_blocks(HARNESS, "wlan", ROOT)
        DATA.mkdir(exist_ok=True)
        for name in BLOCKS:
            rows = blocks[name]
            with (DATA / f"{name}.csv").open(
                "w", newline="", encoding="utf-8"
            ) as f:
                w = csv.DictWriter(
                    f, fieldnames=list(rows[0]), lineterminator="\n"
                )
                w.writeheader()
                w.writerows(rows)
        return {name: blocks[name] for name in BLOCKS}
    out = {}
    for name in BLOCKS:
        with (DATA / f"{name}.csv").open(encoding="utf-8") as f:
            out[name] = [
                {k: _num(v) for k, v in r.items()} for r in csv.DictReader(f)
            ]
    return out


def _binding_power() -> list[tuple[int, float]]:
    """The L-LTF's long symbols' mean power through the Python face.

    The unit-power convention (#3148) at M = 1 and 4: the 128 M samples of
    the two long symbols, no window. Exact in real arithmetic; the C pin
    (test_wlan_core.c section 10) derives the 1e-4 tolerance.
    """
    psdu = np.arange(100, dtype=np.uint8)
    out = []
    for m in (1, 4):
        x = Wlan(
            constellation="qam16", code_rate="r34", window=0, oversample=m
        ).ppdu(psdu, 93)
        ltf = x[192 * m : 320 * m].astype(np.complex128)
        out.append((m, float(np.mean(np.abs(ltf) ** 2))))
    return out


def _db(v: float) -> str:
    return "exact 0" if v == float("-inf") else f"{v:.1f}"


def characterise(live, sweep, power) -> None:
    R.md("## 2. Characterisation")
    R.md()
    R.md("### 2.1 The error a window adds to a symbol body (C 10e, 10f)")
    R.md()
    R.md(
        "Each SIGNAL and Data symbol's 64 M-sample transform, starting "
        "`start` samples into the symbol, against the same transform of the "
        "unwindowed render at the same timing, on the 52 occupied "
        "subcarriers; 54 Mb/s, M = 4, w and start in units of M. The window "
        "weights only a symbol's first w samples (its own rise, the previous "
        "symbol's fall), so for w <= start the body is the same samples and "
        "the error is exactly zero. Above it the expectation, for "
        "independent unit-power symbols, is "
        "`EVM^2 = 2 sum_{j=start}^{w-1} fall_j^2 / (64 M)`, "
        "`fall_j = cos^2 (pi (j + 1/2) / (2 w))`, and the measured "
        "occupied-subcarrier error can never exceed the time-domain error "
        "ratio (Parseval: the unwindowed body has all its energy on the "
        "occupied subcarriers). Start 16 is a receiver taking the last 64 M "
        "samples; start 8 is one sampling eight 20 MHz samples early. The "
        "rows are `data/evm.csv`."
    )
    R.md()
    R.table(
        ["start", "w", "EVM dB", "time-domain dB", "expectation dB"],
        [
            [
                f"{int(r['start_m'])} M",
                f"{int(r['window_m'])} M",
                _db(r["evm_db"]),
                _db(r["parseval_db"]),
                _db(r["expect_db"]),
            ]
            for r in sweep["evm"]
        ],
    )
    R.md()
    R.md(
        "802.11's relative constellation error limit at 54 Mb/s is -25 dB "
        "(Table 17-19): the window alone uses it up near w = 24 M for the "
        "late receiver and w = 12 M for the early one "
        f"({R.ref('window_cost_by_rx_timing')})."
    )
    R.md()
    R.md("### 2.2 The transmit spectral mask (17.3.9.3)")
    R.md()
    R.md(
        "A Welch average of periodic-Hann frames of 512 M samples, hop "
        "256 M, zero-padded 8 times, taken only inside one PPDU's SIGNAL and "
        "Data samples (a frame across two PPDUs would carry an uncrossfaded "
        "cut) or, in the `ppdu` rows, over the whole PPDU. The 100 kHz "
        "resolution bandwidth is realized by integrating the padded bins "
        "within 50 kHz of the frequency (97.7 kHz realized: a power-of-two "
        "transform cannot land on 100 kHz at fs = 20 M MHz), and a level is "
        "dBr against the largest such in-band sum, the larger of the two "
        "sides. The standard error of a level, for K frames of 50% "
        "overlapped Hann (overlap correlation 1/6), not crediting the "
        "integration: `se = (10 / ln 10) sqrt ((1 + 2/36) / K)`. 54 Mb/s, "
        "4095-octet PSDUs. The full sweep's rows are `data/mask.csv`."
    )
    R.md()
    R.md(
        "Predicted on paper before the run, from the expected PSD of the "
        "discrete symbol pulse, unwindowed, against the in-band peak: "
        "-24.9 / -32.4 / -35.2 dBr at M = 4 and -25.0 / -33.0 / -36.7 at "
        "M = 8, failing -40 at 30 MHz; with the standard's 2 M transition "
        "-27.5 / -55.0 / -68.9, passing."
    )
    R.md()
    R.table(
        [
            "M",
            "w",
            "span",
            "K",
            "se dB",
            "RBW Hz",
            "pk/mean dB",
            "11 MHz",
            "20 MHz",
            "30 MHz",
        ],
        [
            [
                f"{int(r['m'])}",
                f"{int(r['window_m'])} M",
                str(r["span"]),
                f"{int(r['frames'])}",
                f"{r['se_db']:.2f}",
                f"{int(r['rbw_hz'])}",
                f"{r['peak_over_mean_db']:.2f}",
                f"{r['dbr_11']:.1f}",
                f"{r['dbr_20']:.1f}",
                f"{r['dbr_30']:.1f}",
            ]
            for r in sweep["mask"]
        ],
    )
    R.md()
    R.md(
        "The mask is -20 / -28 / -40 dBr at 11 / 20 / 30 MHz. Unwindowed "
        "fails the 30 MHz point at both M, which is the property of asking "
        "for window 0; Wlan's default is the standard's 2 M "
        f"({R.ref('default_window_fails_mask')}). The in-band peak-to-mean "
        "reads above the 1.25 dB derived for this estimator "
        f"({R.ref('mask_peak_reference_biased')})."
    )
    R.md()
    R.md("The live subset this report and ctest both run (`--check --emit`):")
    R.md()
    R.table(
        ["M", "w", "K", "se dB", "11 MHz", "20 MHz", "30 MHz"],
        [
            [
                f"{int(r['m'])}",
                f"{int(r['window_m'])} M",
                f"{int(r['frames'])}",
                f"{r['se_db']:.2f}",
                f"{r['dbr_11']:.1f}",
                f"{r['dbr_20']:.1f}",
                f"{r['dbr_30']:.1f}",
            ]
            for r in live["mask"]
        ],
    )
    R.md()
    R.md("### 2.3 L-LTF spectral flatness (17.3.9.7.3)")
    R.md()
    R.md(
        "The worst deviation of an occupied subcarrier's power from their "
        "mean, over the L-LTF's second long symbol, M = 4: "
        + ", ".join(
            f"w = {int(r['window_m'])} M: {r['worst_db']:.4f} dB"
            for r in sweep["flatness"]
        )
        + ". The L-LTF's tones are all +-1 and no window up to 32 M reaches "
        "that symbol, so 0 was predicted. The rows are `data/flatness.csv`."
    )
    R.md()
    R.md("### 2.4 PAPR per Data symbol body (M = 4, w = 2 M)")
    R.md()
    R.md(
        "The fraction of bodies above each level, with a Wilson 95% "
        "interval, beside a CITED model, "
        "`P = 1 - (1 - e^-g)^(2.8 N)`, N = 52 (Ochiai and Imai's "
        "oversampled-OFDM approximation; its 2.8 is empirical and not "
        "derived here). The model overestimates at 8 and 10 dB; no gate. "
        "The rows are `data/papr.csv`."
    )
    R.md()
    R.table(
        ["Mb/s", "PAPR >", "count", "fraction", "95% interval", "model"],
        [
            [
                str(r["rate"]).split(".")[0],
                f"{int(r['gamma_db'])} dB",
                f"{int(r['over'])} / {int(r['symbols'])}",
                f"{r['rate_hat']:.4f}",
                f"[{r['lo']:.4f}, {r['hi']:.4f}]",
                f"{r['model']:.4f}",
            ]
            for r in sweep["papr"]
        ],
    )
    R.md()
    R.md("### 2.5 The unit-power convention through the binding")
    R.md()
    R.md(
        "`doppler.wlan.Wlan.ppdu`, the L-LTF's two long symbols, no window: "
        + ", ".join(f"M = {m}: {p:.6f}" for m, p in power)
        + " (#3148's convention: 1)."
    )
    R.md()


def review() -> None:
    R.md("## 3. Review")
    R.md()
    R.find(
        "wlan_inventory_pins_added",
        "FIXED",
        "Reading `wlan_core.h` against `test_wlan_core.c` found 38 claims: "
        "31 pinned (the eight Table 17-6 pairs and the refusal of the other "
        "eight, oversampling 1 to 128, `ppdu_len`, the scrambler, every "
        "`_why` sentence), 2 at literals and 5 absent. Added: the window "
        "reaches a symbol body only above 16 M (32 against 33 at M = 2, bit "
        "for bit), each argument refusal of `dp_wlan_layout` leaving the "
        "modulator untouched (six rows), and the QAM unit-power tolerance "
        "derived as 4 sigma of the rate's own symbol count instead of a "
        "fixed 0.03 that passed at 2.25 sigma on one seed.",
    )
    R.find(
        "wlan_layout_window_refused_late",
        "FIXED",
        "`dp_wlan_layout` checked its window only through the modulator: "
        "for 80 M < window <= 160 M the L-STF and L-LTF classes were added "
        "to the caller's modulator before the SIGNAL class refused. The "
        "window ceiling is one helper that `dp_wlan_params_why` and the "
        "layout's up-front guard both call, and the header says a refusal "
        "by the arguments leaves the modulator untouched (C 10f).",
    )
    R.find(
        "wlan_create_dead_rate_check",
        "FIXED",
        "`dp_wlan_create` re-checked the rate after `dp_wlan_params_why` "
        "had passed. Every in-range pair builds (C section 1) and the "
        "allocation aborts rather than return NULL, so the branch was "
        "dead, and it was the only way `dp_wlan_create_why` could have "
        "returned NULL with its reason unset. `dp_wlan_create` has no such "
        "branch.",
    )
    R.find(
        "default_window_fails_mask",
        "FIXED",
        "`Wlan` rendered with no window by default, and an oversampled "
        "PPDU then failed the transmit mask's 30 MHz point by 4.6 dB at "
        "M = 4 and 3.1 dB at M = 8 (§2.2). The default is now the "
        "standard's 2 M transition, which passes every point with 7.9 dB "
        "to spare; `window=0`, by asking, still fails by those margins, as "
        "a stated property of the rectangular choice. At M = 1 the mask "
        "beyond 10 MHz is not representable at all. wfmgen's ofdm source "
        "takes the same default: its samples equal `Wlan`'s at M = 1, 2 "
        "and 4 (`test_the_default_window_is_the_standards_100_ns` in "
        "`src/doppler/wfm/tests/test_ofdm_source.py`, and the C pin "
        "`test_the_default_window_is_the_standards_100_ns`), so the mask "
        "result carries over to it (#3190).",
    )
    R.find(
        "mask_peak_reference_biased",
        "BY DESIGN",
        "The estimator that ran (20 integrated padded Hann bins) has an "
        "expected in-band peak-to-mean of 1.25 dB, derived by convolving "
        "the symbol pulse's PSD with the Hann kernel's power response; it "
        "reads 1.58 dB at K = 368 and 1.97 dB at K = 92. The excess is the "
        "upward bias of a maximum taken over a noisy estimate, and it "
        "scales as the noise does: 0.72 dB at K = 92 and 0.33 dB at "
        "K = 368, a ratio of 2.2 against sqrt (4) = 2. The reference reads "
        "high, so every dBr level is conservative by about that much.",
    )
    R.find(
        "window_cost_by_rx_timing",
        "BY DESIGN",
        "A window is free for a receiver that takes the last 64 M samples "
        "as long as it stays inside the 16 M guard interval, and costs from "
        "the first sample past it; a receiver sampling 8 M early pays from "
        "w > 8 M. The header's sentence (above 16 M the cross-fade reaches "
        "the body) is the first case; §2.1 gives both, so a caller choosing "
        "a window above the standard's 2 M can read its cost here.",
    )
    R.find(
        "annex_i_not_transcribed",
        "GAP",
        "Annex I's stage-by-stage example (the SIGNAL and Data bits, their "
        "coded and interleaved forms, the subcarrier values and the time "
        "samples), the FCS byte order over its PSDU, and the pilot polarity "
        "against Eq. 17-25's printed values are not held: the text is not "
        "transcribed in the tree. The PPDU is held against an in-build "
        "oracle and the tables against their printed forms instead (C "
        "sections 9, 10c). #3148.",
    )
    R.md()


def limits(live, power) -> None:
    R.md("## 4. Limits")
    R.md()
    evm = live["evm"]
    R.limit(
        all(int(r["exact"]) == (r["window_m"] <= r["start_m"]) for r in evm),
        "a window adds exactly no error to a body when w <= the receiver's "
        "start, at both timings",
        by="wlan_tx_quality --check; test_wlan_core.c 10e",
        control="w = start + 1 moves the first body sample",
        red="window applied to the body: red",
    )
    R.limit(
        all(
            r["evm_db"] <= r["parseval_db"] + 1e-3
            for r in evm
            if not int(r["exact"])
        ),
        "the occupied-subcarrier error never exceeds the time-domain error "
        "ratio (Parseval)",
        by="wlan_tx_quality --check",
    )
    m0 = next(r for r in live["mask"] if int(r["window_m"]) == 0)
    m2 = next(r for r in live["mask"] if int(r["window_m"]) == 2)
    R.limit(
        m0["dbr_30"] - MASK_DB[2] >= 3 * m0["se_db"],
        "unwindowed at M = 4 is above the -40 dBr point at 30 MHz by at "
        "least three standard errors",
        by="wlan_tx_quality --check",
        control="the window-2M row",
        red="a window planted on the w = 0 render: red",
    )
    R.limit(
        all(
            MASK_DB[i] - m2[f"dbr_{f}"] >= 3 * m2["se_db"]
            for i, f in enumerate(("11", "20", "30"))
        ),
        "the standard's 2 M transition at M = 4 is under every mask point "
        "by at least three standard errors",
        by="wlan_tx_quality --check",
        control="the unwindowed row",
        red="the window silently dropped: red",
    )
    R.limit(
        all(abs(p - 1.0) < 1e-4 for _, p in power),
        "the binding's L-LTF has unit mean power at M = 1 and 4",
        by="this report, section 2.5; test_wlan_core.c sections 10, 10c",
        control="the Annex I scale 1/64 reads 52/64^2",
    )
    R.md()


def build(write: bool = True) -> Report:
    live = harness_blocks(HARNESS, "wlan", ROOT, ("--check", "--emit"))
    sweep = _sweep(write)
    power = _binding_power()

    R.md("# Wlan -- certification evidence")
    R.md()
    R.md("## 1. The object")
    R.md()
    R.md(
        "IEEE 802.11 non-HT OFDM (Clause 17): a PSDU in, the constellation "
        "points and the layout the OFDM modulator takes out, and the object "
        "`Wlan` that renders a whole PPDU at a rate, window and "
        "oversampling. Design: [docs/design/wlan.md]"
        "(../../../../../../docs/design/wlan.md). Contract: "
        "`native/inc/doppler/wlan/wlan_core.h`. Pins: "
        "`native/tests/wlan/test_wlan_core.c`. The modulator underneath has "
        "its own report (ofdm)."
    )
    R.md()
    R.md("### Claim coverage")
    R.md()
    R.md(
        "Every prose claim of the header was mapped onto the C test before "
        f"anything was measured ({R.ref('wlan_inventory_pins_added')}):"
    )
    R.md()
    R.table(
        ["claims", "where pinned", "proof"],
        [
            [
                "the standard's tables against their printed form (RATE "
                "bits, L-STF, L-LTF, encodings, 2/3 and 3/4 masks); perm2 "
                "re-derived",
                "C section 9",
                "a sign-flip and a wrong-mask control",
            ],
            [
                "the eight Table 17-6 pairs, mbps both ways, the other "
                "eight refused",
                "C section 1",
                "a 37 Mb/s control row",
            ],
            [
                "the chain stage by stage: scrambler, FCS, SIGNAL and Data "
                "bits, BCC, interleaver, mapper",
                "C sections 2-7, 11",
                "recurrences, catalogue CRC, inverses",
            ],
            [
                "the PPDU's layout, pilots, unit power, oversampling "
                "x_M[Mn] = x_1[n], and window",
                "C sections 8, 10, 10b, 10c, 10e",
                "naive DFT; half-sample shift control",
            ],
            [
                "the object's ranges and every `_why` sentence; layout "
                "refusals leave the modulator untouched",
                "C sections 10d, 10f",
                "minimal pair per refusal",
            ],
            [
                "Annex I stage by stage; pilot polarity in print",
                "not held",
                f"{R.ref('annex_i_not_transcribed')}",
            ],
        ],
    )
    R.md()

    characterise(live, sweep, power)
    review()
    limits(live, power)

    R.executive(
        "Wlan",
        source=(
            "Generated by `validate.py` in this folder. Sections 2.1 to 2.4 "
            "are measured in C by `native/validation/wlan_tx_quality.c`, "
            "section 2.5 through the binding. The predictions and the PAPR "
            "model printed beside the measurements are labelled as such. "
            "Re-run to regenerate."
        ),
        takeaways=[
            "**`Wlan`'s default window is the standard's 2 M and passes the "
            "transmit mask** by 7.9 dB or more; `window=0`, by asking, reads "
            "-35.4 dBr at 30 MHz against -40 at M = 4 "
            f"({R.ref('default_window_fails_mask')}; wfmgen's ofdm source "
            "takes the same default).",
            "**A window is free up to the 16 M guard interval** for a "
            "receiver taking the last 64 M samples, exactly; past it the "
            "error grows to -25 dB near 24 M, and an early receiver pays "
            f"from 8 M ({R.ref('window_cost_by_rx_timing')}, §2.1).",
            "**Every header claim is pinned in C beside a control**, and a "
            "layout refused by its arguments leaves the modulator "
            f"untouched ({R.ref('wlan_layout_window_refused_late')}).",
            "**Conformance stops at the printed tables**: Annex I's worked "
            "PPDU is not transcribed "
            f"({R.ref('annex_i_not_transcribed')}).",
        ],
    )
    R.summary(
        "\n- The runs behind §2 are `native/validation/wlan_tx_quality.c` "
        "(`--check --emit` live, the full sweep in `data/`); the pins are "
        "`native/tests/wlan/test_wlan_core.c`."
    )
    if write:
        R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
