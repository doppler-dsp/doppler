"""The correlator rename codemod's exit rule (#2886 review).

The codemod refuses a table entry that matches nothing, because on the
first run that is a typo in the table. On a re-run the same rule refused a
tree the codemod had already renamed, so an entry now passes when its new
name is already present. Three trees pin the rule: one with a name left to
rename, the same tree again once renamed (nothing to rename, exit 0), and
one where an entry is neither found nor applied (exit 1, nothing written).
"""

import importlib.util
import subprocess

from doppler.tests._repo import repo_root

SCRIPT = repo_root() / "scripts" / "codemod_correlator_rename.py"


def _load():
    spec = importlib.util.spec_from_file_location("_codemod", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _repo(tmp_path, text):
    """A one-file git repo, its file in the codemod's scope."""
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    src = tmp_path / "native" / "src" / "x.c"
    src.parent.mkdir(parents=True)
    src.write_text(text, encoding="utf-8")
    subprocess.run(["git", "-C", str(tmp_path), "add", "."], check=True)
    return src


def _applied_tree(mod, skip=None):
    """Every entry's NEW name, so every entry reads as already applied,
    except `skip`, whose old and new names are both left out."""
    keep = [n for o, _, n in mod._table() if o != skip]
    if skip is not None:
        # A new name another entry shares would still read as applied.
        dropped = next(n for o, _, n in mod._table() if o == skip)
        keep = [n for n in keep if n != dropped]
    return "\n".join(keep) + "\n"


def test_rewrites_then_reports_nothing_to_rename(
    tmp_path, monkeypatch, capsys
):
    mod = _load()
    monkeypatch.setattr(mod, "ROOT", tmp_path)
    src = _repo(tmp_path, _applied_tree(mod) + "dp_corr2d_reset (s);\n")

    assert mod.main([]) == 0
    assert "dp_corr2d_reset" not in src.read_text(encoding="utf-8")
    assert "dp_correlator_reset (s);" in src.read_text(encoding="utf-8")

    capsys.readouterr()
    assert mod.main([]) == 0
    assert "nothing to rename" in capsys.readouterr().out


def test_an_entry_neither_found_nor_applied_fails(tmp_path, monkeypatch):
    mod = _load()
    monkeypatch.setattr(mod, "ROOT", tmp_path)
    # The fold type's new name is its own, so leaving it out leaves the
    # entry unapplied.
    text = _applied_tree(mod, skip="dp_corr2d_fold_t") + "dp_corr2d_reset;\n"
    src = _repo(tmp_path, text)

    assert mod.main([]) == 1
    assert src.read_text(encoding="utf-8") == text  # nothing written
