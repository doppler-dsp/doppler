"""Auto port allocation for doppler pipeline blocks."""

from __future__ import annotations

import json
from pathlib import Path

_BASE_PORT = 5600
_MAX_PORT = 5700
_CHAINS_DIR = Path.home() / ".doppler" / "chains"


def _in_use() -> set[int]:
    """Return all ports referenced in active chain state files."""
    ports: set[int] = set()
    if not _CHAINS_DIR.exists():
        return ports
    for f in _CHAINS_DIR.glob("*.json"):
        try:
            state = json.loads(f.read_text())
        except (json.JSONDecodeError, OSError):
            continue
        for block in state.get("blocks", []):
            for key in ("bind_port", "connect_port"):
                if (p := block.get(key)) is not None:
                    ports.add(int(p))
    return ports


def allocate(n: int) -> list[int]:
    """Return *n* consecutive free ports starting from _BASE_PORT."""
    used = _in_use()
    allocated: list[int] = []
    candidate = _BASE_PORT
    while len(allocated) < n:
        if candidate > _MAX_PORT:
            raise RuntimeError(
                f"No free ports in range {_BASE_PORT}–{_MAX_PORT}. "
                "Clean up old chains with `doppler ps` and `doppler stop`."
            )
        if candidate not in used and candidate not in allocated:
            allocated.append(candidate)
        candidate += 1
    return allocated
