#!/usr/bin/env python3
"""Every characterization subject must be runnable and must have a fast twin.

A **characterization** (``src/doppler/<mod>/tests/characterization/<subject>/
characterize.py``) sweeps an object across its whole operating envelope. It
takes minutes, so `make characterize` runs it deliberately and **nothing runs
it per push**. That is a deliberate trade, and it has an obvious failure mode:
a sweep nobody runs can rot for months, and the rot is invisible because no
gate goes red.

This is the floor under that trade. It checks the two things that make the
category honest, and it costs nothing:

1. **The subject is runnable as a script** — it has an ``if __name__ ==
   "__main__":`` block, which is exactly what `make characterize` invokes. A
   subject without one is silently a no-op: the target "passes" having swept
   nothing.
2. **The subject has a fast twin** — some ``test_*.py`` under the module's
   ``tests/`` imports it. The twin is what actually runs per push, so it is
   the only thing keeping the sweep's helpers, geometry and scene builders
   working between deliberate runs.

It also checks the subject is **importable as a package** (``__init__.py`` all
the way down), because the twin imports it by dotted path and a missing
``__init__.py`` turns that into a collection error rather than a clear failure.

And it checks that **a subject's artifacts start at the subject**, not at the
working directory. Every script in a subject's folder that names a file it
writes (``"x.png"``, ``f"pool-{s}.png"``, ``"run.json"``) must anchor the name:
inside :func:`doppler.util.fromto`, which starts at the calling script, or on
the right of a ``/`` whose left names the directory. A bare name is relative
to wherever the run was started, so the same sweep wrote its figure into the
repo root under `make characterize` and beside itself when run from its
folder. Two subjects shipped that way (doppler#2406).

**Be clear about what this does NOT check.** It does not run the sweep and it
does not verify any envelope. The artifact rule reads literals, so a path
built without one (a name in a variable, ``Path("out") / name``) is not
seen; and a literal on the right of a ``/`` counts as anchored whatever the
left is, so ``Path("out") / "x.png"``, relative to the cwd through its
left, passes. A regression that moves a pull-in boundary while
every import still succeeds passes this gate and waits for the next `make
characterize`. Claiming otherwise would be the same mistake the validation
tree made when 44 claims were asserted by nobody
(``docs/dev/contributing/validation.md``) — so this gate's promise is
deliberately narrow
and stated rather than implied.

Parsed with ``ast``, never imported: importing a subject would drag in the
compiled extension it characterizes, which makes a lint gate depend on a
built tree. Discovery is by glob, so a new subject is covered the moment its
folder exists — there is no list here to forget to update.
"""

from __future__ import annotations

import ast
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "doppler"
GLOB = "*/tests/characterization/*/characterize.py"

#: A file name a subject writes: no directory part, an artifact's extension.
ARTIFACT_EXT = (
    ".png",
    ".svg",
    ".pdf",
    ".json",
    ".csv",
    ".txt",
    ".npz",
    ".npy",
    ".tlm",
    ".events",
)


def _has_main_block(tree: ast.Module) -> bool:
    """True if the module has a top-level ``if __name__ == "__main__":``.

    Matched structurally rather than by text so a comment mentioning
    ``__main__`` cannot satisfy it, which is the whole point of using ast.
    """
    for node in tree.body:
        if not isinstance(node, ast.If):
            continue
        test = node.test
        if (
            isinstance(test, ast.Compare)
            and isinstance(test.left, ast.Name)
            and test.left.id == "__name__"
            and any(
                isinstance(c, ast.Constant) and c.value == "__main__"
                for c in test.comparators
            )
        ):
            return True
    return False


def _dotted(path: Path) -> str:
    """Dotted import path of a subject, as its fast twin spells it."""
    return ".".join(path.relative_to(SRC.parent).with_suffix("").parts)


