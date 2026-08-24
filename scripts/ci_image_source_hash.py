#!/usr/bin/env python3
"""Hash the inputs that actually determine the CI toolchain image.

``make ci-image-check`` asks one question: is the pinned image still the
environment this tree describes? It answered it by hashing whole files::

    cat bootstrap.toml deploy/docker/Dockerfile.ci | sha256sum

which hashes the *file*, not the *image*. Every byte of ``bootstrap.toml``
counted, including ``[project] version`` — a string the image build never
reads. So syncing doppler's release version into ``bootstrap.toml``, which is
what stops it going stale, would have invalidated the image on **every
release** and demanded a rebuild plus a repin for a number no layer consumes.
Measured before changing anything: editing that one line failed
``ci-image-check`` and printed the rebuild instructions. A reformatted comment
did the same.

What the image is built from
----------------------------
``Dockerfile.ci`` copies ``bootstrap.toml`` in and runs ``jbx install-deps``
plus ``jbx install-deps -g docs``. The installed package set is therefore
determined by the ``[dev.*]`` / ``[docs.*]`` tables and by
``[tools.install-deps] groups``. ``[project]`` carries the name and version
and reaches no layer.

So this hashes **everything in bootstrap.toml except ``[project]``**, plus the
Dockerfile verbatim.

Excluding rather than including is deliberate. An include-list ("hash
``[dev.*]`` and ``[docs.*]``") silently stops covering a table added later,
which is the failure mode where a gate keeps passing because it stopped
looking. Excluding one table that has been reasoned about leaves every future
table covered by default: the safe direction is the one where forgetting means
an extra rebuild, not a missed one.

Parsing rather than grepping also means comments and whitespace stop counting,
which is the same correctness point from the other side — a note added above a
package list is not a different image.

Usage
-----
    python scripts/ci_image_source_hash.py            # print the hash
    python scripts/ci_image_source_hash.py --explain  # print what is hashed

Both this and ``.github/workflows/ci-image.yml`` must agree, or the check and
the thing it checks are computing different numbers. They agree by
CONSTRUCTION: the workflow shells out to ``make -s ci-image-source-hash``,
which runs this file. There is no second copy of the derivation.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

import tomllib

ROOT = Path(__file__).resolve().parent.parent

#: Tables in bootstrap.toml that do not reach any image layer. Keep this list
#: short and justified — every entry is something the gate stops watching.
#: ``[project]`` is name + version: identity, not environment.
EXCLUDED_TABLES = ("project",)


def source_material() -> tuple[str, bytes]:
    """Return the canonical bootstrap payload and the Dockerfile bytes.

    The payload is JSON with sorted keys rather than the original TOML, so
    the hash is a function of the *values* jbx will act on and not of their
    spelling. Two files that install the same packages hash the same.
    """
    bootstrap = ROOT / "bootstrap.toml"
    dockerfile = ROOT / "deploy" / "docker" / "Dockerfile.ci"
    cfg = tomllib.loads(bootstrap.read_text(encoding="utf-8"))
    for table in EXCLUDED_TABLES:
        cfg.pop(table, None)
    payload = json.dumps(cfg, sort_keys=True, separators=(",", ":"))
    return payload, dockerfile.read_bytes()


def compute() -> str:
    payload, dockerfile = source_material()
    h = hashlib.sha256()
    h.update(payload.encode("utf-8"))
    h.update(dockerfile)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--explain",
        action="store_true",
        help="print the material being hashed instead of the hash",
    )
    args = ap.parse_args()
    if args.explain:
        payload, dockerfile = source_material()
        print(f"bootstrap.toml minus {list(EXCLUDED_TABLES)}:")
        print(payload)
        print(f"\ndeploy/docker/Dockerfile.ci: {len(dockerfile)} bytes")
        return 0
    print(compute())
    return 0


if __name__ == "__main__":
    sys.exit(main())
