"""Shell-fence drift gate: documented CLI invocations are checked in CI.

The Python and C fences in docs/ have fail-closed gates; shell fences
were the last untested class — and it is exactly where real bugs shipped
(#458: a ``doppler compose init`` missing its positional BLOCK, a
``compose up --file`` flag that does not exist, a ``logs`` invocation
missing its chain ID — none of them could ever have worked, and nothing
noticed until a human ran them).

This gate walks every ```` ```sh/```bash/```console ```` fence under
docs/ (includes resolved, same plumbing as the other gates) and applies
three independent checks:

**Parse-validation** (every fence): each ``doppler ...`` and
``doppler-specan ...`` command line is parsed through the CLI's real
``build_parser()`` — argparse itself rejects unknown flags, missing
positionals, and bad choices, with no side effects. Each
``python <path>``/``python3 <path>`` line asserts the script path exists
in the repo. Lines belonging to heredoc bodies, comments, and (in
``console`` fences) output lines are ignored; ``$``-prefixed prompts are
stripped; backslash continuations are joined.

**Make goals** (every fence, #1348): each ``make <goal>`` names a target
that exists. This is the DUAL of ``standard.mk``'s ``help-check``, which
asserts *target ⇒ documented*; this asserts *documented ⇒ target*, and
neither implies the other. The goal set is read from ``make -s help`` —
the same ``$(ALL_TARGETS)`` help-check walks — rather than from a second
list that could disagree with it. Without it a renamed target leaves a
runbook confidently wrong at the one moment it is being followed, and
the failure is ``No rule to make target`` in front of a human
mid-release.

**Execution** (fences that qualify): a fence that runs ``wfmgen`` or
``cat``, touches no live transport (``nats://``), streams no unbounded
run and names no ``<placeholder>`` runs end-to-end under ``bash -e`` in a
throwaway cwd — ``wfmgen`` is the real bundled binary, so a documented
flag that does not exist fails here even though it has no Python parser.
Repo-relative ``src/...`` paths are rewritten absolute so fences run from
the tmp dir.

Such a fence may only use the commands in ``_EXEC_ALLOWED``, the ones
that are safe to run here. One that uses anything else **fails**, naming
the command: before #1787 it was quietly parse-validated instead, so a
``cmp`` that asserted the page's claim was run by nobody and passed.
The fix is to add the command to ``_EXEC_ALLOWED`` when it is read-only
and needs no network, or to mark the fence ``no-exec=REASON``.

``<!-- docs-snippet: skip=REASON -->`` works exactly as in the other
gates (reason mandatory).

Run locally
-----------
    uv run pytest -m docs_snippets src/doppler/tests/test_sh_doc_snippets.py
"""

from __future__ import annotations

import contextlib
import functools
import io
import os
import re
import shlex
import shutil
import signal
import subprocess
from typing import TYPE_CHECKING

import pytest

if TYPE_CHECKING:
    from pathlib import Path

from doppler.tests._docs_snippet_common import (
    DOCS,
    REPO,
    iter_fences,
    page_segments,
    resolve_snippets,
)

pytestmark = pytest.mark.docs_snippets

_EXCLUDED_PARTS = frozenset({"c-api", "archive"})
_EXCLUDED_RELPATHS = frozenset({"api.md", "benchmarks.md"})

# Commands safe to actually execute inside a fence (no network, no
# package managers, no state outside the tmp cwd). wfmgen is the real
# bundled binary -- deterministic, file-writing only. cmp and grep read
# files and write nothing; a page uses them to assert its own claim
# (#1787).
_EXEC_ALLOWED = frozenset(
    {
        "wfmgen",
        "cat",
        "echo",
        "printf",
        "ls",
        "cd",
        "python",
        "python3",
        "cmp",
        "grep",
    }
)

_HEREDOC_RE = re.compile(r"<<-?\s*'?(?P<tag>\w+)'?")

