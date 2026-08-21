#!/usr/bin/env python3
"""A `docs/x.md §N` citation must name a section that is really there.

Prose cites design sections by number 92 times across this tree, and nothing
checked any of them. Renumbering a document, or citing from memory, leaves a
reference that reads authoritative and points at the wrong argument — the
reader who follows it concludes the claim is unsupported.

That is not hypothetical. `carrier_nda_core.h` cited `docs/design/mpsk.md`
§2.3 three times: for the one-AGC-per-receiver argument, for the
squaring-loss measurement, and for the lock statistic's H0 variance. §2.3 is
"The invariant" — rate-keyed constants — and contains none of them. All
three were in the document, in §3.2 and §4.2. doppler#795 was filed on the
strength of the middle one, reporting that the `~6 dB Es/N0` floor "has no
measurement behind it that I can find anywhere in the tree"; it has a
measured table, six rows by three columns, in §3.2.

## The two halves, and why the second one is what matters

Checking that §N EXISTS catches a renumber or a deletion. It would NOT have
caught the case above, because §2.3 exists — it is simply about something
else.

So a citation may also name the section's TITLE, and when it does, the title
is checked against the heading. `§3.2, "The NDA discriminator + lock signal"`
is self-verifying: get the number wrong and the gate says so. Titles are
optional by design — requiring all 92 at once would be a mechanical edit
nobody reviews — and every one added is coverage that cannot regress.
"""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

#: `docs/design/mpsk.md` §3.2, "Optional title"  — backticks optional, and the
#: section may be a bare number or dotted.
_CITE = re.compile(
    r"(?P<doc>docs/[\w./-]+\.md)`?\s*§\s*(?P<sec>\d+(?:\.\d+)*)"
    r"(?P<title>\s*,\s*[\"“]([^\"”]{3,90})[\"”])?"
)

#: `### 3.2 The NDA discriminator + lock signal (canonical definition)`
_HEAD = re.compile(
    r"^#{1,6}\s+(?P<sec>\d+(?:\.\d+)*)\.?\s+(?P<name>.+?)\s*$", re.M
)

SEARCH = (
    "native/inc",
    "native/src",
    "native/tests",
    "native/validation",
    "docs",
    "scripts",
    "src/doppler",
)
SUFFIX = {".h", ".c", ".md", ".py"}


def _norm(text: str) -> str:
    """Collapse a citation title to one line, comment leaders removed.

    A title long enough to be worth quoting wraps, and in a C header every
    continuation line starts with ` * `. Comparing the raw capture then fails
    on whitespace rather than on the thing being checked — which would make
    the gate unusable in exactly the files that cite most.
    """
    return (
        " ".join(re.sub(r"\n\s*[*#]?\s*", " ", text).split()).strip().lower()
    )


def headings(doc: pathlib.Path) -> dict[str, str]:
    if not doc.is_file():
        return {}
    return {
        m.group("sec"): m.group("name")
        for m in _HEAD.finditer(doc.read_text(encoding="utf-8"))
    }


def main() -> int:
    cache: dict[pathlib.Path, dict[str, str]] = {}
    bad: list[str] = []
    n_cites = n_titled = 0

    files: list[pathlib.Path] = []
    for d in SEARCH:
        base = ROOT / d
        if base.is_dir():
            files += [p for p in base.rglob("*") if p.suffix in SUFFIX]

    me = pathlib.Path(__file__).resolve()
    for f in sorted(set(files)):
        if f.resolve() == me:  # this file's own example is documentation
            continue
        try:
            text = f.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        for m in _CITE.finditer(text):
            doc = ROOT / m.group("doc")
            sec = m.group("sec")
            line = text.count("\n", 0, m.start()) + 1
            where = f"{f.relative_to(ROOT)}:{line}"
            if doc not in cache:
                cache[doc] = headings(doc)
            heads = cache[doc]
            if not heads:
                bad.append(
                    f"  {where}: cites {m.group('doc')}, which has no "
                    f"numbered sections (or does not exist)"
                )
                continue
            n_cites += 1
            if sec not in heads:
                near = ", ".join(sorted(heads)[:6])
                bad.append(
                    f"  {where}: {m.group('doc')} has no §{sec}"
                    f"  (it has {near}, …)"
                )
                continue
            if m.group("title"):
                n_titled += 1
                want = _norm(m.group(4))
                got = heads[sec]
                if want not in _norm(got):
                    bad.append(
                        f'  {where}: §{sec} is "{got}",\n'
                        f'      not "{_norm(m.group(4))}" — the number or '
                        f"the title is "
                        f"wrong, and a reader who follows it lands on the "
                        f"wrong argument"
                    )

    if bad:
        print("check_doc_sections: a citation points somewhere it should not.")
        print(
            "  Prose that cites a section by number is a claim like any "
            "other.\n"
        )
        print("\n".join(bad))
        return 1
    print(
        f"check_doc_sections: OK — {n_cites} section citation(s) resolve, "
        f"{n_titled} of them title-checked"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
