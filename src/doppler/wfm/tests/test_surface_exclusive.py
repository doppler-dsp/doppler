"""Two surface rows no face takes together, refused from ONE declaration.

doppler#1683: a scene with ``"payload"`` beside ``"frame"`` exited 0 and
dropped the payload, while the CLI refused ``--bits`` beside ``--frame`` --
the exclusion was written for one face only. It is now the manifest's
``exclusive`` key on a surface row, rendered by ``gen_wfm_defaults.py`` into
``WFM_SURFACE_EXCLUSIVE`` (a reason per face, each a literal), and every
face refuses from that one row: the CLI (both flags), the scene reader (both
keys), the object (both members set, at ``dp_wfm_source_error``), ``--help``
and the schema. Each test reads the declaration rather than restating it,
so deleting the manifest row turns every face red at once.

The one ``*REPS`` sentence (the manifest's ``field_reps``) is pinned here
too: the CLI and a scene refuse ``*REPS`` on a field that does not repeat
with the generated reason, not two hand-written copies of it.
"""

from __future__ import annotations

import importlib.util
import json
import re
import subprocess
import sys
from typing import TYPE_CHECKING

import numpy as np
import pytest

from doppler.tests._repo import repo_root
from doppler.wfm import Composer, FrameDesc, Segment, Synth, cli, field_bits

if TYPE_CHECKING:
    from pathlib import Path

ROOT = repo_root(__file__)
SURFACE_H = ROOT / "native/inc/doppler/wfm/wfm_surface.h"


def _generator():
    scripts = ROOT / "scripts"
    sys.path.insert(0, str(scripts))
    try:
        spec = importlib.util.spec_from_file_location(
            "gen_wfm_defaults", scripts / "gen_wfm_defaults.py"
        )
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    finally:
        sys.path.remove(str(scripts))


GEN = _generator()
ROWS = GEN.surface_rows(
    GEN.tomllib.loads(GEN.MANIFEST.read_text(encoding="utf-8"))
)


def _pair(a: str, b: str) -> dict:
    """The declared exclusion between two source rows -- it must exist."""
    for e in GEN.exclusions(ROWS):
        if e["owner"] == "source" and {e["a"], e["b"]} == {a, b}:
            return e
    pytest.fail(f"no `exclusive` declaration between {a} and {b}")


PAIR = _pair("bits", "frame")

#: The description these tests carry: a 4-bit payload field of its own.
FRAME = {"fields": [{"name": "payload", "spec": "1010"}]}


def _run(args: list[str], tmp_path: Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        [cli._runnable(), *args, "--output", str(tmp_path / "out.dat")],
        capture_output=True,
        text=True,
        timeout=120,
    )


def _frame_file(tmp_path: Path) -> Path:
    f = tmp_path / "frame.json"
    f.write_text(json.dumps(FRAME), encoding="utf-8")
    return f


def _scene(**src) -> str:
    seg = {"type": "bits", "fs": 1e6, "sps": 4, "num_samples": 64, **src}
    return json.dumps({"version": 1, "segments": [seg]})


# ── the CLI ──────────────────────────────────────────────────────────────────


def test_the_cli_refuses_bits_beside_frame(tmp_path):
    p = _run(
        [
            "--type",
            "bits",
            "--bits",
            "1100",
            "--frame",
            str(_frame_file(tmp_path)),
        ],
        tmp_path,
    )
    assert p.returncode == 2, p.stderr
    assert PAIR["cli_why"] in p.stderr


def test_the_cli_refuses_a_bits_file_beside_frame(tmp_path):
    """--bits-file fills the member --bits names, so it is the same pair."""
    bits = tmp_path / "p.bin"
    bits.write_bytes(b"\xa5")
    p = _run(
        [
            "--type",
            "bits",
            "--bits-file",
            str(bits),
            "--frame",
            str(_frame_file(tmp_path)),
        ],
        tmp_path,
    )
    assert p.returncode == 2, p.stderr
    assert PAIR["cli_why"] in p.stderr


def test_the_cli_takes_either_alone(tmp_path):
    f = _frame_file(tmp_path)
    for args in (["--bits", "1100"], ["--frame", str(f)]):
        p = _run(["--type", "bits", "--count", "64", *args], tmp_path)
        assert p.returncode == 0, p.stderr


# ── a scene ──────────────────────────────────────────────────────────────────