def _imports_in(path: Path) -> set[str]:
    """Every module named by an ``import``/``from ... import`` in ``path``.

    Returns the module strings only; a ``from X import a, b`` contributes
    ``X`` rather than ``X.a``, which is what a subject import looks like.
    """
    try:
        tree = ast.parse(path.read_text(encoding="utf-8"))
    except (OSError, SyntaxError):
        return set()
    out: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            out.update(a.name for a in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module:
            out.add(node.module)
    return out


def _artifact_name(node: ast.expr) -> str | None:
    """The file name ``node`` spells, if it is a bare artifact name.

    A plain string, or an f-string whose literal parts carry no directory
    and whose last part ends in an artifact extension (``f"pool-{s}.png"``).
    Anything with a ``/``, a space or another suffix is not a file name.
    """
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        text = node.value
    elif isinstance(node, ast.JoinedStr):
        parts = [
            v.value
            for v in node.values
            if isinstance(v, ast.Constant) and isinstance(v.value, str)
        ]
        if not parts or not isinstance(node.values[-1], ast.Constant):
            return None
        text = "{}".join(parts)
    else:
        return None
    if "/" in text or " " in text or "\\" in text:
        return None
    stem, dot, ext = text.rpartition(".")
    if not stem or not dot or f".{ext}" not in ARTIFACT_EXT:
        return None
    return text


def _is_fromto(node: ast.AST) -> bool:
    """True for a call to ``fromto`` (bare or as ``util.fromto``)."""
    if not isinstance(node, ast.Call):
        return False
    f = node.func
    return (isinstance(f, ast.Name) and f.id == "fromto") or (
        isinstance(f, ast.Attribute) and f.attr == "fromto"
    )


def cwd_relative_artifacts(source: str) -> list[tuple[int, str]]:
    """Every artifact name in ``source`` that is relative to the cwd.

    A name is anchored when some enclosing call is ``fromto`` (it starts at
    the calling script, or wherever the caller points it), or when it is the
    right operand of a ``/`` (the left names the directory). Anything else
    is a bare name, and a bare name lands wherever the run was started.

    The left of a ``/`` is not judged: ``Path("out") / "x.png"`` passes
    though its directory is relative to the cwd. The rule catches the bare
    name that shipped twice (doppler#2406), not every relative path.

    Returns ``(line, name)`` for each unanchored one, in source order.

    >>> cwd_relative_artifacts('fig.savefig(out or "x.png")')
    [(1, 'x.png')]
    >>> cwd_relative_artifacts('fig.savefig(fromto(out or "x.png"))')
    []
    >>> cwd_relative_artifacts('open(out / f"run-{i}.json", "w")')
    []
    >>> cwd_relative_artifacts('label = "code.lock"')
    []
    """
    tree = ast.parse(source)
    parent: dict[ast.AST, ast.AST] = {}
    for node in ast.walk(tree):
        for child in ast.iter_child_nodes(node):
            parent[child] = node
    found: list[tuple[int, str]] = []
    for node in ast.walk(tree):
        if not isinstance(node, (ast.Constant, ast.JoinedStr)):
            continue
        if isinstance(parent.get(node), ast.JoinedStr):
            continue  # a piece of an f-string; the f-string is judged whole
        name = _artifact_name(node)
        if name is None:
            continue
        up = parent.get(node)
        if (
            isinstance(up, ast.BinOp)
            and isinstance(up.op, ast.Div)
            and up.right is node
        ):
            continue
        anchored = False
        while up is not None:
            if _is_fromto(up):
                anchored = True
                break
            up = parent.get(up)
        if not anchored:
            found.append((node.lineno, name))
    return sorted(found)


def main() -> int:
    subjects = sorted(SRC.glob(GLOB))
    if not subjects:
        # Not a failure: the category is allowed to be empty. Say so, rather
        # than printing a bare OK that reads as "all subjects fine".
        print("check_characterization: OK — no characterization subjects")
        return 0

    problems: list[str] = []
    for subject in subjects:
        rel = subject.relative_to(ROOT)
        dotted = _dotted(subject)

        # 1. every level a package, so the twin's dotted import resolves
        for parent in subject.parents:
            if parent == SRC.parent:
                break
            if not (parent / "__init__.py").exists():
                problems.append(
                    f"{rel}: {parent.relative_to(ROOT)}/__init__.py is "
                    f"missing — the fast twin imports this by dotted path"
                )

        # 2. runnable as a script, which is what `make characterize` does
        try:
            tree = ast.parse(subject.read_text(encoding="utf-8"))
        except SyntaxError as exc:
            problems.append(f"{rel}: does not parse — {exc}")
            continue
        if not _has_main_block(tree):
            problems.append(
                f'{rel}: no `if __name__ == "__main__":` block — '
                f"`make characterize` would sweep nothing and still pass"
            )

        # 3. a fast twin somewhere under the module's tests/
        tests_dir = subject.parents[2]  # <mod>/tests/
        twins = [
            t
            for t in sorted(tests_dir.rglob("test_*.py"))
            if dotted in _imports_in(t)
        ]
        if not twins:
            problems.append(
                f"{rel}: no fast twin — no test_*.py under "
                f"{tests_dir.relative_to(ROOT)} imports `{dotted}`, so "
                f"nothing exercises it between `make characterize` runs"
            )

        # 4. every script in the subject anchors the files it writes. A
        # sibling that does not parse is reported here: rule 2 parses only
        # characterize.py, and a broken telemetry.py would otherwise pass
        # every rule unread.
        for script in sorted(subject.parent.glob("*.py")):
            try:
                bare = cwd_relative_artifacts(
                    script.read_text(encoding="utf-8")
                )
            except SyntaxError as exc:
                if script != subject:  # characterize.py's is rule 2's
                    problems.append(
                        f"{script.relative_to(ROOT)}: does not parse — {exc}"
                    )
                continue
            for line, name in bare:
                problems.append(
                    f"{script.relative_to(ROOT)}:{line}: `{name}` is "
                    f"relative to the working directory, so it lands "
                    f"wherever the run started -- name it through "
                    f"doppler.util.fromto, or join it onto a directory"
                )

    if problems:
        print("check_characterization: FAIL", file=sys.stderr)
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        print(
            "\n  A characterization is run deliberately, so its fast twin is "
            "the only\n  per-push cover it has. See "
            "src/doppler/dsss/tests/characterization/__init__.py",
            file=sys.stderr,
        )
        return 1

    print(
        f"check_characterization: OK — {len(subjects)} subject(s), "
        f"each runnable, each with a fast twin, none writing into the cwd"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
