#!/usr/bin/env python3
"""A `docs/x.md §N` citation must name a section that is really there.

Prose across this tree cites design sections by number in the hundreds, and
nothing checked any of them. Renumbering a document, or citing from memory,
leaves a reference that reads authoritative and points at the wrong argument
— the reader who follows it concludes the claim is unsupported.

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

## The three shapes a citation is written in

The first version of this gate recognised one shape — a `docs/…md` path with
`§N` directly after it — and reported "OK, 135 citations resolve" while a
third as many again sat unexamined beside them. That is the worse failure of
the two: a gate that names a number the reader trusts, over a set it chose.
`async-dsss-receiver.md` was split and its §12 moved to the companion
measurements page; the citations that broke were found by grep, not here.

So all three are recognised now, and a citation is a `(document, §number)`
pair however it is spelled:

1. **Path first** — ``docs/design/mpsk.md §3.2``, or ``… section 3.2``, and
   the number may be on the next line of a comment (``mpsk.md`` then
   `` * §3.2``): the async receiver's headers and validators spell it both
   ways, and the first version saw neither.
2. **A continuation** — ``…mpsk.md §3.4 and §3.5``, ``§7.1, §8``,
   ``§8.2/§8.3``. The second number inherits the first's document, and
   only across a joiner (`,` `;` `/` `and`), never across plain prose, so a
   page's own ``§4.1 and §4.2`` self-reference stays unclaimed.
3. **A markdown link** — ``[design §9.5](../design/mpsk.md)``, where the
   path comes *after* the number and is usually relative to the citing file.

Relative targets resolve against the citing file, which is what lets
``docs/design/*.md`` cite each other by bare filename — the spelling those
pages actually use.

## Which files are scanned

Every text file git tracks. The list this replaced named seven directories;
`native/benchmarks` and `native/examples` were not among them, so a benchmark
citing a section that had since moved was checked by nobody — found by
sabotaging a real citation and watching the gate stay green, which is the
only way an inclusion list's gaps ever are found. The suffix list that
followed (`.h .c .md .py`) was the same mistake one level down:
`native/validation/CMakeLists.txt` cited seven sections of a page that no
longer had them, and nothing read it. What counts as ours is already
recorded, once, by git, and a released claim in `CHANGELOG.md` cites a
section like any other.

## What it cannot check

A number that resolves is not a fact that survives. When an entry is cut
down and keeps its heading, every citation of it still resolves, while the
number it was cited FOR (a cost, a table row) may be gone from under it.
Only a reader who follows the citation sees that; a title, where one is
given, narrows it, and a cut that removes a cited number should search for
its citers (`git grep "§12.14"`) before it lands. Nor are these read:

- a range's upper end: ``§12.22-12.24`` checks §12.22 only;
- a continuation without its own mark: ``§12.15, 12.16`` checks §12.15;
- the step form: ``§12 step 7`` is read as §12, the step ignored;
- a lettered section: ``§12.3a`` is read as §12.3;
- a bare ``§N`` or ``section N`` with no page and no ``design`` before it,
  which in a file that names its page once is shorthand, and in any other
  is unanchored: name the page.

## A citation that names no page

``design §12.22`` and ``design section 12.22`` claim a design page without
saying which, so no gate can check them, and the async receiver's split
moved its §12 to another page while ~50 of them still said "design". They
are refused unless the file is in `scripts/.doc-section-bare-exempt`, a
ratchet of the ones that already existed: a file's count may only fall, and
a count that has fallen must be lowered in the list. Name the page —
``docs/design/<page>.md §N`` — and the citation is checked like the rest.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

#: `docs/design/mpsk.md` §3.2, "Optional title"  — backticks optional, the
#: section a bare number or dotted, `§` or the word `section`, and a comment
#: leader allowed between them (a path that ends one line of a C comment and
#: a `§` that starts the next).
_CITE = re.compile(
    r"(?P<doc>docs/[\w./-]+\.md)`?(?:\s|\*|#|//)*"
    r"(?:§|\b(?i:sections?)\b)(?:\s|\*|#|//)*(?P<sec>\d+(?:\.\d+)*)"
    r"(?P<title>\s*,\s*[\"“]([^\"”]{3,90})[\"”])?"
)

#: `design §12.22`, `design section 12.22`: a design page, unnamed.
_BARE = re.compile(
    r"\b(?i:design)(?:\s|\*|#|//)+(?:§|(?i:sections?)\b)(?:\s|\*|#|//)*"
    r"(?P<sec>\d+(?:\.\d+)*)"
)

#: The ratchet of unnamed citations that predate the rule: `<count> <path>`.
BARE_EXEMPT = pathlib.Path("scripts/.doc-section-bare-exempt")

#: The continuation: a second number sharing the first citation's document.
#: A JOINER is required — `,` `;` `/` or the word "and" — so that a page's
#: own `§4.1 and §4.2` self-reference, which names no document, is not
#: swept up, and so that two unrelated sentences never chain. The class
#: admits `*` and `#` because a C block comment and a markdown blockquote
#: both put a leader at the head of a wrapped line.
_TAIL = re.compile(
    r"(?P<sep>[\s,;/*#]*(?:and[\s,;/*#]*)?)§\s*(?P<sec>\d+(?:\.\d+)*)"
)

#: How far a continuation may reach. Long enough for `, ` and a wrapped
#: line's comment leader; far too short to cross a paragraph.
_TAIL_SPAN = 24

#: `[design §9.5](../design/mpsk.md)` — the number is in the link LABEL and
#: the path follows it, usually relative to the citing file. A label may
#: carry more than one (`[…'s §8.2/§8.3](x.md)`), so the numbers are pulled
#: from the label separately.
_LINK = re.compile(
    r"\[(?P<label>[^\]]*§[^\]]*)\]\((?P<doc>[^)\s]+?\.md)(?:#[^)\s]*)?\)"
)

#: A bare `§N` inside a link label.
_SEC = re.compile(r"§\s*(?P<sec>\d+(?:\.\d+)*)")

#: `### 3.2 The NDA discriminator + lock signal (canonical definition)`
_HEAD = re.compile(
    r"^#{1,6}\s+(?P<sec>\d+(?:\.\d+)*)\.?\s+(?P<name>.+?)\s*$", re.M
)

#: A Python file that names this script holds citations as SPECIMENS rather
#: than as claims: this script's own docstring, whose worked examples cite
#: sections that are deliberately wrong, and the test that seeds one of every
#: shape to prove the gate goes red. Derived rather than listed, so a second
#: such test is covered the moment it exists — and this file is covered by
#: the line below, which contains the mark it defines.
#:
#: Restricted to `.py` deliberately. A documentation page is free to name
#: this gate and to cite sections in the same breath, and must not be able
#: to silence itself by doing so.
SPECIMEN_MARK = "check_doc_sections"


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


def resolve(
    doc: str, citing: pathlib.Path, root: pathlib.Path
) -> pathlib.Path:
    """The document a citation names, as a path on disk.

    A `docs/…` spelling is repository-relative, which is how a C header or
    the changelog names a page. Anything else is relative to the file doing
    the citing — the spelling `docs/design/*.md` pages use for each other,
    and the reason the markdown-link shape needs resolving at all rather
    than a prefix match. The exception is a `changelog.d/` fragment, which is
    read from the root `CHANGELOG.md` it is assembled into.
    """
    if doc.startswith("docs/"):
        return root / doc
    # A changelog fragment is read where `changelog-assemble` puts it, in the
    # root CHANGELOG.md, so its links resolve from there (#2571).
    home = citing.parent
    if citing.resolve().is_relative_to((root / "changelog.d").resolve()):
        home = root
    return (home / doc).resolve()


def citations(
    text: str, citing: pathlib.Path
) -> list[tuple[str, str, str | None, int]]:
    """Every `(document, section, title, offset)` this file claims.

    Offsets are of the `§` itself, and are what deduplicates the shapes:
    a link label that also happens to match the path-first form is one
    citation, counted once, not two.
    """
    out: dict[int, tuple[str, str, str | None, int]] = {}

    for m in _CITE.finditer(text):
        doc = m.group("doc")
        out[m.start("sec")] = (doc, m.group("sec"), m.group(4), m.start())
        # The continuation run: each number inherits `doc`, and each is the
        # anchor for the next, so `§1, §2, §3` resolves all three.
        pos = m.end("title") if m.group("title") else m.end("sec")
        while (tail := _TAIL.match(text, pos)) is not None:
            sep = tail.group("sep")
            if len(sep) > _TAIL_SPAN or not re.search(r"[,;/]|and", sep):
                break
            out[tail.start("sec")] = (
                doc,
                tail.group("sec"),
                None,
                tail.start(),
            )
            pos = tail.end("sec")

    for m in _LINK.finditer(text):
        doc = m.group("doc")
        if "://" in doc:
            continue
        base = m.start("label")
        for s in _SEC.finditer(m.group("label")):
            out[base + s.start("sec")] = (
                doc,
                s.group("sec"),
                None,
                base + s.start(),
            )

    return [out[k] for k in sorted(out)]


def headings(doc: pathlib.Path) -> dict[str, str]:
    if not doc.is_file():
        return {}
    return {
        m.group("sec"): m.group("name")
        for m in _HEAD.finditer(doc.read_text(encoding="utf-8"))
    }


def source_files(root: pathlib.Path) -> list[pathlib.Path]:
    """Every file of ours that could carry a citation.

    What counts as "ours" is asked of git rather than listed here. The list
    this replaced named seven directories, and `native/benchmarks` and
    `native/examples` were not among them — so a benchmark citing a section
    that had since moved to another page was never checked. That was found
    by sabotaging a real citation and watching the gate stay green, which is
    the only way an inclusion list's gaps ever are found.

    A tree that is not a git checkout — the seeded one the gate's own test
    builds — is walked instead.
    """
    try:
        out = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z"],
            capture_output=True,
            check=True,
        ).stdout.decode("utf-8", "replace")
        names = [n for n in out.split("\0") if n]
    except (OSError, subprocess.CalledProcessError, UnicodeError):
        names = []
    paths = [root / n for n in names] if names else list(root.rglob("*"))
    return [p for p in paths if p.is_file()]


def _claimed(text: str) -> set[int]:
    """Offsets of every `§N` number a link label already ties to a page."""
    return {
        m.start("label") + s.start("sec")
        for m in _LINK.finditer(text)
        for s in _SEC.finditer(m.group("label"))
    }


def bare(text: str, claimed: set[int]) -> list[int]:
    """Offsets of the `design §N` citations that name no page.

    A number some other shape already claimed is not bare: the label of
    `[design §3.2](widget.md)` names its page in the link.
    """
    return [
        m.start()
        for m in _BARE.finditer(text)
        if m.start("sec") not in claimed
    ]


def exempt(root: pathlib.Path) -> dict[str, int]:
    """The ratchet: each listed file's allowed count of unnamed citations."""
    path = root / BARE_EXEMPT
    if not path.is_file():
        return {}
    out: dict[str, int] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            n, name = line.split(None, 1)
            out[name] = int(n)
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--root",
        type=pathlib.Path,
        default=ROOT,
        help="tree to scan; the gate's own test points this at a seeded one, "
        "so that it is proven to go red rather than observed to be green",
    )
    root = ap.parse_args(argv).root.resolve()

    cache: dict[pathlib.Path, dict[str, str]] = {}
    bad: list[str] = []
    n_cites = n_titled = 0

    files = source_files(root)
    if not files:
        print(
            "check_doc_sections: FAIL — no files to scan; the scan matched "
            "nothing, so it did not run, so it has not passed"
        )
        return 1

    allowed = exempt(root)
    n_bare: dict[str, list[str]] = {}
    for f in sorted(set(files)):
        try:
            text = f.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        if f.suffix == ".py" and SPECIMEN_MARK in text:
            continue
        if f.relative_to(root) == BARE_EXEMPT:
            continue
        cites = citations(text, f)
        rel = f.relative_to(root).as_posix()
        hits = bare(text, _claimed(text))
        if hits:
            n_bare[rel] = [
                f"{rel}:{text.count(chr(10), 0, at) + 1}" for at in hits
            ]
        for name, sec, title, at in cites:
            doc = resolve(name, f, root)
            line = text.count("\n", 0, at) + 1
            where = f"{f.relative_to(root).as_posix()}:{line}"
            if doc not in cache:
                cache[doc] = headings(doc)
            heads = cache[doc]
            if not heads:
                bad.append(
                    f"  {where}: cites {name}, which has no "
                    f"numbered sections (or does not exist)"
                )
                continue
            n_cites += 1
            if sec not in heads:
                near = ", ".join(sorted(heads)[:6])
                bad.append(
                    f"  {where}: {name} has no §{sec}  (it has {near}, …)"
                )
                continue
            if title:
                n_titled += 1
                want = _norm(title)
                got = heads[sec]
                if want not in _norm(got):
                    bad.append(
                        f'  {where}: §{sec} is "{got}",\n'
                        f'      not "{want}" — the number or '
                        f"the title is "
                        f"wrong, and a reader who follows it lands on the "
                        f"wrong argument"
                    )

    for rel, where in sorted(n_bare.items()):
        ok = allowed.get(rel, 0)
        if len(where) > ok:
            bad.append(
                f"  {rel}: {len(where)} citation(s) say `design §N` without "
                f"naming the page ({ok} allowed by {BARE_EXEMPT}); name it, "
                f"docs/design/<page>.md §N:\n      " + "\n      ".join(where)
            )
    for rel, ok in sorted(allowed.items()):
        have = len(n_bare.get(rel, []))
        if have < ok:
            bad.append(
                f"  {BARE_EXEMPT}: {rel} is allowed {ok} unnamed "
                f"citation(s) and has {have}; lower its entry (the list "
                f"only shrinks)"
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
    sys.exit(main(sys.argv[1:]))
