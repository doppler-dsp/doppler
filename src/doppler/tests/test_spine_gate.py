"""The spine gate: every row of the lifecycle card, taken away in turn.

``scripts/check_spine.py`` (``make spine-check``) prints, for each object a
branch touches, which of the card's phases are present, and fails on a gap
nobody declared (doppler#3140). These cases run the real script on a
scratch repository holding one complete object, ``widget``, under the real
card, and remove one artifact at a time.

Every assertion here has its negative control beside it, as
``docs/dev/contributing/validation.md`` ("Three layers that prove a pin")
asks: the same question put to a named wrong case, which must answer the
other way. ``complete`` is that case for every removal: the untouched tree,
judged once, on which each expected failure is shown not to happen.
"""

from __future__ import annotations

import importlib.util
import re
import shutil
import subprocess
import sys
from typing import TYPE_CHECKING, NamedTuple

import pytest

from doppler.tests._platform import skip_module_posix_only
from doppler.tests._repo import repo_root

skip_module_posix_only("execs issue-link-check.sh and make, as lint does")

if TYPE_CHECKING:
    from collections.abc import Callable
    from pathlib import Path
    from types import ModuleType

    Branch = Callable[..., Path]

REPO = repo_root(__file__)
SCRIPTS = REPO / "scripts"
SCRIPT = SCRIPTS / "check_spine.py"
PAGE = "docs/dev/contributing/adding-algorithms.md"
LOG = "docs/dev/contributing/validation-log.md"
RATCHET = "scripts/.spine-gaps"
BASE = ("--base", "main")
REPORT = "src/doppler/sig/tests/validation/widget/results.md"
BLOB = "https://github.com/doppler-dsp/doppler/blob/main"
FRAGMENT = "changelog.d/added/widget.md"
CLOSES = "feat: the widget\n\nCloses #1\n"
ALL_PRESENT = {
    "Why": "present",
    "Declare": "present",
    "Implement": "present",
    "Pin": "present",
    "Bind": "present",
    "Instrument": "present",
    "Explore": "present",
    "Certify": "present",
    "User story": "present",
    "Land": "present",
}

#: The object, complete: one file per artifact a row looks for.
TREE = {
    "just-makeit.toml": 'include = ["objects/*.toml", "modules/*.toml"]\n\n'
    '[project]\nname = "doppler"\nversion = "0.1.0"\n',
    "modules/sig.toml": '[module.sig]\nobjects = ["widget"]\n',
    "objects/widget.toml": '[widget]\nclass_name = "Widget"\n'
    'serializable = "true"\n\n[[widget.methods]]\nname = "stats"\n'
    'single = true\nrecord_name = "WidgetStats"\n',
    "docs/design/widget.md": "# Widget\n\nWhy `Widget` is what it is.\n",
    "native/inc/doppler/widget/widget_core.h": "/* the contract */\n",
    "native/src/widget/widget_core.c": "/* the algorithm */\n",
    "native/tests/widget/test_widget_core.c": "/* the pins */\n",
    "native/src/sig/sig_ext_widget.c": "/* jm:generated */\n",
    "src/doppler/sig/__init__.py": '__all__ = ["Widget"]\n',
    "src/doppler/sig/sig.pyi": "class Widget: ...\n",
    "native/validation/widget_sweep.c": "/* the envelope */\n",
    REPORT: "# WIDGET — validation report\n",
    LOG: "# Log\n\n<!-- validation-log:start -->\n\n"
    f"| [WIDGET]({BLOB}/{REPORT}) | `sig` |\n\n"
    "<!-- validation-log:end -->\n",
    "docs/guide/widgets.md": "# Widgets\n\nUse `Widget` to ...\n",
    "src/doppler/examples/widget_demo.py": "from doppler.sig import Widget\n"
    "w = Widget()\n",
    "src/doppler/examples/.no-figure": "widget_demo.py: prints one number\n",
    "native/examples/widget_demo.c": "#include <doppler/widget/widget_core.h>"
    "\nint main (void) { return 0; }\n",
    "native/benchmarks/widget/bench_widget_core.c": "/* the C row */\n",
    "src/doppler/sig/benchmarks/bench_widget.py": "from doppler.sig import "
    "Widget\n\n\ndef test_widget(benchmark):\n    benchmark(Widget)\n",
    # `make changelog-check` as the gate reads it: a verdict line and an
    # exit code. The stub passes when the branch added a fragment.
    # The gate's own file, so the base has it and the ratchet has a
    # baseline: a list that arrives WITH its gate is not growth.
    "scripts/check_spine.py": "# the gate\n",
    "Makefile": "changelog-check:\n"
    "\t@if git diff --name-only $(CHANGELOG_BASE)...HEAD | grep -q "
    "'^changelog.d/'; then echo 'changelog-check: touched -- OK'; "
    "else echo 'changelog-check: says nothing -- FAIL'; exit 1; fi\n",
}


