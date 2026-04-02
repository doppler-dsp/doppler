"""
Tests for doppler dopplerfile — YAML-defined block loading.

Covers:
  - load() parses name / role / executable / config / args
  - auto-map: --bind for source, --connect for sink
  - auto-map: config fields become --flag-name value
  - auto-map: lists JSON-encoded, bool flags are bare
  - explicit args template overrides auto-map
  - discover() finds ~/.doppler/blocks/<name>.yml
  - discover() finds ./<name>.yml (CWD)
  - discover() returns None when not found
  - blocks.get() falls back to dopplerfile discovery
  - missing required field raises KeyError
"""

from __future__ import annotations

from pathlib import Path

import pytest
import yaml

import doppler_cli.blocks.fir  # noqa: F401
import doppler_cli.blocks.specan  # noqa: F401
import doppler_cli.blocks.tone  # noqa: F401
from doppler_cli import blocks as block_registry
from doppler_cli import dopplerfile as df


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _write_dopplerfile(tmp_path: Path, doc: dict) -> Path:
    p = tmp_path / f"{doc['name']}.yml"
    p.write_text(yaml.dump(doc))
    return p


SIMPLE_DOC = {
    "name": "chirp",
    "role": "source",
    "executable": "./chirp.py",
    "config": {
        "sample_rate": 2048000.0,
        "sweep_rate": 50000.0,
        "tone_power": -20.0,
    },
}


# ---------------------------------------------------------------------------
# load()
# ---------------------------------------------------------------------------


class TestLoad:
    def test_name(self, tmp_path):
        path = _write_dopplerfile(tmp_path, SIMPLE_DOC)
        cls = df.load(path)
        assert cls.name == "chirp"

    def test_role(self, tmp_path):
        path = _write_dopplerfile(tmp_path, SIMPLE_DOC)
        cls = df.load(path)
        assert cls.role == "source"

    def test_config_defaults(self, tmp_path):
        path = _write_dopplerfile(tmp_path, SIMPLE_DOC)
        cls = df.load(path)
        cfg = cls.Config()
        assert cfg.sample_rate == 2048000.0
        assert cfg.sweep_rate == 50000.0

    def test_missing_name_raises(self, tmp_path):
        bad = {"role": "source", "executable": "./x.py"}
        p = tmp_path / "bad.yml"
        p.write_text(yaml.dump(bad))
        with pytest.raises(KeyError):
            df.load(p)


# ---------------------------------------------------------------------------
# Auto-map command building
# ---------------------------------------------------------------------------


class TestAutoMap:
    def _block(self, tmp_path, doc=None):
        path = _write_dopplerfile(tmp_path, doc or SIMPLE_DOC)
        return df.load(path)()

    def test_executable_first(self, tmp_path):
        blk = self._block(tmp_path)
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert cmd[0] == "./chirp.py"

    def test_source_gets_bind(self, tmp_path):
        blk = self._block(tmp_path)
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert "--bind" in cmd
        assert "tcp://127.0.0.1:5600" in cmd

    def test_source_no_connect(self, tmp_path):
        blk = self._block(tmp_path)
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert "--connect" not in cmd

    def test_sink_gets_connect(self, tmp_path):
        doc = {**SIMPLE_DOC, "role": "sink"}
        blk = self._block(tmp_path, doc)
        cmd = blk.command(blk.__class__.Config(), "tcp://127.0.0.1:5600", None)
        assert "--connect" in cmd
        assert "--bind" not in cmd

    def test_chain_gets_both(self, tmp_path):
        doc = {**SIMPLE_DOC, "role": "chain"}
        blk = self._block(tmp_path, doc)
        cmd = blk.command(
            blk.__class__.Config(),
            "tcp://127.0.0.1:5600",
            "tcp://127.0.0.1:5601",
        )
        assert "--connect" in cmd
        assert "--bind" in cmd

    def test_config_field_mapping(self, tmp_path):
        blk = self._block(tmp_path)
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert "--sample-rate" in cmd
        assert "--sweep-rate" in cmd
        assert "--tone-power" in cmd

    def test_list_field_json_encoded(self, tmp_path):
        doc = {**SIMPLE_DOC, "config": {"taps": []}}
        blk = self._block(tmp_path, doc)
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        idx = cmd.index("--taps")
        assert cmd[idx + 1] == "[]"

    def test_bool_true_is_bare_flag(self, tmp_path):
        doc = {**SIMPLE_DOC, "config": {"verbose": True}}
        blk = self._block(tmp_path, doc)
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert "--verbose" in cmd
        # bare flag — no separate value token follows it
        idx = cmd.index("--verbose")
        assert idx == len(cmd) - 1 or not cmd[idx + 1].startswith("True")

    def test_bool_false_omitted(self, tmp_path):
        doc = {**SIMPLE_DOC, "config": {"verbose": False}}
        blk = self._block(tmp_path, doc)
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert "--verbose" not in cmd


# ---------------------------------------------------------------------------
# Explicit args template
# ---------------------------------------------------------------------------


