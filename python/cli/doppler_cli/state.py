"""Chain state persistence in ~/.doppler/chains/."""

from __future__ import annotations

import json
import os
import signal
from dataclasses import dataclass, field
from pathlib import Path

_CHAINS_DIR = Path.home() / ".doppler" / "chains"


@dataclass
class BlockState:
    name: str
    pid: int
    bind_port: int | None = None  # output port this block binds
    connect_port: int | None = None  # input port this block connects to


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
                }
                for b in self.blocks
            ],
        }

    @classmethod
    def load(cls, chain_id: str) -> "ChainState":
        path = _CHAINS_DIR / f"{chain_id}.json"
        if not path.exists():
            raise KeyError(f"No chain with id {chain_id!r}")
        data = json.loads(path.read_text())
        blocks = [
            BlockState(
                name=b["name"],
                pid=b["pid"],
                bind_port=b.get("bind_port"),
                connect_port=b.get("connect_port"),
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


def pid_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
        return True
    except (ProcessLookupError, PermissionError):
        return False


def stop_chain(chain: ChainState, kill: bool = False) -> None:
    """Send SIGTERM (or SIGKILL) to all block processes."""
    sig = signal.SIGKILL if kill else signal.SIGTERM
    for block in chain.blocks:
        if pid_alive(block.pid):
            try:
                os.kill(block.pid, sig)
            except ProcessLookupError:
                pass
    chain.delete()