class Run(NamedTuple):
    """One run of the gate: its exit code, both streams, widget's rows."""

    rc: int
    out: str
    err: str
    states: dict[str, str]
    cells: dict[str, str]


def _w(root: Path, rel: str, text: str) -> None:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")


def _git(root: Path, *args: str) -> str:
    done = subprocess.run(
        ["git", "-c", "user.email=t@t", "-c", "user.name=t", *args],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    return done.stdout.strip()


def _card() -> str:
    """The real lifecycle page, so the fixture runs under the real card."""
    return (REPO / PAGE).read_text(encoding="utf-8")


def _gate(root: Path, *flags: str) -> Run:
    done = subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root), *BASE, *flags],
        capture_output=True,
        text=True,
        check=False,
    )
    states: dict[str, str] = {}
    cells: dict[str, str] = {}
    table = done.stdout.partition("### `widget`")[2]
    for line in table.splitlines():
        m = re.match(r"\| \d+ \| ([^|]+) \| \**([a-z/]+)\** \| (.*) \|", line)
        if m:
            states[m[1].strip()] = m[2]
            cells[m[1].strip()] = m[3]
    return Run(done.returncode, done.stdout, done.stderr, states, cells)


@pytest.fixture(scope="module")
def pristine(tmp_path_factory: pytest.TempPathFactory) -> Path:
    """A repository whose ``main`` holds the complete object."""
    root = tmp_path_factory.mktemp("spine") / "repo"
    for rel, text in TREE.items():
        _w(root, rel, text)
    _w(root, PAGE, _card())
    _git(root, "init", "-q", "-b", "main")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "chore: the tree\n\nNo-issue:\n")
    return root


@pytest.fixture
def branch(pristine: Path, tmp_path: Path) -> Branch:
    """Make a copy of the repository with one commit on a work branch.

    ``before`` edits ``main`` first (a gap that predates the branch);
    ``change`` edits the branch. The commit always touches the widget's
    source, so the widget is the object the branch touches, and adds a
    changelog fragment unless ``fragment`` is False. Each call makes a
    repository of its own, so a test builds its case and its control.
    """
    made: list[None] = []

    def make(
        change: Callable[[Path], None] | None = None,
        *,
        msg: str = CLOSES,
        fragment: bool = True,
        before: Callable[[Path], None] | None = None,
    ) -> Path:
        made.append(None)
        root = tmp_path / f"repo{len(made)}"
        shutil.copytree(pristine, root)
        if before:
            before(root)
            _git(root, "add", "-A")
            _git(root, "commit", "-q", "-m", "chore: before\n\nNo-issue:\n")
        _git(root, "checkout", "-q", "-b", "work")
        _w(root, "native/src/widget/widget_core.c", "/* the change */\n")
        if fragment:
            _w(root, FRAGMENT, "- **Widget.** It widgets.\n")
        if change:
            change(root)
        _git(root, "add", "-A")
        _git(root, "commit", "-q", "-m", msg)
        return root

    return make


@pytest.fixture(scope="module")
def complete(pristine: Path, tmp_path_factory: pytest.TempPathFactory) -> Run:
    """The negative control of every removal: the untouched branch."""
    root = tmp_path_factory.mktemp("complete") / "repo"
    shutil.copytree(pristine, root)
    _git(root, "checkout", "-q", "-b", "work")
    _w(root, "native/src/widget/widget_core.c", "/* the change */\n")
    _w(root, FRAGMENT, "- **Widget.** It widgets.\n")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", CLOSES)
    return _gate(root)


