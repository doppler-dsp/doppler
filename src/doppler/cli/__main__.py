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

try:
    import doppler.cli.blocks.fir
    import doppler.cli.blocks.specan
    import doppler.cli.blocks.tone  # noqa: F401
except ImportError as _e:
    print(
        f"doppler CLI requires optional dependencies: {_e}\n"
        f"Install with: pip install 'doppler-dsp[cli]'",
        file=sys.stderr,
    )
    sys.exit(1)


def build_parser() -> argparse.ArgumentParser:
    """Build the doppler argument parser.

    Separate from ``main()`` so tooling can validate documented CLI
    invocations against the real parser without executing anything --
    the docs shell-fence gate (``test_sh_doc_snippets.py``) parses
    every ``doppler ...`` line in the docs through this.
    """
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
    compose_sub = p_compose.add_subparsers(
        dest="compose_cmd", metavar="SUBCOMMAND"
    )

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
        "--name",
        default=None,
        metavar="NAME",
        help=(
            "Human-readable chain name (default: random hex ID). "
            "Used as the filename stem and chain ID."
        ),
    )
    p_init.add_argument(
        "--out",
        default=None,
        metavar="FILE",
        help=(
            "Write compose file to FILE (default: ~/.doppler/chains/<ID>.yml)"
        ),
    )

    p_up = compose_sub.add_parser(
        "up", help="Start a chain from a compose file"
    )
    p_up.add_argument(
        "file",
        metavar="FILE",
        nargs="?",
        default=None,
        help="Compose file to start (default: most recently created)",
    )

    p_down = compose_sub.add_parser("down", help="Stop a running chain")
    p_down.add_argument("id", metavar="ID")

    # main()'s bare `doppler compose` fallback prints this subparser's
    # help; stash it on the parser so the two stay one object.
    parser.compose_parser = p_compose  # type: ignore[attr-defined]
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    # Dispatch
    if args.command == "ps":
        from doppler.cli.ps import cmd_ps

        cmd_ps()

    elif args.command == "stop":
        from doppler.cli.ps import cmd_stop

        cmd_stop(args.id)

    elif args.command == "kill":
        from doppler.cli.ps import cmd_kill

        cmd_kill(args.id)

    elif args.command == "inspect":
        from doppler.cli.ps import cmd_inspect

        cmd_inspect(args.id)

    elif args.command == "logs":
        from doppler.cli.ps import cmd_logs

        cmd_logs(args.id, args.block)

    elif args.command == "compose":
        if args.compose_cmd == "init":
            from doppler.cli.compose import init

            out = Path(args.out) if args.out else None
            path = init(args.blocks, out=out, name=args.name)
            print(f"wrote {path}")

        elif args.compose_cmd == "up":
            from doppler.cli.compose import up
            from doppler.cli.state import _CHAINS_DIR

            if args.file:
                p = Path(args.file)
                # Bare name (no path separators) → resolve to chains dir
                if not p.parts[1:] and not p.suffix:
                    p = _CHAINS_DIR / f"{args.file}.yml"
                compose_file = p
            else:
                ymls = sorted(
                    _CHAINS_DIR.glob("*.yml"),
                    key=lambda p: p.stat().st_mtime,
                    reverse=True,
                )
                if not ymls:
                    print("no compose files found in ~/.doppler/chains/")
                    sys.exit(1)
                compose_file = ymls[0]
                print(f"using {compose_file}")
            state = up(compose_file)
            print(f"started chain {state.id} ({len(state.blocks)} blocks)")
            # Print block status lines (e.g. specan web URL)
            import yaml

            import doppler.cli.blocks as _reg

            doc = yaml.safe_load(compose_file.read_text())
            for section in ("source", "sink"):
                entry = doc.get(section, {})
                btype = entry.get("type")
                if not btype:
                    continue
                cfg_dict = {k: v for k, v in entry.items() if k != "type"}
                try:
                    cls = _reg.get(btype)
                    cfg = cls.Config(**cfg_dict)
                    for line in cls().status_lines(cfg):
                        print(f"  {line}")
                except Exception:
                    pass

        elif args.compose_cmd == "down":
            from doppler.cli.compose import down

            down(args.id)
            print(f"stopped chain {args.id}")

        else:
            parser.compose_parser.print_help()
            sys.exit(1)

    else:
        parser.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()
