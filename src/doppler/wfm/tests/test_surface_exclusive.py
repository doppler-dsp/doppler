"""Two surface rows no face takes together, refused from ONE declaration.

doppler#1683: a scene with two spellings of one thing beside each other
exited 0 and dropped one, while the CLI refused it -- the exclusion was
written for one face only. It is now the manifest's ``exclusive`` key on a
surface row, rendered by ``gen_wfm_defaults.py`` into
``WFM_SURFACE_EXCLUSIVE`` (a reason per face, each a literal), and every
face refuses from that one row: the CLI (both flags), the scene reader (both
keys), the object (both members set, at ``dp_wfm_source_error``; pinned in
C, since Python has no file face), ``--help`` and the schema. The pair is a
payload's two data sources, ``data`` and ``data_from_file``. Each test
reads the declaration rather than restating it, so deleting the manifest
row turns every face red at once.

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
from doppler.wfm import Composer, Segment, Synth, cli

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


#: The one exclusion a source row declares: a payload has one data source
#: (doppler#1718 retired the bits/frame pair with --bits itself).
PAIR = _pair("data", "data_from_file")


def _run(args: list[str], tmp_path: Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        [cli._runnable(), *args, "--output", str(tmp_path / "out.dat")],
        capture_output=True,
        text=True,
        timeout=120,
    )


def _scene(**src) -> str:
    seg = {"type": "bits", "fs": 1e6, "sps": 4, **src}
    return json.dumps({"version": 1, "segments": [seg]})


# ── a scene ──────────────────────────────────────────────────────────────────


def test_a_scene_refuses_both_data_sources(tmp_path):
    payload = tmp_path / "p.bin"
    payload.write_bytes(b"\xa5")
    scene = tmp_path / "scene.json"
    scene.write_text(
        _scene(data="1100", data_from_file=str(payload)), encoding="utf-8"
    )
    p = _run(["--from-file", str(scene)], tmp_path)
    assert p.returncode != 0, "a scene took two data sources"
    assert PAIR["json_why"] in p.stderr


def test_composer_from_json_refuses_with_the_scene_reason(tmp_path):
    payload = tmp_path / "p.bin"
    payload.write_bytes(b"\xa5")
    with pytest.raises(ValueError) as e:
        Composer.from_json(_scene(data="1100", data_from_file=str(payload)))
    assert str(e.value) == PAIR["json_why"]


def test_the_scene_and_the_cli_give_one_reason():
    """One declaration: the faces differ only in how they spell the pair."""
    tail = PAIR["cli_why"].split(": ", 1)[1]
    assert PAIR["json_why"].endswith(tail)
    assert PAIR["obj_why"].endswith(tail)


# ── help and schema ──────────────────────────────────────────────────────────


def test_help_says_each_flag_is_not_with_the_other():
    h = subprocess.run(
        [cli._runnable(), "--help"], capture_output=True, text=True
    ).stdout
    text = " ".join(h.split())
    assert re.search(r"--data FIELD .*? Not with [^.]*--data-from-file", text)


def test_the_schema_refuses_both_keys():
    from jsonschema import Draft202012Validator

    v = Draft202012Validator(
        json.loads(
            (ROOT / "docs/schema/wfmgen.schema.json").read_text("utf-8")
        )
    )
    both = _scene(data="1100", data_from_file="p.bin")
    assert not v.is_valid(json.loads(both))
    assert v.is_valid(json.loads(_scene(data="1100")))
    assert v.is_valid(json.loads(_scene(data_from_file="p.bin")))


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
        ["--type", "bits", "--data", "1100", "--sync", "0101*2"], tmp_path
    )
    assert p.returncode == 2
    assert _macro("WFM_SURFACE_REPS_WHY_CLI") in p.stderr


def test_a_scene_refuses_reps_off_the_preamble_with_the_generated_reason():
    with pytest.raises(ValueError) as e:
        Composer.from_json(_scene(data="1100*2"))
    assert str(e.value) == _macro("WFM_SURFACE_REPS_WHY_JSON")


def test_the_reps_reason_names_the_field_that_repeats():
    """`field_reps` decides which field repeats; the sentence follows it."""
    reps = [r for r in ROWS if r["field_reps"]]
    assert [r["name"] for r in reps] == ["acq_code"]
    assert "--acq-code" in _macro("WFM_SURFACE_REPS_WHY_CLI")
    assert '"acq_code"' in _macro("WFM_SURFACE_REPS_WHY_JSON")


# ── #1619 F6a: the data source's pair, and its surface-only row ──────────────
#
# `--data` (a Field, or a bit array in Python) and `--data-from-file` (a path,
# or `-` for stdin) are ONE exclusion, declared on `data`. The file row is
# SURFACE-ONLY: a path has no jm field type, and Python deliberately has no
# file face (docs/design/payload-data-source.md section 4.9), so the row is
# declared inside the exclusion and the generator checks it against the C
# struct in both directions.

DATA_PAIR = PAIR


def test_the_data_pair_is_one_declaration_with_a_surface_only_row():
    file_row = next(r for r in ROWS if r["name"] == "data_from_file")
    assert (
        file_row.get("surface_only") and file_row["kind"] == "WFM_SV_BESPOKE"
    )
    assert (file_row["cli"], file_row["json"]) == (
        "--data-from-file",
        "data_from_file",
    )
    assert "--data" in DATA_PAIR["cli_why"]
    assert '"data_from_file"' in DATA_PAIR["json_why"]


def _man():
    return GEN.tomllib.loads(GEN.MANIFEST.read_text(encoding="utf-8"))


def test_a_surface_only_row_must_name_a_real_member():
    """Forward: a row whose member is not in `wfm_source_t` is refused."""
    header = GEN.COMPOSE_H.read_text(encoding="utf-8")
    assert GEN.surface_only_errors(_man(), header) == []
    man = _man()
    for f in man["module"]["wfm_compose"]["source"]["fields"]:
        for e in f.get("exclusive", []):
            if "row" in e:
                e["row"]["member"] = "no_such_member"
    errs = GEN.surface_only_errors(man, header)
    assert any("no_such_member" in e for e in errs), errs


def test_a_path_member_must_have_a_surface_only_row():
    """Backward: a `const char *` member no row sets is refused."""
    header = GEN.COMPOSE_H.read_text(encoding="utf-8").replace(
        "} wfm_source_t;",
        "    const char *probe_path; /* A path no face can set. */\n"
        "} wfm_source_t;",
    )
    errs = GEN.surface_only_errors(_man(), header)
    assert any("probe_path" in e for e in errs), errs


def test_python_has_no_data_from_file_kwarg():
    """Section 4.9: a file is `cvt.bytes_to_bin` of its bytes, passed as
    `data`. A Python file face would be a second route to the same bits, so
    its absence is the design, pinned here so a "fix" that adds it goes red.
    """
    with pytest.raises(TypeError):
        Synth(type="bits", data_from_file="x.bin")


def test_a_scene_refuses_stdin_by_name():
    """A scene replays from its record; stdin's bytes are gone once read."""
    with pytest.raises(ValueError, match="stdin"):
        Composer.from_json(
            json.dumps(
                {
                    "version": 1,
                    "segments": [
                        {
                            "type": "bpsk",
                            "data_from_file": "-",
                            "data_len": 8,
                            "fill": "0",
                        }
                    ],
                }
            )
        )


