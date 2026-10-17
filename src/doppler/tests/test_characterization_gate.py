"""`check_characterization.py`'s artifact rule: no subject writes into the cwd.

Two subjects named their figure with a bare file name, so `make
characterize` wrote it into the repo root while a run from the subject's own
folder wrote it beside the script (doppler#2406). The gate's rule 4 refuses
an unanchored artifact name. These tests hold the rule's two halves: the
shapes it must refuse and the anchored ones it must pass, and that `main()`
actually applies it, on a throwaway tree, so a rule nothing calls cannot pass.
"""

from __future__ import annotations

import doctest
import sys

import pytest

from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
SCRIPTS = REPO / "scripts"


def _gate():  # type: ignore[no-untyped-def]
    """The gate as a module, for its rule and its entry point."""
    sys.path.insert(0, str(SCRIPTS))
    try:
        import check_characterization as g
    finally:
        sys.path.remove(str(SCRIPTS))
    return g


@pytest.mark.parametrize(
    "line",
    [
        # the two shapes that shipped (doppler#2406)
        'out_path = out_path or "dsss_burst_receiver_characterization.png"',
        'out_path = out_path or "burst_capture_characterization.png"',
        'fig.savefig("x.png")',
        'Path("run.json").write_text(s)',
        'open("pool.events", "w")',
        'fig.savefig(out or f"pool-{s}.png")',
    ],
)
def test_a_bare_artifact_name_is_refused(line):
    assert _gate().cwd_relative_artifacts(line), line


@pytest.mark.parametrize(
    "line",
    [
        'out_path = fromto(out_path or "dll_lock.png")',
        'fig.savefig(util.fromto("x.png", here))',
        'Capture(t, te, str(a.out / "timing.tlm"), clock)',
        'open(out / f"pool-{s}.png", "wb")',
        'name = "code.lock"',  # a probe name, not a file
        'print(f"wrote {out_path}")',
        'ROOT / "build/pool-telemetry"',
    ],
)
def test_an_anchored_or_non_file_string_passes(line):
    assert _gate().cwd_relative_artifacts(line) == [], line


def test_main_applies_the_rule(tmp_path, monkeypatch, capsys):
    g = _gate()
    src = tmp_path / "src" / "doppler"
    subject = src / "mod" / "tests" / "characterization" / "subj"
    subject.mkdir(parents=True)
    for d in (src, src / "mod", src / "mod" / "tests", subject.parent):
        (d / "__init__.py").write_text("")
    (subject / "__init__.py").write_text("")
    (subject / "characterize.py").write_text(
        "def main(out=None):\n"
        '    open(out or "subj.png", "wb")\n'
        'if __name__ == "__main__":\n'
        "    main()\n"
    )
    (src / "mod" / "tests" / "test_subj.py").write_text(
        "from doppler.mod.tests.characterization.subj.characterize import"
        " main\n"
    )
    monkeypatch.setattr(g, "ROOT", tmp_path)
    monkeypatch.setattr(g, "SRC", src)
    assert g.main() == 1
    err = capsys.readouterr().err
    assert "characterize.py:2: `subj.png` is relative to the working" in err


def test_main_reports_a_sibling_that_does_not_parse(
    tmp_path, monkeypatch, capsys
):
    # Rule 2 parses characterize.py only, so a broken sibling script was
    # skipped by rule 4 and reported by nothing. It is reported once.
    g = _gate()
    src = tmp_path / "src" / "doppler"
    subject = src / "mod" / "tests" / "characterization" / "subj"
    subject.mkdir(parents=True)
    for d in (src, src / "mod", src / "mod" / "tests", subject.parent):
        (d / "__init__.py").write_text("")
    (subject / "__init__.py").write_text("")
    (subject / "characterize.py").write_text(
        'if __name__ == "__main__":\n    pass\n'
    )
    (subject / "telemetry.py").write_text("def broken(:\n")
    (src / "mod" / "tests" / "test_subj.py").write_text(
        "import doppler.mod.tests.characterization.subj.characterize\n"
    )
    monkeypatch.setattr(g, "ROOT", tmp_path)
    monkeypatch.setattr(g, "SRC", src)
    assert g.main() == 1
    err = capsys.readouterr().err
    assert err.count("telemetry.py: does not parse") == 1


def test_docstring_examples_run():
    result = doctest.testmod(_gate())
    assert result.attempted > 0
    assert result.failed == 0
