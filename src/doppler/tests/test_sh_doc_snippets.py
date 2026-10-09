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


#: Words that end one simple command and start the next.
_SEPARATORS = frozenset({"|", "||", "&&", ";", "&"})
#: A leading ``NAME=value`` environment assignment, not a command.
_ASSIGNMENT = re.compile(r"[A-Za-z_]\w*=")


def _simple_commands(line: str) -> list[list[str]]:
    """Split a command line into its simple commands, each as its words.

    One parser for everything the gate asks of a line, so the CLI checks
    and the execution allowlist cannot disagree about what a command is
    (#1974). Shell operators split even with no space around them
    (``cat x|xxd``), because ``shlex`` runs with ``punctuation_chars``.
    Quoted text stays one word, so a ``;`` or ``|`` inside a
    ``python3 -c "..."`` string or a grep pattern is not a separator. A
    leading ``!`` and ``NAME=value`` assignments are stripped, so the
    first word is the command that runs. Raises ``ValueError`` on an
    unbalanced quote.

    Not seen, by construction: a command inside ``$(...)`` or backticks.
    """
    lex = shlex.shlex(line, posix=True, punctuation_chars=True)
    lex.whitespace_split = True
    lex.commenters = "#"
    commands: list[list[str]] = [[]]
    for word in lex:
        if word in _SEPARATORS:
            commands.append([])
        else:
            commands[-1].append(word)
    out = []
    for words in commands:
        while words and (
            words[0] in ("!", "(", "{") or _ASSIGNMENT.match(words[0])
        ):
            words = words[1:]
        if words:
            out.append(words)
    return out


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
    and the whole fence went unexecuted (#1787). A quote that never closes
    raises ``ValueError`` naming the line it opened on, rather than take
    every line after it as one argument.
    """
    lines: list[str] = []
    heredoc_end: str | None = None
    pending = ""
    in_quote = False
    opened = 0
    for n, raw in enumerate(code.splitlines(), 1):
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
            pending, in_quote, opened = line, True, n
            continue
        m = _HEREDOC_RE.search(line)
        if m:
            heredoc_end = m.group("tag")
        lines.append(line)
    if in_quote:
        raise ValueError(f"a quote opened on fence line {opened} never closes")
    if pending:
        lines.append(pending)
    return lines


def _validate_cli_line(line: str, blockid: str, make_dir: Path = REPO) -> None:
    """Parse a doppler/doppler-specan/python line against reality."""
    for seg in _simple_commands(line):
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
    return any(
        cmd[0] in ("wfmgen", "cat")
        for line in cmd_lines
        for cmd in _simple_commands(line)
    )


def _unlisted(cmd_lines: list[str]) -> list[str]:
    """The commands a fence runs that are not in ``_EXEC_ALLOWED``.

    Every simple command on every line, not only the first word of the
    line: after ``|``, ``&&`` or ``;`` a command runs just the same (#1974).
    """
    return sorted(
        {cmd[0] for line in cmd_lines for cmd in _simple_commands(line)}
        - _EXEC_ALLOWED
    )


def _check_fence(
    code: str,
    blockid: str,
    make_dir: Path,
    *,
    cwd: Path,
    scripts: Path,
    no_exec: bool = False,
) -> int:
    """Parse-validate one fence and, when it qualifies, run it.

    Returns the number of command lines checked. Everything the page test
    does to a fence happens here, so the gate's own failure modes can be
    tested on a seeded fence (``test_the_gate_*`` below) and not only by
    breaking a real page once.
    """
    console = code.lstrip().startswith("$")
    try:
        cmd_lines = _command_lines(code, console=console)
    except ValueError as e:
        raise AssertionError(
            f"{blockid}: {e}\n--- fence ---\n{code}"
        ) from None

    for line in cmd_lines:
        _validate_cli_line(line, blockid, make_dir)

    if no_exec or not _executable(code, cmd_lines, console):
        return len(cmd_lines)
    # A fence the gate would run, but for a command it does not know, is
    # not run; it fails here rather than pass (#1787).
    unlisted = _unlisted(cmd_lines)
    assert not unlisted, (
        f"{blockid}: this fence would execute, but it runs "
        f"{', '.join(unlisted)}, which is not in _EXEC_ALLOWED. Unexecuted, "
        f"not even its exit status would be checked. Add the command to "
        f"_EXEC_ALLOWED if it is read-only and needs no network, or mark the "
        f"fence <!-- docs-snippet: no-exec=REASON -->.\n--- fence ---\n{code}"
    )
    # Console fences carry displayed output -- execute only the stripped
    # command lines. sh/bash fences run verbatim (they may contain heredocs
    # the line extractor elides).
    body = "\n".join(cmd_lines) if console else code
    # Absolute-ify repo-relative paths so the fence runs from the shared
    # throwaway cwd without touching the repo.
    script = scripts / "fence.sh"
    script.write_text(
        re.sub(r"(?<![\w/])src/", f"{REPO}/src/", body) + "\n",
        encoding="utf-8",
    )
    # A script FILE with stdin closed, not the script on bash's stdin: on
    # stdin, a command that reads stdin (`cat`, `grep pat`, `cmp - f`) eats
    # the rest of the fence, which then never runs and passes. One shared
    # cwd per page, fences in order: an earlier fence's heredoc-written
    # spec file (scene.json) is visible to a later fence's
    # `wfmgen --from-file scene.json`, the "page is one notebook" model of
    # the Python gate. bytes, not text: a fence may legitimately write raw
    # IQ to stdout. Own process group + killpg on timeout: subprocess's
    # timeout kills only bash itself, and an orphaned grandchild (a wfmgen
    # that turned out to stream) would keep writing forever -- this exact
    # leak once filled /tmp with 8 GB of IQ.
    proc = subprocess.Popen(
        ["bash", "-e", str(script)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=cwd,
        start_new_session=True,
    )
    try:
        _, err_b = proc.communicate(timeout=120)
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
    return len(cmd_lines)


PAGES = _discover_pages()


@pytest.mark.parametrize(
    "page", PAGES, ids=[str(p.relative_to(DOCS)) for p in PAGES]
)
def test_sh_page_fences(
    page: Path, tmp_path: Path, tmp_path_factory: pytest.TempPathFactory
) -> None:
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

    # Scripts live outside the page's cwd, so a fence's `ls` sees only what
    # the page made.
    scripts = tmp_path_factory.mktemp("sh-fence")
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
        n_checked += _check_fence(
            resolve_snippets(code),
            blockid,
            make_dir,
            cwd=tmp_path,
            scripts=scripts,
            no_exec=no_exec,
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


# ── the gate's own failure modes, on seeded fences ──────────────────────────
# Each was a way a fence could pass without being checked. They are tested
# here, against _check_fence, so they stay red if the gate regresses: a
# one-time sabotage of a real page proves the gate once and then nothing.


def _gate(code: str, tmp_path: Path) -> int:
    cwd = tmp_path / "cwd"
    scripts = tmp_path / "scripts"
    cwd.mkdir()
    scripts.mkdir()
    return _check_fence(code, "seeded", REPO, cwd=cwd, scripts=scripts)


@pytest.mark.parametrize(
    "code",
    [
        "cat /dev/null\nxxd /dev/null",  # a line of its own (#1787)
        "cat /dev/null | xxd",  # after a pipe (#1974)
        "cat /dev/null && xxd /dev/null",  # after &&
        "cat /dev/null;xxd /dev/null",  # after ; with no space
        "FOO=1 xxd /dev/null\ncat /dev/null",  # behind an assignment
    ],
    ids=["own-line", "pipe", "and", "semicolon", "assignment"],
)
def test_the_gate_fails_a_fence_running_an_unlisted_command(
    code: str, tmp_path: Path
) -> None:
    with pytest.raises(AssertionError, match="runs xxd, which is not in"):
        _gate(code, tmp_path)


def test_the_gate_reads_past_bang_and_assignments(tmp_path: Path) -> None:
    """`! cmp` and `FOO=1 cat` run cmp and cat, both allowlisted."""
    code = "printf a > a\nprintf b > b\n! cmp -s a b\nFOO=1 cat a"
    assert _gate(code, tmp_path) == 4


def test_the_gate_fails_an_unclosed_quote(tmp_path: Path) -> None:
    code = 'cat /dev/null\npython3 -c "\nprint(1)\ncat /dev/null'
    with pytest.raises(AssertionError, match="line 2 never closes"):
        _gate(code, tmp_path)


def test_a_multi_line_python_string_runs_as_one_command(
    tmp_path: Path,
) -> None:
    """It runs, and its exit status is checked: exit 3 fails the fence."""
    code = 'cat /dev/null\npython3 -c "\nimport sys\nsys.exit(3)"'
    with pytest.raises(AssertionError, match="exit 3"):
        _gate(code, tmp_path)


def test_a_stdin_reader_does_not_swallow_the_rest(tmp_path: Path) -> None:
    """On bash's stdin, `cat` would read the next line as data and pass.

    From a script file with stdin closed, `cat` sees EOF, the failing `ls`
    runs, and the fence fails as it should.
    """
    with pytest.raises(AssertionError, match="exit 2"):
        _gate("cat\nls /no/such/path", tmp_path)
