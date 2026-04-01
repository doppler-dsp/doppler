"""doppler ps / stop / kill / logs / inspect commands."""

from __future__ import annotations

import json
import subprocess
import sys

from rich.console import Console
from rich.table import Table

from doppler_cli.state import ChainState, list_chains, pid_alive, stop_chain

console = Console()


def cmd_ps() -> None:
    """List running chains."""
    chains = list_chains()
    if not chains:
        console.print("[dim]No chains running.[/dim]")
        return

    table = Table(show_header=True, header_style="bold")
    table.add_column("ID", style="cyan")
    table.add_column("Started")
    table.add_column("Blocks")
    table.add_column("Status")

    for chain in chains:
        alive = [b for b in chain.blocks if pid_alive(b.pid)]
        status = (
            "[green]running[/green]"
            if len(alive) == len(chain.blocks)
            else f"[yellow]{len(alive)}/{len(chain.blocks)} alive[/yellow]"
        )
        block_names = ", ".join(b.name for b in chain.blocks)
        table.add_row(chain.id, chain.started[:19], block_names, status)

    console.print(table)


def cmd_stop(chain_id: str) -> None:
    """Gracefully stop a chain (SIGTERM)."""
    chain = ChainState.load(chain_id)
    stop_chain(chain, kill=False)
    console.print(f"Stopped chain [cyan]{chain_id}[/cyan].")


def cmd_kill(chain_id: str) -> None:
    """Forcefully kill a chain (SIGKILL)."""
    chain = ChainState.load(chain_id)
    stop_chain(chain, kill=True)
    console.print(f"Killed chain [cyan]{chain_id}[/cyan].")


def cmd_inspect(chain_id: str) -> None:
    """Print resolved config and PIDs for a chain."""
    chain = ChainState.load(chain_id)
    console.print_json(
        json.dumps(
            {
                "id": chain.id,
                "started": chain.started,
                "compose": chain.compose,
                "blocks": [
                    {
                        "name": b.name,
                        "pid": b.pid,
                        "alive": pid_alive(b.pid),
                        "bind_port": b.bind_port,
                        "connect_port": b.connect_port,
                    }
                    for b in chain.blocks
                ],
            }
        )
    )


def cmd_logs(chain_id: str, block_name: str | None = None) -> None:
    """Stream logs for a chain or a specific block.

    Since each block is an independent subprocess, we tail the journal
    (systemd) or fall back to printing PIDs for manual inspection.
    """
    chain = ChainState.load(chain_id)
    targets = (
        [b for b in chain.blocks if b.name == block_name]
        if block_name
        else chain.blocks
    )
    if not targets:
        console.print(
            f"[red]Block {block_name!r} not found in chain {chain_id!r}.[/red]"
        )
        sys.exit(1)

    pids = [str(b.pid) for b in targets]

    # Try journalctl first (systemd systems)
    try:
        subprocess.run(  # noqa: S603
            ["journalctl", "--follow", "--output=short-iso"]
            + [f"_PID={p}" for p in pids],
            check=False,
        )
    except FileNotFoundError:
        # No journalctl — tell the user the PIDs
        console.print(
            f"[yellow]journalctl not available.[/yellow] Block PIDs: {', '.join(pids)}"
        )
        console.print(
            "Use: [bold]tail -f /proc/<PID>/fd/1[/bold] on Linux, "
            "or attach your preferred log viewer."
        )
