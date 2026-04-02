"""
doppler dopplerfile — YAML-defined custom block loader.

A dopplerfile lets you register a pipeline block without writing
Python.  Drop a small YAML file next to your script and
``doppler compose init`` picks it up automatically:

    # chirp.yml
    name: chirp
    role: source
    executable: ./chirp.py
    config:
        sample_rate: 2048000.0
        sweep_rate:  50000.0
        tone_power:  -20.0
        noise_floor: -90.0

Arg mapping
-----------
By default every config field is passed as ``--{field-with-dashes}
value`` and the socket addresses are injected as:

    source / chain  →  ``--bind  {output_addr}``
    chain  / sink   →  ``--connect {input_addr}``

For scripts that use different flag names, add an explicit ``args``
section using ``{placeholder}`` substitution:

    args:
        output: "{output_addr}"
        rate:   "{sample_rate}"

Available placeholders are ``{output_addr}``, ``{input_addr}``, and
any config field name.

Discovery order for ``doppler compose init <name>``:
    1. Built-in Python registry
    2. ~/.doppler/blocks/<name>.yml
    3. ./<name>.yml  (current working directory)
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Type

import yaml
from pydantic import create_model

from doppler_cli.blocks import Block, BlockConfig

_BLOCKS_DIR = Path.home() / ".doppler" / "blocks"


# ---------------------------------------------------------------------------
# Config synthesis
# ---------------------------------------------------------------------------


def _make_config(defaults: dict[str, Any]) -> Type[BlockConfig]:
    """Dynamically build a pydantic BlockConfig from a defaults dict."""
    fields: dict[str, Any] = {}
    for k, v in defaults.items():
        fields[k] = (type(v), v)
    return create_model("DopplerfileConfig", __base__=BlockConfig, **fields)


# ---------------------------------------------------------------------------
# Block synthesis
# ---------------------------------------------------------------------------


def _make_block(
    name: str,
    role: str,
    executable: str,
    config_defaults: dict[str, Any],
    args_template: dict[str, str] | None,
    dependencies: list[str] | None = None,
) -> Type[Block]:
    """Return a live Block subclass described by a dopplerfile document."""

    ConfigClass = _make_config(config_defaults)

    def _command(
        self,
        config: BlockConfig,
        input_addr: str | None,
        output_addr: str | None,
    ) -> list[str]:
        exe = self.__class__._exe
        tmpl = self.__class__._args_template
        cmd: list[str] = [exe]

        if tmpl is not None:
            # Explicit template: substitute placeholders
            ctx: dict[str, Any] = dict(config.model_dump())
            ctx["output_addr"] = output_addr or ""
            ctx["input_addr"] = input_addr or ""
            for flag, value_tmpl in tmpl.items():
                cmd += [f"--{flag}", value_tmpl.format(**ctx)]
        else:
            # Auto-map: inject socket addresses, then all config fields
            if output_addr is not None:
                cmd += ["--bind", output_addr]
            if input_addr is not None:
                cmd += ["--connect", input_addr]
            for field, value in config.model_dump().items():
                flag = "--" + field.replace("_", "-")
                if isinstance(value, (list, dict)):
                    cmd += [flag, json.dumps(value)]
                elif isinstance(value, bool):
                    if value:
                        cmd.append(flag)
                else:
                    cmd += [flag, str(value)]

        # Wrap with uv run --with <dep> ... for dependency isolation
        deps = self.__class__._dependencies
        if deps:
            with_flags: list[str] = []
            for dep in deps:
                with_flags += ["--with", dep]
            cmd = ["uv", "run"] + with_flags + cmd

        return cmd

    cls = type(
        f"DopplerfileBlock_{name}",
        (Block,),
        {
            "name": name,
            "role": role,
            "Config": ConfigClass,
            "_exe": executable,
            "_args_template": args_template,
            "_dependencies": dependencies or [],
            "command": _command,
        },
    )
    return cls  # type: ignore[return-value]


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------


def load(path: Path) -> Type[Block]:
    """Parse *path* as a dopplerfile and return a Block subclass.

    Parameters
    ----------
    path:
        Path to a YAML dopplerfile.

    Returns
    -------
    A Block subclass ready to be used by the compose engine.

    Raises
    ------
    KeyError
        If the dopplerfile is missing a required field.
    """
    doc = yaml.safe_load(path.read_text())
    return _make_block(
        name=doc["name"],
        role=doc["role"],
        executable=doc["executable"],
        config_defaults=doc.get("config", {}),
        args_template=doc.get("args"),
        dependencies=doc.get("dependencies"),
    )


def discover(name: str) -> Type[Block] | None:
    """Search for a dopplerfile that defines *name*.

    Search order:

    1. ``~/.doppler/blocks/<name>.yml``
    2. ``./<name>.yml`` (current working directory)

    Returns ``None`` if no dopplerfile is found.
    """
    candidates = [
        _BLOCKS_DIR / f"{name}.yml",
        Path.cwd() / f"{name}.yml",
    ]
    for path in candidates:
        if path.exists():
            return load(path)
    return None
