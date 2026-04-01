"""doppler compose — init, up, down."""

from __future__ import annotations

import secrets
import subprocess
from datetime import datetime, timezone
from pathlib import Path

import yaml

from doppler_cli import blocks as block_registry
from doppler_cli.ports import allocate
from doppler_cli.state import BlockState, ChainState, stop_chain

_CHAINS_DIR = Path.home() / ".doppler" / "chains"


# ------------------------------------------------------------------
# compose init
# ------------------------------------------------------------------


def init(block_names: list[str], out: Path | None = None) -> Path:
    """Scaffold a compose file with defaults for the given block names.

    The first block must be a source, the last a sink.  Ports are
    auto-assigned and written into the file so it is fully explicit.

    Parameters
    ----------
    block_names:
        Ordered list of block names, e.g. ``["tone", "fir", "specan"]``.
    out:
        Path to write the compose file.  Defaults to
        ``~/.doppler/chains/<ID>.yml``.

    Returns
    -------
    Path to the written compose file.
    """
    if len(block_names) < 2:
        raise ValueError("Need at least a source and a sink.")

    # Resolve block classes and build default configs
    block_classes = [block_registry.get(n) for n in block_names]
    configs = [cls.Config() for cls in block_classes]

    # Validate roles
    if block_classes[0].role != "source":
        raise ValueError(
            f"First block must be a source, got {block_names[0]!r} "
            f"(role={block_classes[0].role!r})"
        )
    if block_classes[-1].role != "sink":
        raise ValueError(
            f"Last block must be a sink, got {block_names[-1]!r} "
            f"(role={block_classes[-1].role!r})"
        )

    # Number of inter-block connections = number of non-sink blocks
    n_ports = len(block_names) - 1
    ports = allocate(n_ports)

    chain_id = secrets.token_hex(3)  # e.g. "a3f7c2"

    # Build YAML document
    doc: dict = {"id": chain_id}

    source_cfg = configs[0].model_dump()
    source_cfg["port"] = ports[0]
    doc["source"] = {"type": block_names[0], **source_cfg}

    chain_blocks = []
    for i, (name, cfg) in enumerate(zip(block_names[1:-1], configs[1:-1])):
        entry = cfg.model_dump()
        entry["port"] = ports[i + 1]
        chain_blocks.append({name: entry})
    if chain_blocks:
        doc["chain"] = chain_blocks

    sink_cfg = configs[-1].model_dump()
    doc["sink"] = {"type": block_names[-1], **sink_cfg}

    # Write file
    if out is None:
        _CHAINS_DIR.mkdir(parents=True, exist_ok=True)
        out = _CHAINS_DIR / f"{chain_id}.yml"

    out.write_text(yaml.dump(doc, default_flow_style=False, sort_keys=False))
    return out


# ------------------------------------------------------------------
# compose up
# ------------------------------------------------------------------


def up(compose_file: Path) -> ChainState:
    """Spawn all blocks described in *compose_file*.

    Returns the ChainState (also persisted to ~/.doppler/chains/).
    """
    doc = yaml.safe_load(compose_file.read_text())
    chain_id: str = doc.get("id") or secrets.token_hex(3)

    source_doc = doc["source"]
    source_type = source_doc.pop("type")
    source_port: int = source_doc.pop("port")
    source_addr = f"tcp://127.0.0.1:{source_port}"

    chain_docs: list[dict] = doc.get("chain", [])
    sink_doc = doc["sink"]
    sink_type = sink_doc.pop("type")

    block_states: list[BlockState] = []

    # --- spawn source ---
    src_cls = block_registry.get(source_type)
    src_cfg = src_cls.Config(**source_doc)
    src_cmd = src_cls().command(src_cfg, None, source_addr)
    src_proc = subprocess.Popen(src_cmd)  # noqa: S603
    block_states.append(
        BlockState(
            name=source_type,
            pid=src_proc.pid,
            bind_port=source_port,
        )
    )

    # --- spawn chain blocks ---
    prev_addr = source_addr
    for entry in chain_docs:
        (name, cfg_dict) = next(iter(entry.items()))
        out_port: int = cfg_dict.pop("port")
        out_addr = f"tcp://127.0.0.1:{out_port}"
        in_port = int(prev_addr.rsplit(":", 1)[-1])

        blk_cls = block_registry.get(name)
        blk_cfg = blk_cls.Config(**cfg_dict)
        blk_cmd = blk_cls().command(blk_cfg, prev_addr, out_addr)
        blk_proc = subprocess.Popen(blk_cmd)  # noqa: S603
        block_states.append(
            BlockState(
                name=name,
                pid=blk_proc.pid,
                connect_port=in_port,
                bind_port=out_port,
            )
        )
        prev_addr = out_addr

    # --- spawn sink ---
    sink_in_port = int(prev_addr.rsplit(":", 1)[-1])
    snk_cls = block_registry.get(sink_type)
    snk_cfg = snk_cls.Config(**sink_doc)
    snk_cmd = snk_cls().command(snk_cfg, prev_addr, None)
    snk_proc = subprocess.Popen(snk_cmd)  # noqa: S603
    block_states.append(
        BlockState(
            name=sink_type,
            pid=snk_proc.pid,
            connect_port=sink_in_port,
        )
    )

    state = ChainState(
        id=chain_id,
        started=datetime.now(timezone.utc).isoformat(),
        compose=str(compose_file),
        blocks=block_states,
    )
    state.save()
    return state


# ------------------------------------------------------------------
# compose down
# ------------------------------------------------------------------


def down(chain_id: str, kill: bool = False) -> None:
    """Stop all blocks in the chain identified by *chain_id*."""
    from doppler_cli.state import ChainState  # noqa: PLC0415

    chain = ChainState.load(chain_id)
    stop_chain(chain, kill=kill)
