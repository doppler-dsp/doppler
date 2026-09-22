"""Chain state persistence in ~/.doppler/chains/."""

from __future__ import annotations

import contextlib
import json
import os
import signal
import sys
from dataclasses import dataclass, field
from pathlib import Path

_CHAINS_DIR = Path.home() / ".doppler" / "chains"


@dataclass
class BlockState:
    name: str
    pid: int
    bind_port: int | None = None  # output port this block binds
    connect_port: int | None = None  # input port this block connects to
    log_file: str | None = None  # stderr/stdout log path


@dataclass
class ChainState:
    id: str
    started: str
    compose: str
    blocks: list[BlockState] = field(default_factory=list)

    def save(self) -> None:
        _CHAINS_DIR.mkdir(parents=True, exist_ok=True)
        path = _CHAINS_DIR / f"{self.id}.json"
        path.write_text(json.dumps(self._to_dict(), indent=2))

    def delete(self) -> None:
        path = _CHAINS_DIR / f"{self.id}.json"
        path.unlink(missing_ok=True)

    def _to_dict(self) -> dict:
        return {
            "id": self.id,
            "started": self.started,
            "compose": self.compose,
            "blocks": [
                {
                    "name": b.name,
                    "pid": b.pid,
                    "bind_port": b.bind_port,
                    "connect_port": b.connect_port,
                    "log_file": b.log_file,
                }
                for b in self.blocks
            ],
        }

    @classmethod
    def load(cls, chain_id: str) -> ChainState:
        path = _CHAINS_DIR / f"{chain_id}.json"
        if not path.exists():
            raise KeyError(f"No chain with id {chain_id!r}")
        data = json.loads(path.read_text(encoding="utf-8"))
        blocks = [
            BlockState(
                name=b["name"],
                pid=b["pid"],
                bind_port=b.get("bind_port"),
                connect_port=b.get("connect_port"),
                log_file=b.get("log_file"),
            )
            for b in data.get("blocks", [])
        ]
        return cls(
            id=data["id"],
            started=data["started"],
            compose=data["compose"],
            blocks=blocks,
        )


def list_chains() -> list[ChainState]:
    if not _CHAINS_DIR.exists():
        return []
    chains = []
    for f in sorted(_CHAINS_DIR.glob("*.json")):
        try:
            chains.append(ChainState.load(f.stem))
        except (KeyError, json.JSONDecodeError, OSError):
            continue
    return chains


#: The signal ``stop_chain(kill=True)`` sends. POSIX has SIGKILL. Windows
#: has no uncatchable signal: ``os.kill`` there calls ``TerminateProcess``
#: for any signal but the two console events, which is already the hard stop
#: SIGKILL means, so SIGTERM is the portable spelling of it.
KILL_SIGNAL: int = getattr(signal, "SIGKILL", signal.SIGTERM)


def pid_alive(pid: int) -> bool:
    """Whether a process with this PID exists, without disturbing it.

    POSIX asks with signal 0, which checks the PID and delivers nothing.
    Windows has no such probe: ``os.kill(pid, 0)`` there is
    ``TerminateProcess(pid, 0)``, so the POSIX spelling would kill the very
    process it asks about. It opens a query-only handle instead and reads
    the exit code, which is ``STILL_ACTIVE`` while the process runs.

    A PID this user may not touch reads as not ours, on both platforms.

    Examples
    --------
    >>> import os
    >>> from doppler.cli.state import pid_alive
    >>> pid_alive(os.getpid())
    True
    """
    if sys.platform == "win32":
        return _win_pid_alive(pid)
    try:
        os.kill(pid, 0)
        return True
    except (ProcessLookupError, PermissionError):
        return False


def _win_pid_alive(pid: int) -> bool:
    import ctypes
    from ctypes import wintypes

    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.OpenProcess.restype = wintypes.HANDLE
    k32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    process_query_limited_information = 0x1000
    still_active = 259
    h = k32.OpenProcess(process_query_limited_information, False, pid)
    if not h:
        return False  # no such process, or not one this user may query
    try:
        code = wintypes.DWORD()
        ok = k32.GetExitCodeProcess(h, ctypes.byref(code))
        return bool(ok) and code.value == still_active
    finally:
        k32.CloseHandle(h)


def stop_chain(chain: ChainState, kill: bool = False) -> None:
    """Send SIGTERM (or :data:`KILL_SIGNAL`) to all block processes."""
    sig = KILL_SIGNAL if kill else signal.SIGTERM
    for block in chain.blocks:
        if pid_alive(block.pid):
            with contextlib.suppress(ProcessLookupError):
                os.kill(block.pid, sig)
    chain.delete()
