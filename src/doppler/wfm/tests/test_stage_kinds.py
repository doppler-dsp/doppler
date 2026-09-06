"""`doppler.wfm`'s stage-kind constants, and the two properties they carry.

The constants replaced five hand copies of one numbering (doppler#1223) —
`CRC16, RS, RANDOMISE, CONV = 0, 1, 2, 3` written out in an example, two test
modules, a validation script and a docs page. They are GENERATED from
`wfm_stage_kind_t` by `scripts/gen_stage_kinds.py`, whose `--check` runs in
`make lint`, so nothing here re-checks that the values match the C: reading
the header a second time would be the sixth copy, one level up.

What is checked here is what the generator cannot see — that the number means
the same stage on the OTHER face, and that the parameter stays open.
"""

from __future__ import annotations

import json

import numpy as np
import pytest

from doppler.tests._repo import repo_root
from doppler.wfm import (
    STAGE_CONV,
    STAGE_CRC16,
    STAGE_INTERLEAVE,
    STAGE_RANDOMISE,
    STAGE_RS,
    STAGE_USER,
    FrameDesc,
)

EMPTY = np.empty(0, np.uint8)

#: The kinds doppler allocates, in the order the C enum fixes.
NAMED = (
    STAGE_CRC16,
    STAGE_RS,
    STAGE_RANDOMISE,
    STAGE_CONV,
    STAGE_INTERLEAVE,
)


#: Where the scene schema states the stage-kind names.
SCHEMA = (
    "$defs",
    "frame_desc",
    "properties",
    "stages",
    "items",
    "properties",
    "kind",
)


def _json_face() -> list[str]:
    """The names the scene JSON accepts, in order, from the SHIPPED schema.

    `docs/schema/wfmgen.schema.json` rather than `[[enum]] stage_kind`, for
    two reasons. It is the face a user actually writes against, so comparing
    against it tests the join rather than the declaration both sides derive
    from. And it is JSON: `tomllib` arrives in 3.11 and this repo's floor is
    3.9, which CI caught on the first push of this file.
    """
    doc = json.loads(
        (repo_root() / "docs" / "schema" / "wfmgen.schema.json").read_text(
            encoding="utf-8"
        )
    )
    node = doc
    for key in SCHEMA:
        assert key in node, f"schema shape moved: no {key!r} under {SCHEMA}"
        node = node[key]
    for alt in node["oneOf"]:
        if "enum" in alt:
            return list(alt["enum"])
    raise AssertionError("no stage-kind enum in the scene schema")


def test_the_constant_is_the_index_the_json_name_resolves_to() -> None:
    """Both faces of one enum, compared rather than assumed.

    The scene JSON takes a stage kind by NAME and the Python API takes it by
    NUMBER, and the two are joined only by list ORDER — `wfm_json.c` pins that
    with `_Static_assert` because a table that drifts does not fail to
    compile, it maps a frame to the wrong transform. This is the same join
    seen from Python: `STAGE_RS` must be where `"rs"` sits.
    """
    names = _json_face()
    assert [names.index(n) for n in names] == list(NAMED), (
        f"{names} does not index as {NAMED}"
    )


def test_named_kinds_are_distinct_and_contiguous_from_zero() -> None:
    # A duplicate or a hole would let two kinds resolve to one kernel, which
    # is a wrong frame rather than an error.
    assert tuple(range(len(NAMED))) == NAMED


def test_user_range_is_above_everything_doppler_allocates() -> None:
    """`STAGE_USER` is a promise, and this is the promise as an assertion.

    doppler never allocates at or above it, so a kind a caller chose today
    cannot collide with a built-in added later. A new `WFM_STAGE_*` appended
    below the sentinel keeps that true and this test green; one added above it
    breaks the guarantee and this test.
    """
    assert max(NAMED) < STAGE_USER


def test_add_stage_accepts_a_callers_own_kind() -> None:
    """The extension point, exercised through the binding.

    `wfm_stage_kind_t` is an OPEN `uint32_t`: a caller allocates from
    `STAGE_USER` up and supplies the kernel through the ops table. The
    description must ACCEPT such a kind — assembly is what later refuses it,
    for want of a kernel, which is the honest place to refuse.
    """
    d = FrameDesc(EMPTY, EMPTY, EMPTY)
    d.add_field(np.ones(8, np.uint8))
    assert d.add_stage(STAGE_USER + 1, first_field=0, n_fields=1) == 0


def test_kind_is_deliberately_not_a_string_enum() -> None:
    """Pinned because it was tried, measured and reverted (doppler#1223).

    jm's `enum` key renders a CLOSED choice list and rejects an integer, so
    binding `kind` that way turned `add_stage(0x1001, ...)` into a
    `TypeError` — deleting the extension point the test above exercises. If
    someone converts it for the nicer error message, this fails and says why.
    """
    d = FrameDesc(EMPTY, EMPTY, EMPTY)
    d.add_field(np.ones(8, np.uint8))
    with pytest.raises(TypeError):
        d.add_stage("crc16", first_field=0, n_fields=1)
