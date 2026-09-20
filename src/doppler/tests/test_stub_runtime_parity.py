"""Every public member a class has at runtime must be in its ``.pyi``.

A stub that lags its extension does not mistype the new API -- it hides
it. A type checker rejects the call as an unknown attribute, and the docs
site renders classes FROM the stub, so the member is absent there too.
For a jm-generated module that cannot happen (both faces come from one
manifest), but a ``no_generate`` module's stub is hand-owned: nothing
regenerates it and, until this test, nothing noticed. ``buffer.pyi``
was missed that way twice -- ``close``/``closed`` (cba74b75), then
``peek``/``write_some``/``reset``/``space`` (doppler#1427).

Registration-free: every ``src/doppler/<mod>/<mod>.pyi`` is discovered,
every class in it that also exists at runtime is compared. The direction
is runtime -> stub only; a stub-only name is a different defect (a typed
promise nothing keeps) and is not this test's.
"""

import ast
import importlib
import pathlib

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[1]

# CPython adds these to every PyStructSequence type; they are not API.
STRUCTSEQ = {"n_fields", "n_sequence_fields", "n_unnamed_fields"}

# Existing breakage. May only shrink: an entry that no longer reproduces
# fails the test too, so a fix cannot leave its excuse behind.
KNOWN_GAPS = {
    ("doppler.stream.stream", "Push"): {"send_eos"},  # doppler#1431
}


def _stub_members(cls: ast.ClassDef) -> set[str]:
    names = set()
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            names.add(node.name)
        elif isinstance(node, ast.AnnAssign):
            if isinstance(node.target, ast.Name):
                names.add(node.target.id)
        elif isinstance(node, ast.Assign):
            names.update(t.id for t in node.targets if isinstance(t, ast.Name))
    return names


def _cases():
    for pyi in sorted(ROOT.glob("*/*.pyi")):
        mod = f"doppler.{pyi.parent.name}.{pyi.stem}"
        for node in ast.parse(pyi.read_text()).body:
            if isinstance(node, ast.ClassDef) and node.name[0] != "_":
                yield pytest.param(mod, node, id=f"{mod}.{node.name}")


CASES = list(_cases())


def test_the_gate_sees_classes():
    # An empty parametrize passes silently; a moved tree must not.
    assert len(CASES) > 50


@pytest.mark.parametrize(("mod", "cls"), CASES)
def test_runtime_members_are_stubbed(mod, cls):
    runtime = getattr(importlib.import_module(mod), cls.name, None)
    if runtime is None:
        pytest.skip("stub-only name (a Protocol or alias)")
    public = {k for k in vars(runtime) if not k.startswith("_")}
    missing = public - STRUCTSEQ - _stub_members(cls)
    assert missing == KNOWN_GAPS.get((mod, cls.name), set()), (
        f"{mod}.{cls.name}: runtime has {sorted(missing)} that the stub "
        "does not (or a KNOWN_GAPS entry is fixed: delete it)"
    )
