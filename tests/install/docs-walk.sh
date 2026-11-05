#!/usr/bin/env bash
# docs-walk.sh -- one stage of the docs walk (doppler#2857), run INSIDE a bare
# container, or `plan` to print what every stage would run, with no container.
#
#   docs-walk.sh plan [stage]
#   docs-walk.sh run <candidate|published> <stage> <repo> <artifact dir> <version>
#
# Stages: pip, tarball, clone, deb, docker. Each stage's script is EXTRACTED
# from the page that tells the reader to do it (tests/install/fence.sh), so a
# wrong URL, flag or target on a page fails here, and an extraction that comes
# back empty is an error rather than a stage that passes by running nothing.
# `pages` lists the files they read; `plan` prints those scripts; src/doppler/tests/test_docs_walk_plan.py runs it
# on every PR, so the extraction is declared once, here.
#
# Two faces. PUBLISHED runs the pages' commands as written, against PyPI, the
# GitHub Release and the pinned tag. CANDIDATE is the pre-release dry run for a
# tree with no tag: the SAME commands with ONE substitution each --
#   pip      the package name `doppler-dsp` becomes the candidate wheel
#   tarball  `jbx get-doppler` becomes extracting the candidate tarball
#   clone    `git clone --branch vX <url>` becomes `git clone <repo>`
#   deb      the release download becomes installing the candidate .debs, and
#            the consumer's `curl` becomes reading a tarball of the tree
# Every other command is the page's own.
#
# What is NOT a page's command, and so is stated here: the machine a reader
# starts from. Each stage's container is the bare image plus `curl` and
# `ca-certificates` (to fetch anything), `git` for the clone stage (no page says to install curl or git: #2883), and Python
# with pip for the pip stage. A build tool the pages do not install is NOT
# provided: the clone stage gets its compiler from source.md's own apt block.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="${WALK_REPO:-$(cd "$HERE/../.." && pwd)}"
MODE="${WALK_MODE:-published}"
ART="${WALK_ART:-/art}"
V="${WALK_VERSION:-$(sed -n 's/^version *= *"\(.*\)"/\1/p' "$REPO/pyproject.toml" | head -1)}"
fence() { bash "$HERE/fence.sh" "$@"; }

die() { echo "docs-walk: $*" >&2; exit 1; }
# A page that stops yielding a command must fail loudly, not run nothing.
nonempty() { # nonempty <what> ; reads stdin, echoes it, fails when empty
    local t; t="$(cat)"
    [ -n "$t" ] || die "extracted nothing for: $1"
    printf '%s\n' "$t"
}
# Lines of a page's fence that match; the page, not this script, owns them.
fenced_lines() { # fenced_lines <page> <heading> <anchor line regex> <keep regex> [lang]
    fence "$REPO/$1" "$2" "$3" "${5:-sh}" | grep -E "$4" | nonempty "$1 ($3)"
}

emit_pip() {
    echo "# --- docs/quickstart.md, Get it!: the virtual environment a Debian/Ubuntu pip needs"
    fenced_lines docs/quickstart.md '^## Get it' 'python3-venv' '^sudo apt install python3-venv' bash
    fenced_lines docs/quickstart.md '^## Get it' 'python3 -m venv' '^(python3 -m venv|\. )' bash
    echo "# --- docs/gallery/delay-doppler.md: install the [examples] extra, run the demo"
    local lines
    lines="$(fenced_lines docs/gallery/delay-doppler.md '^# ' '^python3 -m venv' '^(python3 -m venv|pip install |python -m )')"
    if [ "$MODE" = candidate ]; then
        # The one substitution: the package name becomes the wheel.
        lines="$(sed 's#doppler-dsp#'"${WALK_WHEEL:-<wheel>}"'#' <<<"$lines")"
    fi
    printf '%s\n' "$lines"
    echo 'python -c "import doppler, matplotlib, scipy; print(\"doppler\", doppler.__version__)"'
    echo "ls -l *.png"
    echo "# --- docs/quickstart.md: the Python blocks that need no broker"
    fenced_lines docs/quickstart.md '^## ' '^pip install' '^pip install ' bash \
        | { if [ "$MODE" = candidate ]; then sed 's#doppler-dsp#'"${WALK_WHEEL:-<wheel>}"'#'; else cat; fi; }
    cat <<PY
python - "$REPO/docs/quickstart.md" <<'EOF'
import re, sys
text = open(sys.argv[1], encoding="utf-8").read()
blocks = [b for b in re.findall(r"\`\`\`python\n(.*?)\n\`\`\`", text, re.S)
          if "nats://" not in b]
assert len(blocks) >= 4, f"only {len(blocks)} broker-free quick start blocks"
for b in blocks:
    exec(compile(b, "quickstart", "exec"), {})
print(f"quick start: {len(blocks)} blocks ran")
EOF
PY
    echo "# --- docs/quickstart.md: the wfmgen lines that need no broker"
    local w
    w="$(fenced_lines docs/quickstart.md '^## ' '^wfmgen ' '^wfmgen ' | grep -v 'nats://' | sed 's/ *#.*//' | nonempty 'wfmgen lines')"
    printf '%s\n' "$w"
    echo "test -s capture.cf32 && test -s scenario.cf32"
}

