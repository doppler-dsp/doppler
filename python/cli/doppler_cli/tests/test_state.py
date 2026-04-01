"""
Tests for chain state persistence.

Covers:
  - ChainState.save() writes a valid JSON file
  - ChainState.load() round-trips correctly
  - ChainState.delete() removes the file
  - load() raises KeyError for missing chain
  - list_chains() returns all saved chains
  - list_chains() skips corrupt files silently
  - pid_alive() returns False for dead PIDs
  - stop_chain() calls the right signal and deletes state
"""

import json
import os
import signal

import pytest

from doppler_cli.state import (
    BlockState,
    ChainState,
    list_chains,
    pid_alive,
    stop_chain,
)


def _make_chain(chain_id: str = "abc123") -> ChainState:
    return ChainState(
        id=chain_id,
        started="2026-01-01T00:00:00+00:00",
        compose="/tmp/fake.yml",
        blocks=[
            BlockState(name="tone", pid=11111, bind_port=5600),
            BlockState(name="specan", pid=11112, connect_port=5600),
        ],
    )


# ---------------------------------------------------------------------------
# Save / load / delete
# ---------------------------------------------------------------------------


class TestPersistence:
    def test_save_creates_file(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)
        chain = _make_chain()
        chain.save()
        assert (tmp_path / "abc123.json").exists()

    def test_save_valid_json(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)
        _make_chain().save()
        data = json.loads((tmp_path / "abc123.json").read_text())
        assert data["id"] == "abc123"
        assert len(data["blocks"]) == 2

    def test_load_round_trips(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)
        original = _make_chain()
        original.save()
        loaded = ChainState.load("abc123")
        assert loaded.id == original.id
        assert loaded.started == original.started
        assert loaded.blocks[0].name == "tone"
        assert loaded.blocks[0].bind_port == 5600
        assert loaded.blocks[1].connect_port == 5600

    def test_load_missing_raises(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)
        with pytest.raises(KeyError):
            ChainState.load("doesnotexist")

    def test_delete_removes_file(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)
        chain = _make_chain()
        chain.save()
        chain.delete()
        assert not (tmp_path / "abc123.json").exists()

    def test_delete_missing_is_silent(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)
        chain = _make_chain()
        chain.delete()  # never saved — should not raise


# ---------------------------------------------------------------------------
# list_chains
# ---------------------------------------------------------------------------


class TestListChains:
    def test_empty_dir(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)
        assert list_chains() == []

    def test_missing_dir(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path / "nonexistent")
        assert list_chains() == []

    def test_returns_all_chains(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)
        _make_chain("aaa111").save()
        _make_chain("bbb222").save()
        ids = {c.id for c in list_chains()}
        assert ids == {"aaa111", "bbb222"}

    def test_skips_corrupt_files(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)
        _make_chain("good000").save()
        (tmp_path / "bad999.json").write_text("not json {{{")
        chains = list_chains()
        assert len(chains) == 1
        assert chains[0].id == "good000"


# ---------------------------------------------------------------------------
# pid_alive
# ---------------------------------------------------------------------------


class TestPidAlive:
    def test_own_pid_is_alive(self):
        assert pid_alive(os.getpid()) is True

    def test_dead_pid_returns_false(self):
        # PID 1 exists but we won't have permission to signal it,
        # which pid_alive treats as "not our process = alive" via
        # PermissionError. Use a clearly invalid large PID instead.
        assert pid_alive(999999999) is False


# ---------------------------------------------------------------------------
# stop_chain
# ---------------------------------------------------------------------------


class TestStopChain:
    def test_sigterm_sent_and_state_deleted(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)

        signals_sent: list[tuple[int, int]] = []

        def fake_kill(pid, sig):
            signals_sent.append((pid, sig))

        monkeypatch.setattr(os, "kill", fake_kill)
        monkeypatch.setattr(state_mod, "pid_alive", lambda pid: True)

        chain = _make_chain()
        chain.save()
        stop_chain(chain, kill=False)

        assert (11111, signal.SIGTERM) in signals_sent
        assert (11112, signal.SIGTERM) in signals_sent
        assert not (tmp_path / "abc123.json").exists()

    def test_sigkill_when_kill_true(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)

        signals_sent: list[tuple[int, int]] = []
        monkeypatch.setattr(
            os, "kill", lambda pid, sig: signals_sent.append((pid, sig))
        )
        monkeypatch.setattr(state_mod, "pid_alive", lambda pid: True)

        chain = _make_chain()
        chain.save()
        stop_chain(chain, kill=True)

        assert all(sig == signal.SIGKILL for _, sig in signals_sent)

    def test_dead_pids_skipped(self, tmp_path, monkeypatch):
        from doppler_cli import state as state_mod

        monkeypatch.setattr(state_mod, "_CHAINS_DIR", tmp_path)
        monkeypatch.setattr(state_mod, "pid_alive", lambda pid: False)

        signals_sent: list = []
        monkeypatch.setattr(os, "kill", lambda pid, sig: signals_sent.append(pid))

        chain = _make_chain()
        chain.save()
        stop_chain(chain)

        assert signals_sent == []
