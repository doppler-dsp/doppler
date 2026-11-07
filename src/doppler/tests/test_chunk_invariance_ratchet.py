"""The chunk-invariance rule in `check_tests_ssot.py`, over a seeded repo.

A streaming object's output must be a function of the INPUT STREAM, not of
how it was split into calls. `native/tests/dp_chunk_inv.h` states that once;
the rule makes using it the default. An object is subject to it when its
header declares `int <p>_set_state (` (it carries state) and a streaming call
(`<p>_step`, `_steps*`, `_execute*`, `_push*`), and has the test when a C test
that includes the harness and calls `dp_chunk_invariance (` also calls the
object from a function wired in as the spec's `.create` or `.process`.

Each case builds a throwaway repo, commits it, and asks the gate. A case that
can only go green proves nothing, so the ones that keep the rule from being
satisfied by accident matter most:

- a test that merely MENTIONS `dp_foo_` beside the harness does not count;
- the harness must be CALLED, not just included;
- an entry whose object now has the test must go, or the ratchet is a waiver
  outliving its reason;
- an entry ADDED since the merge base fails, which is what keeps the file a
  ratchet and not an allowlist (and it applies to `state:` too).
"""

from __future__ import annotations

import importlib.util
import subprocess
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from collections.abc import Callable
    from pathlib import Path
    from types import ModuleType

    Seed = Callable[..., ModuleType]

SCRIPT = repo_root(__file__) / "scripts" / "check_tests_ssot.py"

HEADER = """\
int dp_foo_set_state (void *s, const void *blob);
size_t dp_foo_execute (void *s, const void *in, size_t n, void *out);
"""

#: A test the rule must accept: harness included and run, with the object
#: called from the function wired in as `.create` and `.process`.
WIRED = """\
#include "dp_chunk_inv.h"
static void *foo_create (void *arg) { return dp_foo_create (arg); }
static size_t foo_process (void *o, const void *in, size_t n, void *out,
                           size_t cap)
{ return dp_foo_execute (o, in, n, out); }
int main (void)
{
  dp_ci_spec_t spec = { .create = foo_create, .process = foo_process };
  return dp_chunk_invariance (&spec, 0, 0);
}
"""


def _load(monkeypatch: pytest.MonkeyPatch) -> ModuleType:
    monkeypatch.syspath_prepend(str(SCRIPT.parent))
    spec = importlib.util.spec_from_file_location("check_tests_ssot", SCRIPT)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        [
            "git",
            "-C",
            str(root),
            "-c",
            "user.name=t",
            "-c",
            "user.email=t@t",
            "-c",
            "commit.gpgsign=false",
            *args,
        ],
        check=True,
        capture_output=True,
    )