def _rm(rel: str) -> Callable[[Path], None]:
    return lambda root: (root / rel).unlink()


def _write(rel: str, text: str) -> Callable[[Path], None]:
    return lambda root: _w(root, rel, text)


def test_a_complete_object_reads_all_present(complete: Run) -> None:
    assert complete.rc == 0, complete.out + complete.err
    assert complete.states == ALL_PRESENT
    # Control: the comparison can fail. A table with one row changed is
    # not the complete one.
    assert {**complete.states, "Pin": "missing"} != ALL_PRESENT


def _prose_only(root: Path) -> None:
    """A page that says the word and names nothing: the bare object name
    in backticks, and the class outside them."""
    (root / "docs/design/widget.md").unlink()
    _w(root, "docs/design/notes.md", "A `widget` and a Widget.\n")


#: (what is taken away, the rows that go missing, a word of the failure).
#: One row each, except Declare: an object no module lists renders into no
#: package, so it has no class either and Bind goes with it.
REMOVALS = [
    ("design-page", _rm("docs/design/widget.md"), {"Why"}, "why/design-page"),
    (
        "module-entry",
        _write("modules/sig.toml", "[module.sig]\nobjects = []\n"),
        {"Declare", "Bind"},
        "declare/module",
    ),
    (
        "header",
        _rm("native/inc/doppler/widget/widget_core.h"),
        {"Implement"},
        "implement/header",
    ),
    (
        "c-test",
        _rm("native/tests/widget/test_widget_core.c"),
        {"Pin"},
        "pin/c-test",
    ),
    (
        "fragment",
        _rm("native/src/sig/sig_ext_widget.c"),
        {"Bind"},
        "bind/fragment",
    ),
    (
        "class",
        _write("src/doppler/sig/__init__.py", "__all__ = []\n"),
        {"Bind"},
        "bind/class",
    ),
    (
        "state-stance",
        _write(
            "objects/widget.toml",
            TREE["objects/widget.toml"].replace('serializable = "true"\n', ""),
        ),
        {"Instrument"},
        "instrument/state",
    ),
    (
        "validator",
        _rm("native/validation/widget_sweep.c"),
        {"Explore"},
        "explore/sweep",
    ),
    ("report", _rm(REPORT), {"Certify"}, "certify/report"),
    (
        "log-row",
        _write(LOG, TREE[LOG].replace("widget/results.md", "x/results.md")),
        {"Certify"},
        "certify/log-row",
    ),
    (
        "guide",
        _write("docs/guide/widgets.md", "# Widgets\n\nNothing named.\n"),
        {"User story"},
        "user-story/guide",
    ),
    (
        "py-example",
        _write("src/doppler/examples/widget_demo.py", "print('hello')\n"),
        {"User story"},
        "user-story/py-example",
    ),
    (
        "gallery-decision",
        _write("src/doppler/examples/.no-figure", "# none\n"),
        {"User story"},
        "user-story/gallery",
    ),
    (
        "c-example",
        _rm("native/examples/widget_demo.c"),
        {"User story"},
        "user-story/c-example",
    ),
    (
        "c-example-of-a-neighbor",
        _write(
            "native/examples/widget_demo.c",
            "#include <doppler/widget_bank/widget_bank_core.h>\n",
        ),
        {"User story"},
        "user-story/c-example",
    ),
    (
        "py-bench-of-a-neighbor",
        _write(
            "src/doppler/sig/benchmarks/bench_widget.py",
            "from doppler.sig import Gadget\n",
        ),
        {"User story"},
        "user-story/py-bench",
    ),
    ("design-page-in-prose-only", _prose_only, {"Why"}, "why/design-page"),
    (
        "c-bench",
        _rm("native/benchmarks/widget/bench_widget_core.c"),
        {"User story"},
        "user-story/c-bench",
    ),
    (
        "py-bench",
        _rm("src/doppler/sig/benchmarks/bench_widget.py"),
        {"User story"},
        "user-story/py-bench",
    ),
]