#: A target line in ``make help``: two-space indent, the name, then the
#: description column. Section headers (``Core:``, ``Lint:``) sit at column
#: zero, so anchoring the indent is what keeps them out of the set. An
#: example project's help spells the line out as ``  make run   ...``, so
#: the ``make `` prefix is optional; its ``  PREFIX=<dir>`` knob lines match
#: too, harmlessly, since a goal never carries an ``=``.
_HELP_TARGET = re.compile(r"^  (?:make )?(\S+)\s{2,}\S")

#: ``make`` flags that consume the NEXT token, so it is not a goal.
_MAKE_FLAG_WITH_VALUE = frozenset({"-C", "-f", "-j", "-o", "-W"})


@functools.cache
def _make_targets(make_dir: Path = REPO) -> frozenset[str]:
    """Every goal ``make help`` lists, asked of make itself.

    Deliberately NOT a second list. ``help-check`` walks ``$(ALL_TARGETS)``
    and ``make help`` prints exactly that, so reading the printed form keeps
    one declaration answering both directions — a read-list and a write-list
    for the same thing can be jointly impossible.

    Computed lazily rather than at import: most CI jobs deselect this gate
    with ``-m 'not docs_snippets'``, and an import-time subprocess would
    cost every one of them for nothing.
    """
    out = subprocess.run(
        ["make", "-s", "help"],
        cwd=make_dir,
        capture_output=True,
        text=True,
        timeout=120,
    ).stdout
    return frozenset(
        m.group(1) for m in map(_HELP_TARGET.match, out.splitlines()) if m
    )


def _make_goals(argv: list[str]) -> list[str]:
    """The goals in a ``make`` argv — flags and ``VAR=value`` removed.

    ``make ci-run TARGET='build test-rust'`` is ONE shlex token carrying an
    ``=``, so the assignment is skipped whole. Splitting on whitespace first
    would read ``test-rust'`` as a goal — measured while inventorying #1348,
    and the reason this reuses the caller's shlex tokens.
    """
    goals: list[str] = []
    i = 0
    while i < len(argv):
        tok = argv[i]
        if tok in _MAKE_FLAG_WITH_VALUE:
            i += 2
            continue
        if tok.startswith("-") or "=" in tok:
            i += 1
            continue
        goals.append(tok)
        i += 1
    return goals


def _wfmgen_works() -> bool:
    """Does `wfmgen` actually RUN, not merely resolve?

    "On PATH" is the wrong question. The wheel installs a console-script shim
    that ``execv``s ``doppler/wfm/_bin/wfmgen``, so in an unbuilt tree the
    NAME resolves fine and the shim dies on a binary that was never copied
    there. Measured on a fresh worktree: `shutil.which` returned a path and
    every fence still failed. Probe it instead.
    """
    w = _wfmgen()
    if w is None:
        return False
    try:
        return (
            subprocess.run(
                [w, "--help"], capture_output=True, timeout=30
            ).returncode
            == 0
        )
    except (OSError, subprocess.SubprocessError):
        return False


def _wfmgen() -> str | None:
    """Where ``wfmgen`` resolves, or None.

    The execution half of this gate runs the REAL binary, so its verdict is a
    property of one specific build -- and the gate never said which. An
    unbuilt tree makes every wfmgen fence die as ``exit 1`` with an empty
    stderr tail (``wfmgen: command not found`` goes to the subshell), which
    reads as "this documented invocation is wrong" rather than "there is no
    binary". Measured on a fresh worktree: `make test-snippets` reported a
    correct, committed flag as broken when nothing had been built at all.

    The path is reported on every failure for the same reason. A fence can
    pass or fail against a DIFFERENT checkout's binary if one is earlier on
    PATH, and a verdict whose subject is unnamed cannot be checked.
    """
    return shutil.which("wfmgen")


def _discover_pages() -> list[Path]:
    pages = []
    for page in sorted(DOCS.rglob("*.md")):
        rel = page.relative_to(DOCS)
        if _EXCLUDED_PARTS.intersection(rel.parts):
            continue
        if str(rel) in _EXCLUDED_RELPATHS:
            continue
        pages.append(page)
    return pages


