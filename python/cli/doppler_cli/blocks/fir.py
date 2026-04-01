"""FIR filter chain block."""

from __future__ import annotations

from doppler_cli.blocks import Block, BlockConfig, register


class FirConfig(BlockConfig):
    taps: list[float] = []


@register
class FirBlock(Block):
    name = "fir"
    Config = FirConfig
    role = "chain"

    def command(
        self,
        config: FirConfig,
        input_addr: str | None,
        output_addr: str | None,
    ) -> list[str]:
        assert input_addr is not None
        assert output_addr is not None
        cmd = [
            "doppler-fir",
            "--connect",
            input_addr,
            "--bind",
            output_addr,
        ]
        if config.taps:
            cmd += ["--taps", *[str(t) for t in config.taps]]
        return cmd
