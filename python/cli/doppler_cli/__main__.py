"""
doppler — signal processing pipeline CLI.

Usage
-----
    doppler ps
    doppler stop <ID>
    doppler kill <ID>
    doppler inspect <ID>
    doppler logs <ID> [--block NAME]

    doppler compose init <BLOCK...>
    doppler compose up <FILE>
    doppler compose down <ID>
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Import blocks to populate the registry
import doppler_cli.blocks.fir  # noqa: F401
import doppler_cli.blocks.specan  # noqa: F401
import doppler_cli.blocks.tone  # noqa: F401


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="doppler",
        description="doppler signal processing pipeline CLI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command", metavar="COMMAND")

    # --- ps ---
    sub.add_parser("ps", help="List running chains")

    # --- stop ---
    p_stop = sub.add_parser("stop", help="Gracefully stop a chain")
    p_stop.add_argument("id", metavar="ID")

    # --- kill ---
    p_kill = sub.add_parser("kill", help="Forcefully kill a chain")
    p_kill.add_argument("id", metavar="ID")

    # --- inspect ---
    p_inspect = sub.add_parser("inspect", help="Show resolved config and PIDs")
    p_inspect.add_argument("id", metavar="ID")

    # --- logs ---
    p_logs = sub.add_parser("logs", help="Stream logs from a chain")
    p_logs.add_argument("id", metavar="ID")
    p_logs.add_argument(
        "--block",
        default=None,
        metavar="NAME",
        help="Show logs for a specific block only",
    )

    # --- compose ---
    p_compose = sub.add_parser("compose", help="Manage compose chains")
    compose_sub = p_compose.add_subparsers(dest="compose_cmd", metavar="SUBCOMMAND")

    p_init = compose_sub.add_parser(
        "init",
        help="Scaffold a compose file with default config",
    )
    p_init.add_argument(
        "blocks",
        nargs="+",
        metavar="BLOCK",
        help="Ordered block names, e.g. tone fir specan",
    )
    p_init.add_argument(
        "--out",
        default=None,
        metavar="FILE",
        help="Write compose file to FILE (default: ~/.doppler/chains/<ID>.yml)",
    )

    p_up = compose_sub.add_parser("up", help="Start a chain from a compose file")
    p_up.add_argument("file", metavar="FILE")

    p_down = compose_sub.add_parser("down", help="Stop a running chain")
    p_down.add_argument("id", metavar="ID")

    args = parser.parse_args()

    # Dispatch
    if args.command == "ps":
        from doppler_cli.ps import cmd_ps  # noqa: PLC0415

        cmd_ps()

    elif args.command == "stop":
        from doppler_cli.ps import cmd_stop  # noqa: PLC0415

        cmd_stop(args.id)

    elif args.command == "kill":
        from doppler_cli.ps import cmd_kill  # noqa: PLC0415

        cmd_kill(args.id)

    elif args.command == "inspect":
        from doppler_cli.ps import cmd_inspect  # noqa: PLC0415

        cmd_inspect(args.id)

    elif args.command == "logs":
        from doppler_cli.ps import cmd_logs  # noqa: PLC0415

        cmd_logs(args.id, args.block)

    elif args.command == "compose":
        if args.compose_cmd == "init":
            from doppler_cli.compose import init  # noqa: PLC0415

            out = Path(args.out) if args.out else None
            path = init(args.blocks, out=out)
            print(f"wrote {path}")

        elif args.compose_cmd == "up":
            from doppler_cli.compose import up  # noqa: PLC0415

            state = up(Path(args.file))
            print(f"started chain {state.id} ({len(state.blocks)} blocks)")

        elif args.compose_cmd == "down":
            from doppler_cli.compose import down  # noqa: PLC0415

            down(args.id)
            print(f"stopped chain {args.id}")

        else:
            p_compose.print_help()
            sys.exit(1)

    else:
        parser.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()