@pytest.mark.parametrize(
    ("change", "rows", "key"),
    [c[1:] for c in REMOVALS],
    ids=[c[0] for c in REMOVALS],
)
def test_removing_an_artifact_turns_its_row_missing(
    branch: Branch,
    complete: Run,
    change: Callable[[Path], None],
    rows: set[str],
    key: str,
) -> None:
    r = _gate(branch(change))
    missing = {p for p, s in r.states.items() if s == "missing"}
    assert missing == rows, r.out
    assert r.rc == 1
    assert f"widget {key}:" in r.err
    # Control: with the artifact in place none of the three holds.
    assert complete.rc == 0
    assert not {p for p, s in complete.states.items() if s == "missing"}
    assert f"widget {key}:" not in complete.err


def _private(imported: bool) -> Callable[[Path], None]:
    """The object's class renamed ``_Widget``: private, so not in
    ``__all__`` (just-makeit gh-2607), and imported by the package or not."""

    def change(root: Path) -> None:
        toml = (root / "objects/widget.toml").read_text(encoding="utf-8")
        _w(root, "objects/widget.toml", toml.replace('"Widget"', '"_Widget"'))
        line = "from .sig import _Widget\n\n" if imported else ""
        _w(root, "src/doppler/sig/__init__.py", line + "__all__ = []\n")

    return change


def test_a_private_class_is_bound_by_its_import(branch: Branch) -> None:
    r = _gate(branch(_private(imported=True)))
    assert r.states["Bind"] == "present", r.out
    assert "widget bind/class:" not in r.err
    # Control: the same private class with no import line is not bound.
    c = _gate(branch(_private(imported=False)))
    assert c.states["Bind"] == "missing", c.out
    assert "widget bind/class:" in c.err


LAND = [
    ("no-changelog", {"fragment": False}, "land/changelog"),
    ("no-issue-line", {"msg": "feat: the widget\n"}, "land/issue-link"),
]


@pytest.mark.parametrize(
    ("how", "key"), [c[1:] for c in LAND], ids=[c[0] for c in LAND]
)
def test_a_branch_that_does_not_land_turns_land_missing(
    branch: Branch, complete: Run, how: dict[str, object], key: str
) -> None:
    r = _gate(branch(**how))
    assert {p for p, s in r.states.items() if s == "missing"} == {"Land"}
    assert r.rc == 1
    assert f"widget {key}:" in r.err
    # Control: the branch with its fragment and its `Closes` lands.
    assert complete.states["Land"] == "present"
    assert f"widget {key}:" not in complete.err


SKIP = "Spine-skip: Explore widget: no sweep yet (#12)"
LINE = "widget explore/sweep: no sweep yet (#12)\n"


def _no_sweep(root: Path) -> None:
    (root / "native/validation/widget_sweep.c").unlink()


def _no_sweep_recorded(root: Path) -> None:
    _no_sweep(root)
    _w(root, RATCHET, LINE)


@pytest.mark.parametrize("phase", ["Explore", "explore", "7"])
def test_a_declared_and_recorded_gap_reads_skipped(
    branch: Branch, phase: str
) -> None:
    msg = f"{CLOSES}\n{SKIP.replace('Explore', phase)}\n"
    r = _gate(branch(_no_sweep_recorded, msg=msg))
    assert r.rc == 0, r.err
    assert r.states["Explore"] == "skipped"
    assert "no sweep yet (#12)" in r.cells["Explore"]
    # Control: the same tree and line with no trailer is the ratchet
    # growing in silence, and is refused.
    silent = _gate(branch(_no_sweep_recorded))
    assert silent.rc == 1
    assert silent.states["Explore"] == "missing"
    assert "is new; the list only shrinks" in silent.err


def test_a_declared_gap_must_be_recorded(branch: Branch) -> None:
    """A trailer is read from the branch's commits, and there are none
    once it merges: the ratchet line is what holds the gap afterwards."""
    r = _gate(branch(_no_sweep, msg=f"{CLOSES}\n{SKIP}\n"))
    assert r.rc == 1
    assert r.states["Explore"] == "skipped"
    assert f"add to {RATCHET}: `{LINE.strip()}`" in r.err
    # Control: with the line it names, the same branch passes.
    ok = _gate(branch(_no_sweep_recorded, msg=f"{CLOSES}\n{SKIP}\n"))
    assert ok.rc == 0, ok.err
    assert "not recorded" not in ok.err