def _quote_open(line: str) -> bool:
    """True when ``line`` leaves a quoted string open for a later line.

    Shell rules, through ``shlex``: a ``#`` starts a comment only outside
    quotes, so a trailing ``# don't`` does not count as an open quote.
    """
    try:
        shlex.split(line, comments=True)
    except ValueError:
        return True
    return False


def _command_lines(code: str, console: bool) -> list[str]:
    """Extract the command lines from a fence body.

    Skips comments, blank lines, and heredoc bodies; joins backslash
    continuations. In ``console`` fences only ``$ ``-prefixed lines are
    commands (the rest is displayed output).

    A quoted string left open on one line continues onto the next, so a
    multi-line ``python3 -c "..."`` is ONE command whose argument keeps its
    newlines and indentation. Read line by line instead, its body became
    commands named ``import`` and ``print(...)``, none of them allowlisted,
    and the whole fence went unexecuted (#1787).
    """
    lines: list[str] = []
    heredoc_end: str | None = None
    pending = ""
    in_quote = False
    for raw in code.splitlines():
        if heredoc_end is not None:
            if raw.strip() == heredoc_end:
                heredoc_end = None
            continue
        if in_quote:
            # The string's content, verbatim: no stripping, and a `#` in
            # it is not a comment.
            pending += "\n" + raw
            if _quote_open(pending):
                continue
            line, pending, in_quote = pending, "", False
        else:
            line = raw.strip()
            if console:
                if not line.startswith("$"):
                    continue
                line = line.lstrip("$").strip()
            if not line or line.startswith("#"):
                continue
            if pending:
                line = pending + " " + line
                pending = ""
        if line.endswith("\\"):
            pending = line[:-1].strip()
            continue
        if _quote_open(line):
            pending, in_quote = line, True
            continue
        m = _HEREDOC_RE.search(line)
        if m:
            heredoc_end = m.group("tag")
        lines.append(line)
    if pending:
        lines.append(pending)
    return lines


def _validate_cli_line(line: str, blockid: str, make_dir: Path = REPO) -> None:
    """Parse a doppler/doppler-specan/python line against reality."""
    try:
        words = shlex.split(line, comments=True)
    except ValueError:
        return  # unbalanced quotes -> heredoc fragment etc.; not a CLI
    if not words:
        return

    # Pipelines / && chains: validate each simple command.
    segments: list[list[str]] = [[]]
    for w in words:
        if w in ("|", "&&", "||", ";"):
            segments.append([])
        else:
            segments[-1].append(w)

    for seg in segments:
        if not seg:
            continue
        cmd, argv = seg[0], seg[1:]
        if cmd in ("python", "python3"):
            script = next((a for a in argv if a.endswith(".py")), None)
            if script and not script.startswith(("-", "/")):
                assert (REPO / script).exists(), (
                    f"{blockid}: documented run-line references a "
                    f"missing script: {script}"
                )
            continue
        if cmd == "make":
            # `make -C <dir>` runs <dir>'s Makefile, so its goals are that
            # Makefile's: `make -C example-projects/uno-q run` from the repo
            # root names uno-q's `run`, which the root has no target for.
            target_dir = make_dir
            if "-C" in argv[:-1]:
                target_dir = make_dir / argv[argv.index("-C") + 1]
            targets = _make_targets(target_dir)
            # An empty set would make every assertion below pass for the
            # wrong reason -- absent output is not a pass.
            assert targets, (
                f"{blockid}: `make -s help` in {target_dir} listed no "
                f"targets, so this "
                f"check would report clean without looking"
            )
            for goal in _make_goals(argv):
                assert goal in targets, (
                    f"{blockid}: documented `make {goal}` names no target.\n"
                    f"  {line}\n"
                    f"  Fix the doc, or add the target -- `make help` in "
                    f"{target_dir} is the list this reads."
                )
            continue
        if cmd == "doppler":
            from doppler.cli.__main__ import build_parser
        elif cmd == "doppler-specan":
            from doppler.specan.__main__ import build_parser
        else:
            continue
        try:
            # argparse prints its own error to stderr; capture it so the
            # assert below is the only output.
            with (
                contextlib.redirect_stderr(io.StringIO()) as err,
                contextlib.redirect_stdout(io.StringIO()),
            ):
                build_parser().parse_args(argv)
        except SystemExit as e:
            # --help exits 0: a valid invocation.
            assert e.code in (0, None), (
                f"{blockid}: documented `{cmd}` invocation does not "
                f"parse against the real CLI:\n"
                f"  {line}\n  {err.getvalue().strip()}"
            )