@pytest.fixture
def seed(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Seed:
    """Build a repo, commit it, and return the gate pointed at it."""

    def build(
        tests: dict[str, str] | None = None,
        header: str = HEADER,
        ratchet: str = "",
    ) -> ModuleType:
        root = tmp_path / "repo"
        (root / "native" / "inc" / "doppler" / "foo").mkdir(parents=True)
        (root / "native" / "tests").mkdir(parents=True)
        (root / "scripts").mkdir()
        (root / "native/inc/doppler/foo/foo_core.h").write_text(header)
        for name, body in (tests or {}).items():
            (root / "native" / "tests" / name).write_text(body)
        (root / "scripts/.chunk-invariance-ratchet").write_text(ratchet)
        (root / "scripts/.state-roundtrip-ratchet").write_text("")
        subprocess.run(["git", "init", "-q", str(root)], check=True)
        _git(root, "add", "-A")
        _git(root, "commit", "-qm", "base")
        mod = _load(monkeypatch)
        monkeypatch.setattr(mod, "ROOT", root)
        monkeypatch.setattr(mod, "TESTS", root / "native" / "tests")
        monkeypatch.setattr(mod, "INC", root / "native" / "inc")
        monkeypatch.setattr(
            mod, "CHUNK_RATCHET", root / "scripts/.chunk-invariance-ratchet"
        )
        monkeypatch.setattr(
            mod, "STATE_RATCHET", root / "scripts/.state-roundtrip-ratchet"
        )
        return mod

    return build


def test_a_wired_test_covers_the_object(seed: Seed) -> None:
    mod = seed({"test_foo_core.c": WIRED})
    assert mod.streaming() == {"dp_foo"}
    assert mod.chunk_tested() == {"dp_foo"}
    assert mod.chunk_invariance() == []


def test_an_untested_streaming_object_fails_and_is_named(seed: Seed) -> None:
    bad = seed().chunk_invariance()
    assert len(bad) == 1
    assert bad[0].startswith("dp_foo:")
    assert "dp_chunk_inv.h" in bad[0]


def test_a_ratchet_entry_forgives_it(seed: Seed) -> None:
    mod = seed(ratchet="dp_foo | not yet\n")
    assert mod.chunk_invariance() == []


def test_a_stateless_object_owes_nothing(seed: Seed) -> None:
    mod = seed(header="size_t dp_foo_execute (void *s);\n")
    assert mod.streaming() == set()


def test_a_stateful_object_that_does_not_stream_owes_nothing(
    seed: Seed,
) -> None:
    mod = seed(header="int dp_foo_set_state (void *s, const void *b);\n")
    assert mod.streaming() == set()


def test_a_private_header_declares_no_object(seed: Seed) -> None:
    # An object's internals (the correlator's search halves, #2752 3.4b)
    # are not a caller's triplet: the same declaration in a public header
    # makes an object, and in a `*_private.h` beside it, none.
    mod = seed(header="int dp_foo_set_state (void *s, const void *b);\n")
    (mod.INC / "doppler/foo/foo_private.h").write_text(
        "int dp__bar_set_state (void *s, const void *b);\n"
    )
    assert mod.serializable() == {"dp_foo"}


def test_naming_the_object_without_the_harness_does_not_count(
    seed: Seed,
) -> None:
    body = "static void *c (void) { return dp_foo_create (0); }\n"
    mod = seed({"test_foo_core.c": body})
    assert mod.chunk_tested() == set()
    assert len(mod.chunk_invariance()) == 1


def test_including_the_harness_without_calling_it_does_not_count(
    seed: Seed,
) -> None:
    body = '#include "dp_chunk_inv.h"\n' + WIRED.split("\n", 1)[1].replace(
        "dp_chunk_invariance (&spec, 0, 0)", "0"
    )
    assert seed({"test_foo_core.c": body}).chunk_tested() == set()


def test_a_helper_mention_beside_the_harness_does_not_count(
    seed: Seed,
) -> None:
    """The false positive the call-site rule exists for.

    The harness drives `bar`; `dp_foo_` is called by a helper nothing wires
    into the spec, so `foo` is never partitioned.
    """
    body = (
        '#include "dp_chunk_inv.h"\n'
        "static void helper (void) { dp_foo_execute (0, 0, 0, 0); }\n"
        "static void *bar_create (void *a) { return a; }\n"
        "static size_t bar_process (void *o, const void *i, size_t n, void *u,"
        " size_t c) { return n; }\n"
        "int main (void)\n"
        "{\n"
        "  dp_ci_spec_t spec = { .create = bar_create,"
        " .process = bar_process };\n"
        "  helper ();\n"
        "  return dp_chunk_invariance (&spec, 0, 0);\n"
        "}\n"
    )
    mod = seed({"test_foo_core.c": body})
    assert mod.chunk_tested() == set()
    assert len(mod.chunk_invariance()) == 1


def test_a_cast_in_the_wiring_is_followed(seed: Seed) -> None:
    """`.destroy = (void (*) (void *))dp_x_destroy` style casts are common."""
    body = WIRED.replace(
        ".create = foo_create", ".create = (void *(*) (void *))foo_create"
    )
    assert seed({"test_foo_core.c": body}).chunk_tested() == {"dp_foo"}


def test_an_entry_whose_object_has_the_test_now_must_go(seed: Seed) -> None:
    mod = seed({"test_foo_core.c": WIRED}, ratchet="dp_foo | stale\n")
    bad = mod.chunk_invariance()
    assert len(bad) == 1
    assert "has the test now" in bad[0]


def test_an_entry_for_a_gone_object_must_go(seed: Seed) -> None:
    mod = seed({"test_foo_core.c": WIRED}, ratchet="dp_ghost | stale\n")
    assert any("dp_ghost" in b for b in mod.chunk_invariance())


@pytest.mark.parametrize("which", ["CHUNK_RATCHET", "STATE_RATCHET"])
def test_an_entry_added_since_the_base_fails_on_either_ratchet(
    seed: Seed, which: str
) -> None:
    """The stricter half now guards `state:` too."""
    mod = seed()
    path = getattr(mod, which)
    path.write_text("dp_foo | forgive me\n")
    bad = mod.ratchet_added(path, "HEAD")
    assert len(bad) == 1
    assert "ADDED" in bad[0]
    # unchanged since the base is fine
    path.write_text("")
    assert mod.ratchet_added(path, "HEAD") == []


#: The merge base's list for the rename cases: two entries that a rename
#: merges into one.
RENAME_BASE = "dp_a | no test yet\ndp_b | no test yet\n"


def _renamed(seed: Seed, head: str) -> list[str]:
    """`ratchet_added` with RENAME_BASE committed and `head` on disk."""
    mod = seed(ratchet=RENAME_BASE)
    mod.CHUNK_RATCHET.write_text(head)
    return mod.ratchet_added(mod.CHUNK_RATCHET, "HEAD")


def test_a_rename_carries_its_lines(seed: Seed) -> None:
    """Two entries merged into one, each named and each gone: a rename.
    The fixture header declares only `dp_foo`, so neither replaced key is a
    live object (condition 4 holds)."""
    assert _renamed(seed, "dp_ab | merged | replaces: dp_a dp_b\n") == []


def test_a_rename_of_one_entry_carries_it(seed: Seed) -> None:
    head = "dp_a | no test yet\ndp_c | renamed | replaces: dp_b\n"
    assert _renamed(seed, head) == []


# ── the carry rule, once (carry_refusals) ───────────────────────────────────
#
# Both lists call this; each condition is tested here and only here, so a
# sabotage of one condition turns exactly its test red. The lists' own
# tests below cover their readers and their liveness sources.


def _never(key: str) -> bool:
    return False


def test_carry_a_rename_holds(monkeypatch: pytest.MonkeyPatch) -> None:
    mod = _load(monkeypatch)
    got = mod.carry_refusals({"a", "b"}, {"ab"}, {"ab": ("a", "b")}, _never)
    assert got == {"ab": []}


def test_carry_condition_1_a_key_the_base_never_held(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A carry needs something to carry."""
    mod = _load(monkeypatch)
    got = mod.carry_refusals({"a"}, {"c"}, {"c": ("ghost",)}, _never)
    assert len(got["c"]) == 1
    assert "not on the base's list" in got["c"][0]


def test_carry_condition_2_a_key_still_listed(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The old line must go, or the list grew by one."""
    mod = _load(monkeypatch)
    got = mod.carry_refusals({"a"}, {"a", "c"}, {"c": ("a",)}, _never)
    assert len(got["c"]) == 1
    assert "still listed at head" in got["c"][0]


def test_carry_condition_3_one_key_backing_two_lines(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """One old entry split into two would grow the list."""
    mod = _load(monkeypatch)
    got = mod.carry_refusals(
        {"a"}, {"c", "d"}, {"c": ("a",), "d": ("a",)}, _never
    )
    assert all(len(w) == 1 for w in got.values())
    assert all("more than one new line" in w[0] for w in got.values())


def test_carry_condition_4_a_key_still_live(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A fixed entry is deleted, not traded for a new one."""
    mod = _load(monkeypatch)
    got = mod.carry_refusals({"a"}, {"c"}, {"c": ("a",)}, {"a"}.__contains__)
    assert len(got["c"]) == 1
    assert "still live at head" in got["c"][0]


def test_carry_a_key_that_replaces_nothing_is_growth(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    mod = _load(monkeypatch)
    got = mod.carry_refusals({"a"}, {"a", "n"}, {"n": ()}, _never)
    assert got == {"n": ["the list may only shrink"]}


def test_trading_a_fixed_entry_for_a_new_object_is_added(
    seed: Seed,
) -> None:
    """The ratchets' liveness source: `dp_a` got its test, so its line
    must go -- not be handed to a new object. Its `_set_state` is still
    declared at head, so serializable() calls it live and the carry is
    refused (condition 4)."""
    mod = seed(
        header=HEADER + "int dp_a_set_state (void *s, const void *b);\n",
        ratchet=RENAME_BASE,
    )
    assert "dp_a" in mod.serializable()  # the premise, not an `if`
    mod.CHUNK_RATCHET.write_text(
        "dp_b | no test yet\ndp_new | x | replaces: dp_a\n"
    )
    bad = mod.ratchet_added(mod.CHUNK_RATCHET, "HEAD")
    assert len(bad) == 1
    assert "'dp_new' ADDED" in bad[0]
    assert "still live at head" in bad[0]


def test_a_new_line_with_no_replaces_is_still_added(seed: Seed) -> None:
    bad = _renamed(seed, "dp_ab | merged\n")
    assert len(bad) == 1
    assert "may only shrink" in bad[0]


def test_the_replaces_field_is_parsed_once_in_the_one_reader(
    seed: Seed,
) -> None:
    """A `|` inside a reason stays in the reason; only a last field that opens
    with `replaces:` is the carry."""
    mod = seed()
    held = mod.parse_ratchet(
        "dp_x | a | b\ndp_y | r | replaces: dp_p dp_q\ndp_z | r | replaces:\n"
    )
    assert held["dp_x"] == ("a | b", ())
    assert held["dp_y"] == ("r", ("dp_p", "dp_q"))
    assert held["dp_z"] == ("r", ())


def _mutator_list(
    mod: ModuleType, monkeypatch: pytest.MonkeyPatch, text: str
) -> Path:
    """Commit the mutator-state list and its gate, so the base holds both."""
    root = mod.ROOT
    lst = root / "scripts" / ".mutator-state-exempt"
    gate = root / mod.MUTATOR_GATE
    gate.parent.mkdir(parents=True)
    gate.write_text("")
    lst.write_text(text)
    _git(root, "add", "-A")
    _git(root, "commit", "-qm", "the mutator list")
    monkeypatch.setattr(mod, "MUTATOR_LIST", lst)
    return lst


def test_a_mutator_entry_added_since_the_base_fails(
    seed: Seed, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The mutator-state list only shrinks (#2085 review): a PR that breaks
    a mutator cannot forgive it with a new line."""
    mod = seed()
    lst = _mutator_list(mod, monkeypatch, "A.b  LOST  held at the base\n")
    assert mod.mutator_list_added("HEAD") == []
    lst.write_text("A.b  LOST  held at the base\nA.c  LOST  forgive me\n")
    bad = mod.mutator_list_added("HEAD")
    assert len(bad) == 1
    assert "'A.c' ADDED" in bad[0]


def test_a_move_into_a_failing_verdict_is_growth(
    seed: Seed, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Relabelling an entry UNPROBED -> LOST after a regression forgives a
    new loss by an edit: the ratchet is on (key, verdict)."""
    mod = seed()
    lst = _mutator_list(mod, monkeypatch, "A.b  UNPROBED  held at the base\n")
    lst.write_text("A.b  LOST  a regression, relabelled\n")
    bad = mod.mutator_list_added("HEAD")
    assert len(bad) == 1
    assert "'A.b' moved UNPROBED -> LOST" in bad[0]


@pytest.mark.parametrize(
    ("before", "after"),
    [
        ("NO_RECIPE", "LOST"),  # a CASES row lets the probe run at last
        ("C_ONLY", "CRASHES"),  # a binding lets it run at all
        ("LOST", "UNPROBED"),  # out of a failing verdict
        ("UNPROBED", "C_ONLY"),
    ],
)
def test_a_move_that_is_not_into_a_failing_verdict_is_free(
    seed: Seed, monkeypatch: pytest.MonkeyPatch, before: str, after: str
) -> None:
    mod = seed()
    lst = _mutator_list(mod, monkeypatch, f"A.b  {before}  at the base\n")
    lst.write_text(f"A.b  {after}  now\n")
    assert mod.mutator_list_added("HEAD") == []


def test_a_mutator_list_missing_at_head_is_an_error(
    seed: Seed, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Moving the list and updating only one reader would otherwise read
    as an empty list that has not grown."""
    mod = seed()
    lst = _mutator_list(mod, monkeypatch, "A.b  LOST  held at the base\n")
    lst.unlink()
    with pytest.raises(LookupError, match="missing at HEAD"):
        mod.mutator_list_added("HEAD")


#: A header declaring the C function behind the mutator row `A.dp_old`.
OLD_FN = "int dp_old (void *s, const void *b);\n"


def test_a_mutator_rename_carries_its_row(
    seed: Seed, monkeypatch: pytest.MonkeyPatch
) -> None:
    """`A.dp_old` becomes `A.dp_new`, same verdict, and `dp_old` is no
    longer declared: a rename (#2752 step 3.2's Corr2D.set_ref)."""
    mod = seed()
    lst = _mutator_list(mod, monkeypatch, "A.dp_old  C_ONLY  why\n")
    lst.write_text("A.dp_new  C_ONLY  why; replaces: A.dp_old\n")
    assert mod.mutator_list_added("HEAD") == []


def test_a_mutator_row_whose_function_is_declared_is_live(
    seed: Seed, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The mutator list's liveness source: `dp_old` is still declared in a
    header, so declared_functions() calls the row live and the carry is
    refused (condition 4)."""
    mod = seed(header=HEADER + OLD_FN)
    assert "dp_old" in mod.declared_functions()  # the premise
    lst = _mutator_list(mod, monkeypatch, "A.dp_old  C_ONLY  why\n")
    lst.write_text("A.dp_new  C_ONLY  why; replaces: A.dp_old\n")
    bad = mod.mutator_list_added("HEAD")
    assert len(bad) == 1
    assert "'A.dp_new' ADDED" in bad[0]
    assert "still live at head" in bad[0]


def test_a_mutator_rename_keeps_its_verdict(
    seed: Seed, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The list's own fifth condition: a carry cannot move a row into
    another verdict, or a regression would be laundered by a rename."""
    mod = seed()
    lst = _mutator_list(mod, monkeypatch, "A.dp_old  UNPROBED  why\n")
    lst.write_text("A.dp_new  LOST  why; replaces: A.dp_old\n")
    bad = mod.mutator_list_added("HEAD")
    assert len(bad) == 1
    assert "UNPROBED -> LOST" in bad[0]
    assert "a rename keeps the verdict" in bad[0]


def test_the_change_that_brings_the_mutator_gate_brings_its_list(
    seed: Seed, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Gate and list both absent at the base: nothing has grown yet."""
    mod = seed()
    lst = mod.ROOT / "scripts" / ".mutator-state-exempt"
    lst.write_text("A.b  LOST  the first list\n")
    monkeypatch.setattr(mod, "MUTATOR_LIST", lst)
    assert mod.mutator_list_added("HEAD") == []


def test_a_new_ratchet_file_has_no_earlier_list_to_have_grown_from(
    seed: Seed,
) -> None:
    mod = seed()
    new = mod.ROOT / "scripts" / ".brand-new"
    new.write_text("dp_foo | x\n")
    assert mod.ratchet_added(new, "HEAD") == []


def test_an_unreadable_base_fails_closed(seed: Seed) -> None:
    mod = seed()
    with pytest.raises(LookupError):
        mod.ratchet_added(mod.CHUNK_RATCHET, "no-such-ref")


#: Two objects whose names nest: `dp_hbdecim_q15_` begins with `dp_hbdecim_`.
NESTED_HEADER = """\
int dp_hbdecim_set_state (void *s, const void *blob);
size_t dp_hbdecim_execute (void *s, const void *in, size_t n, void *out);
int dp_hbdecim_q15_set_state (void *s, const void *blob);
size_t dp_hbdecim_q15_execute (void *s, const void *in, size_t n, void *out);
"""


def _wired_to(symbol: str) -> str:
    return WIRED.replace("dp_foo_create", symbol + "_create").replace(
        "dp_foo_execute", symbol + "_execute"
    )


def test_a_nested_name_is_credited_to_its_own_object_only(seed: Seed) -> None:
    """A test wiring only `dp_hbdecim_q15_*` covers `dp_hbdecim_q15` and
    NOT `dp_hbdecim`, whose name it begins with. Credited to both, the gate
    would tell the maintainer `dp_hbdecim` "has the test now, delete the
    line", and the ratchet would then keep it off the list for good without
    that object ever being partitioned."""
    mod = seed(
        {"test_q15_core.c": _wired_to("dp_hbdecim_q15")},
        header=NESTED_HEADER,
        ratchet="dp_hbdecim | no test yet\n",
    )
    assert mod.chunk_tested() == {"dp_hbdecim_q15"}
    # The entry for `dp_hbdecim` is still earned: nothing tests it.
    assert mod.chunk_invariance() == []


def test_the_shorter_name_is_still_credited_when_it_is_wired(
    seed: Seed,
) -> None:
    mod = seed(
        {"test_hbdecim_core.c": _wired_to("dp_hbdecim")},
        header=NESTED_HEADER,
    )
    assert mod.chunk_tested() == {"dp_hbdecim"}
