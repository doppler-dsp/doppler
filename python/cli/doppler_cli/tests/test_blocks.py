"""
Tests for the block registry and individual block configs/commands.

Covers:
  - Registry: register, get, unknown block error
  - ToneBlock: defaults, config override, command argv
  - FirBlock: defaults, taps in command, no-taps omission
  - SpecanBlock: terminal and web mode commands
"""

import pytest

import doppler_cli.blocks.fir  # noqa: F401 — populate registry
import doppler_cli.blocks.specan  # noqa: F401
import doppler_cli.blocks.tone  # noqa: F401
from doppler_cli.blocks import get, all_blocks
from doppler_cli.blocks.tone import ToneBlock, ToneConfig
from doppler_cli.blocks.fir import FirBlock, FirConfig
from doppler_cli.blocks.specan import SpecanBlock, SpecanConfig


# ---------------------------------------------------------------------------
# Registry
# ---------------------------------------------------------------------------


class TestRegistry:
    def test_known_blocks_present(self):
        names = set(all_blocks())
        assert {"tone", "fir", "specan"} <= names

    def test_get_returns_correct_class(self):
        assert get("tone") is ToneBlock
        assert get("fir") is FirBlock
        assert get("specan") is SpecanBlock

    def test_get_unknown_raises(self):
        with pytest.raises(KeyError, match="Unknown block"):
            get("nonexistent_block")

    def test_roles(self):
        assert ToneBlock.role == "source"
        assert FirBlock.role == "chain"
        assert SpecanBlock.role == "sink"


# ---------------------------------------------------------------------------
# ToneBlock
# ---------------------------------------------------------------------------


class TestToneBlock:
    def test_default_config(self):
        cfg = ToneConfig()
        assert cfg.sample_rate == 2.048e6
        assert cfg.tone_freq == 100e3
        assert cfg.tone_power == -20.0
        assert cfg.noise_floor == -90.0

    def test_config_override(self):
        cfg = ToneConfig(tone_freq=50e3, tone_power=-30.0)
        assert cfg.tone_freq == 50e3
        assert cfg.tone_power == -30.0

    def test_command_contains_bind(self):
        cfg = ToneConfig()
        cmd = ToneBlock().command(cfg, None, "tcp://127.0.0.1:5600")
        assert "--bind" in cmd
        assert "tcp://127.0.0.1:5600" in cmd

    def test_command_contains_params(self):
        cfg = ToneConfig(sample_rate=1e6, tone_freq=200e3)
        cmd = ToneBlock().command(cfg, None, "tcp://127.0.0.1:5600")
        assert "--fs" in cmd
        assert "1000000.0" in cmd
        assert "--tone-freq" in cmd
        assert "200000.0" in cmd

    def test_command_no_input_addr(self):
        cfg = ToneConfig()
        cmd = ToneBlock().command(cfg, None, "tcp://127.0.0.1:5600")
        assert "--connect" not in cmd


# ---------------------------------------------------------------------------
# FirBlock
# ---------------------------------------------------------------------------


class TestFirBlock:
    def test_default_config_empty_taps(self):
        cfg = FirConfig()
        assert cfg.taps == []

    def test_command_contains_connect_and_bind(self):
        cfg = FirConfig()
        cmd = FirBlock().command(
            cfg,
            "tcp://127.0.0.1:5600",
            "tcp://127.0.0.1:5601",
        )
        assert "--connect" in cmd
        assert "tcp://127.0.0.1:5600" in cmd
        assert "--bind" in cmd
        assert "tcp://127.0.0.1:5601" in cmd

    def test_command_with_taps(self):
        cfg = FirConfig(taps=[0.25, 0.5, 0.25])
        cmd = FirBlock().command(
            cfg,
            "tcp://127.0.0.1:5600",
            "tcp://127.0.0.1:5601",
        )
        assert "--taps" in cmd
        assert "0.25" in cmd
        assert "0.5" in cmd

    def test_command_no_taps_flag_when_empty(self):
        cfg = FirConfig(taps=[])
        cmd = FirBlock().command(
            cfg,
            "tcp://127.0.0.1:5600",
            "tcp://127.0.0.1:5601",
        )
        assert "--taps" not in cmd

    def test_requires_both_addrs(self):
        cfg = FirConfig()
        with pytest.raises(AssertionError):
            FirBlock().command(cfg, None, "tcp://127.0.0.1:5601")
        with pytest.raises(AssertionError):
            FirBlock().command(cfg, "tcp://127.0.0.1:5600", None)


# ---------------------------------------------------------------------------
# SpecanBlock
# ---------------------------------------------------------------------------


class TestSpecanBlock:
    def test_default_config(self):
        cfg = SpecanConfig()
        assert cfg.mode == "terminal"
        assert cfg.web_port == 8080

    def test_terminal_command(self):
        cfg = SpecanConfig()
        cmd = SpecanBlock().command(cfg, "tcp://127.0.0.1:5601", None)
        assert "--source" in cmd
        assert "socket" in cmd
        assert "--address" in cmd
        assert "tcp://127.0.0.1:5601" in cmd
        assert "--web" not in cmd

    def test_web_command_includes_web_flag(self):
        cfg = SpecanConfig(mode="web", web_port=9090)
        cmd = SpecanBlock().command(cfg, "tcp://127.0.0.1:5601", None)
        assert "--web" in cmd
        assert "--port" in cmd
        assert "9090" in cmd

    def test_optional_display_params(self):
        cfg = SpecanConfig(span=200e3, rbw=500.0, level=-40.0)
        cmd = SpecanBlock().command(cfg, "tcp://127.0.0.1:5601", None)
        assert "--span" in cmd
        assert "--rbw" in cmd
        assert "--level" in cmd

    def test_requires_input_addr(self):
        cfg = SpecanConfig()
        with pytest.raises(AssertionError):
            SpecanBlock().command(cfg, None, None)
