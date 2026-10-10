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
spelled whole (``$(STOCK_REGISTRY)``, ``${STOCK_REGISTRY}``,
``${STOCK_REGISTRY:?msg}`` or ``$STOCK_REGISTRY``: the one form the
pre-pull reads too, so a spelling with a fallback, ``${STOCK_REGISTRY:-...}``
say, is refused), names a
registry host other than Docker Hub's (ghcr.io pins, quay.io's
manylinux, public.ecr.aws), names the repo's OWN image (its first path
component starts with ``--own-prefix``, the Makefile's DOCKER_IMAGE), or is
an earlier build stage. Anything else -- ``debian:stable``,
``library/nats``, ``docker.io/library/ubuntu:24.04`` -- is a Docker Hub
pull and fails.

**And every stock PULL goes through scripts/stock-pull.sh (doppler#1979).**
ECR Public, where STOCK_REGISTRY points, rate-limits anonymous pulls per IP
too, and runners share IPs: ``toomanyrequests: Rate exceeded`` failed a
package leg before the code was reached. The helper retries a rate limit
with backoff; nothing else does. So, for a stock image:

- ``docker pull`` of one appears only in the helper;
- ``docker run|create`` of one says ``--pull=never`` before the image: the
  helper pulled it, and the run must not pull again, once and unretried;
- ``docker build`` with ``--build-arg STOCK_REGISTRY=`` is preceded by
  ``stock-pull.sh --dockerfile <the same -f file>``: in the same make
  recipe, or earlier in the same script, because BuildKit does not retry a
  FROM it has to pull. A stock image handed in as any OTHER build-arg
  (``--build-arg BASE=$(STOCK_REGISTRY)/...``) is refused: the pre-pull
  reads only the ARG's default, so BuildKit would pull it unretried;
- a workflow build on buildx's container driver (a job that runs
  setup-buildx-action) pulls inside BuildKit's own container, where no
  pre-pull reaches. Each is named in scripts/.stock-pull-exempt, which may
  only shrink: an unlisted one fails, so does a listed one that is gone,
  and so does a listed one the list at the merge base with ``--base``
  (origin/main) did not have (doppler#1982).

Every docker command on a line is checked, not only the first: a command's
arguments end where it does (``;``, ``&&``, ``||``, ``|``). A Makefile line
is read as make runs it, its variables expanded, so a macro that holds a
docker command (``CI_DOCKER_RUN = docker run ...``) is checked wherever it
is used.

The helper's ``--dockerfile`` reads a Dockerfile's pulls through this file's
``stock_froms`` (``--stock-froms FILE --registry R``), the same reader the
FROM rule uses, and both take the one ``_READS_VAR`` form, so the pre-pull
and the gate cannot disagree on a FROM.

Files ``--vendored`` names (standard.mk and its VENDORED_FILES) are held
verbatim to canonical, so a fix there belongs upstream; their references are
listed but do not fail. Not parsed: an inline Dockerfile piped to
``docker build -`` (the one in the tree takes ``$(PKG_IMAGE)``, quay.io).

Usage:  python3 scripts/check_stock_images.py [--root DIR]
            [--own-prefix NAME] [--vendored "FILE ..."] [--base REF]
Exit 0 when every stock image in scope comes through the registry variable.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

from _gitbase import BaseUnreadableError, in_git_repo, show_at_base

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
#: The ONE spelling of a stock reference: the variable whole, then the
#: image. The FROM rule accepts it and stock_froms() substitutes it, so the
#: gate and the pre-pull cannot disagree. ``${STOCK_REGISTRY:?msg}`` is the
#: variable whole too: it fails when unset and never falls back. A spelling
#: WITH a fallback (``:-``, ``-``, ``:=``, ``:+``) is refused by _verdict(),
#: since its fallback is a registry nobody chose.
_READS_VAR = re.compile(
    r"^\$(?:\(" + VAR + r"\)|\{" + VAR + r"(?::\?[^}]*)?\}|" + VAR + r"\b)"
)
_PURE_VAR = re.compile(
    r"^\$(?:\$?)(?:\{([A-Za-z_]\w*)\}|\(([A-Za-z_]\w*)\)|([A-Za-z_]\w*))$"
)
_MAKE_REF = re.compile(r"\$[({]([A-Za-z_][A-Za-z0-9_.-]*)[)}]")
#: The head of a `docker run|create|pull` and of a `docker [buildx] build`.
#: Only the head: a command's arguments are the _tokens() after it, which
#: stop where the command does, so a second docker command on the same
#: logical line is a match of its own rather than part of the first's.
_DOCKER = re.compile(
    r"\bdocker\s+(?:container\s+|image\s+)?(run|create|pull)\b"
)
_BUILD = re.compile(r"\bdocker\s+(?:buildx\s+)?build\b")
_FILE_ARG = re.compile(r"(?:^|\s)(?:-f|--file)(?:\s+|=)(\S+)")
_TARGET_ARG = re.compile(r"(?:^|\s)--target(?:\s+|=)(\S+)")
#: The one pull helper (doppler#1979), called with its Dockerfile mode.
HELPER = "scripts/stock-pull.sh"
_PREPULL = re.compile(
    r"(?:stock-pull\.sh['\"]?|\$\(STOCK_PULL\))\s+--dockerfile\s+(.*)"
)
#: A workflow step that builds with buildx's action.
_BUILD_PUSH = re.compile(r"\buses:\s*['\"]?docker/build-push-action@")
_SETUP_BUILDX = "docker/setup-buildx-action"
#: Container-driver builds no pre-pull can reach; shrink-only (#1979).
EXEMPT = "scripts/.stock-pull-exempt"
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
        "--pull",
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
    if VAR in ref:
        return (
            f"names {VAR} with a fallback, which {HELPER} does not read: "
            f"write ${{{VAR}}}/IMAGE, or $({VAR})/IMAGE in make"
        )
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


def _image_slot(
    toks: list[str], line: str, text: str, mk: MakeVars | None
) -> tuple[list[str] | None, int]:
    """The value(s) the image slot of one ``docker run`` can take, and the
    slot's index in ``toks`` (the options before it end there).

    The values are None when the slot is opaque (a variable nothing here
    resolves); the index is ``len(toks)`` when there is no slot.
    """
    loops = {m.group(1): m.group(2) for m in _FOR.finditer(line)}
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
                return words.split(), i
            values = _shell_assignments(text, name)
            if values and all(v == "" or v.startswith("-") for v in values):
                i += 1  # an option held in a variable
                continue
            images = [v for v in values if v and not v.startswith("-")]
            return (images or None), i
        return [tok], i
    return None, len(toks)


def _dockerfile_refs(text: str) -> list[tuple[int, str, str]]:
    """Every image a Dockerfile pulls: ``(line, op, ref)``.

    Each ``FROM`` that is not an earlier stage (``FROM ${ARG}`` read as the
    ARG's default, a build-arg overriding it being the caller's business),
    and each ``COPY --from=`` that is not a stage. The one reader of a
    Dockerfile's pulls: the gate below and stock-pull.sh's pre-pull
    (`--stock-froms`) both go through it (doppler#1979).
    """
    out: list[tuple[int, str, str]] = []
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
        if op == "FROM":
            rest = [w for w in words[1:] if not w.startswith("--")]
            if not rest:
                continue
            ref = rest[0]
            pv = _PURE_VAR.match(ref)
            if ref.lower() in stages:
                pass  # an earlier build stage, not a pull
            elif pv and not _READS_VAR.match(ref):
                name = pv.group(1) or pv.group(2) or pv.group(3)
                if args.get(name):
                    out.append((n, op, args[name]))
            else:
                out.append((n, op, ref))
            if len(rest) >= 3 and rest[1].upper() == "AS":
                stages.add(rest[2].lower())
        elif op == "COPY":
            for w in words[1:]:
                if w.startswith("--from="):
                    src = w.split("=", 1)[1]
                    if src.lower() not in stages and not src.isdigit():
                        out.append((n, op, src))
    return out


def stock_froms(text: str, registry: str) -> list[str]:
    """The stock images a Dockerfile pulls, with ``registry`` substituted
    for STOCK_REGISTRY, each once, in order: what `stock-pull.sh
    --dockerfile` pulls before a build, because BuildKit does not retry a
    rate limit on a FROM (doppler#1979)."""
    out: list[str] = []
    for _, _, ref in _dockerfile_refs(text):
        if _READS_VAR.match(ref):
            ref = _READS_VAR.sub(lambda _: registry, ref, count=1)
            if ref not in out:
                out.append(ref)
    return out


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
    for n, op, ref in _dockerfile_refs(text):
        seen += 1
        why = _verdict(ref, own)
        if why:
            bad.append(f"{rel}:{n}: {op} {ref} -- {why}")
    return bad, seen


def _pull_rule(
    rel: str, n: int, verb: str, opts: list[str], ref: str
) -> list[str]:
    """A stock image pulled any way but through the helper (doppler#1979).

    ``docker pull`` of one is the helper's job alone. ``docker run`` /
    ``create`` pulls a missing image itself, once and without retry, so it
    must say ``--pull=never``: the image is already there because the helper
    put it there, or the run fails loudly instead of meeting a 429 bare.
    ``opts`` are the tokens BEFORE the image: after it, ``--pull=never`` is
    an argument to the container's command, and docker never sees it.
    """
    if verb == "pull":
        if rel == HELPER:
            return []
        return [
            f"{rel}:{n}: docker pull {ref} -- a stock pull outside {HELPER}, "
            "which retries a rate limit (#1979)"
        ]
    never = "--pull=never" in opts or any(
        t == "--pull" and nxt == "never" for t, nxt in zip(opts, opts[1:])
    )
    if never:
        return []
    return [
        f"{rel}:{n}: docker {verb} {ref} -- pulls the stock image itself "
        f"with no retry: pull it with {HELPER} first and pass --pull=never "
        "before the image (#1979)"
    ]


def _stock_build_args(toks: list[str]) -> list[str]:
    """``NAME=VALUE`` build-args, other than STOCK_REGISTRY itself, whose
    value names a stock image. BuildKit pulls it, unretried, and the
    pre-pull cannot: stock_froms() reads a ``FROM ${NAME}`` as the ARG's
    default, not as what a caller overrides it with (doppler#1979)."""
    out: list[str] = []
    for i, tok in enumerate(toks):
        if tok == "--build-arg" and i + 1 < len(toks):
            kv = toks[i + 1]
        elif tok.startswith("--build-arg="):
            kv = tok.split("=", 1)[1]
        else:
            continue
        name, _, value = kv.partition("=")
        if name != VAR and VAR in value:
            out.append(kv)
    return out


def _site_key(rel: str, dockerfile: str, target: str) -> str:
    return f"{rel} {dockerfile}" + (f":{target}" if target else "")


def _build_push_site(lines: list[str], i: int) -> tuple[str, str]:
    """``(file, target)`` of the build-push-action step whose ``uses:`` is
    line ``i``, read up to the next list item (the next step)."""
    file = target = ""
    for raw in lines[i + 1 :]:
        s = raw.strip()
        if s.startswith("- "):
            break
        if s.startswith("file:"):
            file = s.split(":", 1)[1].strip().strip("'\"")
        elif s.startswith("target:"):
            target = s.split(":", 1)[1].strip().strip("'\"")
    return file, target


def _exemption_keys(text: str) -> dict[str, int]:
    """``{site key: line}`` from the text of the exemption list: the one
    reader, for the list as it is and as the merge base had it."""
    out: dict[str, int] = {}
    for n, raw in enumerate(text.splitlines(), 1):
        words = raw.split()
        if len(words) >= 2 and not words[0].startswith("#"):
            out[f"{words[0]} {words[1]}"] = n
    return out


def _exemptions(root: Path) -> dict[str, int]:
    """``{site key: line}`` from the shrink-only exemption list."""
    path = root / EXEMPT
    if not path.exists():
        return {}
    return _exemption_keys(path.read_text(encoding="utf-8"))


def _added_exemptions(
    root: Path, base: str, exempt: dict[str, int]
) -> list[str]:
    """Entries the list at the merge base with ``base`` did not have.

    The list may only SHRINK, so a key absent there is refused even when it
    names a real container-driver build: that build is the thing to fix. A
    tree that is not a checkout (a test's seeded tree) has no base; a base
    without the list is the PR that introduces it; a base git cannot read
    fails closed, since a ratchet that cannot read its baseline has not
    been checked (scripts/_gitbase.py).
    """
    if not in_git_repo(root):
        return []
    try:
        then = show_at_base(root, base, EXEMPT)
    except BaseUnreadableError:
        return [
            f"{EXEMPT}: cannot resolve {base}, so an ADDED entry cannot be "
            "told from an old one. Fetch it: git fetch --no-tags --depth=1 "
            "origin +refs/heads/main:refs/remotes/origin/main"
        ]
    if then is None:
        return []
    before = _exemption_keys(then)
    return [
        f"{EXEMPT}:{n}: '{key}' is not in the list at the merge base with "
        f"{base} -- the list only shrinks; build on the daemon and pre-pull "
        "instead (#1982)"
        for key, n in sorted(exempt.items(), key=lambda kv: kv[1])
        if key not in before
    ]


def offenders(
    root: Path, own: str, vendored: set[str], base: str = "origin/main"
) -> tuple[list[str], list[str], int, int]:
    """(failures, vendored findings, references checked, files read)."""
    files = [f for f in _files(root) if _kind(f)]
    mk = MakeVars(root, files)
    bad: list[str] = []
    seen = 0
    #: buildx container-driver builds: {site key: "rel:line"} (#1979).
    sites: dict[str, str] = {}
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
            expand = mk.expand if kind == "make" else (lambda s: s)
            container = kind == "workflow" and _SETUP_BUILDX in text
            raw_lines = text.splitlines()
            # What a pre-pull must precede: the make recipe a line is in, or
            # everything before it in a script or workflow.
            scope: list[str] = []
            for n, line in _logical(text):
                if kind == "make" and not line.startswith("\t"):
                    scope = []
                # As make runs it: a macro holding a docker command is one.
                cmd = expand(line)
                for m in _USES_DOCKER.finditer(cmd):
                    seen += 1
                    if _verdict(m.group(1), own):
                        found.append(f"{rel}:{n}: uses docker://{m.group(1)}")
                for m in _DOCKER.finditer(cmd):
                    verb, toks = m.group(1), _tokens(cmd[m.end() :])
                    cands, slot = _image_slot(
                        toks, cmd, text, mk if kind == "make" else None
                    )
                    for ref in cands or []:
                        seen += 1
                        why = _verdict(ref, own)
                        if why:
                            found.append(
                                f"{rel}:{n}: docker image {ref} -- {why}"
                            )
                        elif _READS_VAR.match(ref):
                            found += _pull_rule(rel, n, verb, toks[:slot], ref)
                for m in _BUILD.finditer(cmd):
                    toks = _tokens(cmd[m.end() :])
                    args = " ".join(toks)
                    fm, tm = _FILE_ARG.search(args), _TARGET_ARG.search(args)
                    dockerfile = (
                        fm.group(1).strip("'\"") if fm else "Dockerfile"
                    )
                    if container:
                        key = _site_key(
                            rel, dockerfile, tm.group(1) if tm else ""
                        )
                        sites[key] = f"{rel}:{n}"
                        continue
                    for kv in _stock_build_args(toks):
                        seen += 1
                        found.append(
                            f"{rel}:{n}: docker build of {dockerfile} with "
                            f"--build-arg {kv} -- BuildKit pulls that stock "
                            f"image unretried and {HELPER} --dockerfile "
                            "reads only the ARG's default; name it in the "
                            f"Dockerfile as ${{{VAR}}}/IMAGE (#1979)"
                        )
                    if VAR not in args:
                        continue
                    seen += 1
                    prepulled = any(
                        dockerfile in _tokens(p.group(1))
                        for prior in scope
                        for p in [_PREPULL.search(expand(prior))]
                        if p
                    )
                    if not prepulled:
                        found.append(
                            f"{rel}:{n}: docker build of {dockerfile} with "
                            "stock FROMs and no pre-pull -- BuildKit does not "
                            "retry a rate limit; run `$(STOCK_PULL) "
                            f"--dockerfile {dockerfile}` before it (#1979)"
                        )
                if kind == "workflow" and _BUILD_PUSH.search(line):
                    i = n - 1
                    file, target = _build_push_site(raw_lines, i)
                    sites[_site_key(rel, file, target)] = f"{rel}:{n}"
                scope.append(line)
        bad += [f"[vendored] {f}" if rel in vendored else f for f in found]
    exempt = _exemptions(root)
    bad += _added_exemptions(root, base, exempt)
    for key, where in sorted(sites.items()):
        seen += 1
        if key not in exempt:
            bad.append(
                f"{where}: buildx container-driver build of "
                f"{key.split(' ', 1)[1]} -- BuildKit pulls its FROMs in its "
                "own container, so no pre-pull reaches it and a 429 is not "
                f"retried; list it in {EXEMPT} (#1982) or build on the daemon"
            )
    for key, n in sorted(exempt.items(), key=lambda kv: kv[1]):
        if key not in sites:
            bad.append(
                f"{EXEMPT}:{n}: '{key}' names no container-driver build any "
                "more -- delete the line; the list only shrinks (#1982)"
            )
    real = [b for b in bad if not b.startswith("[vendored] ")]
    upstream = [b for b in bad if b.startswith("[vendored] ")]
    return real, upstream, seen, len(files)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--own-prefix", default="doppler")
    ap.add_argument("--vendored", default="")
    ap.add_argument(
        "--stock-froms",
        metavar="DOCKERFILE",
        help="print the stock images DOCKERFILE pulls, one per line, with "
        "--registry for STOCK_REGISTRY (what stock-pull.sh --dockerfile "
        "pre-pulls), and exit",
    )
    ap.add_argument("--registry", default="")
    ap.add_argument(
        "--base",
        default="origin/main",
        help="ref whose merge base holds the exemption list this one may "
        "only shrink from",
    )
    a = ap.parse_args()
    if a.stock_froms:
        if not a.registry:
            ap.error("--stock-froms needs --registry")
        text = Path(a.stock_froms).read_text(encoding="utf-8")
        for ref in stock_froms(text, a.registry):
            print(ref)
        return 0
    root = a.root.resolve()
    real, upstream, seen, n_files = offenders(
        root, a.own_prefix, set(a.vendored.split()), a.base
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
            "\n  Name a stock image through the Makefile's STOCK_REGISTRY "
            "(doppler#1950):\n  `$(STOCK_REGISTRY)/debian:stable` in make, "
            "`${STOCK_REGISTRY}/debian:stable`\n  in a Dockerfile (`ARG "
            "STOCK_REGISTRY`, no default) or a script. PULL it only\n  "
            f"through {HELPER} (doppler#1979): `$(STOCK_PULL) IMAGE` then\n"
            "  `docker run --pull=never`, or `$(STOCK_PULL) --dockerfile F` "
            "before `docker build -f F`."
        )
        return 1
    print(
        f"check_stock_images: OK -- {seen} image reference(s) in {n_files} "
        f"file(s), every stock image through {VAR} and every stock pull "
        f"through {HELPER}"
        + (
            f"; {len(upstream)} in vendored files, for canonical"
            if upstream
            else ""
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
