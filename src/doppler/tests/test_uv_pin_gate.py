"""The uv pin gates, exercised over seeded trees and a copied lock.

uv.lock's bytes depend on the uv that writes it: a version bump re-locked by
uv 0.11.16 changed 2 lines, by 0.11.28 308 -- the v0.65.0 release commit
(doppler#1940). So uv is pinned once, in pyproject.toml's ``[tool.uv]
required-version``, and two gates hold it:

- ``scripts/check_uv_pin.py`` (``make lint-uv-pin``): the pin is exact, and
  every uv installer reads it. Each case seeds a tree and points ``--root``
  at it.
- ``make lint-uv-lock``: the committed uv.lock is exactly what the pinned uv
  writes. Each case copies the real lock into a scratch project and runs the
  real target there through ``UV="uv --directory <copy>"``, so the test
  exercises the Makefile's own LOCK_CMD rather than a restatement of it.
"""

from __future__ import annotations

import importlib.util
import os
import re
import shutil
import subprocess
import sys
from pathlib import PurePath
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_uv_pin.py"
COMPOSITE = ".github/actions/setup-uv/action.yml"
DOCKERFILE = "deploy/docker/Dockerfile"

PIN = '[tool.uv]\nrequired-version = "==0.12.24"\n'
GOOD_COMPOSITE = """\
runs:
  using: composite
  steps:
    - name: attempt 1
      uses: astral-sh/setup-uv@v9.0.0
      with:
        version-file: pyproject.toml
"""


def _seed(
    tmp_path: Path,
    pyproject: str = PIN,
    composite: str | None = GOOD_COMPOSITE,
    files: dict[str, str] | None = None,
    allow: str | None = "# the bare-uv allow list, empty\n",
) -> Path:
    """A project tree. ``allow`` is the bare-uv list's text (None: no list),
    unless ``files`` gives the list itself."""
    tmp_path.mkdir(parents=True, exist_ok=True)
    if allow is not None:
        files = {"scripts/.bare-uv-allow": allow, **(files or {})}
    (tmp_path / "pyproject.toml").write_text(
        '[project]\nname = "x"\n\n' + pyproject, encoding="utf-8"
    )
    if composite is not None:
        f = tmp_path / COMPOSITE
        f.parent.mkdir(parents=True)
        f.write_text(composite, encoding="utf-8")
    for rel, text in (files or {}).items():
        f = tmp_path / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text(text, encoding="utf-8")
    return tmp_path


def _load():
    """Import the gate by path (scripts/ is not a package)."""
    spec = importlib.util.spec_from_file_location("check_uv_pin", SCRIPT)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _run(root: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), *args],
        capture_output=True,
        text=True,
    )


# ── rule 1: the pin itself ───────────────────────────────────────────────────


def test_an_exact_pin_and_a_clean_tree_pass(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path))
    assert r.returncode == 0, r.stdout
    assert "uv 0.12.24" in r.stdout


@pytest.mark.parametrize(
    ("pyproject", "said"),
    [
        ("", "no [tool.uv] required-version"),
        # A range is not a pin: 0.11.16 and 0.11.28 both satisfy this one
        # and write different lock bytes.
        ('[tool.uv]\nrequired-version = ">=0.11,<0.12"\n', "not an exact"),
        ('[tool.uv]\nrequired-version = "0.12.24"\n', "not an exact"),
        # The key under some other table is no pin at all.
        ('[tool.other]\nrequired-version = "==0.12.24"\n', "no [tool.uv]"),
    ],
)
def test_a_missing_or_inexact_pin_is_refused(
    tmp_path: Path, pyproject: str, said: str
) -> None:
    r = _run(_seed(tmp_path, pyproject=pyproject))
    assert r.returncode == 1, r.stdout
    assert said in r.stdout


def test_a_uv_toml_pin_is_a_second_declaration(tmp_path: Path) -> None:
    """uv.toml outranks pyproject.toml for uv and setup-uv alike."""
    files = {"uv.toml": 'required-version = "==0.11.28"\n'}
    r = _run(_seed(tmp_path, files=files))
    assert r.returncode == 1, r.stdout
    assert "uv.toml" in r.stdout


