#!/usr/bin/env bash
# consumer-walk.sh -- runs INSIDE a clean Debian/Ubuntu container
# (make consumer-walk): do what the docs tell a new .deb user to do, using the
# docs' OWN commands, and require each step to work.
#
#   1. install the published .deb packages   (docs/install/c.md, "System packages")
#   2. install the build tools               (the same page, "Build something against it")
#   3. fetch the consumer project            (docs/example-projects/consumer.md, "Get the code")
#   4. `make run` in it                      (the consumer README, "Build and run")
#   5. the three Debian-multiarch conditions each FAIL to configure when broken,
#      and the good case configures          (docs/install/c.md)
#
# Steps 1-4 are the commands EXTRACTED from the pages, and the extraction is
# declared once, in docs-walk.sh (`docs-walk.sh plan deb`): this walk runs what
# it prints and checks the result, so the page is the thing under test. Step 5 is
# this script's own: the cmake projects are minimal stand-ins for the conditions
# the page states (doppler#2856: the pages were README-only and the .deb path
# led nowhere).
#
# Usage: consumer-walk.sh <repo dir, read-only>     (needs network)
set -euo pipefail
SRC="${1:?usage: consumer-walk.sh <repo dir>}"
export DEBIAN_FRONTEND=noninteractive
say() { printf '\n### %s\n' "$*"; }
die() { echo "FAIL: $*" >&2; exit 1; }

case "$(uname -m)" in
    x86_64) deb=amd64 ;;
    aarch64|arm64) deb=arm64 ;;
    *) die "no .deb for $(uname -m)" ;;
esac
# Containers run as root with no sudo; the pages say `sudo`.
sudo() { "$@"; }
# ...and apt asks "Do you want to continue? [Y/n]", which a person answers.
apt() { command apt -y -q "$@"; }
export -f sudo apt

# The extraction is docs-walk.sh's (and fence.sh's): see steps 1-4.
# Every extracted step must be non-empty: an empty step would "pass" silently.
need() { [ -s "$1" ] || die "extracting $1 from the page gave nothing"; }

apt-get update -qq >/dev/null
apt-get install -y -qq curl ca-certificates >/dev/null
cd /root

say "1-4. the pages' own commands: what docs-walk.sh plans for the deb stage"
# ONE declaration of those four steps (the .deb block, the build tools, the
# consumer fetch, the README's `make run`): docs-walk.sh. This walk adds only
# what is its own, the checks around them and step 5.
WALK_REPO="$SRC" bash "$SRC/tests/install/docs-walk.sh" plan deb \
    | grep -v '^#####' > steps.sh
need steps.sh
cat steps.sh
bash -euo pipefail steps.sh > run.log 2>&1 || { tail -n 25 run.log; die "the pages' deb steps failed"; }
dpkg -s libdoppler-dsp-dev >/dev/null || die "-dev not installed"
[ -f consumer/CMakeLists.txt ] && [ -f consumer/main.c ] || die "fetch gave no project"
tail -n 8 run.log
grep -q "0 of 8 samples off" run.log || die "the consumer did not report a clean cycle"

say "5. the Debian multiarch conditions (docs/install/c.md)"
mk() { mkdir -p "$1"; cat > "$1/CMakeLists.txt"; echo 'int main(void){return 0;}' > "$1/m.c"; }
mk good <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(t C)
find_package(doppler REQUIRED)
EOF
mk none <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(t NONE)
find_package(doppler REQUIRED)
EOF
mk before <<'EOF'
cmake_minimum_required(VERSION 3.16)
find_package(doppler REQUIRED)
project(t C)
EOF
mk toonew <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(t C)
find_package(doppler 999.0.0 REQUIRED)
EOF
cmake -S good -B good/b >/dev/null 2>&1 || die "good case did not configure"
echo "[good]              configures"
for t in none before toonew; do
    if cmake -S $t -B $t/b >/dev/null 2>&1; then die "[$t] configured but the page says it fails"; fi
    echo "[$t] fails to configure, as the page says"
done
apt-get remove -y -qq libdoppler-dsp-dev >/dev/null 2>&1
if cmake -S good -B good/b2 >/dev/null 2>&1; then die "runtime-only configured but the page says it fails"; fi
echo "[runtime package only] fails to configure, as the page says"

echo; echo "consumer-walk: PASS"
