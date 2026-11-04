"""Run each sabotage of acq_core.c against test_acq_core, one at a time.

The sabotages of record §18.5, from a checkout at the step 2.2 commit:
``python3 docs/design/acq-surface-measurements/equiv-2825/sabotage.py
[name ...]``; its output is ``sabotage.log`` beside it.

Each edit must match; the file is restored from git after each run. Prints,
per sabotage, the test verdict, the failing equivalence scenes and the
carrier and rate scenes' lines.
"""

import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
SRC = ROOT / "native" / "src" / "acq" / "acq_core.c"
D_EXPR = "(double)signed_r * st->fs / st->carrier_freq_hz;"

SABOTAGES = [
    (
        "ramp sign flipped",
        "const double w = -2.0 * M_PI * s / (double)nx;",
        "const double w = +2.0 * M_PI * s / (double)nx;",
    ),
    (
        "no ramp",
        "acq_ramp (prod, nx, w, 1, prod); /* over the signed bins */",
        "(void)w; /* sabotage: no ramp */",
    ),
    (
        "ramp x1.5",
        "const double d = " + D_EXPR,
        "const double d = 1.5 * " + D_EXPR,
    ),
    (
        "ramp tile |sq|",
        "const double d = " + D_EXPR,
        "const double d = (double)labs (signed_r)"
        " * st->fs / st->carrier_freq_hz;",
    ),
    (
        "ramp tile unsigned",
        "const double d = " + D_EXPR,
        "const double d = (double)r * st->fs / st->carrier_freq_hz;",
    ),
    (
        "de-chirp conjugated",
        "= (float _Complex)dp_acq_rate_phasor (a, t);",
        "= conjf ((float _Complex)dp_acq_rate_phasor (a, t));",
    ),
    (
        "slow-time pad misplaced",
        "st->colbuf[i] = frame[i * nx + j]",
        "st->colbuf[i + ny_f - ny] = frame[i * nx + j]",
    ),
    (
        "row map: slow row unsigned",
        "long m = dp_fftfreq_index (r, tiles) * (long)dI"
        " + dp_fftfreq_index (i, dI);",
        "long m = dp_fftfreq_index (r, tiles) * (long)dI + (long)i;",
    ),
]


def run(name: str, old: str, new: str) -> None:
    """Apply one sabotage, run test_acq_core, restore, print the verdict."""
    text = SRC.read_text(encoding="utf-8")
    sites = text.count(old)
    if sites == 0:
        print(f"== {name}: NO MATCH")
        return
    # The pad sabotage edits both colbuf writes (with and without a chirp).
    SRC.write_text(text.replace(old, new), encoding="utf-8")
    out = subprocess.run(
        ["make", "test", "CTEST_SELECT=-V -R ^test_acq_core$"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    ).stdout
    subprocess.run(
        ["git", "checkout", "--", "native/src/acq/acq_core.c"],
        cwd=ROOT,
        check=True,
    )
    caught = "tests passed, 0 tests failed" not in out
    verdict = "RED" if caught else "PASSED (not caught)"
    eq_fail = sorted(set(re.findall(r"^135: FAIL: (.+)$", out, re.M)))
    other = len(re.findall(r"^135: FAIL /", out, re.M))
    print(
        f"== {name} ({sites} site(s)): {verdict}; equivalence fails: "
        f"{eq_fail}; other checks failing: {other}"
    )
    for line in out.splitlines():
        if line.startswith("135:   map equivalence") and any(
            k in line for k in ("MOVED", "HIT-OFF", "carrier", "rates")
        ):
            print("   ", line[5:].strip())


def main() -> None:
    """Run the named sabotages, or all of them."""
    only = sys.argv[1:]
    for name, old, new in SABOTAGES:
        if not only or name in only:
            run(name, old, new)


if __name__ == "__main__":
    main()