def test_print_version_is_the_makefiles_reader(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path), "--print-version")
    assert (r.returncode, r.stdout) == (0, "0.12.24\n")
    bad = _run(_seed(tmp_path / "b", pyproject=""), "--print-version")
    assert bad.returncode == 1
    assert bad.stdout == ""


# ── rule 2: setup-uv has one home, and it names the pin file ─────────────────


def test_a_direct_setup_uv_in_a_workflow_is_named(tmp_path: Path) -> None:
    wf = (
        "jobs:\n  a:\n    steps:\n"
        "      - uses: actions/checkout@v7\n"
        "      - name: Install uv\n        uses: astral-sh/setup-uv@v7\n"
    )
    r = _run(_seed(tmp_path, files={".github/workflows/w.yml": wf}))
    assert r.returncode == 1, r.stdout
    assert ".github/workflows/w.yml: step 'Install uv' uses" in r.stdout


@pytest.mark.parametrize(
    ("with_block", "said"),
    [
        # setup-uv's own search falls back to the latest uv on a missing pin.
        ("      with:\n        python-version: '3.12'\n", "lacks"),
        ("", "lacks"),
        (
            "      with:\n        version-file: pyproject.toml\n"
            "        version: 0.12.24\n",
            "second copy",
        ),
    ],
)
def test_a_composite_step_must_name_the_pin_file(
    tmp_path: Path, with_block: str, said: str
) -> None:
    composite = (
        "runs:\n  using: composite\n  steps:\n"
        "    - name: attempt 1\n      uses: astral-sh/setup-uv@v9.0.0\n"
        + with_block
    )
    r = _run(_seed(tmp_path, composite=composite))
    assert r.returncode == 1, r.stdout
    assert said in r.stdout


def test_a_composite_that_installs_no_uv_is_refused(tmp_path: Path) -> None:
    composite = "runs:\n  using: composite\n  steps:\n    - run: true\n"
    r = _run(_seed(tmp_path, composite=composite))
    assert r.returncode == 1, r.stdout
    assert "installs no uv" in r.stdout


# ── rule 3: every other installer reads UV_VERSION ───────────────────────────


@pytest.mark.parametrize(
    ("rel", "text"),
    [
        # The manylinux leg's line, joined across its continuation.
        (
            ".github/workflows/r.yml",
            "x: |\n  $PYTHON -m pip install --quiet numpy \\\n    uv build\n",
        ),
        ("scripts/a.sh", "pip3 install 'uv>=0.12'\n"),
        # Escaped inside a double-quoted `bash -c "..."`.
        ("scripts/a.sh", 'bash -c "pip install \\"uv\\" build"\n'),
        ("scripts/a.sh", "pip install uv==0.12.24\n"),  # a literal copy
        ("scripts/a.sh", "pipx install uv\n"),
        ("scripts/a.sh", "uv tool install uv\n"),
        ("scripts/a.sh", "pip install uv; echo done\n"),
        ("scripts/a.sh", "uv self update\n"),
        (DOCKERFILE, "RUN curl -fsSL https://astral.sh/uv/install.sh | sh\n"),
        (
            DOCKERFILE,
            "RUN curl -fsSL https://astral.sh/uv/0.12.24/install.sh\n",
        ),
        (DOCKERFILE, "COPY --from=ghcr.io/astral-sh/uv:latest /uv /bin/\n"),
        (DOCKERFILE, "ARG UV_VERSION=0.12.24\n"),
    ],
)
def test_an_installer_that_does_not_read_the_pin_is_named(
    tmp_path: Path, rel: str, text: str
) -> None:
    r = _run(_seed(tmp_path, files={rel: text}))
    assert r.returncode == 1, r.stdout
    assert f"{rel}:" in r.stdout


