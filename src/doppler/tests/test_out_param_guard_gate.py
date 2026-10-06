"""The strided-`out=` guard gate, exercised over a seeded tree.

`scripts/check_out_param_guard.py` reads every binding that coerces an `out=`
buffer and requires it to refuse a non-contiguous one, because
`PyArray_FROM_OTF` on a strided array makes a COPY: the kernel fills the copy,
the copy is dropped, and the caller's buffer is never written (doppler#1440).

It matched one spelling of the coercion. When #1446 handed fragments to jm, the
render said `jm_array_arg (out_obj` instead -- the same call behind a helper --
and the gate quietly stopped seeing those wrappers: 121 became 39, nothing
wrong in the code. Only the gate's own floor noticed. So the cases below pin
that the gate reads BOTH spellings, and that the floor still fires when it
reads nothing.

Each case seeds a fake tree, because a gate that can only be run against the
real bindings cannot be sabotaged.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPT = repo_root(__file__) / "scripts" / "check_out_param_guard.py"

#: The shape the gate parses: a column-0 function, braces in column 0.
WRAPPER = """\
static PyObject *
Obj_steps (ObjObject *self, PyObject *args, PyObject *kwds)
{{
  PyArrayObject *out_arr = {call};
  return NULL;
}}
"""

GUARD = "if (!PyArray_IS_C_CONTIGUOUS ((PyArrayObject *)out_obj)) return NULL;"
FROM_OTF = "(PyArrayObject *)PyArray_FROM_OTF (out_obj, NPY_FLOAT, 0)"
JM_ARG = 'jm_array_arg (out_obj, NPY_FLOAT, 0, "out")'


def _tree(tmp_path: Path, call: str, *, guarded: bool) -> Path:
    body = WRAPPER.format(call=call)
    if guarded:
        body = body.replace("  return NULL;", f"  {GUARD}\n  return NULL;")
    src = tmp_path / "native" / "src" / "m"
    src.mkdir(parents=True)
    (src / "m_ext_obj.c").write_text(body, encoding="utf-8")
    return tmp_path


def _run(root: Path, *extra: str):
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--root",
            str(root),
            "--min-wrappers",
            "1",
            *extra,
        ],
        capture_output=True,
        text=True,
    )


@pytest.mark.parametrize("call", [FROM_OTF, JM_ARG], ids=["from_otf", "jm"])
def test_a_guarded_wrapper_passes(tmp_path: Path, call: str) -> None:
    r = _run(_tree(tmp_path, call, guarded=True))
    assert r.returncode == 0, r.stdout
    assert "1 out= wrapper" in r.stdout


@pytest.mark.parametrize("call", [FROM_OTF, JM_ARG], ids=["from_otf", "jm"])
def test_an_unguarded_wrapper_fails_in_either_spelling(
    tmp_path: Path, call: str
) -> None:
    """The regression this exists for: a render that lost the guard.

    The `jm_array_arg` case is the one the gate could not see before.
    """
    r = _run(_tree(tmp_path, call, guarded=False))
    assert r.returncode == 1
    assert "Obj_steps()" in r.stdout
    assert "m_ext_obj.c" in r.stdout


def test_a_gate_that_reads_nothing_is_not_a_pass(tmp_path: Path) -> None:
    """Fewer wrappers than the floor means the pattern stopped matching."""
    root = _tree(tmp_path, FROM_OTF, guarded=True)
    r = subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 1
    assert "stopped matching" in r.stdout


def test_the_real_tree_passes(tmp_path: Path) -> None:
    """And the real bindings, hand-owned and generated, all refuse it."""
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True
    )
    assert r.returncode == 0, r.stdout