def test_the_cli_refuses_the_data_pair_with_the_generated_reason(tmp_path):
    (tmp_path / "d.bin").write_bytes(b"\xab")
    p = _run(
        [
            "--type",
            "bpsk",
            "--data",
            "0xAB",
            "--data-from-file",
            str(tmp_path / "d.bin"),
        ],
        tmp_path,
    )
    assert p.returncode == 2
    assert DATA_PAIR["cli_why"] in p.stderr


def test_python_data_is_drawn_frame_by_frame():
    """Python's `data=` is a bit array, split into data_len-bit frames."""
    bits = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 4, np.uint8)  # 32 bits
    s = Synth(
        type="bits",
        modulation="none",
        sps=1,
        snr=200.0,
        data=bits,
        data_len=16,
    )
    x = s.steps(32).real
    assert np.array_equal((x > 0.5).astype(np.uint8), bits)


def test_python_data_on_a_dsss_burst_is_a_burst_per_chunk():
    """On a dsss burst, each burst is the preamble then the NEXT chunk's
    frame spread by the data code -- two chunks, two different bursts --
    and the run is those bursts (doppler#1719)."""
    acq = np.array([1, 0, 0, 1], np.uint8)
    code = np.array([1, 1, 0, 1], np.uint8)
    bits = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 4, np.uint8)  # 2 x 16
    kw = {
        "type": "dsss",
        "sps": 1,
        "snr": 200.0,
        "acq_code": acq,
        "acq_reps": 1,
        "data_code": code,
        "data": bits,
        "data_len": 16,
    }
    nch = 4 + 16 * 4
    x = Synth(**kw).steps(2 * nch + 8).real
    want = np.concatenate(
        [
            np.concatenate(
                [acq, (bits[b * 16 : (b + 1) * 16, None] ^ code).ravel()]
            )
            for b in range(2)
        ]
    )
    assert np.array_equal(x[: 2 * nch], np.where(want, -1.0, 1.0))
    assert not np.any(x[2 * nch :]), "silence once the data has ended"
    seg = Segment(**kw, fs=1e6)
    assert Composer([seg]).compose().size == 2 * nch


