"""The doc-claims gate, exercised over a seeded tree.

`scripts/check_doc_claims.py` refuses two things the snippet and example
runners cannot see, because both run green:

- a claim computed and discarded — `np.array_equal(a, b)` or `a == b` as a
  bare statement in a ``python`` fence or an example;
- an example with no `assert`, no `raise` and no `sys.exit(<expr>)`, which
  can only ever exit 0.

Each case seeds a fake `docs/` and `src/doppler/examples/`, because a gate
that can only be tested against the real tree cannot be sabotaged without
breaking doppler. Each red case is the sabotage of the green case beside it:
the same code with its `assert` taken away.

The cases that must NOT fire are seeded too — a ``pycon`` fence (its
expected output is the check), an assignment of a comparison, a
`--8<--` include line (which parses as a comparison until it is resolved),
a `sys.exit(main())` example and a registered two-process pair — because a
gate that fires on those is one someone turns off.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_doc_claims.py"

#: An example that checks what it shows.
_CHECKED = "import numpy as np\nassert np.array_equal([1], [1])\n"


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )


def _tree(
    tmp_path: Path,
    page: str = "",
    examples: dict[str, str] | None = None,
    pairs: str = "",
) -> Path:
    docs = tmp_path / "docs" / "guide"
    docs.mkdir(parents=True)
    (docs / "page.md").write_text(page, encoding="utf-8")
    ex = tmp_path / "src" / "doppler" / "examples"
    ex.mkdir(parents=True)
    for name, body in (examples or {"ok.py": _CHECKED}).items():
        (ex / name).write_text(body, encoding="utf-8")
    if pairs:
        (ex / ".examples-pairs").write_text(pairs, encoding="utf-8")
    return tmp_path


def _fence(code: str, lang: str = "python") -> str:
    return f"# A page\n\n```{lang}\n{code}```\n"


def test_the_real_tree_is_clean() -> None:
    r = _run(REPO)
    assert r.returncode == 0, r.stderr


def test_an_asserted_claim_passes(tmp_path: Path) -> None:
    page = _fence("import numpy as np\nassert np.array_equal([1], [1])\n")
    r = _run(_tree(tmp_path, page))
    assert r.returncode == 0, r.stderr


def test_a_discarded_array_equal_is_refused(tmp_path: Path) -> None:
    # The sabotage of the case above: the same line, its assert removed.
    page = _fence("import numpy as np\nnp.array_equal([1], [1])  # True\n")
    r = _run(_tree(tmp_path, page))
    assert r.returncode == 1
    assert "docs/guide/page.md:5:" in r.stderr
    assert "np.array_equal([1], [1])" in r.stderr


def test_a_discarded_comparison_is_refused(tmp_path: Path) -> None:
    page = _fence("x = [1, 2]\nlen(x) == 2\n")
    r = _run(_tree(tmp_path, page))
    assert r.returncode == 1
    assert "len(x) == 2" in r.stderr


def test_a_discarded_all_is_refused(tmp_path: Path) -> None:
    page = _fence("x = [1, 2]\nall(v > 0 for v in x)\n")
    r = _run(_tree(tmp_path, page))
    assert r.returncode == 1


def test_a_stored_comparison_is_not_a_claim(tmp_path: Path) -> None:
    page = _fence("x = [1, 2]\nsame = len(x) == 2\nprint(same)\n")
    r = _run(_tree(tmp_path, page))
    assert r.returncode == 0, r.stderr


def test_a_pycon_fence_is_exempt(tmp_path: Path) -> None:
    # Its expected output is the check, and the doctest runner compares it.
    page = _fence(">>> 1 == 1\nTrue\n", lang="pycon")
    r = _run(_tree(tmp_path, page))
    assert r.returncode == 0, r.stderr


def test_an_include_line_is_resolved_not_read(tmp_path: Path) -> None:
    # `--8<-- "path:region"` parses as `--8 < -"path:region"`, a comparison.
    # Resolved, it is the included example's code, which asserts.
    page = _fence(
        '--8<-- "src/doppler/examples/dsss_burst_receiver_demo.py:receiver"\n'
    )
    r = _run(_tree(tmp_path, page))
    assert r.returncode == 0, r.stderr


def test_an_example_that_checks_nothing_is_refused(tmp_path: Path) -> None:
    r = _run(_tree(tmp_path, examples={"demo.py": "print('it worked')\n"}))
    assert r.returncode == 1
    assert "src/doppler/examples/demo.py: checks nothing" in r.stderr


def test_a_discarded_claim_in_an_example_is_refused(tmp_path: Path) -> None:
    body = "import numpy as np\nassert True\nnp.allclose([1], [1])\n"
    r = _run(_tree(tmp_path, examples={"demo.py": body}))
    assert r.returncode == 1
    assert "src/doppler/examples/demo.py:3:" in r.stderr


def test_an_exit_code_example_checks_something(tmp_path: Path) -> None:
    body = (
        "import sys\n\ndef main():\n    return 0 if 1 + 1 == 2 else 1\n\n"
        "sys.exit(main())\n"
    )
    r = _run(_tree(tmp_path, examples={"demo.py": body}))
    assert r.returncode == 0, r.stderr


def test_a_constant_exit_checks_nothing(tmp_path: Path) -> None:
    body = "import sys\nprint('done')\nsys.exit(0)\n"
    r = _run(_tree(tmp_path, examples={"demo.py": body}))
    assert r.returncode == 1


def test_a_registered_pair_is_exempt(tmp_path: Path) -> None:
    # Its evidence regex in .examples-pairs is its check.
    r = _run(
        _tree(
            tmp_path,
            examples={"tx.py": "print('Packets: 3')\n", "rx.py": "pass\n"},
            pairs="# comment\ntx.py + rx.py: Packets: +[1-9]\n",
        )
    )
    assert r.returncode == 0, r.stderr
