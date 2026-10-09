#!/usr/bin/env python3
"""Gate: a stock image comes through STOCK_REGISTRY, never Docker Hub by name.

CI pulled stock distro images (debian, ubuntu, python, almalinux, fedora,
nats) from Docker Hub by bare name, anonymously. With several PRs in flight
that hits Docker Hub's unauthenticated rate limit, and the Docker, glibc 2.28
and Linux-packages legs of unrelated PRs failed with ``429 Too Many
Requests`` -- a red ``CI passed`` that said nothing about the code
(doppler#1950).

The fix has one home: the Makefile's ``STOCK_REGISTRY`` (AWS's ECR Public
mirror of the Docker Official Images: no credentials, the same digests).
Dockerfiles take it as ``ARG STOCK_REGISTRY`` with no default, the Makefile
and scripts as ``$(STOCK_REGISTRY)`` / ``${STOCK_REGISTRY}``. This gate
refuses any other way of naming a stock image.

Where it looks (registration-free: every tracked file of these kinds):

- **Dockerfiles** (``Dockerfile``, ``Dockerfile.*``, ``*.Dockerfile``):
  every ``FROM``, every ``COPY --from=`` that is not a stage, the default of
  any ``ARG`` a ``FROM`` names whole (``FROM ${BUILD_BASE}``), and a
  ``# syntax=`` parser directive -- BuildKit pulls that frontend from Docker
  Hub too, which is why doppler's Dockerfiles carry none.
- **Makefiles, shell scripts and workflows**: the image argument of every
  ``docker run|create|pull`` and every ``uses: docker://``. A make variable
  is resolved through the Makefile's assignments, a ``for d in $(LIST)``
  loop variable through its list, and a shell variable through its
  assignments in the same file. A variable none of those resolves is
  opaque -- an argument the gate cannot see is not reported -- and a
  variable whose every value is an option (``net=--network=none``) is
  stepped over like the option it is.

A reference is fine when it starts with the ``STOCK_REGISTRY`` variable,
names a registry host other than Docker Hub's (ghcr.io pins, quay.io's
manylinux, public.ecr.aws), names the repo's OWN image (its first path
component starts with ``--own-prefix``, the Makefile's DOCKER_IMAGE), or is
an earlier build stage. Anything else -- ``debian:stable``,
``library/nats``, ``docker.io/library/ubuntu:24.04`` -- is a Docker Hub
pull and fails.

Files ``--vendored`` names (standard.mk and its VENDORED_FILES) are held
verbatim to canonical, so a fix there belongs upstream; their references are
listed but do not fail. Not parsed: an inline Dockerfile piped to
``docker build -`` (the one in the tree takes ``$(PKG_IMAGE)``, quay.io).

Usage:  python3 scripts/check_stock_images.py [--root DIR]
            [--own-prefix NAME] [--vendored "FILE ..."]
Exit 0 when every stock image in scope comes through the registry variable.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VAR = "STOCK_REGISTRY"

_DOCKER_HUB = frozenset(
    {
        "docker.io",
        "index.docker.io",
        "registry-1.docker.io",
        "registry.hub.docker.com",
    }
)
#: Not scanned: prose, foreign code, and this gate and its test, which spell
#: out the forms they refuse.
_SKIP_PREFIXES = ("vendor/", "docs/")
_SKIP_FILES = (
    "scripts/check_stock_images.py",
    "src/doppler/tests/test_stock_images_gate.py",
)

#: An image reference: optional registry host, path, tag and/or digest.
_IMAGE = re.compile(
    r"(?:[A-Za-z0-9.-]+(?::[0-9]+)?/)?"
    r"[a-z0-9]+(?:[._-][a-z0-9]+)*(?:/[a-z0-9]+(?:[._-][a-z0-9]+)*)*"
    r"(?::[A-Za-z0-9_][A-Za-z0-9_.-]{0,127})?(?:@sha256:[0-9a-f]{64})?"
)
_READS_VAR = re.compile(
    r"^\$(?:\(" + VAR + r"\)|\{" + VAR + r"\b|" + VAR + r"\b)"
)
_PURE_VAR = re.compile(
    r"^\$(?:\$?)(?:\{([A-Za-z_]\w*)\}|\(([A-Za-z_]\w*)\)|([A-Za-z_]\w*))$"
)
_MAKE_REF = re.compile(r"\$[({]([A-Za-z_][A-Za-z0-9_.-]*)[)}]")
_DOCKER = re.compile(
    r"\bdocker\s+(?:container\s+|image\s+)?(?:run|create|pull)\b(.*)"
)
_USES_DOCKER = re.compile(r"\buses:\s*['\"]?docker://([^\s'\"]+)")
_FOR = re.compile(r"\bfor\s+([A-Za-z_]\w*)\s+in\s+(.*?)\s*;\s*do\b")
_DIRECTIVE = re.compile(r"^#\s*([A-Za-z]+)\s*=\s*(\S+)\s*$")
#: ``docker run`` options that consume the next token as their value.
_VALUE_OPTS = frozenset(
    [
        "-v",
        "--volume",
        "-w",
        "--workdir",
        "-e",
        "--env",
        "--env-file",
        "-u",
        "--user",
        "--name",
        "-p",
        "--publish",
        "--network",
        "--net",
        "--platform",
        "--entrypoint",
        "--mount",
        "-m",
        "--memory",
        "--cpus",
        "--label",
        "-l",
        "--hostname",
        "-h",
        "--add-host",
        "--device",
        "--cap-add",
        "--cap-drop",
        "--security-opt",
        "--tmpfs",
        "--ulimit",
        "--log-driver",
        "--log-opt",
        "--gpus",
        "--shm-size",
        "--restart",
        "--pid",
        "--ipc",
        "--uts",
        "--runtime",
        "--cidfile",
        "--stop-signal",
        "--stop-timeout",
        "--health-cmd",
        "--group-add",
        "--dns",
    ]
)


def _kind(rel: str) -> str | None:
    name = rel.rsplit("/", 1)[-1]
    if name == "Dockerfile" or name.startswith("Dockerfile."):
        return "dockerfile"
    if name.endswith((".Dockerfile", ".dockerfile")):
        return "dockerfile"
    if name == "Makefile" or name.endswith(".mk"):
        return "make"
    if rel.startswith(".github/") and name.endswith((".yml", ".yaml")):
        return "workflow"
    if name.endswith(".sh"):
        return "shell"
    return None


def _files(root: Path) -> list[str]:
    """Tracked files when ``root`` is a checkout, every file otherwise."""
    if (root / ".git").exists():
        out = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z"],
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        paths = [p for p in out.split("\0") if p]
    else:
        paths = [
            f.relative_to(root).as_posix()
            for f in root.rglob("*")
            if f.is_file() and ".git" not in f.parts
        ]
    return sorted(
        p
        for p in paths
        if not p.startswith(_SKIP_PREFIXES) and p not in _SKIP_FILES
    )


def _logical(text: str, comments: bool = False) -> list[tuple[int, str]]:
    """Lines with backslash continuations joined; comment lines dropped.

    Each entry is (1-based number of the first physical line, joined text).
    """
    out: list[tuple[int, str]] = []
    buf, start = "", 0
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.rstrip()
        if not buf and not comments and line.lstrip().startswith("#"):
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


def _tokens(s: str) -> list[str]:
    """Shell-ish words: quotes, ``$(...)`` and ``${{ ... }}`` kept whole,
    stopping where the command ends (``;``, ``&&``, ``||``, ``|``)."""
    toks: list[str] = []
    i, cur, n = 0, "", len(s)
    while i < n:
        c = s[i]
        if c in "'\"":
            j = s.find(c, i + 1)
            j = n - 1 if j < 0 else j
            cur += s[i + 1 : j]
            i = j + 1
            continue
        if s.startswith("${{", i):
            j = s.find("}}", i)
            j = n - 2 if j < 0 else j
            cur += s[i : j + 2]
            i = j + 2
            continue
        if s.startswith("$(", i) or s.startswith("${", i):
            depth, j = 0, i + 1
            while j < n:
                if s[j] in "({":
                    depth += 1
                elif s[j] in ")}":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            cur += s[i : j + 1]
            i = j + 1
            continue
        if c.isspace():
            if cur:
                toks.append(cur)
                cur = ""
            i += 1
            continue
        if c in ";|&)":
            break
        cur += c
        i += 1
    if cur:
        toks.append(cur)
    return toks


class MakeVars:
    """Simple-assignment make variables, enough to resolve image names."""

    _ASSIGN = re.compile(
        r"^(?:override\s+|export\s+)?([A-Za-z_][A-Za-z0-9_.-]*)\s*"
        r"(\?|:{1,3}|\+|!)?=\s*(.*)$"
    )

    def __init__(self, root: Path, files: list[str]) -> None:
        self.vals: dict[str, str] = {}
        for rel in files:
            if _kind(rel) != "make":
                continue
            text = (root / rel).read_text(encoding="utf-8")
            for _, line in _logical(text):
                m = self._ASSIGN.match(line)
                if not m or line.startswith(("\t", " ")):
                    continue
                name, op, val = m.group(1), m.group(2) or "", m.group(3)
                if op == "?" and name in self.vals:
                    continue
                if op == "+":
                    val = (self.vals.get(name, "") + " " + val).strip()
                self.vals[name] = val.strip()

    def expand(self, s: str, depth: int = 0) -> str:
        if depth > 8:
            return s

        def sub(m: re.Match[str]) -> str:
            name = m.group(1)
            if name == VAR or name not in self.vals:
                return m.group(0)
            return self.expand(self.vals[name], depth + 1)

        return _MAKE_REF.sub(sub, s)


def _verdict(ref: str, own: str) -> str | None:
    """Why ``ref`` is a Docker Hub pull, or None when it is fine."""
    if _READS_VAR.match(ref):
        return None
    if "$" in ref:
        return None  # still a variable after resolution: opaque
    if not _IMAGE.fullmatch(ref):
        return None
    first, _, rest = ref.partition("/")
    has_host = bool(rest) and (
        "." in first or ":" in first or first == "localhost"
    )
    if has_host and first not in _DOCKER_HUB:
        return None
    path = rest if has_host else ref
    if own and path.startswith(own) and not path.startswith("library/"):
        return None
    return "a Docker Hub pull"


def _shell_assignments(text: str, name: str) -> list[str]:
    pat = re.compile(
        r"(?:^|[\s;&|(])(?:local\s+|export\s+|readonly\s+)?"
        + re.escape(name)
        + r"=(\"[^\"]*\"|'[^']*'|[^\s;&|)]*)"
    )
    return [m.group(1).strip("'\"") for m in pat.finditer(text)]


def _image_candidates(
    args: str, line: str, text: str, mk: MakeVars | None
) -> list[str] | None:
    """The value(s) the image slot of one ``docker run`` can take.

    None when the slot is opaque (a variable nothing here resolves).
    """
    loops = {m.group(1): m.group(2) for m in _FOR.finditer(line)}
    toks = _tokens(args)
    i = 0
    while i < len(toks):
        tok = toks[i]
        if tok.startswith("-"):
            i += 2 if (tok in _VALUE_OPTS and "=" not in tok) else 1
            continue
        if tok.startswith("$(call ") or tok.startswith("$(shell "):
            i += 1  # a function call: expands to options, not the image
            continue
        if mk is not None:
            tok = mk.expand(tok)
        m = _PURE_VAR.match(tok)
        if m and not _READS_VAR.match(tok):
            name = m.group(1) or m.group(2) or m.group(3)
            if name in loops:
                words = loops[name]
                if mk is not None:
                    words = mk.expand(words)
                return words.split()
            values = _shell_assignments(text, name)
            if values and all(v == "" or v.startswith("-") for v in values):
                i += 1  # an option held in a variable
                continue
            images = [v for v in values if v and not v.startswith("-")]
            return images or None
        return [tok]
    return None


def _check_dockerfile(rel: str, text: str, own: str) -> tuple[list[str], int]:
    bad: list[str] = []
    seen = 0
    for n, raw in enumerate(text.splitlines(), 1):
        m = _DIRECTIVE.match(raw)
        if not m:
            break
        if m.group(1).lower() == "syntax":
            seen += 1
            if _verdict(m.group(2), own):
                bad.append(
                    f"{rel}:{n}: `# syntax={m.group(2)}` -- BuildKit pulls "
                    "that frontend from Docker Hub; the built-in one needs "
                    "no directive"
                )
    args: dict[str, str] = {}
    stages: set[str] = set()
    for n, line in _logical(text):
        words = line.split()
        if not words:
            continue
        op = words[0].upper()
        if op == "ARG" and len(words) > 1:
            name, _, default = words[1].partition("=")
            args[name] = default.strip("'\"")
            continue
        refs: list[str] = []
        if op == "FROM":
            rest = [w for w in words[1:] if not w.startswith("--")]
            if not rest:
                continue
            ref = rest[0]
            pv = _PURE_VAR.match(ref)
            if ref.lower() in stages:
                pass  # an earlier build stage, not a pull
            elif pv and not _READS_VAR.match(ref):
                # `FROM ${BUILD_BASE}`: what it pulls is the ARG's default
                # (a build-arg overriding it is the caller's business).
                name = pv.group(1) or pv.group(2) or pv.group(3)
                if args.get(name):
                    refs.append(args[name])
            else:
                refs.append(ref)
            if len(rest) >= 3 and rest[1].upper() == "AS":
                stages.add(rest[2].lower())
        elif op == "COPY":
            for w in words[1:]:
                if w.startswith("--from="):
                    src = w.split("=", 1)[1]
                    if src.lower() not in stages and not src.isdigit():
                        refs.append(src)
        for ref in refs:
            seen += 1
            why = _verdict(ref, own)
            if why:
                bad.append(f"{rel}:{n}: {op} {ref} -- {why}")
    return bad, seen


def offenders(
    root: Path, own: str, vendored: set[str]
) -> tuple[list[str], list[str], int, int]:
    """(failures, vendored findings, references checked, files read)."""
    files = [f for f in _files(root) if _kind(f)]
    mk = MakeVars(root, files)
    bad: list[str] = []
    seen = 0
    for rel in files:
        try:
            text = (root / rel).read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        kind = _kind(rel)
        found: list[str] = []
        if kind == "dockerfile":
            found, n_refs = _check_dockerfile(rel, text, own)
            seen += n_refs
        else:
            for n, line in _logical(text):
                for m in _USES_DOCKER.finditer(line):
                    seen += 1
                    if _verdict(m.group(1), own):
                        found.append(f"{rel}:{n}: uses docker://{m.group(1)}")
                for m in _DOCKER.finditer(line):
                    cands = _image_candidates(
                        m.group(1), line, text, mk if kind == "make" else None
                    )
                    for ref in cands or []:
                        seen += 1
                        why = _verdict(ref, own)
                        if why:
                            found.append(
                                f"{rel}:{n}: docker image {ref} -- {why}"
                            )
        bad += [f"[vendored] {f}" if rel in vendored else f for f in found]
    real = [b for b in bad if not b.startswith("[vendored] ")]
    upstream = [b for b in bad if b.startswith("[vendored] ")]
    return real, upstream, seen, len(files)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--own-prefix", default="doppler")
    ap.add_argument("--vendored", default="")
    a = ap.parse_args()
    root = a.root.resolve()
    real, upstream, seen, n_files = offenders(
        root, a.own_prefix, set(a.vendored.split())
    )
    if seen == 0:
        print(
            "check_stock_images: no image references found -- nothing checked"
        )
        return 1
    for b in upstream:
        print(f"check_stock_images: {b} (fix it in canonical)")
    for b in real:
        print(f"check_stock_images: {b}")
    if real:
        print(
            "\n  Pull a stock image through the Makefile's STOCK_REGISTRY "
            "(doppler#1950):\n  `$(STOCK_REGISTRY)/debian:stable` in make, "
            "`${STOCK_REGISTRY}/debian:stable`\n  in a Dockerfile (`ARG "
            "STOCK_REGISTRY`, no default) or a script."
        )
        return 1
    print(
        f"check_stock_images: OK -- {seen} image reference(s) in {n_files} "
        f"file(s), every stock image through {VAR}"
        + (
            f"; {len(upstream)} in vendored files, for canonical"
            if upstream
            else ""
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
