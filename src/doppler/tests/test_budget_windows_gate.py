"""The budget-window gate, exercised over a seeded tree.

`scripts/check_budget_windows.py` counts, per file, the calls that open an
error-rate window on the analytic budget (`dp_ber_settle`,
`dp_ber_settle_from`, `settle_from`, `ber_settle_from`) or score by a
best-lag search (`_best_ber`), and holds each file to its row in
`scripts/.budget-window-exempt`, which only shrinks (#3231).

The failing cases: a new call with no row, a call over its row, and a row
above its file's count. The passing cases are load-bearing too: a file at
its row, prose and strings that name the functions, the definitions, and a
file that opens its window with `dp_ber_lock_all()`.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_budget_windows.py"


def _seed(root: Path, files: dict[str, str], rows: str = "") -> Path:
    for rel, body in files.items():
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")
    lst = root / "scripts" / ".budget-window-exempt"
    lst.parent.mkdir(parents=True, exist_ok=True)
    lst.write_text("# header\n" + rows, encoding="utf-8")
    return root


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
    )


C_CALL = "s = dp_ber_settle (bt, bc, NULL, lc, n, &ok);\n"
LOCK_ALL = "s = dp_ber_lock_all (flags, 2, n, 200, 0.9, lim, NULL);\n"


def test_a_file_at_its_row_passes(tmp_path: Path) -> None:
    root = _seed(
        tmp_path,
        {"native/validation/a.c": C_CALL + C_CALL},
        "native/validation/a.c  2  reason\n",
    )
    r = _run(root)
    assert r.returncode == 0, r.stdout


def test_prose_definitions_and_lock_all_pass(tmp_path: Path) -> None:
    root = _seed(
        tmp_path,
        {
            "native/validation/b.c": (
                "/* was dp_ber_settle (bt, bc, NULL, NULL, n, &ok) */\n"
                'const char *m = "dp_ber_settle (x)";\n' + LOCK_ALL
            ),
            "native/tests/c.c": "static double\n_best_ber (const float *s)\n"
            "{\n  return 0.0;\n}\n",
            "src/d.py": '"""settle_from(probes) is gone."""\n'
            "# settle_from(x)\n"
            "def settle_from(p):\n    return 0\n",
            "native/tests/dp_ber_test.h": C_CALL,
        },
    )
    r = _run(root)
    assert r.returncode == 0, r.stdout


def test_a_new_call_fails(tmp_path: Path) -> None:
    root = _seed(tmp_path, {"src/e.py": "w = settle_from(probes)\n"})
    r = _run(root)
    assert r.returncode == 1
    assert "src/e.py: 1 budget-window call(s) and no row" in r.stdout


def test_a_call_over_its_row_fails(tmp_path: Path) -> None:
    root = _seed(
        tmp_path,
        {"native/tests/f.c": C_CALL + "b = _best_ber (s, n, d, k);\n"},
        "native/tests/f.c  1  reason\n",
    )
    r = _run(root)
    assert r.returncode == 1
    assert "native/tests/f.c: 2 call(s), over its row's 1" in r.stdout


def test_a_row_above_its_count_fails(tmp_path: Path) -> None:
    root = _seed(
        tmp_path,
        {"native/tests/g.c": LOCK_ALL},
        "native/tests/g.c  1  reason\n",
    )
    r = _run(root)
    assert r.returncode == 1
    assert "under its row's 1: shrink the row" in r.stdout
