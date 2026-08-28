"""The per-symbol half of the docstring ratchet, driven over seeded flags.

`scripts/check_docstring_coverage.py` used to ratchet only a per-module count
of incomplete surfaces. That container cannot represent the failure it is meant
to catch: the interesting event is a **pair that cancels**.

Measured 2026-08-28 adopting just-makeit 0.70.0. `doppler.track`'s incomplete
count read 1 -> 1 while `BpskReceiver` gained a description and
`MpskReceiverR` lost one, so the module was silent over a real loss;
`doppler.wfm` tripped in the same run only because nothing there happened to
improve at the same time. The regression was found by a hand-written
before/after diff, which is not a gate.

The cancelling pair is the case that needs proof, so it is the first test here.
A gate proven only against the tree it ships with is a gate nobody has watched
fail.
"""

from __future__ import annotations

import importlib.util

from doppler.tests._repo import repo_root

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_docstring_coverage.py"
BASELINE = REPO / "docs" / ".docstring-coverage-baseline"


def _gate():
    """Import the gate as a module — the real code path `--check` runs."""
    spec = importlib.util.spec_from_file_location("_dcov", SCRIPT)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def test_cancelling_pair_is_caught() -> None:
    """The 0.70.0 shape: one symbol loses, another gains, count unchanged.

    This is the whole reason the per-symbol rule exists. Both maps have
    exactly one covered symbol, so any count-based rule sees no change.
    """
    gate = _gate()
    base = {"m.Lost": "s", "m.Gained": ""}
    now = {"m.Lost": "", "m.Gained": "s"}

    assert sum(bool(v) for v in base.values()) == sum(
        bool(v) for v in now.values()
    ), "the fixture must be count-neutral, or it proves nothing"

    losses = gate.symbol_losses(base, now, have_runtime=False)
    assert len(losses) == 1
    assert "m.Lost" in losses[0]
    assert "LOST" in losses[0]
    assert "m.Gained" not in " ".join(losses)


def test_gaining_a_face_is_not_a_loss() -> None:
    gate = _gate()
    assert gate.symbol_losses({"m.A": ""}, {"m.A": "s"}, False) == []
    assert gate.symbol_losses({"m.A": "s"}, {"m.A": "sr"}, True) == []


def test_removed_symbol_is_not_a_loss() -> None:
    """A rename or deletion is the module count's business, not this rule.

    Absence is unambiguous because `face_flags` emits every symbol it scored,
    including ones covered on no face — so a symbol that merely lost its
    docstring is still present with an empty flag string, and is caught above.
    """
    gate = _gate()
    assert gate.symbol_losses({"m.Gone": "sr"}, {}, True) == []


def test_runtime_loss_is_ignored_when_not_built() -> None:
    """Unscored is not failed — a missing build must not fail the runtime face.

    Without this, running the gate in a tree with no compiled extension would
    report every runtime-covered symbol as regressed.
    """
    gate = _gate()
    base, now = {"m.A": "sr"}, {"m.A": "s"}
    assert gate.symbol_losses(base, now, have_runtime=False) == []
    caught = gate.symbol_losses(base, now, have_runtime=True)
    assert len(caught) == 1 and "runtime" in caught[0]


def test_stub_loss_is_caught_even_while_runtime_holds() -> None:
    gate = _gate()
    losses = gate.symbol_losses({"m.A": "sr"}, {"m.A": "r"}, True)
    assert len(losses) == 1 and "stub" in losses[0]


def test_baseline_round_trips_flags(tmp_path) -> None:
    """write_baseline -> read_baseline preserves the per-symbol flags.

    A writer and reader that disagree would leave the rule quietly inert,
    which is the failure mode the committed-baseline test below guards.
    """
    gate = _gate()
    gate.BASELINE_FILE = str(tmp_path / "baseline")
    rows = {
        "m": {
            "stub_incomplete": 3,
            "runtime_incomplete": 4,
        }
    }
    flags = {"m.A": "sr", "m.B": "s", "m.C": ""}
    gate.write_baseline(rows, True, 0, flags)

    mods, got = gate.read_baseline()
    assert mods["m"] == {"stub": 3, "runtime": 4}
    # Only covered symbols are written; the uncovered one has nothing to lose.
    assert got == {"m.A": "sr", "m.B": "s"}


def test_face_flags_emits_every_symbol_including_uncovered() -> None:
    """The invariant that makes "absent means removed" safe to act on.

    `symbol_losses` skips a symbol missing from the current map, reading it as
    renamed or deleted. That is only sound because `face_flags` emits EVERY
    symbol it scored, so one that merely lost its last docstring is still
    present carrying an empty flag string. Were `face_flags` to omit the
    uncovered — the obvious way to write it, since only covered symbols reach
    the baseline file — a symbol losing both faces would vanish and be read as
    removed, silently swallowing exactly the regression this gate exists for.

    Scored on the stub face alone so this runs in an unbuilt tree.
    """
    gate = _gate()
    mods = gate.parse_stub_face()
    ignore = gate.load_ignore()
    flags = gate.face_flags(mods, False, ignore)

    scored = {s.qual for syms in mods.values() for s in syms} - ignore
    assert flags.keys() == scored, "face_flags dropped symbols it scored"
    assert any(v == "" for v in flags.values()), (
        "no uncovered symbol present — the invariant is untested by this tree"
    )


def test_committed_baseline_actually_arms_the_rule() -> None:
    """The shipped baseline must carry a `[symbols]` section.

    Without one the per-symbol rule compares against nothing and reports no
    findings because it never looks. `--check` refuses outright in that state;
    this asserts the refusal is not what CI is quietly living with.
    """
    text = BASELINE.read_text(encoding="utf-8")
    assert "[symbols]" in text, "baseline lost its [symbols] section"

    body = text.split("[symbols]", 1)[1]
    entries = [
        ln
        for ln in body.splitlines()
        if ln.strip() and not ln.lstrip().startswith("#")
    ]
    assert len(entries) > 1000, f"only {len(entries)} symbols recorded"
    # The two classes jm 0.70.0 regressed must be represented, or this gate
    # would not have caught the event it was built for.
    assert any(ln.startswith("doppler.track.MpskReceiverR ") for ln in entries)
    assert any(ln.startswith("doppler.wfm.FrameDesc ") for ln in entries)
