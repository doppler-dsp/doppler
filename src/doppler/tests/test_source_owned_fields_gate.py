"""The owned-Field gate, exercised over a seeded tree.

`scripts/check_source_owned_fields.py` holds four hand-written sites to the
`wfm_seq_t` members of `wfm_source_t`: a new Field must be mentioned at each
(copied, freed or dropped), so it cannot be missed in a fourth file the way
`mac_header` was missed by the Plan. The cases seed a fake tree, so the gate
is tested without breaking the real one, and each asserts the gate goes RED.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_source_owned_fields.py"

_HEADER = """\
typedef struct {
    int type;
    wfm_seq_t acq_code; /**< a wfm_seq_t named in a comment is not one */
    wfm_seq_t data;
    wfm_seq_t mac_header;
    wfm_seq_t retired_bits; /* exempt: nothing reads it */
} wfm_source_t;
"""
_SITES = (
    "native/src/wfm/wfm_compose.c",
    "native/src/wfm/wfm_json.c",
    "native/src/wfm/wfm_plan.c",
    "native/src/app/wfmgen.c",
)
_BODY = "free (s->acq_code.bits); free (s->data.bits); s->mac_header = z;\n"


def _tree(tmp: Path, **override: str) -> Path:
    """A tree whose every site handles every Field, less `override`."""
    (tmp / "native/inc/doppler/wfm").mkdir(parents=True)
    (tmp / "native/inc/doppler/wfm/wfm_compose.h").write_text(
        _HEADER, encoding="utf-8"
    )
    for site in _SITES:
        p = tmp / site
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(override.get(site, _BODY), encoding="utf-8")
    return tmp


def _run(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
        check=False,
    )


def test_a_tree_that_handles_every_field_passes(tmp_path: Path) -> None:
    r = _run(_tree(tmp_path))
    assert r.returncode == 0, r.stdout
    assert "3 owned Field(s)" in r.stdout


def test_a_site_that_misses_a_field_is_refused(tmp_path: Path) -> None:
    plan = "native/src/wfm/wfm_plan.c"
    r = _run(_tree(tmp_path, **{plan: "free (s->acq_code.bits);\n"}))
    assert r.returncode == 1
    assert f"{plan}: does not mention the owned Field `mac_header`" in r.stdout
    assert "`data`" in r.stdout


def test_a_field_added_to_the_struct_is_refused_at_every_site(
    tmp_path: Path,
) -> None:
    root = _tree(tmp_path)
    h = root / "native/inc/doppler/wfm/wfm_compose.h"
    text = h.read_text(encoding="utf-8")
    h.write_text(
        text.replace("int type;", "int type;\n wfm_seq_t pad;"),
        encoding="utf-8",
    )
    r = _run(root)
    assert r.returncode == 1
    assert r.stdout.count("`pad`") == len(_SITES)


def test_the_real_tree_passes() -> None:
    r = _run(REPO)
    assert r.returncode == 0, r.stdout
