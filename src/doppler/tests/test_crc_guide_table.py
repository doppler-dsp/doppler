"""The CRC guide's table of named models is held to ``dp_crc.h``.

``docs/guide/crc.md`` lists the nine named catalogue instances with their
parameters, ``refin`` and check value. The header is the source: the six
numbers, ``refin`` among them, are fields of each macro, and the check
value and the stated ``refin`` are in the macro's own comment (the unit test
pins the check values to the catalogue, and this one holds the field to the
comment).
A value changed in one place and not the other fails here, so the listing
cannot go stale the way a hand-kept copy does.

Run it with ``make test-python TEST_PATHS=src/doppler/tests/``.
"""

from __future__ import annotations

import re

from doppler.tests._repo import repo_root

ROOT = repo_root()
HEADER = ROOT / "native" / "inc" / "doppler" / "dp_crc.h"
GUIDE = ROOT / "docs" / "guide" / "crc.md"

# one macro, with the doc comment directly above it (not an earlier one)
MACRO = re.compile(
    r"/\*\*((?:(?!/\*\*).)*?)\*/\s*#define (DP_CRC_\w+)\s*\\?\s*"
    r"\(\(dp_crc_t\)\{([^}]*)\}\)",
    re.S,
)


def header_models() -> dict[str, tuple]:
    """Each macro's (width, poly, init, refin, refout, xorout, check)."""
    out = {}
    text = HEADER.read_text(encoding="utf-8")
    for m in MACRO.finditer(text):
        doc = " ".join(
            line.strip().lstrip("*").strip()
            for line in m.group(1).splitlines()
        )
        name = m.group(2)
        fields = {
            k.strip().lstrip("."): int(v.strip().rstrip("ulUL"), 0)
            for k, v in (
                kv.split("=")
                for kv in m.group(3).replace("\\\n", " ").split(",")
            )
        }
        width, poly, init, refout, xorout = (
            fields[k] for k in ("width", "poly", "init", "refout", "xorout")
        )
        refin = re.search(r"`refin` (?:is )?(true|false)", doc)
        check = re.search(r"Check(?: value)? (0x[0-9A-Fa-f]+)", doc)
        assert refin and check, f"{name}: the comment must state refin, check"
        assert bool(fields["refin"]) == (refin.group(1) == "true"), (
            f"{name}: the .refin field disagrees with the comment"
        )
        out[name] = (
            width,
            poly,
            init,
            refin.group(1) == "true",
            bool(refout),
            xorout,
            int(check.group(1), 16),
        )
    return out


def guide_models() -> dict[str, tuple]:
    """The guide's table rows, parsed the same way."""
    out = {}
    for line in GUIDE.read_text(encoding="utf-8").splitlines():
        cells = [
            c.strip().strip("`") for c in line.strip().strip("|").split("|")
        ]
        if len(cells) == 8 and cells[0].startswith("DP_CRC_"):
            out[cells[0]] = (
                int(cells[1]),
                int(cells[2], 16),
                int(cells[3], 16),
                cells[4] == "true",
                cells[5] == "true",
                int(cells[6], 16),
                int(cells[7], 16),
            )
    return out


def test_the_header_has_nine_named_models():
    assert len(header_models()) == 9


def test_the_guide_table_is_the_headers():
    assert guide_models() == header_models()


def test_every_named_model_reflects_its_input_and_output_together():
    """The guide says so; a model that differs would need a new sentence."""
    for name, (_w, _p, _i, refin, refout, _x, _c) in header_models().items():
        assert refin == refout, name