@pytest.mark.parametrize(
    "text",
    [
        "pip install uv==${UV_VERSION}\n",
        # The manylinux leg's form: escaped for the host shell.
        'bash -euxc "pip install numpy uv==\\${UV_VERSION:?}"\n',
        'pip install "uv==$(UV_VERSION)"\n',
        'RUN : "${UV_VERSION:?}" \\\n && curl -fsSL '
        '"https://astral.sh/uv/${UV_VERSION}/install.sh" | sh\n',
        "COPY --from=ghcr.io/astral-sh/uv:${UV_VERSION} /uv /bin/\n",
        "uv self update ${UV_VERSION}\n",
        "ARG UV_VERSION\n",
        # Not uv: a package whose name merely starts with it, and uv used
        # to install something else.
        "pip install uvicorn uv-dynamic-versioning\n",
        'uv tool install --python python3 "just-makeit==${JM_VERSION}"\n',
        "# pip install uv -- a comment\n",
    ],
)
def test_an_installer_that_reads_the_pin_or_a_non_installer_passes(
    tmp_path: Path, text: str
) -> None:
    r = _run(_seed(tmp_path, files={DOCKERFILE: text}))
    assert r.returncode == 0, r.stdout


def test_prose_is_not_in_scope(tmp_path: Path) -> None:
    """A docs page may show `uv self update 0.12.24` to a reader."""
    files = {"docs/setup.md": "uv self update 0.12.24\n", "README.md": "x"}
    r = _run(_seed(tmp_path, files=files))
    assert r.returncode == 0, r.stdout


def test_the_live_tree_passes() -> None:
    r = subprocess.run(
        [sys.executable, str(SCRIPT)], capture_output=True, text=True
    )
    assert r.returncode == 0, r.stdout


# ── the artifact: uv.lock is the pinned uv's own output ──────────────────────


def _lock_gate(tmp_path: Path, edit: tuple[str, str] | None = None):
    """Run the real `make lint-uv-lock` against a copy of the project."""
    for name in ("pyproject.toml", "uv.lock", "README.md"):
        shutil.copy(REPO / name, tmp_path / name)
    if edit is not None:
        lock = tmp_path / "uv.lock"
        old, new = edit
        text = lock.read_text(encoding="utf-8")
        assert old in text, f"seed edit {old!r} no longer applies"
        lock.write_text(text.replace(old, new, 1), encoding="utf-8")
    # The uv that launched this test: `uv run` exports its own path as UV,
    # and under the pin no other uv could have run it. PATH is the fallback.
    uv = os.environ.get("UV") or shutil.which("uv")
    assert uv, "no uv found; run this through make"
    # POSIX spellings: the recipe runs under make's shell, where a Windows
    # path's backslashes would be eaten as escapes (ci.yml runs this suite
    # on windows-latest too).
    uv_cmd = f"{PurePath(uv).as_posix()} --directory {tmp_path.as_posix()}"
    return subprocess.run(
        ["make", "-s", "-C", str(REPO), "lint-uv-lock", f"UV={uv_cmd}"],
        capture_output=True,
        text=True,
    )


def test_the_committed_lock_is_the_pinned_uvs_output(tmp_path: Path) -> None:
    r = _lock_gate(tmp_path)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "lint-uv-lock: OK" in r.stdout


@pytest.mark.parametrize(
    "edit",
    [
        # An older uv's format revision: 0.11.x writes 3, 0.12.x writes 5.
        ("\nrevision = 5\n", "\nrevision = 3\n"),
        # The marker spelling #1940 is about: 0.11.16 spelled out a marker
        # the resolution fork already implies, and still resolves the same.
        (
            '{ name = "zipp" },',
            '{ name = "zipp", marker = "python_full_version < \'3.10\'" },',
        ),
    ],
)
def test_a_lock_no_pinned_uv_wrote_is_refused(
    tmp_path: Path, edit: tuple[str, str]
) -> None:
    r = _lock_gate(tmp_path, edit)
    assert r.returncode != 0, r.stdout
    assert "is not what uv" in r.stdout
    # Read-only: the probe must not have repaired the copy behind our back.
    assert edit[1] in (tmp_path / "uv.lock").read_text(encoding="utf-8")


