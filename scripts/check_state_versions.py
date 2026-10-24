#!/usr/bin/env python3
"""A state magic's version must never go back to a number it has shipped.

Every serialized blob opens with `[magic][version]`, and `set_state` accepts
a blob only when both match (`dp_state_validate`, in `doppler/dp_state.h`).
That is what makes the promise in `docs/design/state-serialization.md` hold --
"a wrong blob is rejected, never reinterpreted" -- but only while each
`(magic, version)` pair names ONE layout for all time. Reuse a pair and a blob
of the old layout passes the envelope and is read as the new one.

doppler#2569 is the case that made this a gate. The parallel pool replaced the
cell-receiver pool under the same magic, `'ADPL'`, and restarted its version at
2. The old pool had shipped as 3 from v0.48.0 to v0.73.0, so the new pool's
next bump, 2 -> 3, would have accepted every one of those blobs whose size
happened to match.

The rule, per magic, over every release tag (`v*`) that carries it:

- the version at the LATEST such tag must also be the HIGHEST it ever shipped
  with, or the current header must step past that highest; and
- the current version may equal the latest tag's (nothing changed since the
  release) or be greater than every shipped version. Anything else names a
  layout some release already used.

Each current magic must also be declared once, beside its own `_STATE_VERSION`
define: a magic with no version define, or a version written as a literal at
the call site, is invisible to this check, so it is refused rather than
skipped.

Nothing is registered. The magics are read from `native/**.h` in the working
tree, the history from the same files at every release tag, and a clone with no
tags fails closed -- with no history there is nothing to compare against, and
"no findings" would be a lie.
"""

from __future__ import annotations

import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

DEFINE = re.compile(
    r"^\s*#\s*define\s+(?P<prefix>[A-Z0-9_]+)_STATE_(?P<kind>MAGIC|VERSION)"
    r"\s+(?P<value>.*)$",
    re.MULTILINE,
)
FOURCC = re.compile(
    r"DP_FOURCC\s*\(\s*'(.)'\s*,\s*'(.)'\s*,\s*'(.)'\s*,\s*'(.)'\s*\)"
)
NUMBER = re.compile(r"^\(?\s*(\d+)[uU]?\s*\)?")
SCAN = ["native/inc", "native/src"]


def _strip(text: str) -> str:
    """Join backslash continuations and drop comments, keeping newlines."""
    text = re.sub(r"\\\n", " ", text)
    text = re.sub(
        r"/\*.*?\*/", lambda m: "\n" * m[0].count("\n"), text, flags=re.DOTALL
    )
    return re.sub(r"//[^\n]*", "", text)


def parse(
    text: str, where: str, errors: list[str], literals: bool = False
) -> dict[str, int]:
    """Map each magic in one file to its version.

    A `<P>_STATE_MAGIC` pairs with the `<P>_STATE_VERSION` in the same file.
    A magic that is not a `DP_FOURCC(...)`, or has no numeric version beside
    it, is reported into `errors`.

    With `literals`, which is for reading a release tag, a magic with no
    version define takes the literal its header was written with,
    `<P>_STATE_MAGIC, 1u` (DP_GET_OPEN, dp_w_hdr and the others all spell it
    that way). The framer shipped like that through v0.74.0. The current
    tree gets no such allowance: there the define is required.
    """
    magics: dict[str, str] = {}
    versions: dict[str, int | None] = {}
    for m in DEFINE.finditer(_strip(text)):
        value = m["value"].strip()
        if m["kind"] == "MAGIC":
            f = FOURCC.search(value)
            if f is None:
                errors.append(
                    f"{where}: {m['prefix']}_STATE_MAGIC is not a "
                    f"DP_FOURCC(...): {value!r}"
                )
                continue
            magics[m["prefix"]] = "".join(f.groups())
        else:
            n = NUMBER.match(value)
            versions[m["prefix"]] = int(n[1]) if n else None
    out: dict[str, int] = {}
    flat = _strip(text)
    for prefix, magic in magics.items():
        v = versions.get(prefix)
        if v is None and literals:
            found = {
                int(n)
                for n in re.findall(
                    rf"\b{prefix}_STATE_MAGIC\s*,\s*(\d+)[uU]?\b", flat
                )
            }
            v = max(found) if found else None
        if v is None:
            errors.append(
                f"{where}: {prefix}_STATE_MAGIC '{magic}' has no "
                f"numeric {prefix}_STATE_VERSION beside it"
            )
            continue
        out[magic] = v
    return out


def _git(root: Path, *args: str, data: bytes | None = None) -> bytes:
    return subprocess.run(
        ["git", *args], cwd=root, input=data, capture_output=True, check=True
    ).stdout


