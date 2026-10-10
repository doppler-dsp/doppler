#!/usr/bin/env python3
"""Every committed gallery plot has a script that ``make gallery`` runs.

``make gallery`` used to hold two lists for one thing: ``GALLERY_SCRIPTS``
(the scripts it runs, and what ``release-freshness-check`` reads) and a
hand-kept list of PNG names to move into ``docs/assets/``. They disagreed.
``plan_background_demo.png`` was moved on every run while its script was
never run, so the plot went stale where no gate could see it, and v0.59.0
shipped a picture of a defect #1639 had already fixed (#1644).

This script is now the one reader of both directions:

``--outputs SCRIPT...``
    Print the PNG each script writes. ``make gallery`` moves exactly these,
    so the move list is derived from ``GALLERY_SCRIPTS`` and cannot disagree
    with it.

``SCRIPT...`` (the check, on ``make lint``)
    Fail when a listed script is missing or names no PNG (it would run and
    its plot would be lost), and when an example under
    ``src/doppler/examples/`` names a PNG that is committed under
    ``docs/assets/`` but is not in ``GALLERY_SCRIPTS`` -- the committed plot
    then has no target that re-renders it. That second direction is the one
    no derivation can close, because the missing script is the one nobody
    wrote down.

``--narrow SCRIPT --characterizations SUBJECT...`` (also on ``make lint``)
    Hold ``make gallery GALLERY_SCRIPTS=SCRIPT`` to SCRIPT alone (#2058). A
    narrowed run used to re-render ``dsss_acq_characterization.png`` too, an
    unrelated diff that is easy to commit by accident. Read from two dry
    runs, so nothing executes:

    - the narrowed run may MENTION no PNG but SCRIPT's plots and no Python
      file but SCRIPT and this one, and it must INVOKE SCRIPT;
    - the default run must INVOKE every characterization subject, and at
      least one must be given, so deleting the step from every run, or
      emptying the list, is not a pass;
    - ``make -n release-freshness-check`` must be handed every subject. A
      full ``make gallery`` is now the only run that renders one, so the
      tag-time gate is what notices a changed subject left unrendered.

    "Mention" is any path in a command; "invoke" is ``python <path>``, with
    a shell loop's ``$var`` resolved to the words it iterates over. The
    asymmetry is deliberate: what must not happen is caught by a mere
    mention, while what must happen needs the command itself, because a
    ``printf`` label or a ``for`` list also names the path. A recipe comment
    that ``make -n`` echoes is not a command and is skipped.

A script's plot is every quoted ``"<name>.png"`` literal in its source --
``savefig("x.png")``, ``out_path="x.png"``, ``main(out="x.png")`` all
qualify. Quoted only: a usage line such as ``[out.png]`` is prose.

There is no waiver. The 19 orphans found when the gate landed were a
ratchet in ``scripts/.gallery-orphans-allow`` until #1647 drained it; at zero
the file and its reader were deleted rather than kept, because an empty
waiver list is only an invitation to waive the next orphan instead of
rendering it.

Usage
-----
``make gallery-scripts-check`` -- ``GALLERY_SCRIPTS`` is passed in rather
than restated here: a second list is one that can disagree with the first.
"""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXAMPLES = "src/doppler/examples"
ASSETS = "docs/assets"

#: A quoted PNG file name: the plot a script writes by default.
PNG_RE = re.compile(r"""["']([A-Za-z0-9][A-Za-z0-9_.-]*\.png)["']""")

#: In a dry run: a path to a PNG it writes or moves, or to a script it runs.
DRY_PNG = re.compile(r"[\w./-]+\.png\b")
DRY_PY = re.compile(r"[\w./-]+\.py\b")

#: A shell loop in a dry run: its variable, and the words it iterates over.
LOOP = re.compile(r"\bfor\s+(\w+)\s+in\s+([^;]*);")
#: What a dry run hands to python: a path, or a loop variable.
RUN = re.compile(r"\bpython3?\s+(\S+)")
#: A loop variable as a word: `$v`, `${v}`, either one quoted.
VAR = re.compile(r'"?\$\{?(\w+)\}?"?')

