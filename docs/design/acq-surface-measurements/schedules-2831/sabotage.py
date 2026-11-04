"""Sabotage the block map in corr2d_core.c; each must turn the test red.

Record §18.6 (#2831): run from a checkout at the step 2.3 Corr2D commit,
``python3 docs/design/acq-surface-measurements/schedules-2831/sabotage.py``.
The source is restored from the text it saved and rebuilt at the end.
"""

import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[4]
SRC = ROOT / "native" / "src" / "corr2d" / "corr2d_core.c"

SABOTAGES = [
    (
        "shared: offset sign in the table",
        "off[j]        = (int32_t)(((r % fw) + fw) % fw);",
        "off[j]        = (int32_t)(((-r % fw) + fw) % fw);",
    ),
    (
        "both: ramp phase sign in the table",
        "cexp (2.0 * M_PI * I * mid * (double)r",
        "cexp (-2.0 * M_PI * I * mid * (double)r",
    ),
    (
        "both: ramp origin at epoch 0",
        "const double mid = 0.5 * (double)(g->d - 1);",
        "const double mid = 0.0;",
    ),
    (
        "slow first: slow-time pad not zero",
        "s->col  = calloc (DD_SLOW_TILE * m->fdi, sizeof *s->col);",
        "s->col  = calloc (DD_SLOW_TILE * m->fdi, sizeof *s->col);"
        " if (s->col) for (size_t z = 0; z < DD_SLOW_TILE * m->fdi; z++)"
        " s->col[z] = 1.0f;",
    ),
    (
        "slow first: row table shifted",
        "row1 ? row1 : out + tl->rows[i] * L);",
        "row1 ? row1 : out + tl->rows[(i + 1) % m->g.di] * L);",
    ),
    (
        "the rule: a tie to rows first",
        "return g->di <= 1 || rows < slow * (1.0 - DD_COUNT_RESOLUTION)",
        "return g->di <= 1 || rows < slow * (1.0 + DD_COUNT_RESOLUTION)",
    ),
    (
        "the rule: no resolution band",
        "rows < slow * (1.0 - DD_COUNT_RESOLUTION)",
        "rows < slow",
    ),
    (
        "the rule: fused ignored",
        "const double rows = (fused ? 0.0 : W * D * dd_tcost (g->l))",
        "const double rows = (W * D * dd_tcost (g->l))",
    ),
    (
        "refusal: a fine grid at one slow row accepted",
        "|| (g->di == 1 && g->f > 1) /* one slow row has no fine grid */",
        "|| 0 /* sabotage */",
    ),
    (
        "rows first: ramp phase sign",
        "pr[j] = v * m->tw[(k * (size_t)tl->off[j]) % fdi];",
        "pr[j] = v * conjf (m->tw[(k * (size_t)tl->off[j]) % fdi]);",
    ),
    (
        "rows first: column pad not zero",
        "s->colr  = calloc (DD_SLOW_TILE * g->di, sizeof *s->colr);",
        "s->colr  = calloc (DD_SLOW_TILE * g->di, sizeof *s->colr);"
        " if (s->colr) for (size_t z = 0; z < DD_SLOW_TILE * g->di; z++)"
        " s->colr[z] = 1.0f;",
    ),
    (
        "rows first: column scatter shifted",
        "float _Complex *dst = c->out + rows[i] * L + l0;",
        "float _Complex *dst = c->out + rows[(i + 1) % DI] * L + l0;",
    ),
    (
        "rows first: weight dropped",
        "pr[j] = xa[j] * tl->wt[j];",
        "pr[j] = xa[j] * m->unit;",
    ),
]

for name, old, new in SABOTAGES:
    text = SRC.read_text(encoding="utf-8")
    n = text.count(old)
    if n != 1:
        print(f"== {name}: {n} matches")
        continue
    SRC.write_text(text.replace(old, new), encoding="utf-8")
    out = subprocess.run(
        ["make", "test", "CTEST_SELECT=-V -R ^test_corr2d_core$"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    ).stdout
    # Restore from the saved text, never from git: the map is uncommitted.
    SRC.write_text(text, encoding="utf-8")
    red = "tests passed, 0 tests failed" not in out
    fails = re.findall(r"^74: FAIL.*$", out, re.M)
    print(
        f"== {name}: {'RED' if red else 'GREEN (not caught)'}; "
        f"{len(fails)} failing check(s)"
    )
    for line in fails[:3]:
        print("   ", line[4:])

# Rebuild the restored source: the last sabotage's binaries must not outlive
# it (a stale build read as a real defect once).
subprocess.run(["make", "build"], cwd=ROOT, capture_output=True, check=False)
