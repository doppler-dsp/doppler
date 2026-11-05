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
# The commands are EXTRACTED from the pages, not retyped here, so the page is
# the thing under test: a page whose command stops working fails this walk
# (doppler#2856: the pages were README-only and the .deb path led nowhere).
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

# fence <file> <heading regex> <line regex>: the first ```sh block after the
# heading that contains the line, printed without its fences.
fence() {
    awk -v h="$2" -v l="$3" '
        $0 ~ h { on = 1 }
        on && /^```sh$/ { blk = ""; infence = 1; next }
        infence && /^```$/ { if (blk ~ l) { printf "%s", blk; exit } infence = 0; next }
        infence { blk = blk $0 "\n" }
    ' "$1"
}

apt-get update -qq >/dev/null
apt-get install -y -qq curl ca-certificates >/dev/null
cd /root

say "1. install the published .deb packages (the page's own block, $deb)"
fence "$SRC/docs/install/c.md" '^Debian / Ubuntu:' 'libdoppler-dsp-dev_' \
    | sed "s/amd64/$deb/g" > step1.sh
cat step1.sh
bash -euo pipefail step1.sh > step1.log 2>&1 || { tail -n 15 step1.log; die "the .deb block did not install"; }
dpkg -s libdoppler-dsp-dev >/dev/null || die "-dev not installed"

say "2. the build tools, the page's one apt line"
fence "$SRC/docs/install/c.md" '^### Build something against it' 'apt install' \
    | grep -E 'apt install' | sed 's/ *#.*//' > step2.sh
cat step2.sh
bash -euo pipefail step2.sh > step2.log 2>&1 || { tail -n 15 step2.log; die "the build-tools line failed"; }

say "3. fetch the consumer project (the consumer page's own command)"
fence "$SRC/docs/example-projects/consumer.md" '^## Get the code' 'tar -xz' > step3.sh
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
