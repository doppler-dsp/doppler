"""custom_block_demo.py — your own pipeline block for the doppler CLI.

A block is a Python class the CLI spawns as a subprocess in a pipeline:
`Block` says how to start it, `BlockConfig` (a pydantic model) declares
its settings with their defaults, and `register` makes it reachable by
name. This script defines a `noise` source, registers it, and checks the
two things the CLI relies on: the config validates and fills defaults,
and `command()` builds the argv the pipeline runs.

Needs the CLI extra: ``pip install 'doppler-dsp[cli]'``.

Run:
  python -m doppler.examples.custom_block_demo
"""

from __future__ import annotations

from doppler.examples._extra import require

require("pydantic", extra="cli")
from pydantic import ValidationError

# --8<-- [start:block]
from doppler.cli import Block, BlockConfig, register


class NoiseConfig(BlockConfig):
    sample_rate: float = 2.048e6
    noise_floor: float = -60.0


@register
class NoiseBlock(Block):
    name = "noise"
    Config = NoiseConfig
    role = "source"  # "source" | "chain" | "sink"

    def command(self, config, input_addr, output_addr):
        assert output_addr is not None
        return [
            "doppler-noise",
            "--bind",
            output_addr,
            "--fs",
            str(config.sample_rate),
            "--noise-floor",
            str(config.noise_floor),
        ]


# --8<-- [end:block]


def main() -> None:
    # register returns the class it was given, so the decorator is inert.
    assert NoiseBlock.name == "noise" and NoiseBlock.role == "source"

    # The config fills its defaults and validates an override.
    cfg = NoiseBlock.Config()
    assert (cfg.sample_rate, cfg.noise_floor) == (2.048e6, -60.0)
    loud = NoiseBlock.Config(noise_floor=-30)
    assert loud.noise_floor == -30.0
    try:
        NoiseBlock.Config(sample_rate="fast")
    except ValidationError:
        pass
    else:
        raise AssertionError("a non-numeric sample_rate must be refused")

    # The argv the pipeline spawns, from a config and the output address.
    argv = NoiseBlock().command(loud, None, "tcp://127.0.0.1:5555")
    assert argv == [
        "doppler-noise",
        "--bind",
        "tcp://127.0.0.1:5555",
        "--fs",
        "2048000.0",
        "--noise-floor",
        "-30.0",
    ], argv
    print("noise block: config validated, argv:", " ".join(argv))


if __name__ == "__main__":
    main()
