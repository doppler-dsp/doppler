"""The -Wall -Wextra gate, exercised over a seeded tree and seeded logs.

`scripts/check_warnings.py` exists because this tree compiled with no warning
flag at all (doppler#1658): 0 of 691 compile lines carried `-Wall`, so an
unused result, a sign compare or an unhandled enum was invisible to every gate
we had. Its own failure modes are the ones that make such a gate decorative,
so the cases below are about those rather than about any one warning:

- a build made WITHOUT the flags finds no warnings and would pass;
- a stale or empty log finds none either;
- one compiler passes what the other rejects;
- an exemption list that can be appended to is an allowlist;
- a list entry that outlives the file's warnings is a waiver with no reason.

Each case seeds a fake repo and fake compiler logs, because a gate that can
only be tested against the real tree cannot be sabotaged -- you would have to
break doppler to check it, and nobody does that twice. (The real tree was
sabotaged once, by hand, in a copy: see the PR that introduced the gate.)
"""

from __future__ import annotations

import json
import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_warnings.py"

OWN = "native/src/x/x_core.c"
FRAGMENT = "native/src/x/x_ext_obj.c"
VENDOR = "vendor/pocketfft/pocketfft_c99.c"
FLAG = "-Wunused-parameter"


def _touch(root: Path, *rels: str) -> None:
    for rel in rels:
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("/* seeded */\n", encoding="utf-8")


def _build(
    root: Path,
    name: str,
    warnings: list[tuple[str, int, str]] | None = None,
    *,
    flags: str = "-Wall -Wextra",
    units: tuple[str, ...] = (OWN,),
    compiled: int | None = None,
    extra_lines: tuple[str, ...] = (),
) -> tuple[Path, Path]:
    """One compiler's seeded build: a log and a compilation database.

    `units` are the in-tree translation units; `compiled` is how many of them
    the LOG shows being built (default: all), so a test can seed the stale-log
    case; `flags` is what the database says each was compiled with.
    """
    _touch(root, *units)
    build = root / f"build-warnings-{name}"
    build.mkdir(exist_ok=True)
    (build / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(build),
                    "file": str(root / u),
                    "command": f"{name} {flags} -c {root / u}",
                }
                for u in units
            ]
        )
    )
    n = len(units) if compiled is None else compiled
    lines = [f"[ 50%] Building C object {i}.o" for i in range(n)]
    for rel, line, flag in warnings or []:
        path = rel if rel.startswith("/") else str(root / rel)
        lines.append(f"{path}:{line}:5: warning: seeded [{flag}]")
    lines += extra_lines
    log = root / f"build-warnings-{name}.log"
    log.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return log, build


def _run(root: Path, exempt: str, *builds: tuple[str, Path, Path]):
    """Run the gate over `builds` -- (compiler name, log, build dir)."""
    lst = root / "scripts" / ".warnings-exempt"
    lst.parent.mkdir(parents=True, exist_ok=True)
    lst.write_text(exempt, encoding="utf-8")
    args: list[str] = []
    for name, log, build in builds:
        args += ["--compiler", name, str(log), str(build)]
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--root",
            str(root),
            "--exempt",
            str(lst),
            *args,
        ],
        capture_output=True,
        text=True,
    )


def _both(root: Path, warnings: list[tuple[str, int, str]], **kw):
    """The same warnings seen by gcc and by clang."""
    g = _build(root, "gcc", warnings, **kw)
    c = _build(root, "clang", warnings, **kw)
    return ("gcc", *g), ("clang", *c)


def test_a_clean_build_passes(tmp_path: Path) -> None:
    """The state the gate steers towards: nothing warns anywhere."""
    r = _run(tmp_path, "", *_both(tmp_path, []))
    assert r.returncode == 0, r.stdout
    assert "0 own" in r.stdout


def test_a_warning_in_own_code_fails(tmp_path: Path) -> None:
    """The defect the gate exists for, in a file nothing has exempted."""
    r = _run(tmp_path, "", *_both(tmp_path, [(OWN, 12, FLAG)]))
    assert r.returncode == 1
    # the message names the line and the flag, or the gate reports a problem
    # the reader then has to go and find
    assert f"{OWN}:12" in r.stdout
    assert FLAG in r.stdout
    assert "Do NOT add the file" in r.stdout


def test_one_compiler_is_enough_to_fail(tmp_path: Path) -> None:
    """gcc passes what clang rejects -- the reason there are two builds.

    The gcc-only findings in this tree were all -Wmaybe-uninitialized and the
    clang-only ones -Wmissing-field-initializers: a gate over either alone
    would have been green on the other's defects.
    """
    clang_only = [(OWN, 7, "-Wmissing-field-initializers")]
    gcc = _build(tmp_path, "gcc", [])
    clang = _build(tmp_path, "clang", clang_only)
    r = _run(tmp_path, "", ("gcc", *gcc), ("clang", *clang))
    assert r.returncode == 1
    assert "-Wmissing-field-initializers" in r.stdout
    assert "(clang)" in r.stdout


