"""A documented default is the default the object actually has.

The ``Synth`` docstring said ``fs : float, default 1.0`` while a bare
``Synth()`` ran at 1e6 Hz (#3142), so a caller who followed the docstring and
gave ``freq`` in cycles per sample got an offset a million times too small.
Nothing compared the two, because the docstring and the constructor are
rendered from different places.

This test reads every ``name : type, default V`` line of a class docstring,
builds the object bare, and requires its attribute ``name`` to equal ``V``.
The known disagreements are the ``EXEMPT`` entries, a ratchet: an entry may
only be removed, and one that has stopped disagreeing fails here until it is.
"""

from __future__ import annotations

import ast
import re

import pytest

from doppler.wfm import Segment, Synth

#: (class, parameter) -> why it still disagrees. Shrink-only.
EXEMPT = {
    ("Synth", "fs"): (
        "#3142, just-buildit/just-makeit#2678: the composer generator gives "
        "a source type fs = 1e6 and renders the Segment field default 1.0"
    ),
}

_LINE = re.compile(r"^(\w+) : [^\n,]+, default ([^\n]+)$", re.M)


def _documented(doc: str) -> list[tuple[str, object]]:
    """The (name, default) pairs a docstring states."""
    out = []
    for name, text in _LINE.findall(doc or ""):
        text = text.strip()
        # an enum default is written ``"tone"`` in the docstring
        text = text.removeprefix("``").removesuffix("``")
        try:
            out.append((name, ast.literal_eval(text)))
        except (ValueError, SyntaxError):
            out.append((name, text))
    return out


def _differences(doc: str, obj: object) -> dict[str, tuple[object, object]]:
    """Parameter -> (documented, actual) for each that differs."""
    bad = {}
    for name, want in _documented(doc):
        got = getattr(obj, name)
        if got != want and str(got) != str(want):
            bad[name] = (want, got)
    return bad


def _disagrees(cls: type) -> dict[str, tuple[object, object]]:
    """The bare-built ``cls`` against its own docstring."""
    return _differences(cls.__doc__ or "", cls())


@pytest.mark.parametrize("cls", [Synth, Segment], ids=lambda c: c.__name__)
def test_documented_defaults_are_the_actual_defaults(cls):
    assert _documented(cls.__doc__), (
        f"{cls.__name__}'s docstring states no defaults"
    )
    bad = _disagrees(cls)
    unexpected = {
        k: v for k, v in bad.items() if (cls.__name__, k) not in EXEMPT
    }
    assert not unexpected, (
        f"{cls.__name__}: documented default differs from the actual one "
        f"(documented, actual): {unexpected}"
    )
    stale = [k for (c, k) in EXEMPT if c == cls.__name__ and k not in bad]
    assert not stale, (
        f"{cls.__name__}: {stale} now agree; remove them from EXEMPT"
    )


def test_the_exempt_entry_is_the_one_in_the_issue():
    """A control: the check does see the Synth.fs disagreement."""
    assert _disagrees(Synth).get("fs") == (1.0, 1_000_000.0)


def test_an_enum_default_is_compared_not_just_unquoted():
    """Controls for the quote stripping.

    A docstring writes an enum default as ``"tone"`` (RST literal marks round
    the quoted string). Stripping the marks leaves the string itself, so a real
    disagreement still differs: documented ``"tone"`` against an attribute
    ``"noise"`` is reported, and against ``"tone"`` it is not. If stripping
    could hide a disagreement the first assertion would fail.
    """
    doc = 'type : str, default ``"tone"``\n    The waveform.\n'

    class Wrong:
        type = "noise"

    class Right:
        type = "tone"

    assert _differences(doc, Wrong()) == {"type": ("tone", "noise")}
    assert _differences(doc, Right()) == {}
