"""The jm manifest's fragment layout holds: one module, one file.

Since the jm 0.114 fragment migration every ``[module.X]`` table lives in
``modules/X.toml``, which ``just-makeit.toml`` includes. Nothing in jm
stops a table from being written back into the root file (an old snippet
pasted in, a ``jm`` command run against a stale checkout), and the readers
would not notice: jm merges both. The layout would just stop being true.

So this holds it, both directions:

- the root file declares no ``[module.X]`` table;
- every ``modules/<name>.toml`` declares exactly one, ``[module.<name>]``,
  and its sub-tables.

The root is read as TOML, not grepped, so a ``[module.x]`` inside a comment
or a string does not count.
"""

import sys
from pathlib import Path

from doppler.tests._repo import repo_root

if sys.version_info >= (3, 11):
    import tomllib
else:  # pragma: no cover - the 3.9/3.10 CI matrix jobs
    import tomli as tomllib

# the checkout, from wherever the tests run (coverage runs a copy of them)
ROOT = repo_root()


def _read(path: Path) -> dict:
    return tomllib.loads(path.read_text(encoding="utf-8"))


def test_the_root_manifest_declares_no_module():
    root = _read(ROOT / "just-makeit.toml")
    assert "modules/*.toml" in root.get("include", []), (
        "just-makeit.toml no longer includes modules/*.toml"
    )
    stray = sorted(root.get("module", {}))
    assert not stray, (
        f"[module.X] table(s) in just-makeit.toml: {stray}. Each module "
        "lives in modules/<name>.toml; move the table there."
    )


def test_each_module_file_declares_its_own_module_only():
    files = sorted((ROOT / "modules").glob("*.toml"))
    assert files, "modules/ holds no manifest: the layout is gone"
    wrong = {}
    for f in files:
        data = _read(f)
        names = sorted(data.get("module", {}))
        others = sorted(k for k in data if k != "module")
        if names != [f.stem] or others:
            wrong[f.name] = {"module": names, "other": others}
    assert not wrong, (
        "a modules/<name>.toml must declare [module.<name>] and nothing "
        f"else: {wrong}"
    )