def test_a_land_skip_needs_no_ratchet_line(branch: Branch) -> None:
    """Land is a fact about the branch; nothing is left in the tree."""
    msg = f"{CLOSES}\nSpine-skip: Land widget: internal only (#5)\n"
    r = _gate(branch(msg=msg, fragment=False))
    assert r.rc == 0, r.err
    assert r.states["Land"] == "skipped"
    assert "internal only (#5)" in r.cells["Land"]
    # Control: a skip of another phase does not cover Land.
    other = msg.replace("Land widget", "Why widget")
    assert _gate(branch(msg=other, fragment=False)).rc == 1


MALFORMED = [
    ("no-issue", "Spine-skip: Explore widget: no sweep yet", "malformed"),
    ("no-reason", "Spine-skip: Explore widget: (#12)", "malformed"),
    ("no-object", "Spine-skip: Explore: no sweep yet (#12)", "malformed"),
    ("lower-case", "spine-skip: Explore widget: later (#12)", "malformed"),
    ("no-colon", "Spine-skip Explore widget: later (#12)", "malformed"),
    ("unknown-phase", "Spine-skip: Polish widget: later (#12)", "no phase"),
    ("unknown-object", "Spine-skip: Explore gadget: later (#12)", "no object"),
]


@pytest.mark.parametrize(
    ("line", "says"), [c[1:] for c in MALFORMED], ids=[c[0] for c in MALFORMED]
)
def test_a_malformed_skip_is_an_error(
    branch: Branch, complete: Run, line: str, says: str
) -> None:
    """On a branch with nothing to skip, so only the line can fail it."""
    r = _gate(branch(msg=f"{CLOSES}\n{line}\n"))
    assert r.rc == 1
    assert says in r.err
    # Control: the same branch without the line passes, and a well-formed
    # trailer on it is not an error.
    assert complete.rc == 0
    assert says not in complete.err
    assert _gate(branch(msg=f"{CLOSES}\n{SKIP}\n")).rc == 0


def test_a_stale_ratchet_line_fails(branch: Branch) -> None:
    r = _gate(branch(before=_write(RATCHET, LINE)))
    assert r.rc == 1
    assert "'widget explore/sweep' is stale" in r.err
    # Control: the same line over a gap that is there holds it, and the
    # row reads missing with the line's reason.
    held = _gate(branch(before=_no_sweep_recorded))
    assert held.rc == 0, held.err
    assert held.states["Explore"] == "missing"
    assert "no sweep yet (#12)" in held.cells["Explore"]
    assert "stale" not in held.err


def test_a_malformed_ratchet_line_fails(branch: Branch) -> None:
    r = _gate(branch(before=_write(RATCHET, "widget explore sweep\n")))
    assert r.rc == 1
    assert "malformed line: widget explore sweep" in r.err
    # Control: a comment is not a line.
    assert _gate(branch(before=_write(RATCHET, "# none\n"))).rc == 0


def test_an_untouched_object_is_checked_and_not_shown(branch: Branch) -> None:
    """A gap in an object the branch does not touch still fails; its
    table is printed only with --all, where Land does not apply."""

    def docs_only(root: Path) -> None:
        source = "native/src/widget/widget_core.c"
        _w(root, source, TREE[source])
        _w(root, "docs/notes.md", "notes\n")

    root = branch(docs_only, before=_no_sweep)
    r = _gate(root)
    assert r.rc == 1
    assert "widget explore/sweep:" in r.err
    assert "The branch touches no object." in r.out
    assert not r.states
    every = _gate(root, "--all")
    assert every.states["Explore"] == "missing"
    assert every.states["Land"] == "n/a"
    # Control: the branch that touches the widget shows it without --all.
    assert _gate(branch(_no_sweep)).states["Explore"] == "missing"


def test_no_commits_ahead_is_inert(pristine: Path, tmp_path: Path) -> None:
    root = tmp_path / "repo"
    shutil.copytree(pristine, root)
    r = _gate(root, "--all")
    assert r.rc == 0, r.err
    assert "No commits ahead of `main`" in r.out
    assert r.states["Land"] == "n/a"
    # Control: the tree is still judged. A gap on main itself fails.
    _no_sweep(root)
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "chore: drop\n\nNo-issue:\n")
    assert _gate(root, "--all").rc == 1