# ── rule 4: a makefile runs uv through $(UV) (doppler#2369) ──────────────────

ALLOW = "scripts/.bare-uv-allow"


@pytest.mark.parametrize(
    "line",
    [
        "\tuv run python scripts/x.py\n",
        "\tuv sync --group dev\n",
        "X_CMD = uv run pytest -q\n",
        # A shell substitution is a bare call too.
        "\tcmake -DPy=$$(uv run python -c 'print(1)')\n",
        "\tcd sub && uv run --project $(CURDIR) tool\n",
        # make's ignore-errors prefix still runs it (#2370 review).
        "\t-uv sync\n",
        "\t@-uv run x\n",
        # A global flag before the subcommand.
        "\tuv -q sync\n",
        "\tuv --project . run x\n",
        "\tuv version\n",
        "X := $(shell uv run x)\n",
        # A continuation joins the call onto the logical line it starts.
        "\techo a \\\n\t  uv run x\n",
        # A shell comment ends at the newline, backslash or not: the next
        # physical line runs.
        "\t@# note \\\n\tuv run x\n",
    ],
)
def test_a_bare_uv_in_a_makefile_is_refused(tmp_path: Path, line: str) -> None:
    r = _run(_seed(tmp_path, files={"Makefile": "t:\n" + line}))
    assert r.returncode == 1, r.stdout
    assert "Makefile:2: bare uv" in r.stdout, r.stdout


def test_uv_through_the_variable_passes(tmp_path: Path) -> None:
    mk = (
        "UV = uv\n"
        "t:\n"
        "\t$(UV) run python scripts/x.py\n"
        "\t$(UV) sync --group dev\n"
        "\tcmake -DPy=$$($(UV) run python -c 'print(1)')\n"
        "\t@# a recipe comment may say uv sync\n"
        "\tmake foo-uv sync\n"
        "# so may a makefile comment: uv run pytest\n"
    )
    r = _run(_seed(tmp_path, files={"Makefile": mk, "sub/rules.mk": mk}))
    assert r.returncode == 0, r.stdout


def test_a_bare_uv_in_an_included_mk_is_refused(tmp_path: Path) -> None:
    r = _run(_seed(tmp_path, files={"mk/x.mk": "t:\n\tuv run tool\n"}))
    assert r.returncode == 1, r.stdout
    assert "mk/x.mk:2: bare uv" in r.stdout, r.stdout


def test_an_allowed_line_passes_and_needs_a_reason(tmp_path: Path) -> None:
    files = {
        "Makefile": "t:\n\tuv run keep-me\n",
        ALLOW: "# header\nuv run keep-me -- stays bare because of a reason\n",
    }
    assert _run(_seed(tmp_path / "ok", files=files)).returncode == 0
    files[ALLOW] = "uv run keep-me\n"
    r = _run(_seed(tmp_path / "noreason", files=files))
    assert r.returncode == 1, r.stdout
    assert "no reason" in r.stdout, r.stdout


def test_a_stale_allow_entry_fails(tmp_path: Path) -> None:
    """The list only shrinks: an entry whose line is gone is red."""
    files = {
        "Makefile": "t:\n\t$(UV) run keep-me\n",
        ALLOW: "uv run keep-me -- was bare until its fix landed\n",
    }
    r = _run(_seed(tmp_path, files=files))
    assert r.returncode == 1, r.stdout
    assert "matches no makefile line" in r.stdout, r.stdout