def test_python_data_on_continuous_dsss_is_a_bit_per_symbol():
    """Continuous dsss has no frame: each data symbol carries the next bit,
    chip n being the code XOR the bit of symbol floor(n / cps), and the run
    ends with the last symbol (doppler#1719)."""
    code = np.array([1, 1, 0, 1, 0], np.uint8)
    bits = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 2, np.uint8)
    fs, rate = 1e6, 1e6 / 3.7
    cps = (fs / 1.0) / rate
    kw = {
        "type": "dsss",
        "sps": 1,
        "snr": 200.0,
        "data_code": code,
        "symbol_rate": rate,
        "data": bits,
    }
    n = int(np.ceil(bits.size * cps))  # 60 chips: floor(59 / 3.7) = 15
    k = np.arange(n)
    want = code[k % code.size] ^ bits[(k / cps).astype(int)]
    x = Synth(**kw, fs=fs).steps(n + 8).real
    assert np.array_equal(x[:n], np.where(want, -1.0, 1.0))
    assert not np.any(x[n:]), "silence once the data has ended"
    assert Composer([Segment(**kw, fs=fs)]).compose().size == n
    with pytest.raises(ValueError, match="data_len does not apply"):
        Synth(**kw, fs=fs, data_len=8).steps(1)  # built at first use


def test_python_data_refuses_text():
    with pytest.raises(ValueError, match="field_bits"):
        Synth(type="bits", data="0xAB", data_len=8)


def test_a_field_doc_renders_its_brackets_literally():
    """options.md's doc cell is PROSE: brackets are notation, not a link.

    #1721's `data_len` doc, `[preamble x reps | sync | data:LEN | crc]`,
    rendered as an unresolved link reference and failed the strict docs
    build. The reference renderer escapes `[`/`]` outside code spans (and
    `|` everywhere, as a table cell needs), so no doc can do that again.
    """
    assert GEN._md_prose("x [a | b] y") == r"x \[a \| b\] y"
    # Inside a code span the brackets are already literal: left alone.
    assert GEN._md_prose("see `[a]` here") == "see `[a]` here"
    # The generator's own links (a type cell) are NOT prose and stay links.
    assert GEN._md("[Field](f.md)") == "[Field](f.md)"