def test_a_scene_refuses_payload_beside_frame(tmp_path):
    """The #1683 defect itself: this exited 0 with the payload dropped."""
    scene = tmp_path / "scene.json"
    scene.write_text(_scene(payload="1100", frame=FRAME), encoding="utf-8")
    p = _run(["--from-file", str(scene)], tmp_path)
    assert p.returncode != 0, "a scene silently dropped the payload again"
    assert PAIR["json_why"] in p.stderr


def test_composer_from_json_refuses_with_the_scene_reason():
    with pytest.raises(ValueError) as e:
        Composer.from_json(_scene(payload="1100", frame=FRAME))
    assert str(e.value) == PAIR["json_why"]


def test_the_scene_and_the_cli_give_one_reason():
    """One declaration: the faces differ only in how they spell the pair."""
    tail = PAIR["cli_why"].split(": ", 1)[1]
    assert PAIR["json_why"].endswith(tail)
    assert PAIR["obj_why"].endswith(tail)


# ── the object ───────────────────────────────────────────────────────────────


def _desc() -> FrameDesc:
    d = FrameDesc()
    d.add_field("payload", field_bits("1010"))
    return d


def test_a_synth_refuses_payload_beside_frame():
    s = Synth(type="bits", sps=4, payload=field_bits("1100"), frame=_desc())
    with pytest.raises(ValueError) as e:
        s.steps(8)
    assert str(e.value) == PAIR["obj_why"]


def test_a_composer_refuses_payload_beside_frame():
    seg = Segment(
        type="bits",
        sps=4,
        num_samples=64,
        payload=field_bits("1100"),
        frame=_desc(),
    )
    with pytest.raises(ValueError):
        Composer([seg])


def test_the_object_takes_either_alone():
    for kw in ({"payload": field_bits("1100")}, {"frame": _desc()}):
        x = Synth(type="bits", sps=4, **kw).steps(16)
        assert np.asarray(x).size == 16


# ── help and schema ──────────────────────────────────────────────────────────


def test_help_says_each_flag_is_not_with_the_other():
    h = subprocess.run(
        [cli._runnable(), "--help"], capture_output=True, text=True
    ).stdout
    text = " ".join(h.split())
    assert re.search(r"--bits FIELD .*? Not with --frame\.", text)
    assert re.search(r"--frame FILE .*? Not with --bits\.", text)


def test_the_schema_refuses_both_keys():
    from jsonschema import Draft202012Validator

    v = Draft202012Validator(
        json.loads(
            (ROOT / "docs/schema/wfmgen.schema.json").read_text("utf-8")
        )
    )
    assert not v.is_valid(json.loads(_scene(payload="1100", frame=FRAME)))
    assert v.is_valid(json.loads(_scene(frame=FRAME)))
    assert v.is_valid(json.loads(_scene(payload="1100")))


# ── the one *REPS sentence ───────────────────────────────────────────────────


def _macro(name: str) -> str:
    """A generated string macro's text, unescaped."""
    m = re.search(
        rf"#define {name} \\\n\s*\"((?:[^\"\\]|\\.)*)\"",
        SURFACE_H.read_text(encoding="utf-8"),
    )
    assert m, f"{name} is not generated"
    return m.group(1).replace('\\"', '"')


def test_the_cli_refuses_reps_off_the_preamble_with_the_generated_reason(
    tmp_path,
):
    p = _run(
        ["--type", "bits", "--bits", "1100", "--sync", "0101*2"], tmp_path
    )
    assert p.returncode == 2
    assert _macro("WFM_SURFACE_REPS_WHY_CLI") in p.stderr


def test_a_scene_refuses_reps_off_the_preamble_with_the_generated_reason():
    with pytest.raises(ValueError) as e:
        Composer.from_json(_scene(payload="1100", sync="0101*2"))
    assert str(e.value) == _macro("WFM_SURFACE_REPS_WHY_JSON")


def test_the_reps_reason_names_the_field_that_repeats():
    """`field_reps` decides which field repeats; the sentence follows it."""
    reps = [r for r in ROWS if r["field_reps"]]
    assert [r["name"] for r in reps] == ["acq_code"]
    assert "--acq-code" in _macro("WFM_SURFACE_REPS_WHY_CLI")
    assert '"acq_code"' in _macro("WFM_SURFACE_REPS_WHY_JSON")