@pytest.mark.parametrize(
    "line",
    [
        "\tuvx diff-cover cov.lcov --fail-under=80\n",
        "\t$(UV) tool run diff-cover cov.lcov\n",
        "\tuv tool run diff-cover cov.lcov\n",
        "GATE_CMD = uvx some-tool --check\n",
        "\t-uvx some-tool\n",
        "\tuv -q tool run some-tool\n",
        # make's prefixes, and the two substitutions (#2374 review).
        "\t@uvx some-tool\n",
        "\t@-uvx some-tool\n",
        "\tv=$$(uvx some-tool --version)\n",
        "V := $(shell uvx some-tool --version)\n",
        # uv's other spellings: make's brace form, flags after `tool`,
        # and a uvx named by its path.
        "\t${UV} tool run some-tool\n",
        "\t$(UV) tool -q run some-tool\n",
        "\t$(HOME)/.local/bin/uvx some-tool\n",
        # uv by path, and a make variable that names it.
        "\t.venv/bin/uv tool run some-tool\n",
        "\t$(HOME)/.local/bin/uv tool run some-tool\n",
        "\t$(_PY_UV) tool run some-tool\n",
        "\t-uv tool run some-tool\n",
        # `--with` and its kin add a package the lock does not hold, among
        # uv's own options: after `run` (and its global flags), or after
        # $(DEV_RUN), and past an option's value.
        "\t$(UV) run --with diff-cover diff-cover cov.lcov\n",
        "\t$(DEV_RUN) --with some-tool some-tool\n",
        "\t$(DEV_RUN) -w some-tool some-tool\n",
        "\t$(UV) run --with=some-tool some-tool\n",
        "\t$(UV) -q run --with some-tool some-tool\n",
        "\t$(UV) run --group dev --with-requirements r.txt x\n",
        "\t$(_PY_UV) run --python 3.12 --with-editable ./x x\n",
        # past a hidden option or an alias that takes a value, which
        # `uv run --help` never lists (#2374 review)
        "\t$(UV) run --trusted-host pypi.org --with some-tool x\n",
        "\t$(UV) run --preview-features f --with some-tool x\n",
        "\t$(UV) run --preview-feature f --with some-tool x\n",
        "\t$(UV) run --python-preference system --with some-tool x\n",
        "\t$(UV) run --python-fetch automatic --with some-tool x\n",
        "\t$(UV) run --max-recursion-depth 3 --with some-tool x\n",
        "\t$(UV) run --config-settings k=v --with some-tool x\n",
        # short flags clustered, or a short option's value attached
        "\t$(UV) run -qw some-tool x\n",
        "\t$(UV) run -wsome-tool x\n",
        "\t$(UV) run -qp 3.12 --with some-tool x\n",
        # A recipe comment ends at the newline: the line after a `\` runs.
        "\t@# a note \\\n\tuvx some-tool\n",
    ],
)
def test_a_tool_run_outside_the_lock_is_refused(
    tmp_path: Path, line: str
) -> None:
    """uvx / `uv tool run` resolve the tool unpinned (doppler#2371): even a
    $(UV) spelling is refused -- the tool belongs in the dev group."""
    r = _run(_seed(tmp_path, files={"Makefile": "t:\n" + line}))
    assert r.returncode == 1, r.stdout
    assert "Makefile:2: runs a tool outside the lock" in r.stdout, r.stdout


def test_a_locked_tool_through_dev_run_passes(tmp_path: Path) -> None:
    mk = (
        "UV = uv\nDEV_RUN = $(UV) run --group dev\n"
        "t:\n\t$(DEV_RUN) diff-cover cov.lcov --fail-under=80\n"
        "# a comment may say uvx diff-cover\n"
        "\t@# so may a recipe comment: uvx diff-cover\n"
        # Names that only contain the word are not a run of it.
        "\tscripts/uvx_helper.sh\n\tscripts/uvx-helper.sh\n"
        "\tfoo-uvx --version\n\trm -f build/uvx.log\n"
        "\tfoo-uv tool run x\n\tmyuv tool run x\n"
        # An option after the command is the command's, not uv's.
        "\t$(UV) run pytest --with-coverage\n"
        "\t$(DEV_RUN) ./configure --with-openssl\n"
        "\t$(DEV_RUN) auditwheel repair x.whl -w dist/\n"
        "\t$(UV) run --frozen pytest -w dist\n"
        "\t$(UV) run --group dev --python 3.12 pytest --with x\n"
        "\t$(UV) run -q pytest -w dist\n"
        "\t$(UV) run -qp 3.12 pytest --with x\n"
        "\t$(UV) run -Ck=v pytest -w dist\n"
    )
    r = _run(_seed(tmp_path, files={"Makefile": mk}))
    assert r.returncode == 0, r.stdout


