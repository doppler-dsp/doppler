#!/usr/bin/env python3
"""Fail when a runnable doc fence or an example computes a claim and drops it.

The snippet gates (``src/doppler/tests/test_*doc_snippets.py``) run every
``python`` fence under ``docs/``, and ``test_examples.py`` runs every script in
``src/doppler/examples/``. They catch code that BREAKS. They cannot catch code
that checks nothing: a line such as

    np.array_equal(plan.render(), scene.compose())   # bit-identical baseline

runs, evaluates to ``True`` or ``False``, and throws the answer away. The page
tells the reader the two are bit-identical, and nothing would notice if they
stopped being so. The v0.60.0 wfmgen audit (doppler#1682) found three of these
on ``main``: two on guide pages, one on an API page.

Two rules, each a refusal:

1. **A discarded claim.** An expression statement whose value is a comparison
   (``a == b``, ``x < y``), or a call to ``array_equal``/``allclose``/
   ``isclose``/``array_equiv``/``all``/``any``, in a ``python`` fence or an
   example script. It should be an ``assert``. ``pycon`` fences are exempt:
   their expected output IS the check, and the doctest runner compares it.
2. **An example that checks nothing.** A script in ``src/doppler/examples/``
   with no ``assert``, no ``raise`` and no ``sys.exit(<expr>)`` (the
   ``return 1`` / ``sys.exit(main())`` shape) can only ever exit 0, which the
   examples gate then reads as "demonstrated AND checked". The two-process
   examples in ``.examples-pairs`` are exempt, because their evidence regex in
   that registry is their check, and ``test_example_pair_runs`` enforces it.

Nothing is registered. The fences are found with the SAME discovery and
``--8<--`` include resolution the snippet runners use
(``src/doppler/tests/_docs_snippet_common.py``), so this gate reads exactly
the code a reader sees and the runners execute. A new page or script is
covered the moment it exists. There is no allow-list and no ratchet: the
count on arrival was fixed to zero in the same change.

Run: python3 scripts/check_doc_claims.py [--root DIR]
"""

from __future__ import annotations

import argparse
import ast
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# The fence discovery is the snippet runners' own, imported rather than
# copied, so the two cannot disagree about which code is documentation.
sys.path.insert(0, str(ROOT / "src"))
from doppler.tests._docs_snippet_common import (  # noqa: E402
    iter_fences,
    resolve_snippets,
)

#: Calls whose only use is their truth value. Called as a statement, the
#: answer is discarded.
_CLAIM_CALLS = {
    "array_equal",
    "allclose",
    "isclose",
    "array_equiv",
    "all",
    "any",
}


def _discarded_claims(tree: ast.AST) -> list[int]:
    """Line numbers of expression statements that compute and drop a claim."""
    lines = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Expr):
            continue
        value = node.value
        if isinstance(value, ast.Compare):
            lines.append(node.lineno)
        elif isinstance(value, ast.Call):
            func = value.func
            name = (
                func.attr
                if isinstance(func, ast.Attribute)
                else getattr(func, "id", "")
            )
            if name in _CLAIM_CALLS:
                lines.append(node.lineno)
    return sorted(lines)


def _checks_something(tree: ast.AST) -> bool:
    """True if a script can exit non-zero on its own: an assert, a raise,
    or ``sys.exit`` / ``exit`` / ``SystemExit`` given a non-constant."""
    for node in ast.walk(tree):
        if isinstance(node, (ast.Assert, ast.Raise)):
            return True
        if isinstance(node, ast.Call) and node.args:
            func = node.func
            name = (
                func.attr
                if isinstance(func, ast.Attribute)
                else getattr(func, "id", "")
            )
            if name in {"exit", "SystemExit"} and not isinstance(
                node.args[0], ast.Constant
            ):
                return True
    return False


def _pair_scripts(examples: Path) -> set[str]:
    """Both halves of every registered two-process example."""
    registry = examples / ".examples-pairs"
    names: set[str] = set()
    if not registry.exists():
        return names
    for raw in registry.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        key = line.partition(":")[0]
        names.update(part.strip() for part in key.split("+"))
    return names


def _fence_findings(root: Path) -> list[str]:
    out = []
    docs = root / "docs"
    for page in sorted(docs.rglob("*.md")):
        text = page.read_text(encoding="utf-8")
        rel = page.relative_to(root).as_posix()
        for _marker, code in iter_fences(text, "python"):
            resolved = resolve_snippets(code)
            try:
                tree = ast.parse(resolved)
            except SyntaxError:
                # Not this gate's question: the snippet runner reports a
                # fence that does not parse.
                continue
            body = resolved.splitlines()
            # The fence's own first line, for a finding a reader can open.
            start = text[: text.find(code)].count("\n") + 1
            for lineno in _discarded_claims(tree):
                where = start + lineno - 1 if resolved == code else start
                stmt = body[lineno - 1].strip()
                out.append(
                    f"{rel}:{where}: a claim is computed and discarded "
                    f"(make it an assert): {stmt}"
                )
    return out


def _example_findings(root: Path) -> list[str]:
    out = []
    examples = root / "src" / "doppler" / "examples"
    pairs = _pair_scripts(examples)
    for script in sorted(examples.glob("*.py")):
        rel = script.relative_to(root).as_posix()
        source = script.read_text(encoding="utf-8")
        try:
            tree = ast.parse(source)
        except SyntaxError as exc:
            out.append(f"{rel}:{exc.lineno}: does not parse: {exc.msg}")
            continue
        body = source.splitlines()
        for lineno in _discarded_claims(tree):
            out.append(
                f"{rel}:{lineno}: a claim is computed and discarded "
                f"(make it an assert): {body[lineno - 1].strip()}"
            )
        if script.name not in pairs and not _checks_something(tree):
            out.append(
                f"{rel}: checks nothing -- no assert, no raise, no "
                "sys.exit(<expr>), so it can only exit 0"
            )
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--root",
        type=Path,
        default=ROOT,
        help="tree to check (default: this checkout)",
    )
    args = parser.parse_args()
    root = args.root.resolve()
    findings = _fence_findings(root) + _example_findings(root)
    for line in findings:
        print(line, file=sys.stderr)
    if findings:
        print(
            f"check_doc_claims: {len(findings)} finding(s): a doc fence or "
            "example that states a result must check it",
            file=sys.stderr,
        )
        return 1
    print("check_doc_claims: OK, every claim in a fence or example is checked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
