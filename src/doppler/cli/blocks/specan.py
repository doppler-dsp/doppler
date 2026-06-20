"""Specan sink block — spectrum analyzer display."""

from __future__ import annotations

from typing import Literal, Optional

from doppler.cli.blocks import Block, BlockConfig, register


class SpecanConfig(BlockConfig):
    mode: Literal["terminal", "web"] = "web"
    center: float = 0.0
    # `Optional[...]`, not `float | None`: pydantic force-evaluates field
    # annotations via get_type_hints at class-definition time, and PEP-604
    # `|` unions are not evaluatable on Python 3.9. `from __future__ import
    # annotations` does not help here — pydantic resolves the strings.
    span: Optional[float] = None  # noqa: UP045
    rbw: Optional[float] = None  # noqa: UP045
    level: Optional[float] = None  # noqa: UP045
    web_port: int = 8080
    web_host: str = "127.0.0.1"


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
            cmd += [
                "--web",
                "--port",
                str(config.web_port),
                "--host",
                config.web_host,
            ]
        return cmd

    def status_lines(self, config: SpecanConfig) -> list[str]:
        if config.mode == "web":
            return [f"specan → http://{config.web_host}:{config.web_port}"]
        return []
