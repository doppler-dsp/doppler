"""`make ci-changes` skips the matrix for a release that moves a derived token.

doppler's release PR (#3316, base 7725738f1, head 8d0f23ec6) was a version
bump in every manifest and in the docs, and the bump check still answered
src=true: docs/install/c.md names the Linux package with the ABI major.minor
(`libdoppler-dsp0.85_0.85.0_amd64.deb`), which a release moves to `0.86`. The
check substitutes the full version only. CI_DERIVED_VERSION_TOKENS, set in the
Makefile, names the template (`libdoppler-dsp{major}.{minor}`) so that token is
accepted (just-buildit.github.io#240; doppler #3317).

Each case seeds a small git repository with the REAL Makefile, standard.mk and
scripts/ (the recipe reads its version through them), plus the three files
that decide the release: pyproject.toml (the version), docs/install/c.md (the
two package lines this release changed, and the rest of the file's version
strings), and CHANGELOG.md (inert). The rest of the release's 117 files are
the same classes: 30 inert files and 86 version-only lines.

Sabotages, each red:
  - the Makefile without CI_DERIVED_VERSION_TOKENS: the release answers
    src=true, naming docs/install/c.md;
  - a bare `0.85 seconds` line that changes with the release: still src=true
    (the token is accepted as written, never a bare number).
"""

from __future__ import annotations

import re
import shutil
import subprocess
from typing import TYPE_CHECKING

from doppler.tests._repo import nested_make_env, repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
OLD, NEW = "0.85.0", "0.86.0"

# This release's two package lines in docs/install/c.md, at base and head.
C_BASE = (
    "Install the package:\n\n"
    "```sh\n"
    'curl -fL -O "$base/libdoppler-dsp0.85_0.85.0_amd64.deb" '
    '-O "$base/libdoppler-dsp-dev_0.85.0_amd64.deb"\n'
    "sudo apt install ./libdoppler-dsp0.85_0.85.0_amd64.deb\n"
    "```\n\n"
    "Source: https://github.com/doppler-dsp/doppler/blob/v0.85.0/README.md\n"
)
C_HEAD = (
    C_BASE.replace("libdoppler-dsp0.85_0.85.0", "libdoppler-dsp0.86_0.86.0")
    .replace("dsp-dev_0.85.0", "dsp-dev_0.86.0")
    .replace("v0.85.0", "v0.86.0")
)


def _git(cwd: Path, *args: str) -> str:
    return subprocess.run(
        ["git", "-c", "user.email=t@t", "-c", "user.name=t", *args],
        cwd=cwd,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def _seed(
    tmp_path: Path,
    *,
    makefile: str | None = None,
    extra_base: str = "",
    extra_head: str = "",
) -> tuple[Path, str]:
    """A repo at base, the same repo at head, and the base sha to diff from."""
    root = tmp_path / "repo"
    root.mkdir()
    shutil.copy(REPO / "standard.mk", root / "standard.mk")
    shutil.copytree(REPO / "scripts", root / "scripts")
    mk = (
        makefile
        if makefile is not None
        else (REPO / "Makefile").read_text(encoding="utf-8")
    )
    (root / "Makefile").write_text(mk, encoding="utf-8")
    pyproject = (REPO / "pyproject.toml").read_text(encoding="utf-8")
    (root / "pyproject.toml").write_text(
        re.sub(r'(?m)^version = "[^"]+"', f'version = "{OLD}"', pyproject),
        encoding="utf-8",
    )
    (root / "docs" / "install").mkdir(parents=True)
    (root / "docs" / "install" / "c.md").write_text(
        C_BASE + extra_base, encoding="utf-8"
    )
    (root / "CHANGELOG.md").write_text("# base\n", encoding="utf-8")
    _git(root, "init", "-q", ".")
    _git(root, "add", "-A")
    _git(root, "commit", "-qm", "base")
    base = _git(root, "rev-parse", "HEAD")
    (root / "pyproject.toml").write_text(
        re.sub(r'(?m)^version = "[^"]+"', f'version = "{NEW}"', pyproject),
        encoding="utf-8",
    )
    (root / "docs" / "install" / "c.md").write_text(
        C_HEAD + extra_head, encoding="utf-8"
    )
    (root / "CHANGELOG.md").write_text("# head\n", encoding="utf-8")
    _git(root, "commit", "-qam", "release")
    return root, base


def _ci_changes(root: Path, base: str) -> subprocess.CompletedProcess[str]:
    # The outer make (`make test-python`) exports the REAL Makefile's
    # CI_DERIVED_VERSION_TOKENS into this process. standard.mk reads it with
    # `?=`, so the seeded Makefile's missing line would be silently supplied by
    # the environment. The token must come from the seeded Makefile alone, and
    # the outer make's flags must not leak in: drop both (nested_make_env drops
    # MAKEFLAGS/MFLAGS/MAKELEVEL).
    env = nested_make_env("CI_DERIVED_VERSION_TOKENS", "GITHUB_OUTPUT")
    return subprocess.run(
        ["make", "-s", "ci-changes", f"BASE={base}"],
        cwd=root,
        env=env,
        capture_output=True,
        text=True,
    )


def test_a_release_that_moves_the_abi_token_is_a_bump(tmp_path: Path) -> None:
    """The release's own shape: the derived token is accepted, so src=false."""
    root, base = _seed(tmp_path)
    r = _ci_changes(root, base)
    assert "src=false" in r.stdout, r.stdout + r.stderr
    assert "a version bump alone" in r.stderr, r.stderr


def test_without_the_token_the_release_is_not_a_bump(tmp_path: Path) -> None:
    """The sabotage: the Makefile without CI_DERIVED_VERSION_TOKENS. The
    package line is then a change the full-version substitution cannot
    explain, and the answer is src=true naming that file."""
    mk = (REPO / "Makefile").read_text(encoding="utf-8")
    without = re.sub(r"(?m)^CI_DERIVED_VERSION_TOKENS = .*\n", "", mk)
    assert without != mk, "the sabotage must remove the line"
    root, base = _seed(tmp_path, makefile=without)
    r = _ci_changes(root, base)
    assert "src=true" in r.stdout, r.stdout + r.stderr
    assert "docs/install/c.md changes more than the version string" in r.stderr


def test_a_bare_number_is_not_accepted(tmp_path: Path) -> None:
    """The control: `took 0.85 seconds` moving with the release must still be
    src=true. The token accepted is the configured template as written."""
    root, base = _seed(
        tmp_path,
        extra_base="It took 0.85 seconds.\n",
        extra_head="It took 0.86 seconds.\n",
    )
    r = _ci_changes(root, base)
    assert "src=true" in r.stdout, r.stdout + r.stderr
    assert "docs/install/c.md changes more than the version string" in r.stderr