emit_tarball() {
    echo "# --- docs/install/c.md: the apt line the page gives for building against it"
    fence "$REPO/docs/install/c.md" '^### Build something against it' 'apt install' | nonempty 'c.md apt line'
    echo "# --- docs/quickstart-c.md: get the library"
    if [ "$MODE" = candidate ]; then
        echo 'mkdir -p "$HOME/.local/doppler"'
        echo "tar -xzf ${WALK_TARBALL:-<tarball>} -C \"\$HOME/.local/doppler\""
    else
        grep -m1 'get-jb.sh' "$REPO/docs/quickstart-c.md" | sed 's/^ *//' | nonempty 'get-jb line'
        fenced_lines docs/quickstart-c.md '^## Get the library' 'jbx get-doppler' '^jbx get-doppler' | sed 's/ *#.*//'
    fi
    echo "# --- docs/quickstart-c.md: the prefix, then the three faces it includes"
    fenced_lines docs/quickstart-c.md '^## Compile it' 'PREFIX=' '^PREFIX='
    # The three faces are `--8<--` includes of this script's regions: require
    # all three, then run the script the page includes.
    for r in cc cmake-commands pkg-config; do
        grep -q "build-three-ways.sh:$r\"" "$REPO/docs/quickstart-c.md" \
            || die "docs/quickstart-c.md no longer includes build-three-ways.sh:$r"
    done
    echo "bash $REPO/tests/install/stream-consumer/build-three-ways.sh \"\$PREFIX\""
}

emit_clone() {
    echo "# --- docs/install/source.md: the apt packages the page lists (included from build-apt-deps.sh)"
    grep -q 'build-apt-deps.sh:install"' "$REPO/docs/install/source.md" \
        || die "docs/install/source.md no longer includes build-apt-deps.sh:install"
    echo "bash $REPO/tests/install/build-apt-deps.sh"
    echo "# --- docs/examples/c.md: clone, then build and run a C example"
    local c
    c="$(fenced_lines docs/examples/c.md '^## ' '^git clone' '^git clone --branch v[0-9.]+ ')"
    if [ "$MODE" = candidate ]; then
        # The one substitution: the pinned tag does not carry this tree, so the
        # URL becomes the repo (cloned into `doppler`, as the URL's name would).
        c="$(sed -E "s# --branch v[0-9.]+##; s#https://github.com/doppler-dsp/doppler#$REPO doppler#" <<<"$c")"
    fi
    printf '%s\n' "$c"
    fenced_lines docs/examples/c.md '^## Halfband' '^make build' '^(make build|\./build/)'
}