#: This script, which `make gallery` runs to derive its move list.
HELPER = f"scripts/{pathlib.Path(__file__).name}"

#: The tag-time gate that refuses a changed gallery script whose plot was
#: not re-rendered; a characterization subject must reach it too.
FRESHNESS = "release-freshness-check"


def plots(path: pathlib.Path) -> list[str]:
    """The PNG names *path* writes, in order of first mention, unique."""
    seen: dict[str, None] = {}
    for m in PNG_RE.finditer(path.read_text(encoding="utf-8")):
        seen.setdefault(m.group(1), None)
    return list(seen)


def check(repo: pathlib.Path, listed: list[str]) -> int:
    rc = 0
    for s in listed:
        p = repo / s
        if not p.is_file():
            rc = 1
            print(f"gallery-scripts: {s} is in GALLERY_SCRIPTS but missing")
        elif not plots(p):
            rc = 1
            print(
                f"gallery-scripts: {s} is in GALLERY_SCRIPTS but names no "
                '"<name>.png" -- `make gallery` would run it and move nothing'
            )

    committed = {p.name for p in (repo / ASSETS).glob("*.png")}
    orphans: dict[str, list[str]] = {}
    for ex in sorted((repo / EXAMPLES).glob("*.py")):
        rel = ex.relative_to(repo).as_posix()
        if rel in listed:
            continue
        mine = [png for png in plots(ex) if png in committed]
        if mine:
            orphans[rel] = mine

    if orphans:
        rc = 1
        print(
            f"gallery-scripts: {len(orphans)} example(s) name a plot "
            f"committed under {ASSETS}/ but are not in GALLERY_SCRIPTS:"
        )
        for s, pngs in orphans.items():
            print(f"    {s}  ->  {', '.join(pngs)}")
        print(
            "  Nothing re-renders that plot, so it goes stale unseen and the\n"
            "  release freshness gate cannot see it either (#1644). Add the\n"
            "  script to GALLERY_SCRIPTS in the Makefile."
        )

    if rc == 0:
        print(f"gallery-scripts: OK -- {len(listed)} scripts, no orphans")
    return rc


