"""The tlm-name-join gate, exercised over a seeded tree.

`scripts/check_tlm_name_join.py` keeps every telemetry probe name joined by
`dp_tlm_name_join()`, which refuses a name too long for its
`DP_TLM_NAME_MAX` buffer. The bare `snprintf (name, DP_TLM_NAME_MAX,
"%s.%s", prefix, suffix)` it replaced truncated silently, and ten
`set_telemetry` implementations carried it until #1898 (#676, #1944).

Each case seeds a fake `native/` tree and points `--root` at it, because a
gate that can only run against the real tree cannot be sabotaged without
breaking a real `set_telemetry`.

Three cases guard what a blunter check would miss. A composite never calls
`dp_tlm_probe()` but builds its child's prefix, so attaching a child is
enough to put a file in scope. A private wrapper around `snprintf` is the
same copy one hop away, so any literal beginning `%s.` counts, not only an
`snprintf` argument. And `"//"` inside a string is not a comment, so a URL
on the same line cannot hide the join after it.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_tlm_name_join.py"

CONVERTED = """\
int
dp_x_set_telemetry (dp_x_state_t *s, dp_tlm_t *tlm, const char *prefix)
{
  char name[DP_TLM_NAME_MAX];
  if (dp_tlm_name_join (name, prefix, "e") != DP_OK)
    return DP_ERR_INVALID;
  return dp_tlm_probe (tlm, name, 1) < 0 ? DP_ERR_INVALID : DP_OK;
}
"""

# What all ten sites looked like before #1898.
BARE = """\
int
dp_x_set_telemetry (dp_x_state_t *s, dp_tlm_t *tlm, const char *prefix)
{
  char name[DP_TLM_NAME_MAX];
  snprintf (name, sizeof name, "%s.%s", prefix, "e");
  return dp_tlm_probe (tlm, name, 1) < 0 ? DP_ERR_INVALID : DP_OK;
}
"""

# A composite: registers nothing itself, builds its child's prefix by hand.
COMPOSITE = """\
int
dp_rx_set_telemetry (dp_rx_state_t *s, dp_tlm_t *tlm, const char *prefix)
{
  char child[DP_TLM_NAME_MAX];
  snprintf (child, sizeof child,
            "%s.car", prefix);
  return dp_costas_set_telemetry (s->car, tlm, child, 1);
}
"""

WRAPPER = """\
static void join (char *o, const char *f, const char *p, const char *s);
int
dp_x_set_telemetry (dp_x_state_t *s, dp_tlm_t *tlm, const char *prefix)
{
  char name[DP_TLM_NAME_MAX];
  join (name, "%s.%s", prefix, "e");
  return dp_tlm_probe (tlm, name, 1);
}
"""

# Not telemetry: a sidecar path is "%s.sigmf-meta" and that is fine.
UNRELATED = """\
void
sidecar (char *out, size_t cap, const char *path)
{
  snprintf (out, cap, "%s.sigmf-meta", path);
}
"""

PROSE = (
    '/* Before #1898 this was snprintf (name, cap, "%s.%s", p, s). */\n'
    '// and the C++-style spelling: "%s.%s"\n' + CONVERTED
)

# The URL's "//" is inside a string. Read as a comment, it would swallow the
# join after it and the gate would pass.
URL_THEN_JOIN = """\
int
dp_x_set_telemetry (dp_x_state_t *s, dp_tlm_t *tlm, const char *prefix)
{
  char name[DP_TLM_NAME_MAX]; const char *u = "http://x"; snprintf (name, \
32, "%s.%s", prefix, u);
  return dp_tlm_probe (tlm, name, 1);
}
"""


def _seed(tmp_path: Path, files: dict[str, str]) -> Path:
    """A minimal tree shaped like native/."""
    for rel, body in files.items():
        p = tmp_path / "native" / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")
    return tmp_path


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
    )


def test_a_converted_tree_passes(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"src/x/x_core.c": CONVERTED}))
    assert r.returncode == 0, r.stdout + r.stderr
    assert "dp_tlm_name_join" in r.stdout


def test_a_bare_snprintf_join_fails(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"src/x/x_core.c": BARE}))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "native/src/x/x_core.c:5" in r.stdout


def test_a_composite_building_a_child_prefix_fails(tmp_path: Path) -> None:
    """No dp_tlm_probe() call, still in scope: it attaches a child."""
    r = _run(_seed(tmp_path, {"src/rx/rx_core.c": COMPOSITE}))
    assert r.returncode == 1, r.stdout + r.stderr
    assert '"%s.car"' in r.stdout


def test_a_private_wrapper_is_still_a_join(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"src/x/x_core.c": WRAPPER}))
    assert r.returncode == 1, r.stdout + r.stderr


def test_the_sanctioned_home_may_hold_the_join(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"src/dp_tlm/dp_tlm_core.c": BARE}))
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_file_outside_telemetry_may_format_freely(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"inc/wfm/wfm_path.h": UNRELATED}))
    assert r.returncode == 0, r.stdout + r.stderr


def test_prose_quoting_the_old_spelling_passes(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"src/x/x_core.c": PROSE}))
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_url_in_a_string_does_not_hide_the_join(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, {"src/x/x_core.c": URL_THEN_JOIN}))
    assert r.returncode == 1, r.stdout + r.stderr


def test_one_offender_among_many_is_still_found(tmp_path: Path) -> None:
    """The scan does not stop at the first clean file."""
    files = {f"src/{n}/{n}_core.c": CONVERTED for n in "abcdef"}
    files["tests/test_z_core.c"] = BARE
    r = _run(_seed(tmp_path, files))
    assert r.returncode == 1, r.stdout + r.stderr
    assert "native/tests/test_z_core.c" in r.stdout