def _executable(code: str, cmd_lines: list[str], console: bool) -> bool:
    """Whether the fence is one the gate runs.

    Every ``False`` here is a property of the fence that makes running it
    wrong or pointless, not a command the gate happens not to know. Those
    are :func:`_unlisted`, and they fail rather than skip.
    """
    if "nats://" in code:
        return False
    if "--realtime" in code or "--continuous" in code:
        return False  # wall-clock-paced or unbounded generation
    if re.search(r"<\w[\w-]*>", code):
        return False  # <placeholder> template invocation, not runnable
    if console and "<<" in code:
        return False  # heredoc bodies were elided from cmd_lines
    # nothing worth executing / no state to establish
    return any(line.split()[0] in ("wfmgen", "cat") for line in cmd_lines)


def _unlisted(cmd_lines: list[str]) -> list[str]:
    """The commands a fence runs that are not in ``_EXEC_ALLOWED``."""
    return sorted({line.split()[0] for line in cmd_lines} - _EXEC_ALLOWED)


PAGES = _discover_pages()


@pytest.mark.parametrize(
    "page", PAGES, ids=[str(p.relative_to(DOCS)) for p in PAGES]
)
def test_sh_page_fences(page: Path, tmp_path: Path) -> None:
    # The page as rendered: a page-level include (an example project's
    # README) is checked like the page's own text, and each fence keeps the
    # file it came from -- a README's `make run` names a target of the
    # Makefile beside that README, not of this repo's.
    segments = page_segments(page)
    text = "".join(seg for _, seg in segments)
    fences = [
        (origin, marker, code)
        for origin, seg in segments
        for marker, code in iter_fences(seg, "sh|bash|console")
    ]
    if not fences:
        pytest.skip("no shell fences on this page")

    # Materialize every ```json title="file.json" fence into the shared
    # cwd first: that idiom *is* the page saying "here is the spec file
    # the commands below consume" (scenes.md's scenario.json), so the
    # displayed file and the executed command line stay one artifact.
    for m in re.finditer(
        r'^[ \t]*```json[^\n]*title="(?P<name>[^"/]+)"[^\n]*\n'
        r"(?P<body>.*?)\n[ \t]*```",
        text,
        re.DOTALL | re.MULTILINE,
    ):
        (tmp_path / m.group("name")).write_text(
            m.group("body"), encoding="utf-8"
        )

    n_checked = 0
    for i, (origin, marker, code) in enumerate(fences):
        blockid = f"{page.relative_to(DOCS)}[sh-block {i}]"
        make_dir = REPO
        if origin != page:
            blockid += f" (from {origin.relative_to(REPO)})"
            make_dir = origin.parent
        no_exec = False
        if marker is not None:
            kind, _, reason = marker.partition("=")
            kind = kind.strip()
            if kind == "skip":
                assert reason.strip(), f"{blockid}: skip= needs a reason"
                continue
            if kind == "cwd":
                # The fence runs from a directory other than its file's
                # (a README step done "from doppler's root"): validate its
                # `make` goals there. Repo-relative, like an include path.
                make_dir = REPO / reason.strip()
                assert make_dir.is_dir(), (
                    f"{blockid}: cwd= {make_dir} is not a directory"
                )
            if kind == "no-exec":
                # Still parse-validated below -- just not run (the
                # fence depends on context the page establishes outside
                # shell, e.g. a file a Python fence wrote).
                assert reason.strip(), f"{blockid}: no-exec= needs a reason"
                no_exec = True
        code = resolve_snippets(code)
        console = code.lstrip().startswith("$")
        cmd_lines = _command_lines(code, console=console)

        for line in cmd_lines:
            _validate_cli_line(line, blockid, make_dir)
            n_checked += 1

        if not no_exec and _executable(code, cmd_lines, console):
            # A fence the gate would run, but for a command it does not
            # know, is unchecked; it fails here rather than pass (#1787).
            unlisted = _unlisted(cmd_lines)
            assert not unlisted, (
                f"{blockid}: this fence would execute, but it runs "
                f"{', '.join(unlisted)}, which is not in _EXEC_ALLOWED. "
                f"Unexecuted, nothing checks what it shows. Add the "
                f"command to _EXEC_ALLOWED if it is read-only and needs no "
                f"network, or mark the fence "
                f"<!-- docs-snippet: no-exec=REASON -->.\n"
                f"--- fence ---\n{code}"
            )
            # Console fences carry displayed output -- execute only the
            # stripped command lines. sh/bash fences run verbatim (they
            # may contain heredocs the line extractor elides).
            body = "\n".join(cmd_lines) if console else code
            # Absolute-ify repo-relative paths so the fence runs from
            # the shared throwaway cwd without touching the repo.
            script = re.sub(r"(?<![\w/])src/", f"{REPO}/src/", body)
            # One shared cwd per page, fences in order: an earlier
            # fence's heredoc-written spec file (scene.json) is visible
            # to a later fence's `wfmgen --from-file scene.json`, the
            # same "page is one notebook" model as the Python gate.
            # bytes, not text: a fence may legitimately write raw IQ to
            # stdout (`wfmgen ... > out.iq` pipelines). Own process
            # group + killpg on timeout: subprocess.run's timeout kills
            # only bash itself, and an orphaned grandchild (a wfmgen
            # that turned out to stream) would keep writing forever --
            # this exact leak once filled /tmp with 8 GB of IQ.
            proc = subprocess.Popen(
                ["bash", "-e"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                cwd=tmp_path,
                start_new_session=True,
            )
            try:
                _, err_b = proc.communicate(script.encode(), timeout=120)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
                raise AssertionError(
                    f"{blockid} timed out after 120 s (process group "
                    f"killed):\n--- fence ---\n{code}"
                ) from None
            stderr = err_b.decode(errors="replace")
            if proc.returncode != 0 and not _wfmgen_works():
                raise AssertionError(
                    f"{blockid}: this fence runs `wfmgen`, and `wfmgen "
                    f"--help` does not succeed here — so nothing on this "
                    f"page was actually checked. The execution half of "
                    f"this gate runs the real binary; an unbuilt tree "
                    f"cannot check it, and reports a correct documented "
                    f"flag as broken.\n"
                    f"  wfmgen resolves to: {_wfmgen()}\n"
                    f"  Build first:  make pyext   (or: make build)\n"
                    f"--- fence ---\n{code}"
                )
            assert proc.returncode == 0, (
                f"{blockid} failed under bash -e (exit "
                f"{proc.returncode}), wfmgen={_wfmgen()}:"
                f"\n--- fence ---\n{code}\n"
                f"--- stderr (tail) ---\n{stderr[-2000:]}"
            )

    if n_checked == 0:
        pytest.skip("no checkable command lines (inert fences)")


def test_discovery_nonempty() -> None:
    total = sum(
        len(
            list(iter_fences(p.read_text(encoding="utf-8"), "sh|bash|console"))
        )
        for p in PAGES
    )
    assert total > 50, f"only {total} shell fences found -- regex broken?"
