"""
Tests for port auto-allocation.

Covers:
  - allocate() returns the requested number of ports
  - Ports are in the valid range
  - Ports are unique within a single allocation
  - Pre-occupied ports (from fake state files) are skipped
  - Requesting too many ports raises RuntimeError
"""

import json
import pytest

from doppler_cli import ports as ports_mod


def _write_fake_state(chains_dir, chain_id: str, used_ports: list[int]):
    chains_dir.mkdir(parents=True, exist_ok=True)
    state = {
        "id": chain_id,
        "started": "2026-01-01T00:00:00+00:00",
        "compose": "/tmp/fake.yml",
        "blocks": [
            {"name": "tone", "pid": 9999, "bind_port": p, "connect_port": None}
            for p in used_ports
        ],
    }
    (chains_dir / f"{chain_id}.json").write_text(json.dumps(state))


# ---------------------------------------------------------------------------
# Basic allocation
# ---------------------------------------------------------------------------


class TestAllocate:
    def test_returns_correct_count(self, tmp_path, monkeypatch):
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        result = ports_mod.allocate(3)
        assert len(result) == 3

    def test_ports_unique(self, tmp_path, monkeypatch):
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        result = ports_mod.allocate(5)
        assert len(set(result)) == 5

    def test_ports_in_valid_range(self, tmp_path, monkeypatch):
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        result = ports_mod.allocate(4)
        for p in result:
            assert ports_mod._BASE_PORT <= p <= ports_mod._MAX_PORT

    def test_single_port(self, tmp_path, monkeypatch):
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        result = ports_mod.allocate(1)
        assert len(result) == 1
        assert result[0] == ports_mod._BASE_PORT


# ---------------------------------------------------------------------------
# Skips occupied ports
# ---------------------------------------------------------------------------


class TestAllocateSkipsOccupied:
    def test_skips_port_in_state_file(self, tmp_path, monkeypatch):
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        base = ports_mod._BASE_PORT
        _write_fake_state(tmp_path, "aaa111", [base])
        result = ports_mod.allocate(1)
        assert base not in result

    def test_skips_multiple_occupied(self, tmp_path, monkeypatch):
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        base = ports_mod._BASE_PORT
        occupied = [base, base + 1, base + 2]
        _write_fake_state(tmp_path, "bbb222", occupied)
        result = ports_mod.allocate(2)
        for p in occupied:
            assert p not in result

    def test_gracefully_skips_corrupt_state(self, tmp_path, monkeypatch):
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        (tmp_path / "corrupt.json").write_text("not json {{{")
        # Should not raise — corrupt files are ignored
        result = ports_mod.allocate(1)
        assert len(result) == 1


# ---------------------------------------------------------------------------
# Exhaustion
# ---------------------------------------------------------------------------


class TestAllocateExhaustion:
    def test_too_many_raises(self, tmp_path, monkeypatch):
        monkeypatch.setattr(ports_mod, "_CHAINS_DIR", tmp_path)
        capacity = ports_mod._MAX_PORT - ports_mod._BASE_PORT + 1
        with pytest.raises(RuntimeError, match="No free ports"):
            ports_mod.allocate(capacity + 1)