def test_a_warning_in_an_exempt_file_is_tolerated(tmp_path: Path) -> None:
    """The fragments warn until they migrate, and that is the carve-out."""
    r = _run(
        tmp_path,
        f"{FRAGMENT}\n",
        *_both(tmp_path, [(FRAGMENT, 40, FLAG)], units=(OWN, FRAGMENT)),
    )
    assert r.returncode == 0, r.stdout
    assert "1 in exempt files" in r.stdout


def test_exempting_one_file_does_not_exempt_its_neighbour(
    tmp_path: Path,
) -> None:
    """An exemption is per FILE: the fragment's sibling is still judged."""
    other = "native/src/x/x_ext_other.c"
    r = _run(
        tmp_path,
        f"{FRAGMENT}\n",
        *_both(
            tmp_path,
            [(FRAGMENT, 1, FLAG), (other, 2, FLAG)],
            units=(OWN, FRAGMENT, other),
        ),
    )
    assert r.returncode == 1
    assert f"{other}:2" in r.stdout
    assert f"{FRAGMENT}:1" not in r.stdout


def test_vendored_code_is_counted_not_judged(tmp_path: Path) -> None:
    """vendor/ is upstream's, kept pristine; its warnings are not ours."""
    _touch(tmp_path, VENDOR)
    r = _run(tmp_path, "", *_both(tmp_path, [(VENDOR, 9, "-Wc11-extensions")]))
    assert r.returncode == 0, r.stdout
    assert "1 vendored" in r.stdout


def test_a_warning_raised_in_a_foreign_header_is_ours(tmp_path: Path) -> None:
    """A Python or system header the compiler flags is judged like our code.

    It can never match vendor/ or the exempt list, so it fails -- a warning
    raised from a header BY our code is ours to fix.
    """
    r = _run(
        tmp_path,
        "",
        *_both(tmp_path, [("/usr/include/python3/x.h", 3, "-Wcpp")]),
    )
    assert r.returncode == 1
    assert "/usr/include/python3/x.h:3" in r.stdout


def test_an_exempt_entry_whose_file_is_gone_fails(tmp_path: Path) -> None:
    """A rename or deletion must take the entry with it."""
    r = _run(tmp_path, "native/src/x/gone.c\n", *_both(tmp_path, []))
    assert r.returncode == 1
    assert "file is gone" in r.stdout or "whose file is gone" in r.stdout
    assert "native/src/x/gone.c" in r.stdout


def test_the_list_may_not_go_slack(tmp_path: Path) -> None:
    """A listed file that warns under NEITHER compiler is a stale waiver.

    This is the half of a ratchet that shipped broken elsewhere in this repo:
    without it the entry outlives the migration that made it unnecessary, and
    the next warning added to that file would pass.
    """
    r = _run(
        tmp_path,
        f"{FRAGMENT}\n",
        *_both(tmp_path, [], units=(OWN, FRAGMENT)),
    )
    assert r.returncode == 1
    assert "gone slack" in r.stdout
    assert FRAGMENT in r.stdout


def test_slack_is_judged_over_both_compilers(tmp_path: Path) -> None:
    """Clean under gcc, dirty under clang: still needed, so not slack.

    Judged per compiler it would call a clang-only file stale and push the
    entry off the list while clang still warns -- so with one compiler the
    gate says it did not judge, rather than guess.
    """
    gcc = _build(tmp_path, "gcc", [], units=(OWN, FRAGMENT))
    clang = _build(
        tmp_path,
        "clang",
        [(FRAGMENT, 4, "-Wmissing-field-initializers")],
        units=(OWN, FRAGMENT),
    )
    both = _run(tmp_path, f"{FRAGMENT}\n", ("gcc", *gcc), ("clang", *clang))
    assert both.returncode == 0, both.stdout
    one = _run(tmp_path, f"{FRAGMENT}\n", ("gcc", *gcc))
    assert one.returncode == 0, one.stdout
    assert "judged only over both" in one.stdout


def test_a_build_without_the_flags_is_not_a_pass(tmp_path: Path) -> None:
    """No flags, no warnings, and no evidence of anything -- the vacuous pass.

    This is exactly how the tree looked before the gate: it compiled, and it
    said nothing, because nobody had asked it to.
    """
    r = _run(
        tmp_path,
        "",
        *_both(tmp_path, [], flags="-O2"),
    )
    assert r.returncode == 1
    assert "WITHOUT -Wall -Wextra" in r.stdout


def test_one_missing_flag_is_enough(tmp_path: Path) -> None:
    """-Wall alone is not the contract: -Wextra carries unused-parameter."""
    r = _run(tmp_path, "", *_both(tmp_path, [], flags="-Wall"))
    assert r.returncode == 1
    assert "WITHOUT" in r.stdout


def test_a_log_that_shows_too_few_compiles_is_not_a_pass(
    tmp_path: Path,
) -> None:
    """A stale or partial log measured nothing about its paired tree."""
    units = (OWN, "native/src/y/y_core.c", "native/src/z/z_core.c")
    r = _run(tmp_path, "", *_both(tmp_path, [], units=units, compiled=1))
    assert r.returncode == 1
    assert "stale or partial log" in r.stdout


