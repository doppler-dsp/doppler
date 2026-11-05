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
# Steps 1-3 run the commands EXTRACTED from the pages (tests/install/fence.sh),
# not retyped here, so the page is the thing under test: a page whose command
# stops working fails this walk. Steps 4 and 5 are this script's own: `make run`
# is the README's instruction, and the cmake projects are minimal stand-ins for
# the conditions the page states (doppler#2856: the pages were README-only and
# the .deb path led nowhere).
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

# The extraction is a script of its own (fence.sh), so the PR-time test and
# this walk read the pages the same way.
fence() { bash "$SRC/tests/install/fence.sh" "$@"; }
# Every extracted step must be non-empty: an empty step would "pass" silently.
need() { [ -s "$1" ] || die "extracting $1 from the page gave nothing"; }

apt-get update -qq >/dev/null
apt-get install -y -qq curl ca-certificates >/dev/null
cd /root

say "1. install the published .deb packages (the page's own block, $deb)"
if [ -n "${WALK_DEB_DIR:-}" ]; then
    # Candidate: the packages are in a directory, not yet a release asset, so
    # the page's download is replaced by installing them (the one substitution).
    echo "sudo apt-get install -y -qq $WALK_DEB_DIR/libdoppler-dsp0*.deb $WALK_DEB_DIR/libdoppler-dsp-dev_*.deb" > step1.sh
else
    fence "$SRC/docs/install/c.md" '^Debian / Ubuntu:' 'libdoppler-dsp-dev_' \
        | sed "s/amd64/$deb/g" > step1.sh
fi
need step1.sh
cat step1.sh
bash -euo pipefail step1.sh > step1.log 2>&1 || { tail -n 15 step1.log; die "the .deb block did not install"; }
dpkg -s libdoppler-dsp-dev >/dev/null || die "-dev not installed"

say "2. the build tools, the page's one apt line"
fence "$SRC/docs/install/c.md" '^### Build something against it' 'apt install' > step2.sh
need step2.sh
cat step2.sh
bash -euo pipefail step2.sh > step2.log 2>&1 || { tail -n 15 step2.log; die "the build-tools line failed"; }

say "3. fetch the consumer project (the consumer page's own command)"
fence "$SRC/docs/example-projects/consumer.md" '^## Get the code' 'tar -xz' > step3.sh
need step3.sh
# Candidate: the pinned release tarball does not carry this tree, so the page's
# `curl` is replaced by reading a tarball of the tree (same `doppler-<V>/`
# prefix); the page's `tar -xz` is untouched.
if [ -n "${WALK_SRC_TARBALL:-}" ]; then
    sed -i "s#curl -fsSL [^ ]*#cat $WALK_SRC_TARBALL#" step3.sh
fi
cat step3.sh
bash -euo pipefail step3.sh || die "the one-line fetch failed"
[ -f consumer/CMakeLists.txt ] && [ -f consumer/main.c ] || die "fetch gave no project"
ls consumer

say "4. make run (the system-install case, as the README leads with)"
(cd consumer && make run) > run.log 2>&1 || { tail -20 run.log; die "make run failed"; }
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
