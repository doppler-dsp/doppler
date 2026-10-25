"""The doc-symbol gate, exercised over a seeded tree.

`scripts/check_doc_symbols.py` checks that a `dp_*` name on a docs page is a
name the C tree defines. The defects it was written for (#2580) are seeded
here and must be caught: a renamed type in prose, a deleted function in a
code span, and a type that never existed inside a fence a `skip=` marker
keeps the compiler away from.

So are the shapes it must NOT flag, because a gate that fires on them gets
turned off: a name only a comment still mentions is NOT defined (that is how
a stale name looks alive), but a name a test helper defines is; a file name,
a `dp_*` pattern and a placeholder name no symbol; generated C API and
archived pages are records.

And the exempt list may only shrink: an entry whose symbol came back, or
whose page stopped naming it, is a finding.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPT = repo_root(__file__) / "scripts" / "check_doc_symbols.py"

HEADER = """\
/* dp_ghost_create() was deleted; only this comment remembers it. */
typedef struct dp_acc_state dp_acc_state_t;
dp_acc_state_t *dp_acc_create (void); // not dp_gone_t
#define DP_OK 0
"""

#: A helper a test header defines: design pages document these on purpose.
TEST_HELPER = "static inline int dp_ber_ci (void) { return 0; }\n"

#: Every shape that must pass.
GOOD = """\
# Accumulator

Backed by `dp_acc_state_t`, built by `dp_acc_create()`, returning `DP_OK`.
The test helper `dp_ber_ci` too. The file `dp_acc.h`, the family `dp_acc_*`,
the template `dp_<obj>_create` and `dp_mytype_create`, `dp_foo_create`.

```c
dp_acc_state_t *a = dp_acc_create ();
```

Prose naming dp_ghost_create outside a code span is not a citation.
"""


def _git(root: Path, *args: str) -> None:
    subprocess.run(["git", *args], cwd=root, check=True, capture_output=True)


def _tree(tmp_path: Path, pages: dict[str, str], exempt: str = "") -> Path:
    root = tmp_path / "repo"
    files = {
        "native/inc/doppler/acc/acc_core.h": HEADER,
        "native/tests/dp_ber_test.h": TEST_HELPER,
        "scripts/.doc-symbol-exempt": exempt,
        "docs/index.md": GOOD,
        **pages,
    }
    for rel, text in files.items():
        (root / rel).parent.mkdir(parents=True, exist_ok=True)
        (root / rel).write_text(text)
    _git(root, "init", "-q")
    _git(root, "add", "-A")
    return root


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
    )


def test_every_shape_that_must_pass_passes(tmp_path):
    r = _run(_tree(tmp_path, {}))
    assert r.returncode == 0, r.stderr
    assert "0 exempt" in r.stdout


def test_a_renamed_type_in_prose_is_refused(tmp_path):
    page = {"docs/api/acc.md": "Backed by `dp_acc_t`.\n"}
    r = _run(_tree(tmp_path, page))
    assert r.returncode == 1
    assert "docs/api/acc.md: names `dp_acc_t`" in r.stderr


def test_a_name_only_a_comment_remembers_is_refused(tmp_path):
    """Comments are stripped: they keep a stale name looking alive."""
    for sym in ("dp_ghost_create", "dp_gone_t"):
        page = {"docs/api/acc.md": f"Call `{sym}`.\n"}
        r = _run(_tree(tmp_path / sym, page))
        assert r.returncode == 1, sym
        assert f"names `{sym}`" in r.stderr


def test_a_name_in_a_skipped_fence_is_refused(tmp_path):
    """The streaming.md case: skip= keeps the compiler out, not this gate."""
    page = {
        "docs/examples/stream.md": (
            "<!-- docs-snippet: skip=illustrative -->\n\n"
            "  ```c\n  dp_cf64_t *data = 0;\n  ```\n"
        )
    }
    r = _run(_tree(tmp_path, page))
    assert r.returncode == 1
    assert "names `dp_cf64_t`" in r.stderr


def test_generated_and_archived_pages_are_not_scanned(tmp_path):
    pages = {
        "docs/c-api/acc.md": "`dp_ghost_create`\n",
        "docs/design/archive/old.md": "`dp_ghost_create`\n",
    }
    assert _run(_tree(tmp_path, pages)).returncode == 0


def test_an_exempt_name_passes(tmp_path):
    page = {"docs/design/x.md": "There is no `dp_source_t`.\n"}
    r = _run(_tree(tmp_path, page, "docs/design/x.md dp_source_t  # why\n"))
    assert r.returncode == 0, r.stderr
    assert "1 exempt" in r.stdout


def test_an_exemption_whose_symbol_came_back_is_stale(tmp_path):
    page = {"docs/design/x.md": "`dp_acc_create`\n"}
    r = _run(_tree(tmp_path, page, "docs/design/x.md dp_acc_create\n"))
    assert r.returncode == 1
    assert "`dp_acc_create` exists now" in r.stderr


def test_an_exemption_the_page_no_longer_needs_is_stale(tmp_path):
    page = {"docs/design/x.md": "Nothing.\n"}
    r = _run(_tree(tmp_path, page, "docs/design/x.md dp_source_t\n"))
    assert r.returncode == 1
    assert "no longer names `dp_source_t`" in r.stderr


def test_a_scan_over_nothing_fails_closed(tmp_path):
    root = _tree(tmp_path, {})
    _git(root, "rm", "-q", "--cached", "-r", "native")
    r = _run(root)
    assert r.returncode == 1
    assert "did not run" in r.stderr