def test_an_allow_entry_cannot_exempt_a_tool_run(tmp_path: Path) -> None:
    """The bare-uv list exempts a bare uv, never a tool run outside the
    lock: the uvx check runs first, so the entry matches nothing."""
    files = {
        "Makefile": "t:\n\tuvx some-tool\n",
        ALLOW: "uvx some-tool -- an entry that tries to exempt it\n",
    }
    r = _run(_seed(tmp_path, files=files))
    assert r.returncode == 1, r.stdout
    assert "Makefile:2: runs a tool outside the lock" in r.stdout, r.stdout
    assert "matches no makefile line" in r.stdout, r.stdout


def test_an_allow_entry_matching_two_lines_fails(tmp_path: Path) -> None:
    """An entry exempts exactly one line, so a short one cannot cover many."""
    files = {
        "Makefile": "t:\n\tuv run a\n\tuv run b\n",
        ALLOW: "uv run -- too short to name one line\n",
    }
    r = _run(_seed(tmp_path, files=files))
    assert r.returncode == 1, r.stdout
    assert "matches 2 lines" in r.stdout, r.stdout


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
        cwd=root,
        check=True,
        capture_output=True,
    )


def _ratchet_repo(tmp_path: Path, base_allow: str | None) -> Path:
    """A checkout whose `base` branch holds ``base_allow`` as the list
    (None: no list there), and whose HEAD is one commit past it."""
    files = {"Makefile": "t:\n\tuv run keep-me\n\tuv run new-one\n"}
    if base_allow is not None:
        files[ALLOW] = base_allow
    root = _seed(tmp_path, files=files, allow=None)
    _git(root, "init", "-q", "-b", "base")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "base")
    _git(root, "checkout", "-q", "-b", "head")
    return root


def test_an_allow_entry_the_base_lacks_is_refused(tmp_path: Path) -> None:
    """The list only shrinks: a new entry fails even with a reason."""
    keep = "uv run keep-me -- an old entry\n"
    root = _ratchet_repo(tmp_path, keep)
    (root / ALLOW).write_text(
        keep + "uv run new-one -- a new entry\n", encoding="utf-8"
    )
    _git(root, "commit", "-q", "-am", "grow the list")
    r = _run(root, "--base", "base")
    assert r.returncode == 1, r.stdout
    assert "`uv run new-one` is not in the list at the merge base" in (
        r.stdout
    ), r.stdout
    assert "keep-me` is not" not in r.stdout, r.stdout


def test_the_change_that_brings_the_list_adds_nothing(tmp_path: Path) -> None:
    """No list at the base: the first list is its own baseline."""
    root = _ratchet_repo(tmp_path, None)
    (root / ALLOW).parent.mkdir(parents=True)
    (root / ALLOW).write_text(
        "uv run keep-me -- one\nuv run new-one -- two\n", encoding="utf-8"
    )
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "bring the list")
    r = _run(root, "--base", "base")
    assert r.returncode == 0, r.stdout


def test_an_unreadable_base_fails_closed(tmp_path: Path) -> None:
    both = "uv run keep-me -- one\nuv run new-one -- two\n"
    root = _ratchet_repo(tmp_path, both)
    assert _run(root, "--base", "base").returncode == 0
    r = _run(root, "--base", "no-such-ref")
    assert r.returncode == 1, r.stdout
    assert "cannot resolve no-such-ref" in r.stdout, r.stdout


