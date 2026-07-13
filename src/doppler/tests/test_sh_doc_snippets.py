"""Shell-fence drift gate: documented CLI invocations are checked in CI.

The Python and C fences in docs/ have fail-closed gates; shell fences
were the last untested class — and it is exactly where real bugs shipped
(#458: a ``doppler compose init`` missing its positional BLOCK, a
``compose up --file`` flag that does not exist, a ``logs`` invocation
missing its chain ID — none of them could ever have worked, and nothing
noticed until a human ran them).

This gate walks every ```` ```sh/```bash/```console ```` fence under
docs/ (includes resolved, same plumbing as the other gates) and applies
two independent checks:

**Parse-validation** (every fence): each ``doppler ...`` and
``doppler-specan ...`` command line is parsed through the CLI's real
``build_parser()`` — argparse itself rejects unknown flags, missing
positionals, and bad choices, with no side effects. Each
``python <path>``/``python3 <path>`` line asserts the script path exists
in the repo. Lines belonging to heredoc bodies, comments, and (in
``console`` fences) output lines are ignored; ``$``-prefixed prompts are
stripped; backslash continuations are joined.

**Execution** (fences that qualify): a fence whose every command is in a
safe allowlist (``wfmgen``, ``cat``, ``echo``, ``printf``, ``ls``,
``cd``, ``python``/``python3``) and that touches no live transport
(``nats://``) runs end-to-end under ``bash -e`` in a throwaway cwd —
``wfmgen`` is the real bundled binary, so a documented flag that does
not exist fails here even though it has no Python parser. Repo-relative
``src/...`` paths are rewritten absolute so fences run from the tmp dir.

``<!-- docs-snippet: skip=REASON -->`` works exactly as in the other
gates (reason mandatory).

Run locally
-----------
    uv run pytest -m docs_snippets src/doppler/tests/test_sh_doc_snippets.py
"""

from __future__ import annotations

import contextlib
import io
import os
import re
import shlex
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
    resolve_snippets,
)

pytestmark = pytest.mark.docs_snippets

_EXCLUDED_PARTS = frozenset({"c-api", "archive"})
_EXCLUDED_RELPATHS = frozenset({"api.md", "benchmarks.md"})

# Commands safe to actually execute inside a fence (no network, no
# package managers, no state outside the tmp cwd). wfmgen is the real
# bundled binary -- deterministic, file-writing only.
_EXEC_ALLOWED = frozenset(
    {"wfmgen", "cat", "echo", "printf", "ls", "cd", "python", "python3"}
)

_HEREDOC_RE = re.compile(r"<<-?\s*'?(?P<tag>\w+)'?")


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


def _command_lines(code: str, console: bool) -> list[str]:
    """Extract the command lines from a fence body.

    Skips comments, blank lines, and heredoc bodies; joins backslash
    continuations. In ``console`` fences only ``$ ``-prefixed lines are
    commands (the rest is displayed output).
    """
    lines: list[str] = []
    heredoc_end: str | None = None
    pending = ""
    for raw in code.splitlines():
        if heredoc_end is not None:
            if raw.strip() == heredoc_end:
                heredoc_end = None
            continue
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
        m = _HEREDOC_RE.search(line)
        if m:
            heredoc_end = m.group("tag")
        lines.append(line)
    if pending:
        lines.append(pending)
    return lines


def _validate_cli_line(line: str, blockid: str) -> None:
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
    if "nats://" in code:
        return False
    if "--realtime" in code or "--continuous" in code:
        return False  # wall-clock-paced or unbounded generation
    if re.search(r"<\w[\w-]*>", code):
        return False  # <placeholder> template invocation, not runnable
    if console and "<<" in code:
        return False  # heredoc bodies were elided from cmd_lines
    if not any(line.split()[0] in ("wfmgen", "cat") for line in cmd_lines):
        return False  # nothing worth executing / no state to establish
    return all(line.split()[0] in _EXEC_ALLOWED for line in cmd_lines)


PAGES = _discover_pages()


@pytest.mark.parametrize(
    "page", PAGES, ids=[str(p.relative_to(DOCS)) for p in PAGES]
)
def test_sh_page_fences(page: Path, tmp_path: Path) -> None:
    text = page.read_text(encoding="utf-8")
    fences = list(iter_fences(text, "sh|bash|console"))
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
    for i, (marker, code) in enumerate(fences):
        blockid = f"{page.relative_to(DOCS)}[sh-block {i}]"
        no_exec = False
        if marker is not None:
            kind, _, reason = marker.partition("=")
            kind = kind.strip()
            if kind == "skip":
                assert reason.strip(), f"{blockid}: skip= needs a reason"
                continue
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
            _validate_cli_line(line, blockid)
            n_checked += 1

        if not no_exec and _executable(code, cmd_lines, console):
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
            assert proc.returncode == 0, (
                f"{blockid} failed under bash -e (exit "
                f"{proc.returncode}):\n--- fence ---\n{code}\n"
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
