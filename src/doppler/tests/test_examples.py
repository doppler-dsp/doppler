"""Fail-closed smoke gate: every example script runs in CI.

The old ``make test-examples-python`` ran a hand-maintained Makefile list
of example scripts -- and the list rotted exactly the way hand lists do:
it froze at 23 entries while the examples directory grew to 62, so most
examples (including several with real self-checks inside) never executed
anywhere and could break silently. This gate replaces the list with the
same **discovered, not registered** idiom as the doc-snippet gates
(``test_doc_snippets.py``): every ``src/doppler/examples/*.py`` (plus the
standalone example) is parametrized on arrival, and the only way out is
an explicit entry in the skip registry with a stated reason.

How each example runs
---------------------
As a subprocess of the current interpreter, from a throwaway working
directory (an example writing a PNG or a capture file is fine and never
pollutes the repo), with ``MPLBACKEND=Agg`` (headless plotting), stdin
closed, and a wall-clock timeout. Exit code 0 is required; stderr is
shown on failure. Examples are expected to *validate themselves* --
assert on a BER threshold, a lock flag, a round-trip equality -- so exit
0 means "demonstrated and checked", not merely "didn't crash".

The registries
--------------
``src/doppler/examples/.examples-skip`` -- one ``script.py: reason`` per
line, ``#`` comments allowed. A reason is mandatory (an entry without
one fails the meta-test), and a stale entry naming a script that no
longer exists also fails, so the registry can only shrink or stay
honest. What belongs in it is an example this gate cannot fairly run,
never one it is merely inconvenient to run -- and the file says which is
which, rather than being described here where the description goes stale
the moment an entry moves.

``.examples-pairs`` is the opposite list: the two-process demos, run as
a pair by ``test_example_pair_runs`` below. They were skips ("needs a
live peer") until it was noticed that this harness already runs every
example as a subprocess, so running two is a second ``Popen`` -- and the
excuse had been costing coverage on exactly the streaming demos a reader
is most likely to copy.

Run locally
-----------
    uv run pytest -m examples src/doppler/tests/test_examples.py
"""

from __future__ import annotations

import os
import re
import socket
import subprocess
import sys
import threading
import time
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

pytestmark = pytest.mark.examples

REPO = repo_root(__file__)
EXAMPLES_DIR = REPO / "src" / "doppler" / "examples"
SKIP_REGISTRY = EXAMPLES_DIR / ".examples-skip"

# Examples that must NOT share the machine, because what they assert IS a
# timing: `.examples-serial`, same `script.py: reason` shape as the skip
# registry. They still run on every push — the Makefile gives them a second,
# serial pass — so this is a scheduling constraint, never an excuse.
SERIAL_REGISTRY = EXAMPLES_DIR / ".examples-serial"

# The two-process examples, as "first.py + second.py: <evidence regex>":
# `.examples-pairs`. They are not skips and not single-process examples --
# they are collected by `test_example_pair_runs` below instead, which is
# why `_discover()` drops them from the one-at-a-time parametrization.
PAIR_REGISTRY = EXAMPLES_DIR / ".examples-pairs"

# Examples living outside src/doppler/examples/ that are part of the
# same guarantee. The standalone example is the "pip install + one file"
# story the install docs tell.
EXTRA_EXAMPLES = [REPO / "example-projects" / "standalone" / "example.py"]

# Wall-clock ceiling per example. The slowest legitimate examples are
# Monte-Carlo characterization runs; anything past this is a hang (a
# blocking recv(), an unbounded realtime loop) and belongs in the skip
# registry instead.
TIMEOUT_S = 300


def _load_registry(path: Path) -> dict[str, str]:
    entries: dict[str, str] = {}
    if not path.exists():
        return entries
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        name, sep, reason = line.partition(":")
        entries[name.strip()] = reason.strip() if sep else ""
    return entries


def _pair_halves(pairs: dict[str, str]) -> list[tuple[str, str]]:
    """The (first, second) script names of every registered pair."""
    out: list[tuple[str, str]] = []
    for key in pairs:
        first, sep, second = key.partition("+")
        out.append((first.strip(), second.strip() if sep else ""))
    return out


def _discover(paired: set[str]) -> list[Path]:
    scripts = sorted(
        s for s in EXAMPLES_DIR.glob("*.py") if s.name not in paired
    )
    scripts.extend(EXTRA_EXAMPLES)
    return scripts