def test_a_missing_list_is_refused(tmp_path: Path) -> None:
    """Absent is not empty: a deletable list could come back as a new
    baseline (#2370 review, the #1976 hole)."""
    r = _run(_seed(tmp_path, files={"Makefile": "t:\n"}, allow=None))
    assert r.returncode == 1, r.stdout
    assert ".bare-uv-allow: missing" in r.stdout, r.stdout


def test_deleting_the_list_then_recreating_it_with_an_entry_fails(
    tmp_path: Path,
) -> None:
    """The base holds an emptied list. Deleting it is refused; recreating it
    with an entry the base lacks is refused as an addition."""
    root = _ratchet_repo(tmp_path, "# emptied\n")
    (root / "Makefile").write_text("t:\n\t$(UV) run x\n", encoding="utf-8")
    _git(root, "rm", "-q", ALLOW)
    _git(root, "commit", "-q", "-am", "delete the list")
    r = _run(root, "--base", "base")
    assert r.returncode == 1, r.stdout
    assert ".bare-uv-allow: missing" in r.stdout, r.stdout

    (root / "Makefile").write_text("t:\n\tuv run new-one\n", encoding="utf-8")
    (root / ALLOW).parent.mkdir(exist_ok=True)
    (root / ALLOW).write_text("uv run new-one -- back\n", encoding="utf-8")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "recreate it")
    r = _run(root, "--base", "base")
    assert r.returncode == 1, r.stdout
    assert "`uv run new-one` is not in the list at the merge base" in (
        r.stdout
    ), r.stdout


def test_the_valued_run_options_are_the_pinned_uvs():
    """RUN_VALUED tells uv's own options from the command's, so it must be
    the set of value-taking options the pinned uv's `uv run --help` lists:
    an option missing from it would end the scan early and miss a
    `--with` after it. The uv asked is the one `make test-python` runs
    pytest under (`$UV`), else the one on PATH -- the coverage job runs
    pytest directly -- and either must be the pin, or the help is the
    wrong one to compare with."""
    gate = _load()
    uv = os.environ.get("UV") or shutil.which("uv")
    assert uv, "no uv: run through `make test-python`, or put uv on PATH"
    pin, problem = gate.read_pin(REPO)
    assert pin, problem
    ver = subprocess.run(
        [uv, "--version"], capture_output=True, text=True, check=True
    ).stdout.split()[1]
    assert ver == pin, f"{uv} is uv {ver}, not the pin {pin}"
    out = subprocess.run(
        [uv, "run", "--help"], capture_output=True, text=True, check=True
    ).stdout
    valued = set()
    for m in re.finditer(
        r"^\s+(?:(-\w), )?(--[\w-]+)[ =]<[^>]+>", out, re.MULTILINE
    ):
        valued.update(x for x in m.groups() if x)
    assert valued, out[:400]
    assert set(gate.RUN_VALUED) == valued
    # The unlisted spellings are the ones the help hides: once the help
    # shows one, it belongs in RUN_VALUED instead.
    assert not (gate.RUN_VALUED_UNLISTED & valued)


def test_the_unlisted_options_were_read_from_the_pinned_uv():
    """`uv run --help` hides RUN_VALUED_UNLISTED's spellings, so the test
    above cannot see a hidden option a newer uv adds. The set names the uv
    it was read from, and a pin bump fails here until someone moves the
    version -- which is the moment to re-read that uv's CLI source
    (crates/uv-cli/src/lib.rs). It cannot make them read it."""
    gate = _load()
    pin, problem = gate.read_pin(REPO)
    assert pin, problem
    assert pin == gate.RUN_VALUED_UNLISTED_UV, (
        f"the uv pin is {pin}; scripts/check_uv_pin.py's "
        f"RUN_VALUED_UNLISTED was read from uv "
        f"{gate.RUN_VALUED_UNLISTED_UV}'s crates/uv-cli/src/lib.rs. Read "
        f"uv {pin}'s copy for the hidden and alias `uv run` options that "
        "take a value, update the set, then RUN_VALUED_UNLISTED_UV."
    )