class TestArgsTemplate:
    def test_explicit_args_used(self, tmp_path):
        doc = {
            **SIMPLE_DOC,
            "args": {
                "output": "{output_addr}",
                "rate": "{sample_rate}",
            },
        }
        path = _write_dopplerfile(tmp_path, doc)
        blk = df.load(path)()
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert "--output" in cmd
        assert "tcp://127.0.0.1:5600" in cmd
        assert "--rate" in cmd

    def test_auto_map_not_applied_with_template(self, tmp_path):
        doc = {
            **SIMPLE_DOC,
            "args": {"output": "{output_addr}"},
        }
        path = _write_dopplerfile(tmp_path, doc)
        blk = df.load(path)()
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        # --bind is the auto-map flag; should NOT appear when using template
        assert "--bind" not in cmd


# ---------------------------------------------------------------------------
# discover()
# ---------------------------------------------------------------------------


class TestDiscover:
    def test_finds_global_blocks_dir(self, tmp_path, monkeypatch):
        monkeypatch.setattr(df, "_BLOCKS_DIR", tmp_path)
        _write_dopplerfile(tmp_path, SIMPLE_DOC)
        cls = df.discover("chirp")
        assert cls is not None
        assert cls.name == "chirp"

    def test_finds_cwd(self, tmp_path, monkeypatch):
        monkeypatch.chdir(tmp_path)
        _write_dopplerfile(tmp_path, SIMPLE_DOC)
        monkeypatch.setattr(df, "_BLOCKS_DIR", tmp_path / "nonexistent")
        cls = df.discover("chirp")
        assert cls is not None

    def test_returns_none_when_not_found(self, tmp_path, monkeypatch):
        monkeypatch.setattr(df, "_BLOCKS_DIR", tmp_path)
        monkeypatch.chdir(tmp_path)
        assert df.discover("no-such-block") is None

    def test_global_takes_priority_over_cwd(self, tmp_path, monkeypatch):
        global_dir = tmp_path / "global"
        cwd_dir = tmp_path / "cwd"
        global_dir.mkdir()
        cwd_dir.mkdir()

        # Global has sweep_rate=1.0, CWD has sweep_rate=2.0
        _write_dopplerfile(global_dir, {**SIMPLE_DOC, "config": {"sweep_rate": 1.0}})
        _write_dopplerfile(cwd_dir, {**SIMPLE_DOC, "config": {"sweep_rate": 2.0}})

        monkeypatch.setattr(df, "_BLOCKS_DIR", global_dir)
        monkeypatch.chdir(cwd_dir)

        cls = df.discover("chirp")
        assert cls.Config().sweep_rate == 1.0


# ---------------------------------------------------------------------------
# Dependency isolation
# ---------------------------------------------------------------------------


class TestDependencies:
    def _block_with_deps(self, tmp_path, deps):
        doc = {**SIMPLE_DOC, "dependencies": deps}
        path = _write_dopplerfile(tmp_path, doc)
        return df.load(path)()

    def test_no_deps_no_uv_wrap(self, tmp_path):
        path = _write_dopplerfile(tmp_path, SIMPLE_DOC)
        blk = df.load(path)()
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert cmd[0] != "uv"

    def test_deps_wrap_with_uv_run(self, tmp_path):
        blk = self._block_with_deps(tmp_path, ["numpy", "scipy"])
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert cmd[:2] == ["uv", "run"]

    def test_deps_each_get_with_flag(self, tmp_path):
        blk = self._block_with_deps(tmp_path, ["numpy", "scipy"])
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert "--with" in cmd
        assert cmd[cmd.index("--with") + 1] == "numpy"
        assert cmd[cmd.index("--with", cmd.index("--with") + 2) + 1] == "scipy"

    def test_deps_executable_still_present(self, tmp_path):
        blk = self._block_with_deps(tmp_path, ["numpy"])
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert "./chirp.py" in cmd

    def test_deps_bind_addr_still_present(self, tmp_path):
        blk = self._block_with_deps(tmp_path, ["numpy"])
        cmd = blk.command(blk.__class__.Config(), None, "tcp://127.0.0.1:5600")
        assert "tcp://127.0.0.1:5600" in cmd


# ---------------------------------------------------------------------------
# blocks.get() fallback
# ---------------------------------------------------------------------------


class TestRegistryFallback:
    def test_get_falls_back_to_dopplerfile(self, tmp_path, monkeypatch):
        monkeypatch.setattr(df, "_BLOCKS_DIR", tmp_path)
        _write_dopplerfile(tmp_path, SIMPLE_DOC)
        cls = block_registry.get("chirp")
        assert cls.name == "chirp"

    def test_get_raises_for_unknown(self, tmp_path, monkeypatch):
        monkeypatch.setattr(df, "_BLOCKS_DIR", tmp_path)
        monkeypatch.chdir(tmp_path)
        with pytest.raises(KeyError, match="no-such-block"):
            block_registry.get("no-such-block")
