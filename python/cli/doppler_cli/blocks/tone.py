"""Tone source block — synthetic complex tone + AWGN."""

from __future__ import annotations

from doppler_cli.blocks import Block, BlockConfig, register


class ToneConfig(BlockConfig):
    sample_rate: float = 2.048e6
    center_freq: float = 0.0
    tone_freq: float = 100e3
    tone_power: float = -20.0
    noise_floor: float = -90.0


@register
class ToneBlock(Block):
    name = "tone"
    Config = ToneConfig
    role = "source"

    def command(
        self,
        config: ToneConfig,
        input_addr: str | None,
        output_addr: str | None,
    ) -> list[str]:
        assert output_addr is not None
        return [
            "doppler-source",
            "--type",
            "tone",
            "--bind",
            output_addr,
            "--fs",
            str(config.sample_rate),
            "--center",
            str(config.center_freq),
            "--tone-freq",
            str(config.tone_freq),
            "--tone-power",
            str(config.tone_power),
            "--noise-floor",
            str(config.noise_floor),
        ]
