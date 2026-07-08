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
  - Live `compose up`: real subprocesses, real nats:// data flow
"""

import time

import pytest
import yaml

import doppler.cli.blocks.fir
import doppler.cli.blocks.specan
import doppler.cli.blocks.tone  # noqa: F401
from doppler.cli import compose as compose_mod
from doppler.cli import ports as ports_mod
from doppler.cli.state import stop_chain

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

    def test_named_chain_id(self, tmp_path, monkeypatch):
        monkeypatch.setattr(compose_mod, "_CHAINS_DIR", tmp_path)
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        path = compose_mod.init(["tone", "specan"], name="filter-test")
        doc = yaml.safe_load(path.read_text())
        assert doc["id"] == "filter-test"
        assert path.name == "filter-test.yml"

    def test_named_chain_filename(self, tmp_path, monkeypatch):
        monkeypatch.setattr(compose_mod, "_CHAINS_DIR", tmp_path)
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        path = compose_mod.init(["tone", "specan"], name="my-chain")
        assert path == tmp_path / "my-chain.yml"

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


# ---------------------------------------------------------------------------
# Live `compose up` — actually spawns the block subprocesses over a real
# nats-server. This is the level `TestComposeInit` above never reaches: it
# only inspects the scaffolded YAML, never runs `up()`. That gap is exactly
# how FirBlock shipped referencing a `doppler-fir` executable that had no
# console-script entry point behind it -- `doppler compose up` with any
# 3-block chain failed with FileNotFoundError, and nothing here caught it.
# ---------------------------------------------------------------------------


def _nats_available() -> bool:
    import socket

    try:
        socket.create_connection(("127.0.0.1", 4222), timeout=0.3).close()
        return True
    except OSError:
        return False


_requires_nats = pytest.mark.skipif(
    not _nats_available(),
    reason="no nats-server on 127.0.0.1:4222 (run `nats-server -js`)",
)


@_requires_nats
class TestComposeUpLive:
    def test_three_block_chain_spawns_and_moves_data(
        self, tmp_path, monkeypatch
    ):
        """tone -> fir -> specan: the exact chain from docs/cli/index.md.

        specan's terminal mode needs a real TTY and can't run headless
        under subprocess.Popen (a separate, pre-existing limitation
        unrelated to this test) -- so this only asserts on tone and fir,
        the two blocks compose actually needs to get right for this bug.
        It pulls directly from fir's output subject to prove real,
        filtered samples cross the whole nats:// wiring compose.up()
        computed, not just that the processes stay alive.
        """
        monkeypatch.setattr(compose_mod, "_CHAINS_DIR", tmp_path)
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)

        path = compose_mod.init(["tone", "fir", "specan"], name="live-test")
        chain = compose_mod.up(path)
        try:
            fir_block = next(b for b in chain.blocks if b.name == "fir")
            tone_block = next(b for b in chain.blocks if b.name == "tone")

            from doppler.cli.state import pid_alive

            time.sleep(1.0)
            assert pid_alive(tone_block.pid), "tone source process died"
            assert pid_alive(fir_block.pid), "fir chain process died"

            from doppler.stream import Pull

            fir_out = f"nats://127.0.0.1:4222/dp-chain-{fir_block.bind_port}"
            with Pull(fir_out) as pull:
                samples, hdr = pull.recv(timeout_ms=5000)
            assert len(samples) > 0
            assert hdr["sample_rate"] > 0
        finally:
            stop_chain(chain, kill=True)