emit_deb() {
    # The architecture the page's file names carry (they say amd64).
    local arch="\$(dpkg --print-architecture)"
    echo "# --- docs/install/c.md: the .debs, the page's own block"
    local blk
    blk="$(fence "$REPO/docs/install/c.md" '^Debian / Ubuntu:' 'libdoppler-dsp-dev_' | nonempty 'c.md .deb block')"
    # The page names amd64; the machine says its own.
    blk="$(sed "s/amd64/$arch/g" <<<"$blk")"
    if [ "$MODE" = candidate ]; then
        # The one substitution: the release URL becomes the candidate's directory.
        blk="$(sed -E "s#^base=https://[^ ]+#base=file://$ART#" <<<"$blk")"
    fi
    printf '%s\n' "$blk"
    echo "# --- docs/install/c.md: the build tools"
    fence "$REPO/docs/install/c.md" '^### Build something against it' 'apt install' | nonempty 'c.md build tools'
    echo "# --- docs/example-projects/consumer.md: fetch the consumer project"
    local get
    get="$(fence "$REPO/docs/example-projects/consumer.md" '^## Get the code' 'tar -xz' | nonempty 'consumer.md fetch')"
    if [ "$MODE" = candidate ]; then
        # The one substitution: the release archive becomes the tree's own.
        get="$(sed -E "s#https://[^ ]+\.tar\.gz#file://$ART/src.tar.gz#" <<<"$get")"
    fi
    printf '%s\n' "$get"
    echo "# --- the consumer README: build and run"
    echo "cd consumer"
    fence "$REPO/example-projects/consumer/README.md" '^## ' '^make run' | nonempty 'consumer README make run'
}

emit_docker() {
    fenced_lines docs/install/docker.md '^## ' 'docker run --rm ghcr.io/doppler-dsp/doppler:[0-9.]+ doppler --help' \
        '^docker run --rm ghcr.io/doppler-dsp/doppler:[0-9.]+ doppler --help'
}

STAGES="pip tarball clone deb docker"
emit() { "emit_$1"; }

case "${1:-}" in
plan)
    for s in ${2:-$STAGES}; do
        echo "##### stage $s"
        emit "$s"
    done
    ;;
pages)
    # Every file the plan reads: the pages it extracts from and the scripts the
    # pages include. The candidate workflow's path filter must cover them all
    # (src/doppler/tests/test_docs_walk_plan.py).
    { grep -ohE "(docs|example-projects)/[A-Za-z0-9_./-]+\.md" "$HERE/docs-walk.sh"
      echo tests/install/build-apt-deps.sh
      echo tests/install/stream-consumer/build-three-ways.sh
      echo tests/install/fence.sh; } | sort -u
    ;;
run)
    MODE="$2"; stage="$3"; REPO="$4"; ART="$5"; V="$6"
    export WALK_MODE="$MODE" WALK_REPO="$REPO" WALK_ART="$ART" WALK_VERSION="$V"
    export DEBIAN_FRONTEND=noninteractive MPLBACKEND=Agg
    # Containers run as root with no sudo, and apt asks questions; the pages
    # say `sudo apt install`.
    sudo() { "$@"; }
    apt() { command apt -y -q "$@"; }
    apt-get() { command apt-get -y -q "$@"; }   # build-apt-deps.sh runs `sudo apt-get install` without -y
    export -f sudo apt apt-get
    # The machine a reader starts from (see the header): fetch tools, git for
    # the clone, Python for pip. Nothing a page is meant to install.
    apt-get update -qq >/dev/null
    base="curl ca-certificates"
    case "$stage" in
    clone) base="$base git" ;;
    pip) base="$base python3" ;;
    esac
    apt-get install -y -qq $base >/dev/null
    git config --global --add safe.directory '*' 2>/dev/null || true
    if [ "$MODE" = candidate ]; then
        export WALK_WHEEL="$(ls "$ART"/doppler_dsp-*.whl 2>/dev/null | head -1 || true)"
        export WALK_TARBALL="$(ls "$ART"/doppler-*linux*.tar.gz 2>/dev/null | head -1 || true)"
        case "$stage" in
        pip) [ -n "$WALK_WHEEL" ] || die "no wheel in $ART" ;;
        tarball) [ -n "$WALK_TARBALL" ] || die "no tarball in $ART" ;;
        esac
    fi
    work="$(mktemp -d)"; cd "$work"
    emit "$stage" > stage.sh
    echo "### STAGE $stage ($MODE, v$V): the commands"
    cat stage.sh
    echo "### running"
    bash -euo pipefail stage.sh
    echo "### stage $stage: PASS"
    ;;
*) die "usage: docs-walk.sh plan [stage] | run <mode> <stage> <repo> <art> <version>" ;;
esac
