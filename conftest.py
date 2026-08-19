# PEP 604 (`X | None`) in the module-level annotations below is evaluated
# eagerly at import on Python 3.9, which this project still supports
# (`requires-python = ">=3.9"`). Deferring annotations keeps them as strings
# so 3.9 can import this file at all -- and conftest.py is imported by every
# pytest run, so getting this wrong takes the whole suite down, not one test.
from __future__ import annotations

import atexit
import pathlib
import shutil
import socket
import subprocess
import tempfile
import time

_IGNORE = pathlib.Path(__file__).parent / "docs" / ".doc-snippet-ignore"

_NATS_ADDR = ("127.0.0.1", 4222)
_nats_proc: subprocess.Popen | None = None
_nats_store: tempfile.TemporaryDirectory | None = None


def _nats_available(timeout: float = 0.3) -> bool:
    """True if a nats-server is already listening on 127.0.0.1:4222."""
    try:
        socket.create_connection(_NATS_ADDR, timeout=timeout).close()
        return True
    except OSError:
        return False


def pytest_configure(config):
    """Start a JetStream broker for the session if one isn't already up.

    The `nats://` paths are covered by real round-trips, not mocks, so
    every one of them needs a live broker on 127.0.0.1:4222. CI starts
    `nats:2.10 -js` in Docker for the two jobs that run the stream suite
    and the examples, and the suites that know about the broker
    self-skip when it is absent.

    That leaves a gap the self-skips cannot close: the `.pyi` and
    `docs/api/*.md` **doctest** gates run as plain pytest invocations
    with no broker step, and a doctest has nowhere to hang a skip
    marker. `detection.pyi` and `stream.pyi` both open real publishers
    in their synthesized examples, so on a developer machine they failed
    with `dp_pub_create failed on nats://127.0.0.1:4222/...` — a real
    gate reporting a real failure that nobody could act on, which is
    the kind of noise that trains you to ignore a red run.

    So rather than teach those gates to bail, start what they need: if
    the port is already open (CI's Docker broker, or a developer's own
    long-running one) nothing happens and we never touch it. Otherwise,
    when a `nats-server` binary is on PATH, bring one up for the
    session against a throwaway JetStream store and shut it down after.
    When there is no binary either, leave the port closed and let the
    existing self-skips do their job — this makes coverage better where
    it can and never worse.
    """
    global _nats_proc, _nats_store
    if _nats_available() or shutil.which("nats-server") is None:
        return

    _nats_store = tempfile.TemporaryDirectory(prefix="doppler-nats-js-")
    try:
        _nats_proc = subprocess.Popen(
            [
                "nats-server",
                "-a",
                _NATS_ADDR[0],
                "-p",
                str(_NATS_ADDR[1]),
                "-js",  # JetStream: the stream suite needs it
                "-sd",
                _nats_store.name,  # throwaway store, never the repo
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except OSError:
        _nats_store.cleanup()
        _nats_proc, _nats_store = None, None
        return

    # Wait for the port rather than sleeping a fixed amount: startup is
    # tens of milliseconds, but a loaded machine can take longer.
    deadline = time.monotonic() + 10.0
    while time.monotonic() < deadline:
        if _nats_available(timeout=0.2):
            atexit.register(_stop_nats)  # covers a hard exit past unconfigure
            return
        if _nats_proc.poll() is not None:  # died on startup (port taken, ...)
            break
        time.sleep(0.05)
    _stop_nats()


def _stop_nats():
    """Terminate the broker this session started, if any."""
    global _nats_proc, _nats_store
    if _nats_proc is not None:
        _nats_proc.terminate()
        try:
            _nats_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            _nats_proc.kill()
            _nats_proc.wait(timeout=5)
        _nats_proc = None
    if _nats_store is not None:
        _nats_store.cleanup()
        _nats_store = None


def pytest_unconfigure(config):
    _stop_nats()


def _display_name(fullname: str, name: str) -> str:
    """Short, unique label -- same disambiguation as scripts/bench_report.py's
    _display_name(). Raw pytest-benchmark ``name``s collide across modules
    (every bench_*.py has its own test_bench_step/test_bench_steps_64k/...),
    so derive a ``module::case`` label from ``fullname`` instead."""
    if "::" in fullname:
        mod = fullname.rsplit("::", 1)[0].rsplit("/", 1)[-1]
        mod = mod.removesuffix(".py").removeprefix("bench_")
        case = name.removeprefix("test_bench_").removeprefix("test_")
        return f"{mod}::{case}" if mod else case
    return name


def pytest_terminal_summary(terminalreporter, exitstatus, config):
    # Doc-snippet burn-down backlog: how many doc pages are not yet gated by
    # the drift gate (docs/.doc-snippet-ignore). Printed every run so the
    # number stays visible and shrinks to zero. See
    # docs/dev/contributing/doc-examples.md.
    if _IGNORE.exists():
        pending = [
            ln
            for ln in _IGNORE.read_text().splitlines()
            if ln.strip() and not ln.startswith("#")
        ]
        if pending:
            terminalreporter.write_line(
                f"doc-snippet backlog: {len(pending)} page(s) not yet gated "
                f"(docs/.doc-snippet-ignore)"
            )

    session = getattr(config, "_benchmarksession", None)
    if session is None:
        return
    rows = [
        (_display_name(b.fullname, b.name), b.extra_info["MSa_s"])
        for b in session.benchmarks
        if "MSa_s" in b.extra_info
    ]
    if not rows:
        return
    terminalreporter.write_sep("-", "throughput")
    for name, msa in rows:
        terminalreporter.write_line(f"  {name}: {msa:.2f} MSa/s")