def test_an_empty_database_is_not_a_pass(tmp_path: Path) -> None:
    """A configure that produced no in-tree unit has measured nothing."""
    r = _run(tmp_path, "", *_both(tmp_path, [], units=()))
    assert r.returncode == 1
    assert "nothing was measured" in r.stdout


def test_driver_warnings_are_reported_but_do_not_gate(tmp_path: Path) -> None:
    """`clang: warning: overriding '-ffast-math'` has no line to point at."""
    note = "clang: warning: overriding '-ffast-math' [-Woverriding-option]"
    g = _build(tmp_path, "gcc", [])
    c = _build(tmp_path, "clang", [], extra_lines=(note,))
    r = _run(tmp_path, "", ("gcc", *g), ("clang", *c))
    assert r.returncode == 0, r.stdout
    assert "driver warning" in r.stdout
    assert "-Woverriding-option" in r.stdout


# ---------------------------------------------------------------------------
# The exempt list's own ratchet: read from git, never from a number in a file.
# ---------------------------------------------------------------------------


def _git(root: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-c", "user.email=t@t", "-c", "user.name=t", *args],
        cwd=root,
        check=True,
        capture_output=True,
    )


def _exempt_only(root: Path, base: str = "HEAD"):
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--root",
            str(root),
            "--exempt",
            str(root / "scripts" / ".warnings-exempt"),
            "--exempt-only",
            "--base",
            base,
        ],
        capture_output=True,
        text=True,
    )


def _repo_with_list(root: Path, entries: list[str]) -> Path:
    """A git repo whose HEAD carries the exempt list and its files."""
    _touch(root, *entries)
    lst = root / "scripts" / ".warnings-exempt"
    lst.parent.mkdir(parents=True, exist_ok=True)
    lst.write_text("".join(f"{e}\n" for e in entries), encoding="utf-8")
    _git(root, "init", "-q")
    _git(root, "add", "-A")
    _git(root, "commit", "-q", "-m", "base")
    return lst


def test_the_list_may_only_shrink(tmp_path: Path) -> None:
    """The ratchet's whole point: a new warning cannot be forgiven by a line.

    Without this, `scripts/.warnings-exempt` is an allowlist, and an
    allowlist is where the next warning goes to be forgiven.
    """
    a, b = "native/src/x/x_ext_a.c", "native/src/x/x_ext_b.c"
    lst = _repo_with_list(tmp_path, [a])
    _touch(tmp_path, b)
    lst.write_text(f"{a}\n{b}\n", encoding="utf-8")
    r = _exempt_only(tmp_path)
    assert r.returncode == 1
    assert "ADDED" in r.stdout
    assert b in r.stdout


def test_removing_an_entry_is_the_intended_motion(tmp_path: Path) -> None:
    """Shrinking is what the list is for, and it must pass."""
    a, b = "native/src/x/x_ext_a.c", "native/src/x/x_ext_b.c"
    lst = _repo_with_list(tmp_path, [a, b])
    lst.write_text(f"{a}\n", encoding="utf-8")
    r = _exempt_only(tmp_path)
    assert r.returncode == 0, r.stdout


def test_an_unchanged_list_passes(tmp_path: Path) -> None:
    _repo_with_list(tmp_path, ["native/src/x/x_ext_a.c"])
    r = _exempt_only(tmp_path)
    assert r.returncode == 0, r.stdout
    assert "none added" in r.stdout


def test_a_list_new_on_this_branch_has_nothing_to_grow_from(
    tmp_path: Path,
) -> None:
    """The PR that introduces the list cannot be told it ADDED every entry."""
    _touch(tmp_path, "README")
    _git(tmp_path, "init", "-q")
    _git(tmp_path, "add", "-A")
    _git(tmp_path, "commit", "-q", "-m", "base")
    a = "native/src/x/x_ext_a.c"
    _touch(tmp_path, a)
    lst = tmp_path / "scripts" / ".warnings-exempt"
    lst.parent.mkdir(parents=True, exist_ok=True)
    lst.write_text(f"{a}\n", encoding="utf-8")
    r = _exempt_only(tmp_path)
    assert r.returncode == 0, r.stdout


def test_an_unresolvable_base_fails_closed(tmp_path: Path) -> None:
    """A shallow clone that never fetched origin/main has not been checked."""
    _repo_with_list(tmp_path, ["native/src/x/x_ext_a.c"])
    r = _exempt_only(tmp_path, base="origin/does-not-exist")
    assert r.returncode == 1
    assert "has not passed" in r.stdout


def test_a_duplicate_entry_is_refused(tmp_path: Path) -> None:
    a = "native/src/x/x_ext_a.c"
    lst = _repo_with_list(tmp_path, [a])
    lst.write_text(f"{a}\n{a}\n", encoding="utf-8")
    r = _exempt_only(tmp_path)
    assert r.returncode == 1
    assert "more than once" in r.stdout
