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

The skip registry
-----------------
``src/doppler/examples/.examples-skip`` -- one ``script.py: reason`` per
line, ``#`` comments allowed. A reason is mandatory (an entry without
one fails the meta-test), and a stale entry naming a script that no
longer exists also fails, so the registry can only shrink or stay
honest. Today's entries are the two-process NATS demos (transmitter/
receiver pairs that need a live peer -- their wire round-trip is covered
by ``stream/tests/``).

Run locally
-----------
    uv run pytest -m examples src/doppler/tests/test_examples.py
"""

from __future__ import annotations

import os
import socket
import subprocess
import sys
from pathlib import Path

import pytest

pytestmark = pytest.mark.examples

REPO = Path(__file__).resolve().parents[3]
EXAMPLES_DIR = REPO / "src" / "doppler" / "examples"
SKIP_REGISTRY = EXAMPLES_DIR / ".examples-skip"

# Examples living outside src/doppler/examples/ that are part of the
# same guarantee. The standalone example is the "pip install + one file"
# story the install docs tell.
EXTRA_EXAMPLES = [REPO / "examples" / "standalone" / "example.py"]

# Wall-clock ceiling per example. The slowest legitimate examples are
# Monte-Carlo characterization runs; anything past this is a hang (a
# blocking recv(), an unbounded realtime loop) and belongs in the skip
# registry instead.
TIMEOUT_S = 300


def _load_registry() -> dict[str, str]:
    entries: dict[str, str] = {}
    if not SKIP_REGISTRY.exists():
        return entries
    for raw in SKIP_REGISTRY.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        name, sep, reason = line.partition(":")
        entries[name.strip()] = reason.strip() if sep else ""
    return entries


def _discover() -> list[Path]:
    scripts = sorted(EXAMPLES_DIR.glob("*.py"))
    scripts.extend(EXTRA_EXAMPLES)
    return scripts


SKIPS = _load_registry()
SCRIPTS = _discover()


def _broker_reachable(host: str = "127.0.0.1", port: int = 4222) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.5):
            return True
    except OSError:
        return False


@pytest.mark.parametrize("script", SCRIPTS, ids=[s.name for s in SCRIPTS])
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

    # A couple of gallery scripts save their figure straight to the
    # relative path docs/assets/ (that is how `make gallery` refreshes
    # the committed figures); give the throwaway cwd that directory so
    # they run anywhere without touching the repo's copy.
    (tmp_path / "docs" / "assets").mkdir(parents=True)

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


def test_registry_entries_have_reasons() -> None:
    missing = [name for name, reason in SKIPS.items() if not reason]
    assert not missing, (
        "skip-registry entries without a reason (format is "
        f"'script.py: reason'): {missing}"
    )


def test_registry_entries_exist() -> None:
    known = {s.name for s in SCRIPTS}
    stale = sorted(set(SKIPS) - known)
    assert not stale, (
        "skip-registry entries naming no existing example (delete the "
        f"line): {stale}"
    )


def test_discovery_nonempty() -> None:
    # If the glob breaks (a rename, a layout change), the parametrized
    # test above silently becomes a no-op -- this keeps it honest.
    assert len(SCRIPTS) > 20, (
        f"only {len(SCRIPTS)} examples discovered -- discovery broken?"
    )
