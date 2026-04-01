"""
Tests for doppler compose init.

Covers:
  - init() writes a valid YAML file
  - Generated file has correct id, source, chain, sink structure
  - Ports are assigned and present in the file
  - Custom output path is respected
  - Single-port chain (source → sink, no middle blocks)
  - Unknown block name raises KeyError
  - Wrong roles raise ValueError
  - Too-few blocks (< 2) raises ValueError
"""

import yaml
import pytest

import doppler_cli.blocks.fir  # noqa: F401 — populate registry
import doppler_cli.blocks.specan  # noqa: F401
import doppler_cli.blocks.tone  # noqa: F401
from doppler_cli import compose as compose_mod
from doppler_cli import ports as ports_mod


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _init(tmp_path, monkeypatch, blocks):
    """Run compose.init with a temp chains dir and return parsed YAML."""
    monkeypatch.setattr(compose_mod, "_CHAINS_DIR", tmp_path)
    monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
    path = compose_mod.init(blocks)
    return path, yaml.safe_load(path.read_text())


# ---------------------------------------------------------------------------
# Structure
# ---------------------------------------------------------------------------


class TestComposeInit:
    def test_file_is_written(self, tmp_path, monkeypatch):
        path, _ = _init(tmp_path, monkeypatch, ["tone", "specan"])
        assert path.exists()

    def test_has_id(self, tmp_path, monkeypatch):
        _, doc = _init(tmp_path, monkeypatch, ["tone", "specan"])
        assert "id" in doc
        assert len(doc["id"]) == 6  # 3 hex bytes → 6 chars

    def test_source_type(self, tmp_path, monkeypatch):
        _, doc = _init(tmp_path, monkeypatch, ["tone", "specan"])
        assert doc["source"]["type"] == "tone"

    def test_sink_type(self, tmp_path, monkeypatch):
        _, doc = _init(tmp_path, monkeypatch, ["tone", "specan"])
        assert doc["sink"]["type"] == "specan"

    def test_source_has_port(self, tmp_path, monkeypatch):
        _, doc = _init(tmp_path, monkeypatch, ["tone", "specan"])
        assert "port" in doc["source"]
        assert doc["source"]["port"] == ports_mod._BASE_PORT

    def test_no_chain_key_for_two_blocks(self, tmp_path, monkeypatch):
        _, doc = _init(tmp_path, monkeypatch, ["tone", "specan"])
        assert "chain" not in doc or doc.get("chain") == []

    def test_chain_block_present(self, tmp_path, monkeypatch):
        _, doc = _init(tmp_path, monkeypatch, ["tone", "fir", "specan"])
        assert "chain" in doc
        assert len(doc["chain"]) == 1
        assert "fir" in doc["chain"][0]

    def test_chain_block_has_port(self, tmp_path, monkeypatch):
        _, doc = _init(tmp_path, monkeypatch, ["tone", "fir", "specan"])
        fir_entry = doc["chain"][0]["fir"]
        assert "port" in fir_entry

    def test_source_defaults_present(self, tmp_path, monkeypatch):
        _, doc = _init(tmp_path, monkeypatch, ["tone", "specan"])
        src = doc["source"]
        assert "sample_rate" in src
        assert "tone_freq" in src

    def test_sink_defaults_present(self, tmp_path, monkeypatch):
        _, doc = _init(tmp_path, monkeypatch, ["tone", "specan"])
        assert "mode" in doc["sink"]

    def test_custom_out_path(self, tmp_path, monkeypatch):
        monkeypatch.setattr(compose_mod, "_CHAINS_DIR", tmp_path)
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        out = tmp_path / "my_chain.yml"
        path = compose_mod.init(["tone", "specan"], out=out)
        assert path == out
        assert out.exists()

    def test_ports_are_unique(self, tmp_path, monkeypatch):
        _, doc = _init(tmp_path, monkeypatch, ["tone", "fir", "specan"])
        src_port = doc["source"]["port"]
        fir_port = doc["chain"][0]["fir"]["port"]
        assert src_port != fir_port


# ---------------------------------------------------------------------------
# Error cases
# ---------------------------------------------------------------------------


class TestComposeInitErrors:
    def test_too_few_blocks(self, tmp_path, monkeypatch):
        monkeypatch.setattr(compose_mod, "_CHAINS_DIR", tmp_path)
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        with pytest.raises(ValueError, match="at least"):
            compose_mod.init(["tone"])

    def test_unknown_block(self, tmp_path, monkeypatch):
        monkeypatch.setattr(compose_mod, "_CHAINS_DIR", tmp_path)
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        with pytest.raises(KeyError, match="Unknown block"):
            compose_mod.init(["tone", "mystery_block"])

    def test_wrong_first_role(self, tmp_path, monkeypatch):
        monkeypatch.setattr(compose_mod, "_CHAINS_DIR", tmp_path)
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        with pytest.raises(ValueError, match="source"):
            compose_mod.init(["specan", "tone"])

    def test_wrong_last_role(self, tmp_path, monkeypatch):
        monkeypatch.setattr(compose_mod, "_CHAINS_DIR", tmp_path)
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        with pytest.raises(ValueError, match="sink"):
            compose_mod.init(["tone", "fir"])