def current(root: Path, errors: list[str]) -> dict[str, int]:
    """The working tree's magics; a magic declared twice is an error."""
    files = (
        _git(
            root,
            "grep",
            "--untracked",
            "-l",
            "-E",
            "_STATE_MAGIC",
            "--",
            *SCAN,
        )
        .decode()
        .split()
    )
    seen: dict[str, tuple[int, str]] = {}
    for path in files:
        for magic, v in parse((root / path).read_text(), path, errors).items():
            if magic in seen:
                errors.append(
                    f"{path}: magic '{magic}' is already declared "
                    f"in {seen[magic][1]}"
                )
            seen[magic] = (v, path)
    return {k: v for k, (v, _) in seen.items()}


History = dict[str, list[tuple[int, int]]]


def shipped(root: Path) -> tuple[list[str], History, dict[str, list[int]]]:
    """Every release tag, oldest first, and per magic its (index, version).

    The third value maps a magic some tag carried with no readable version to
    the indexes of those tags.

    Read with one `git grep` over all the tags and one `cat-file --batch`.
    """
    tags = (
        _git(root, "tag", "-l", "v[0-9]*", "--sort=v:refname").decode().split()
    )
    hist: History = defaultdict(list)
    blind: dict[str, list[int]] = defaultdict(list)
    if not tags:
        return tags, hist, blind
    hits = (
        _git(root, "grep", "-l", "-E", "_STATE_MAGIC", *tags, "--", *SCAN)
        .decode()
        .split()
    )
    if not hits:
        return tags, hist, blind
    blobs = _git(
        root,
        "cat-file",
        "--batch",
        data="".join(h + "\n" for h in hits).encode(),
    )
    index = {t: i for i, t in enumerate(tags)}
    pos = 0
    per_tag: dict[str, dict[str, int]] = defaultdict(dict)
    for h in hits:
        header_end = blobs.index(b"\n", pos)
        size = int(blobs[pos:header_end].split()[2])
        body = blobs[header_end + 1 : header_end + 1 + size].decode(
            errors="replace"
        )
        pos = header_end + 1 + size + 1
        tag = h.split(":", 1)[0]
        # A magic a tag carried with no version this can read would count as
        # never released, and so pass at ANY number: that is a finding, once
        # per magic. A tag's other oddities (a define since fixed) are not.
        errs: list[str] = []
        per_tag[tag].update(parse(body, h, errs, literals=True))
        for e in errs:
            m = re.search(r"_STATE_MAGIC '(.{4})' has no", e)
            if m:
                blind[m[1]].append(index[tag])
    for tag, magics in per_tag.items():
        for magic, v in magics.items():
            hist[magic].append((index[tag], v))
    for rows in hist.values():
        rows.sort()
    return tags, hist, blind


def check(root: Path) -> tuple[list[str], str]:
    """The findings, and a one-line account of what was compared."""
    errors: list[str] = []
    now = current(root, errors)
    tags, hist, blind = shipped(root)
    if not tags:
        return [
            *errors,
            "no release tags (v*) in this clone: fetch them "
            "(git fetch --tags) -- with no history this gate "
            "cannot check anything",
        ], ""
    for magic, at in sorted(blind.items()):
        first, last = tags[min(at)], tags[max(at)]
        errors.append(
            f"'{magic}' shipped at {first}..{last} with no version this "
            f"can read (no _STATE_VERSION define, no `_STATE_MAGIC, Nu` "
            f"literal), so its history is unknown: teach parse() its spelling"
        )
    for magic, v in sorted(now.items()):
        rows = hist.get(magic)
        if not rows:
            continue  # never released: any version is free
        latest_i, latest = rows[-1]
        top = max(r[1] for r in rows)
        top_tags = [tags[i] for i, r in rows if r == top]
        if v > top or (v == latest == top):
            continue
        span = (
            top_tags[0]
            if len(top_tags) == 1
            else f"{top_tags[0]}..{top_tags[-1]}"
        )
        was = (
            f"{span} shipped {top}"
            if latest == top
            else f"{tags[latest_i]} shipped {latest} and {span} shipped {top}"
        )
        errors.append(
            f"'{magic}' is version {v}, but {was}: set it to {top + 1}u or "
            f"more, past every version this magic has carried"
        )
    released = sum(1 for m in now if m in hist)
    return errors, (
        f"{len(now)} magics, {released} of them released, "
        f"against {len(tags)} tags"
    )


def main() -> int:
    root = Path(
        _git(Path.cwd(), "rev-parse", "--show-toplevel").decode().strip()
    )
    errors, what = check(root)
    for e in errors:
        print(f"state-versions-check: {e}", file=sys.stderr)
    if errors:
        return 1
    print(
        f"state-versions-check: OK, {what}: no magic reuses a version "
        f"it has shipped"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
