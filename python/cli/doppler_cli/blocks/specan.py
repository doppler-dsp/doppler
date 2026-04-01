"""Specan sink block — spectrum analyzer display."""

from __future__ import annotations

from typing import Literal

from doppler_cli.blocks import Block, BlockConfig, register


class SpecanConfig(BlockConfig):
    mode: Literal["terminal", "web"] = "web"
    center: float = 0.0
    span: float | None = None
    rbw: float | None = None
    level: float | None = None
    web_port: int = 8080


@register
class SpecanBlock(Block):
    name = "specan"
    Config = SpecanConfig
    role = "sink"

    def command(
        self,
        config: SpecanConfig,
        input_addr: str | None,
        output_addr: str | None,
    ) -> list[str]:
        assert input_addr is not None
        cmd = [
            "doppler-specan",
            "--source",
            "pull",
            "--address",
            input_addr,
        ]
        if config.center:
            cmd += ["--center", str(config.center)]
        if config.span is not None:
            cmd += ["--span", str(config.span)]
        if config.rbw is not None:
            cmd += ["--rbw", str(config.rbw)]
        if config.level is not None:
            cmd += ["--level", str(config.level)]
        if config.mode == "web":
            cmd += ["--web", "--port", str(config.web_port)]
        return cmd
