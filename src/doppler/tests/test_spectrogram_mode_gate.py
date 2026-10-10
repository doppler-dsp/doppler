"""The Spectrogram-mode gate, exercised over a seeded tree.

`scripts/check_spectrogram_mode.py` holds the owner's rule on #1968: dB rows
are asked for by name, so no `dp_spectrogram_create` call passes its mode as
an integer literal. The constructor takes a plain `int`, so the compiler
cannot refuse one, and #2097 renumbered the modes (power is now the zero
value): a call written as a number would have changed meaning without a
diff.

The cases seed a small git repository, because the gate reads `git
ls-files` and a gate that can only run against the real tree cannot be
sabotaged without breaking doppler. They cover the shapes the tree actually
has: a call on one line, one wrapped inside a header's `@code` block with
`*` continuations, a Markdown fence, casts and parentheses, a prose mention
that is not a call, and the empty scan that must fail rather than pass.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_spectrogram_mode.py"

#: Every shape of a named mode the tree has, which must all pass.
_NAMED_C = """\
#include "doppler/spectrogram/spectrogram_core.h"
int main (void)
{
  int mode = DP_SPECTROGRAM_DB;
  dp_spectrogram_state_t *a
      = dp_spectrogram_create (8, 4, 0, 0.0f, DP_SPECTROGRAM_POWER);
  dp_spectrogram_state_t *b = dp_spectrogram_create (8, 4, (int)w[1],
                                                     0.0f, mode);
  return !a || !b;
}
"""

_NAMED_H = """\
/**
 * @code
 * dp_spectrogram_state_t *s = dp_spectrogram_create (8, 4, 3, 0.0f,
 *                                                    DP_SPECTROGRAM_DB);
 * @endcode
 */
dp_spectrogram_state_t *dp_spectrogram_create (size_t nfft, size_t hop,
                                               int window, float beta,
                                               int mode);
"""

_NAMED_MD = """\
# A page

Call `dp_spectrogram_create()` to make one.

```c
dp_spectrogram_state_t *s
    = dp_spectrogram_create (1024, 256, 0, 0.0f, DP_SPECTROGRAM_POWER);
```
"""


def _seed(tmp_path: Path, files: dict[str, str]) -> None:
    for rel, text in files.items():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    subprocess.run(["git", "-C", str(tmp_path), "add", "."], check=True)


def _run(tmp_path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
        check=False,
    )


def test_named_modes_pass_in_every_shape(tmp_path: Path) -> None:
    _seed(
        tmp_path,
        {"a.c": _NAMED_C, "inc/s.h": _NAMED_H, "docs/p.md": _NAMED_MD},
    )
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    # the declaration and the four calls are read; the prose mention is not
    assert "5 call(s) in 3 file(s)" in r.stdout


def test_a_literal_mode_fails_and_names_the_line(tmp_path: Path) -> None:
    bad = _NAMED_C.replace("0.0f, DP_SPECTROGRAM_POWER);", "0.0f, 1);")
    _seed(tmp_path, {"a.c": bad})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "a.c:6: mode = 1" in r.stdout


def test_every_spelling_of_a_literal_fails(tmp_path: Path) -> None:
    for spelling in ("0", "(1)", "(int) 0", "0x1", "1u", "-1", "'\\0'"):
        call = f"x = dp_spectrogram_create (8, 4, 0, 0.0f, {spelling});\n"
        _seed(tmp_path / spelling.replace("'", "q"), {"a.c": call})
        r = _run(tmp_path / spelling.replace("'", "q"))
        assert r.returncode == 1, (spelling, r.stdout)


def test_a_literal_wrapped_in_a_header_comment_fails(tmp_path: Path) -> None:
    bad = _NAMED_H.replace("DP_SPECTROGRAM_DB);", "0);")
    _seed(tmp_path, {"inc/s.h": bad})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "inc/s.h:3: mode = 0" in r.stdout


def test_a_literal_in_a_markdown_fence_fails(tmp_path: Path) -> None:
    bad = _NAMED_MD.replace("DP_SPECTROGRAM_POWER", "0")
    _seed(tmp_path, {"docs/p.md": bad})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "docs/p.md:7: mode = 0" in r.stdout


def test_an_untracked_file_is_not_the_gates(tmp_path: Path) -> None:
    _seed(tmp_path, {"a.c": _NAMED_C})
    (tmp_path / "scratch.c").write_text(
        "x = dp_spectrogram_create (8, 4, 0, 0.0f, 0);\n"
    )
    assert _run(tmp_path).returncode == 0


def test_a_scan_that_finds_no_call_fails(tmp_path: Path) -> None:
    _seed(tmp_path, {"a.c": "int main (void) { return 0; }\n"})
    r = _run(tmp_path)
    assert r.returncode == 1
    assert "scan is broken" in r.stdout