def test_a_rewritten_base_commit_lends_no_skip(
    pristine: Path, tmp_path: Path
) -> None:
    """An --amend on the base's tip keeps its author, date and subject:
    issue-link-check refuses the branch, and its trailer is not read."""
    root = tmp_path / "repo"
    shutil.copytree(pristine, root)
    _w(root, "notes.md", "one\n")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "docs: notes\n\nNo-issue:\n")
    _git(root, "checkout", "-q", "-b", "work")
    _no_sweep_recorded(root)
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "--amend", "-m", f"docs: notes\n\n{SKIP}\n")
    r = _gate(root)
    assert r.rc == 1
    assert "base commit under a new hash" in r.err
    assert "is new; the list only shrinks" in r.err
    # Control: the same change as a commit of the branch's own is read.
    _git(root, "reset", "-q", "--soft", "main")
    _git(root, "commit", "-q", "-m", f"{CLOSES}\n{SKIP}\n")
    own = _gate(root)
    assert "base commit under a new hash" not in own.err
    assert "is new; the list only shrinks" not in own.err


def _page(root: Path, old: str, new: str) -> None:
    text = (root / PAGE).read_text(encoding="utf-8")
    assert old in text
    _w(root, PAGE, text.replace(old, new, 1))


def test_a_card_row_with_no_checker_fails(
    branch: Branch, complete: Run
) -> None:
    def add_row(root: Path) -> None:
        _page(
            root,
            "\n<!-- spine-card:end -->",
            "| 11 | **Retire** | a notice | this page | review only |\n"
            "\n<!-- spine-card:end -->",
        )

    r = _gate(branch(add_row))
    assert r.rc == 1
    assert "phase 11 'Retire' has no checker" in r.err
    # Control: the card as it is has a checker for every row.
    assert "has no checker" not in complete.err


def test_a_checker_the_card_does_not_list_fails(
    branch: Branch, complete: Run
) -> None:
    r = _gate(
        branch(lambda root: _page(root, "| **Explore** ", "| **Roam** "))
    )
    assert r.rc == 1
    assert "a checker for 'explore', which the card" in r.err
    assert "phase 7 'Roam' has no checker" in r.err
    # Control: the same card with the row's name intact.
    assert "a checker for" not in complete.err


def test_the_principles_are_printed_from_the_page(
    branch: Branch, complete: Run
) -> None:
    assert "**A deviation is declared.**" in complete.out
    r = _gate(
        branch(
            lambda root: _page(
                root, "**A deviation is declared.**", "**Say so.**"
            )
        )
    )
    assert "**Say so.**" in r.out
    # Control: the edited text is the page's, not the script's.
    assert "**A deviation is declared.**" not in r.out
    assert "**Say so.**" not in complete.out


def test_the_summary_file_gets_the_tables(branch: Branch) -> None:
    root = branch()
    out = root.parent / "summary.md"
    out.write_text("# job\n", encoding="utf-8")
    r = _gate(root, "--summary", str(out))
    text = out.read_text(encoding="utf-8")
    assert text.startswith("# job\n")
    assert "### `widget`" in text
    assert "| 10 | Land | present |" in text
    # Control: without the flag nothing is appended.
    out.write_text("# job\n", encoding="utf-8")
    assert _gate(root).rc == r.rc == 0
    assert out.read_text(encoding="utf-8") == "# job\n"


# ── CLAUDE.md's copy of the principles ──────────────────────────────────


def _principles() -> str:
    card = _card().split("<!-- spine-card:start -->")[1]
    body = card[card.index("**The principles.**") :]
    return body[: body.index("**The phases.**")].strip("\n")


def _claude(body: str) -> str:
    return (
        "# Guide\n\n<!-- spine-principles:start -->\n\n"
        f"{body}\n\n<!-- spine-principles:end -->\n\nMore.\n"
    )


