#!/usr/bin/env python3
"""Gate: every ``curl`` in the CI image and the workflows says ``--fail``.

Without ``-f``/``--fail``, curl treats an HTTP error as a successful transfer:
it writes the error or rate-limit page where the file should go and exits 0.
The failure then surfaces somewhere else, naming the wrong thing. On
2026-10-01 the CI image build failed at ``tar`` with "gzip: stdin: not in
gzip format", on a nats-server URL that served a valid tarball before and
after (doppler#1738). Piped into ``bash``, the same error page is executed.

The rule: a ``curl`` invocation under ``deploy/docker/`` or ``.github/``
carries ``--fail`` (or ``--fail-with-body``, or ``f`` inside a short-flag
cluster such as ``-fsSL``). An invocation is ``curl`` at a command position:
the start of a line or a ``RUN``, or after ``;``, ``&``, ``|``, ``(``,
``$(``, ``if``, ``then``, ``do``, ``while``, ``!`` or ``exec``. So
``apt-get install ... curl git`` installs a package and is not one.
Comment lines are skipped, and a backslash continuation is joined first, so
flags on the next line count.

Usage:  python3 scripts/check_curl_fail.py [--root DIR]
Exit 0 when every curl invocation in scope fails on an HTTP error.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

#: Where the rule applies, relative to the root.
#: ``docker`` is the shared CI image's extension point, docker/ci-extra.sh.
SCOPES = ("deploy/docker", "docker", ".github")

#: ``curl`` where a command starts. Group 1 is everything after it.
_INVOKE = re.compile(
    r"(?:^|[;&|(]|\$\(|\b(?:RUN|if|then|do|while|exec)\s|!\s)\s*curl\b(.*)"
)
#: Where one command ends and the next begins.
_END = re.compile(r"\s(?:\|\|?|&&|;)\s|\)")


def _logical_lines(text: str) -> list[tuple[int, str]]:
    """Comment-free lines with backslash continuations joined.

    Each entry is (1-based number of the first physical line, joined text).
    """
    out: list[tuple[int, str]] = []
    buf, start = "", 0
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.rstrip()
        if not buf and line.lstrip().startswith("#"):
            continue
        if not buf:
            start = n
        if line.endswith("\\"):
            buf += line[:-1] + " "
            continue
        out.append((start, buf + line))
        buf = ""
    if buf:
        out.append((start, buf))
    return out


def _fails_on_http_error(args: str) -> bool:
    """Whether a curl argument string asks it to fail on an HTTP error."""
    args = _END.split(args, maxsplit=1)[0]
    for tok in args.split():
        if tok in ("--fail", "--fail-with-body"):
            return True
        if re.fullmatch(r"-[A-Za-z]*f[A-Za-z]*", tok):
            return True
    return False


def offenders(root: Path) -> tuple[list[str], int]:
    """(``path:line: command`` for each curl without --fail, files read)."""
    bad: list[str] = []
    files = sorted(
        f for scope in SCOPES for f in (root / scope).rglob("*") if f.is_file()
    )
    for f in files:
        try:
            text = f.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        for n, line in _logical_lines(text):
            for m in _INVOKE.finditer(line):
                if not _fails_on_http_error(m.group(1)):
                    cmd = " ".join(("curl" + m.group(1)).split())[:100]
                    bad.append(f"{f.relative_to(root).as_posix()}:{n}: {cmd}")
    return bad, len(files)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    root = ap.parse_args().root.resolve()
    bad, n = offenders(root)
    if n == 0:
        print("check_curl_fail: no files in scope -- nothing was checked")
        return 1
    for b in bad:
        print(f"check_curl_fail: {b}")
    if bad:
        print(
            "\n  A curl without --fail saves an HTTP error page as the file "
            "and exits 0.\n  Use `curl -fsSL --retry 5 --retry-all-errors "
            "--retry-delay 2`, and\n  download an installer to a file before "
            "running it (doppler#1738)."
        )
        return 1
    print(
        f"check_curl_fail: OK -- {n} file(s), every curl fails on HTTP error"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
