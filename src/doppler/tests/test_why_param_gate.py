"""The refusal-reason gate, exercised over a seeded tree.

`scripts/check_why_param.py` holds every public ``why`` parameter to one
shape, ``const char **``, because that shape was a habit declared nowhere
and the next function invented a second one (``char *why, size_t
why_cap``). The rule's home is ``docs/dev/contributing/error-convention.md``.

The cases below seed a fake ``native/inc/``, because a gate that can only be
tested against the real tree cannot be sabotaged: you would have to break
doppler to check it. The first two are the sabotage pair: the buffer shape
that was proposed must fail, and the convention must pass.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_why_param.py"

_CONVENTION = """\
/** @brief Parse a spec. @param why Receives the reason on refusal. */
int widget_parse (const char *spec, int *out, const char **why);
"""

#: The second shape, as it was proposed: a caller-owned buffer.
_BUFFER = """\
int widget_parse (const char *spec, int *out, char *why, size_t why_cap);
"""

#: A `_why`-suffixed name is the same parameter under a longer name.
_SUFFIXED = """\
int widget_load (const char *path, const char *parse_why);
"""

#: Spacing is not a second shape.
_SPACED = """\
int widget_parse (const char *spec,
                  const char * * why);
"""

#: `why` in prose, an example and a macro is not a parameter.
_NOT_PARAMS = """\
/**
 * @code
 * char *why = 0;
 * widget_parse ("x", &n, &why);
 * @endcode
 */
#define WIDGET_WHY(why) (why)
int widget_parse (const char *spec, int *out, const char **why);
"""


def _seed(tmp_path: Path, body: str) -> None:
    p = tmp_path / "native" / "inc" / "widget" / "widget.h"
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(body, encoding="utf-8")


def _run(tmp_path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path)],
        capture_output=True,
        text=True,
    )


def test_the_convention_passes(tmp_path: Path) -> None:
    _seed(tmp_path, _CONVENTION)
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    # The count proves the scan saw the parameter; OK over zero is blind.
    assert "1 refusal-reason parameter(s)" in r.stdout


def test_a_caller_owned_buffer_fails(tmp_path: Path) -> None:
    """The sabotage: the shape that was actually proposed."""
    _seed(tmp_path, _BUFFER)
    r = _run(tmp_path)
    assert r.returncode == 1, r.stdout
    assert "widget_parse()" in r.stdout
    assert "`char*`" in r.stdout
    # `why_cap` ends in neither `why` nor `_why`, so it is not judged; the
    # reported finding is the buffer itself.
    assert "why_cap" not in r.stdout


def test_a_suffixed_name_is_held_to_the_rule(tmp_path: Path) -> None:
    _seed(tmp_path, _SUFFIXED)
    r = _run(tmp_path)
    assert r.returncode == 1, r.stdout
    assert "parse_why" in r.stdout


def test_spacing_is_not_a_second_shape(tmp_path: Path) -> None:
    _seed(tmp_path, _SPACED)
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout


def test_comments_examples_and_macros_are_not_parameters(
    tmp_path: Path,
) -> None:
    _seed(tmp_path, _NOT_PARAMS)
    r = _run(tmp_path)
    assert r.returncode == 0, r.stdout
    assert "1 refusal-reason parameter(s)" in r.stdout