def test_an_equal_copy_passes(branch: Branch) -> None:
    r = _gate(branch(_write("CLAUDE.md", _claude(_principles()))))
    assert r.rc == 0, r.err
    # Control: one word of the copy changed fails.
    drift = _claude(_principles().replace("Reuse first", "Reuse last"))
    assert _gate(branch(_write("CLAUDE.md", drift))).rc == 1


def test_a_different_copy_fails_and_fix_repairs_it(branch: Branch) -> None:
    drift = _claude(_principles().replace("Reuse first", "Reuse last"))
    root = branch(_write("CLAUDE.md", drift))
    r = _gate(root)
    assert r.rc == 1
    assert "CLAUDE.md: its copy of the principles differs" in r.err
    fixed = _gate(root, "--fix")
    assert fixed.rc == 0
    text = (root / "CLAUDE.md").read_text(encoding="utf-8")
    assert text == _claude(_principles())
    after = _gate(root)
    assert "CLAUDE.md" not in after.err
    # Control: --fix on a copy that is right leaves the file's bytes.
    assert _gate(root, "--fix").rc == 0
    assert (root / "CLAUDE.md").read_text(encoding="utf-8") == text
    assert text != drift


def test_a_claude_md_without_markers_fails(
    branch: Branch, complete: Run
) -> None:
    root = branch(_write("CLAUDE.md", "# Guide\n\nNo copy here.\n"))
    r = _gate(root)
    assert r.rc == 1
    assert "CLAUDE.md: no <!-- spine-principles:start -->" in r.err
    # --fix does not invent the markers' place.
    _gate(root, "--fix")
    assert _gate(root).rc == 1
    # Controls: the flag that makes the markers optional passes the same
    # tree; a tree with no CLAUDE.md has no copy to hold.
    assert _gate(root, "--copy-markers-optional").rc == 0
    assert complete.rc == 0
    assert "CLAUDE.md" not in complete.err


# ── The pieces, on their own ────────────────────────────────────────────


@pytest.fixture
def spine(monkeypatch: pytest.MonkeyPatch) -> ModuleType:
    """The script as a module, with scripts/ importable as when it runs."""
    monkeypatch.syspath_prepend(str(SCRIPTS))
    spec = importlib.util.spec_from_file_location("_t_check_spine", SCRIPT)
    assert spec is not None
    assert spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


TOUCHES = [
    ("native/inc/doppler/mpsk_receiver/mpsk_receiver_core.h", "mpsk_receiver"),
    ("native/src/fir/fir_core.c", "fir"),
    ("native/tests/fir/test_fir_symbols.c", "fir"),
    ("native/benchmarks/fir/bench_fir_core.c", "fir"),
    ("objects/fir.toml", "fir"),
    ("native/validation/mpsk_receiver_ber.c", "mpsk_receiver"),
    ("native/src/filter/filter_ext_fir.c", "fir"),
    ("src/doppler/filter/benchmarks/bench_fir.py", "fir"),
    ("src/doppler/filter/tests/validation/fir/results.md", "fir"),
]

#: Paths that are no object's: a primitive's validator (`mpsk` is a header
#: directory, not a component), a module's own files, and a page.
TOUCHES_NOTHING = [
    "native/validation/mpsk_soft.c",
    "modules/filter.toml",
    "src/doppler/filter/__init__.py",
    "docs/design/fir.md",
    "objects/README.md",
]


def test_a_path_is_attributed_to_its_object(spine: ModuleType) -> None:
    objects = ["fir", "mpsk_receiver"]
    names = {"fir", "mpsk", "mpsk_receiver"}
    for path, obj in TOUCHES:
        assert spine.touched([path], objects, names) == {obj}, path
    # Control: the same function answers nothing for a path that is not
    # an object's, so the positives above are not an always-true.
    for path in TOUCHES_NOTHING:
        assert spine.touched([path], objects, names) == set(), path


def test_the_real_card_and_the_checkers_agree(spine: ModuleType) -> None:
    _principles, phases = spine.read_card(REPO)
    assert spine.card_errors(phases) == []
    assert [p.number for p in phases] == list(range(1, 11))
    # Control: one more row, and the same call reports it.
    extra = [*phases, spine.Phase(11, "Retire", "review only")]
    assert len(spine.card_errors(extra)) == 1
