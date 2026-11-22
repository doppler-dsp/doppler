#!/usr/bin/env python3
"""Every owned Field of a source is handled at every site that owns one.

`wfm_source_t` carries its bit sequences as `wfm_seq_t` members (`acq_code`,
`data_code`, `data`, `fill`, `mac_header`, ...), each with a heap `bits` array
the source's owner must copy and free. The owners are four hand-written sites,
because each owns a different subset on purpose (the composer owns every one
and the frame description; the Plan's noise-only copy drops the data it does
not render; the JSON reader frees what it parsed; the CLI frees what it
parsed), so one shared list cannot serve them: a copy site that owned `data`
would be wrong for the Plan.

What can be held is that a NEW Field is not forgotten at one of them: the
`mac_header` Field was missed by the Plan's copy (a struct-copied pointer to
the composer's freed bits), found by a reviewer and not by a test. This gate
reads the `wfm_seq_t` members of `wfm_source_t` and requires each one
(`retired_*` excepted: nothing reads them, and the Python binding, which jm
generates, frees them itself) to be mentioned in every site file, so adding a
Field fails here until each site says what it does with it, even if what it
does is drop it.

Usage:  python3 scripts/check_source_owned_fields.py [--root DIR]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

HEADER = "native/inc/doppler/wfm/wfm_compose.h"
#: The four hand-written sites that own a source's arrays.
SITES = (
    "native/src/wfm/wfm_compose.c",
    "native/src/wfm/wfm_json.c",
    "native/src/wfm/wfm_plan.c",
    "native/src/app/wfmgen.c",
)
#: Members nothing reads (the Python binding frees them itself).
EXEMPT = re.compile(r"^retired_")


def source_fields(text: str) -> list[str]:
    """The `wfm_seq_t` member names of `wfm_source_t`, in order."""
    m = re.search(r"typedef struct\s*\{(.*?)\}\s*wfm_source_t\s*;", text, re.S)
    if not m:
        raise SystemExit(f"no wfm_source_t in {HEADER}")
    body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
    return re.findall(r"\bwfm_seq_t\s+(\w+)\s*;", body)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--root", default=str(Path(__file__).resolve().parent.parent)
    )
    root = Path(ap.parse_args().root)
    fields = [
        f
        for f in source_fields((root / HEADER).read_text(encoding="utf-8"))
        if not EXEMPT.match(f)
    ]
    problems = []
    for site in SITES:
        text = (root / site).read_text(encoding="utf-8")
        for f in fields:
            if not re.search(rf"(?:->|\.){f}\b", text):
                problems.append(
                    f"{site}: does not mention the owned Field `{f}` of"
                    f" wfm_source_t: copy it, free it or drop it here"
                )
    if problems:
        print("\n".join(problems))
        print(f"source-fields: {len(problems)} problem(s).")
        return 1
    print(
        f"source-fields: OK -- {len(fields)} owned Field(s), each handled at"
        f" {len(SITES)} site(s)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