def dry_run(repo: pathlib.Path, target: str, *make_args: str) -> list[str]:
    """The commands ``make <target>`` would run, executing nothing.

    Parameters
    ----------
    repo : pathlib.Path
        The tree whose Makefile is read.
    target : str
        ``gallery``, or ``release-freshness-check``.
    *make_args : str
        Added to make's command line, e.g. ``GALLERY_SCRIPTS=<one>``.

    Returns
    -------
    list of str
        One entry per command. ``make -n`` prints a line continued with a
        backslash as several lines, so those are joined first; it also
        echoes a tab-indented ``# ...`` recipe comment, which is dropped,
        so a path a comment names is neither a destination nor a run.

    Notes
    -----
    Make's own variables are dropped from the environment: run under
    ``make lint``, make passes its command line to children in MAKEFLAGS,
    and a GALLERY_SCRIPTS there would narrow the run read as the default.
    """
    drop = {"MAKEFLAGS", "MFLAGS", "MAKELEVEL"}
    r = subprocess.run(
        ["make", "-n", "--no-print-directory", target, *make_args],
        cwd=repo,
        env={k: v for k, v in os.environ.items() if k not in drop},
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    if r.returncode != 0:
        raise SystemExit(
            f"gallery-scripts: `make -n {target}` failed\n{r.stderr}"
        )
    joined = r.stdout.replace("\\\n", " ")
    return [
        ln
        for ln in joined.splitlines()
        if ln.strip() and not ln.lstrip().startswith("#")
    ]


def invoked(commands: list[str]) -> set[str]:
    """Every path *commands* hand to python, loop variables resolved.

    ``for script in a.py b.py; do uv run python $script; done`` invokes
    ``a.py`` and ``b.py``: the argument is a variable, so it is replaced by
    the words its loop iterates over. A variable no loop sets resolves to
    nothing, which can only make a required run look absent, never present.
    """
    text = "\n".join(commands)
    loops: dict[str, list[str]] = {}
    for var, words in LOOP.findall(text):
        loops.setdefault(var, []).extend(words.split())
    out: set[str] = set()
    for arg in RUN.findall(text):
        m = VAR.fullmatch(arg)
        out.update(loops.get(m.group(1), []) if m else [arg])
    return out


def check_narrow(
    repo: pathlib.Path, script: str, characterizations: list[str]
) -> int:
    """Hold ``make gallery GALLERY_SCRIPTS=<script>`` to that script alone.

    Two dry runs. The narrowed one may mention a PNG only if *script*
    writes it, and a Python file only if it is *script* or this helper, and
    it must invoke *script*, so a narrowed run that does nothing is not a
    pass. The default one must invoke every characterization subject, and
    an empty list is refused, so a fix that deletes the step from every run
    is not a pass either. Each subject must also reach
    ``release-freshness-check``, the only gate that sees it go stale.

    Returns
    -------
    int
        0 when every condition holds, 1 otherwise; each failure is printed.
    """
    if not characterizations:
        print(
            "gallery-scripts: --narrow got no characterization subjects, so "
            "the default run would be checked for nothing. Pass "
            "GALLERY_CHARACTERIZATIONS (is it empty, or renamed?)."
        )
        return 1

    rc = 0
    narrowed = dry_run(repo, "gallery", f"GALLERY_SCRIPTS={script}")
    text = "\n".join(narrowed)
    own = set(plots(repo / script))
    stray = sorted(
        {p for p in DRY_PNG.findall(text) if pathlib.Path(p).name not in own}
        | (set(DRY_PY.findall(text)) - {script, HELPER})
    )
    if stray:
        rc = 1
        print(
            f"gallery-scripts: `make gallery GALLERY_SCRIPTS={script}` "
            "names what that script does not write or run:"
        )
        for p in stray:
            print(f"    {p}")
        print(
            "  A narrowed run must render only what it names, or it leaves\n"
            "  an unrelated plot changed in the tree (#2058)."
        )
    if script not in invoked(narrowed):
        rc = 1
        print(f"gallery-scripts: a narrowed run does not run {script}")

    runs = invoked(dry_run(repo, "gallery"))
    # Only the full run renders a characterization now, so the tag-time
    # freshness gate is what notices a changed subject with its plot left
    # unrendered. It must be handed every subject to notice it.
    fresh = " ".join(dry_run(repo, FRESHNESS, "VERSION=0.0.0")).split()
    for c in characterizations:
        if c not in runs:
            rc = 1
            print(
                f"gallery-scripts: the default `make gallery` no longer "
                f"runs {c}"
            )
        if c not in fresh:
            rc = 1
            print(f"gallery-scripts: `make {FRESHNESS}` is not given {c}")

    if rc == 0:
        print(
            f"gallery-scripts: OK -- GALLERY_SCRIPTS={script} names only its "
            f"own plots; the default run renders {len(characterizations)} "
            f"characterization(s), and {FRESHNESS} reads them"
        )
    return rc


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--repo",
        default=str(ROOT),
        help="repo root (a test points this at its fixture)",
    )
    ap.add_argument(
        "--outputs",
        action="store_true",
        help="print each script's PNG names instead of checking",
    )
    ap.add_argument(
        "--narrow",
        metavar="SCRIPT",
        help="check a run narrowed to SCRIPT, from make's dry run",
    )
    ap.add_argument(
        "--characterizations",
        nargs="*",
        default=[],
        help="with --narrow: the subjects the default run must still render",
    )
    ap.add_argument(
        "scripts", nargs="*", help="the Makefile's GALLERY_SCRIPTS"
    )
    args = ap.parse_args(argv)
    repo = pathlib.Path(args.repo)

    if args.narrow:
        return check_narrow(repo, args.narrow, args.characterizations)

    if args.outputs:
        for s in args.scripts:
            for png in plots(repo / s):
                print(png)
        return 0
    return check(repo, args.scripts)


if __name__ == "__main__":
    raise SystemExit(main())