SKIPS = _load_registry(SKIP_REGISTRY)
SERIALS = _load_registry(SERIAL_REGISTRY)
PAIRS = _load_registry(PAIR_REGISTRY)
PAIRED = {name for half in _pair_halves(PAIRS) for name in half if name}
SCRIPTS = _discover(PAIRED)


def _param(script: Path):
    """One parametrize entry, marked `examples_serial` when the registry
    says this example measures something a parallel run would perturb.

    The mark is what lets the Makefile run the bulk under xdist and these
    in a second serial pass, without either side naming a script — the
    registry stays the only place a name appears.
    """
    if script.name in SERIALS:
        return pytest.param(script, marks=pytest.mark.examples_serial)
    return script


def _broker_reachable(host: str = "127.0.0.1", port: int = 4222) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.5):
            return True
    except OSError:
        return False


@pytest.mark.parametrize(
    "script",
    [_param(s) for s in SCRIPTS],
    ids=[s.name for s in SCRIPTS],
)
def test_example_runs(script: Path, tmp_path: Path) -> None:
    reason = SKIPS.get(script.name)
    if reason is not None:
        # "broker: ..." entries are conditional, not dead: the example
        # needs only a live NATS broker (no peer process), so it runs
        # wherever one is reachable -- CI's python-tests job starts one
        # -- and self-skips elsewhere, same idiom as the stream suite.
        if reason.startswith("broker:"):
            if not _broker_reachable():
                pytest.skip(f"registry: {reason} (no broker on :4222)")
        else:
            pytest.skip(f"registry: {reason}")

    # The throwaway cwd is deliberately BARE -- an example writes its
    # figure as a plain filename into whatever directory it is run from,
    # and `make gallery` does the moving into docs/assets/. This harness
    # used to mkdir docs/assets/ here, which made the one script that
    # hard-coded that prefix (awgn_demo.py) pass the gate while it could
    # not run anywhere else: the published runtime image's own documented
    # command, `docker run ... python awgn_demo.py`, died on
    # FileNotFoundError (gh-954). Creating the directory an example
    # expects is accommodating the defect, not testing for it.

    env = dict(os.environ, MPLBACKEND="Agg")
    proc = subprocess.run(
        [sys.executable, str(script)],
        cwd=tmp_path,
        env=env,
        stdin=subprocess.DEVNULL,
        capture_output=True,
        text=True,
        timeout=TIMEOUT_S,
    )
    assert proc.returncode == 0, (
        f"{script.relative_to(REPO)} exited {proc.returncode}\n"
        f"--- stdout (tail) ---\n{proc.stdout[-2000:]}\n"
        f"--- stderr (tail) ---\n{proc.stderr[-2000:]}"
    )


# Wall-clock ceiling for a pair: how long the two halves may run before
# the evidence must have appeared. Generous next to the ~2 s each pair
# actually needs, because the assertion is polled and the processes are
# stopped the moment it matches -- the ceiling is only reached when the
# pair is genuinely broken.
PAIR_DEADLINE_S = 60


def _drain(proc: subprocess.Popen, sink: list[str]) -> None:
    """Pump one child's merged stdout into `sink` on a daemon thread.

    A pipe nobody reads is a pair that deadlocks rather than fails: both
    halves print per packet, so the 64 KiB pipe buffer fills well inside
    the deadline and the child then blocks in write() forever.
    """

    def pump() -> None:
        assert proc.stdout is not None
        for line in proc.stdout:
            sink.append(line)

    threading.Thread(target=pump, daemon=True).start()


def _start(script: str, cwd: Path) -> tuple[subprocess.Popen, list[str]]:
    proc = subprocess.Popen(
        [sys.executable, str(EXAMPLES_DIR / script)],
        cwd=cwd,
        env=dict(os.environ, MPLBACKEND="Agg", PYTHONUNBUFFERED="1"),
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    sink: list[str] = []
    _drain(proc, sink)
    return proc, sink


def _stop(proc: subprocess.Popen) -> int:
    """Let the half finish if it is going to, else SIGTERM, then wait.

    The grace is not politeness, it is correctness: a half with its own
    stopping condition (requester.py sends `--count` requests and
    returns) is mid-teardown the instant its evidence appears, and a
    SIGTERM landing during interpreter shutdown kills it outright -- the
    gate then reports rc -15 for a demo that was exiting 0 on its own.
    Measured, not guessed: that is exactly how this gate first went red.

    Everything past the grace is a server half that runs until stopped.
    Those install a SIGTERM handler that leaves the loop and returns
    through main(), so a clean shutdown is part of what this checks: a
    half needing SIGKILL reports the signal as its returncode and fails.
    """
    try:
        return proc.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
        pass
    if proc.poll() is None:
        proc.terminate()
    try:
        proc.wait(timeout=15)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=15)
    return proc.returncode


