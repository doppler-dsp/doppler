"""Sabotage the one map and its callers; each must turn its gate red.

Record §18.6 and §18.7 (#2831): run from a checkout at the step 2.3
commits, ``python3 docs/design/acq-surface-measurements/schedules-2831/
sabotage.py``. Each source is restored from the text it saved, and the
tree is rebuilt at the end.

A case is ``(name, old, new)`` for the map in corr2d_core.c, gated by
test_corr2d_core, or ``(name, old, new, file, gate)``: ``gate`` is
``ctest:<test>`` or ``snippets:<filter>``, the header ``@code`` gate.
"""

import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[4]
CORR2D = "native/src/corr2d/corr2d_core.c"
ACQ = "native/src/acq/acq_core.c"
BC = "native/src/burst_capture/burst_capture_core.c"
BUF = "native/inc/doppler/buffer/buffer.h"

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
    # §18.7: the carrier's failure paths and the position a blob records.
    (
        "carrier: set_carrier writes the field before the regrid",
        "  if (carrier_freq_hz == state->carrier_freq_hz)\n"
        "    return DP_OK;\n",
        "  if (carrier_freq_hz == state->carrier_freq_hz)\n    return DP_OK;\n"
        "  state->carrier_freq_hz = carrier_freq_hz;\n",
        ACQ,
        "ctest:test_acq_regrid_failure",
    ),
    (
        "carrier: the regrid commits it before its builds",
        "  const size_t new_f    = acq_map_f (new_carrier, new_db, "
        "new_freq_bins);\n",
        "  const size_t new_f    = acq_map_f (new_carrier, new_db, "
        "new_freq_bins);\n  st->carrier_freq_hz = new_carrier;\n",
        ACQ,
        "ctest:test_acq_regrid_failure",
    ),
    (
        "carrier: set_state restores the framer before its rebuild",
        "  if (ex.carrier_freq_hz != st->carrier_freq_hz\n",
        "  (void)dp_f32_framer_set_state (&st->framer, acq_state_carry "
        "((void *)blob));\n  st->samples_consumed = ex.samples_consumed + "
        "1;\n  if (ex.carrier_freq_hz != st->carrier_freq_hz\n",
        ACQ,
        "ctest:test_acq_regrid_failure",
    ),
    (
        "position: BurstCapture drops its child's position check",
        "      || child_at != fed)\n",
        "      || 0)\n",
        BC,
        "ctest:test_burst_capture_core",
    ),
    (
        "position: state_position drops the carry",
        "  *position = ex.samples_consumed + pending;\n",
        "  *position = ex.samples_consumed;\n",
        ACQ,
        "snippets:dp_acq_state_position",
    ),
    (
        "position: state_pending counts frames x hop (framer test)",
        "      *pending = written - (f ? (f - 1) * fr->hop"
        " + fr->frame_n : 0);",
        "      *pending = written - f * fr->hop;                         ",
        BUF,
        "ctest:test_framer_core",
    ),
    (
        "position: state_pending counts frames x hop (header example)",
        "      *pending = written - (f ? (f - 1) * fr->hop"
        " + fr->frame_n : 0);",
        "      *pending = written - f * fr->hop;                         ",
        BUF,
        "snippets:_framer_state_pending",
    ),
]


def run_gate(gate: str) -> tuple[bool, list[str]]:
    """Run one gate; return (red, the failing lines it printed)."""
    kind, what = gate.split(":", 1)
    if kind == "ctest":
        r = subprocess.run(
            ["make", "test", f"CTEST_SELECT=-V -R ^{what}$"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        out = r.stdout
        red = "tests passed, 0 tests failed" not in out
        return red, [
            ln.split(": ", 1)[1]
            for ln in re.findall(r"^\d+: FAIL.*$", out, re.M)
        ]
    subprocess.run(
        ["make", "build"], cwd=ROOT, capture_output=True, check=False
    )
    r = subprocess.run(
        ["make", "test-snippets", f"PAGE={what}"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    return r.returncode != 0, re.findall(r"^FAILED .*$", r.stdout, re.M)


for case in SABOTAGES:
    name, old, new = case[:3]
    path = ROOT / (case[3] if len(case) > 3 else CORR2D)
    gate = case[4] if len(case) > 4 else "ctest:test_corr2d_core"
    text = path.read_text(encoding="utf-8")
    n = text.count(old)
    if n != 1:
        print(f"== {name}: {n} matches")
        continue
    path.write_text(text.replace(old, new), encoding="utf-8")
    try:
        red, fails = run_gate(gate)
    finally:
        # Restore from the saved text, never from git: it may be
        # uncommitted.
        path.write_text(text, encoding="utf-8")
    print(
        f"== {name}: {'RED' if red else 'GREEN (not caught)'}; "
        f"{len(fails)} failing check(s) [{gate}]",
        flush=True,
    )
    for line in fails[:3]:
        print("   ", line)

# Rebuild the restored source: the last sabotage's binaries must not outlive
# it (a stale build read as a real defect once).
subprocess.run(["make", "build"], cwd=ROOT, capture_output=True, check=False)