@pytest.mark.parametrize("pair", sorted(PAIRS))
def test_example_pair_runs(pair: str, tmp_path: Path) -> None:
    """Run both halves of a two-process example; require the wire moved.

    The single-process gate cannot run these -- each half blocks on its
    peer -- but that is a property of running one script at a time, not
    of the demos. They get a second Popen here rather than a skip, so the
    streaming story a reader is most likely to copy executes on every
    push like everything else.
    """
    if not _broker_reachable():
        pytest.skip("two-process example: no NATS broker on :4222")

    first, second = _pair_halves({pair: ""})[0]
    evidence = re.compile(PAIRS[pair])

    p1, out1 = _start(first, tmp_path)
    # The first half must be up before its peer connects, and its banner
    # is the readiness signal: a fixed sleep is what makes a pair gate
    # flaky on a loaded machine.
    deadline = time.monotonic() + 15
    while not out1 and time.monotonic() < deadline:
        if p1.poll() is not None:
            pytest.fail(
                f"{first} exited {p1.returncode} before its peer started"
                f"\n{''.join(out1)[-2000:]}"
            )
        time.sleep(0.05)

    p2, out2 = _start(second, tmp_path)

    deadline = time.monotonic() + PAIR_DEADLINE_S
    matched = False
    while time.monotonic() < deadline:
        if evidence.search("".join(out1) + "".join(out2)):
            matched = True
            break
        if p1.poll() is not None and p2.poll() is not None:
            break
        time.sleep(0.1)

    rc2, rc1 = _stop(p2), _stop(p1)
    tails = (
        f"--- {first} (rc {rc1}) ---\n{''.join(out1)[-1500:]}\n"
        f"--- {second} (rc {rc2}) ---\n{''.join(out2)[-1500:]}"
    )
    assert matched, (
        f"{pair}: both halves ran but nothing matched /{PAIRS[pair]}/ "
        f"within {PAIR_DEADLINE_S}s -- the pair exchanged nothing\n{tails}"
    )
    assert rc1 == 0, f"{first} exited {rc1}\n{tails}"
    assert rc2 == 0, f"{second} exited {rc2}\n{tails}"


def test_pair_registry_is_well_formed() -> None:
    """Both halves name a real script, and neither is also skipped."""
    problems = []
    for key, regex in PAIRS.items():
        first, second = _pair_halves({key: ""})[0]
        if not second:
            problems.append(f"{key!r} is not 'first.py + second.py'")
            continue
        for name in (first, second):
            if not (EXAMPLES_DIR / name).exists():
                problems.append(f"{key}: no such example {name}")
            if name in SKIPS:
                problems.append(
                    f"{key}: {name} is also in .examples-skip -- a paired "
                    f"example is covered here, not excused there"
                )
        if not regex:
            problems.append(f"{key}: no evidence pattern")
    assert not problems, "\n".join(problems)


@pytest.mark.parametrize(
    ("which", "entries"), [("skip", SKIPS), ("serial", SERIALS)]
)
def test_registry_entries_have_reasons(which, entries) -> None:
    missing = [name for name, reason in entries.items() if not reason]
    assert not missing, (
        f"{which}-registry entries without a reason (format is "
        f"'script.py: reason'): {missing}"
    )


@pytest.mark.parametrize(
    ("which", "entries"), [("skip", SKIPS), ("serial", SERIALS)]
)
def test_registry_entries_exist(which, entries) -> None:
    known = {s.name for s in SCRIPTS}
    stale = sorted(set(entries) - known)
    assert not stale, (
        f"{which}-registry entries naming no existing example (delete the "
        f"line): {stale}"
    )


def test_serial_registry_is_disjoint_from_skips() -> None:
    """A skipped example cannot also need a serial pass.

    Both registries would be satisfiable at once and the result reads as
    "runs, carefully" while the example never runs at all — the exact
    class of quiet no-op both registries exist to prevent.
    """
    both = sorted(set(SERIALS) & set(SKIPS))
    assert not both, (
        f"in BOTH .examples-skip and .examples-serial: {both} — a skipped "
        "example does not need a scheduling constraint"
    )


def test_discovery_nonempty() -> None:
    # If the glob breaks (a rename, a layout change), the parametrized
    # test above silently becomes a no-op -- this keeps it honest.
    assert len(SCRIPTS) > 20, (
        f"only {len(SCRIPTS)} examples discovered -- discovery broken?"
    )
