#!/usr/bin/env python3
"""Publish and render doppler's representative benchmark numbers.

Numbers are measured **by hand on a representative machine** and committed
under ``benchmarks/published/v<version>/`` — they are deliberately not pulled
from CI, whose shared runners are non-deterministic. Each release is measured
in two
builds so users can see the from-source upside:

* **portable** — the shipped baseline (``-march=x86-64-v2 -ffast-math``), i.e.
  what a PyPI wheel delivers;
* **native** — ``-DDOPPLER_NATIVE=ON`` (``-march=native``), peak on the exact
  CPU, what a from-source build gets.

Layout (committed)::

    benchmarks/published/v0.10.1/
        portable.json  portable-c.json     # pytest-benchmark + jm_bench
        native.json    native-c.json

Each snapshot carries an injected ``doppler_meta`` (when, commit, compiler +
flags, CPU state, library versions) so the page is self-describing and a
regression is diagnosable.

Metric: throughput benches record ``extra_info["MSa_s"]`` → **MSa/s, higher is
better**; latency ops (no sample count) → **mean time per call, lower is
better**.

Workflow / commands
-------------------
::

    # on a representative machine, for each build:
    make pyext                                  # portable (default)
    make bench && make bench-publish VERSION=0.10.1 BUILD=portable
    make pyext CMAKE_ARGS=-DDOPPLER_NATIVE=ON   # native
    make bench && make bench-publish VERSION=0.10.1 BUILD=native

    make bench-docs        # render docs/benchmarks.md from published/
    make bench-report      # portable trend across releases, to stdout
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import subprocess

PUBLISHED = "benchmarks/published"
LOCAL = "benchmarks/history"
BUILDS = ("portable", "native")
SUITES = ("python", "c")
SUITE_LABEL = {"c": "C (jm_bench)", "python": "Python (pytest-benchmark)"}
VER_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")
# Flags worth quoting on the page (the ones that actually move benchmark
# numbers)
FLAG_RE = re.compile(
    r"-O\S+|-march=\S+|-mtune=\S+|-mprefer-vector-width=\S+"
    r"|-ffast-math|-funsafe-math\S*"
)


def _version_key(v: str):
    m = VER_RE.match(v)
    return tuple(int(x) for x in m.groups()) if m else None


def _display_name(suite: str, bench: dict) -> str:
    """Short, unique label. Python `name`s collide across modules, so derive a
    ``module::case`` label from ``fullname``; C names are already unique."""
    name = bench.get("name", "?")
    full = bench.get("fullname") or name
    if suite == "python" and "::" in full:
        mod = full.rsplit("::", 1)[0].rsplit("/", 1)[-1]
        mod = mod.removesuffix(".py").removeprefix("bench_")
        case = name.removeprefix("test_bench_").removeprefix("test_")
        return f"{mod}::{case}" if mod else case
    return name


def _parse(data, suite):
    """{fullname: {disp, mean, msas}} for one snapshot's benchmarks."""
    out = {}
    for b in data.get("benchmarks", []):
        mean = b.get("stats", {}).get("mean")
        if not (isinstance(mean, (int, float)) and mean > 0):
            continue
        msas = b.get("extra_info", {}).get("MSa_s")
        out[b.get("fullname") or b["name"]] = {
            "disp": _display_name(suite, b),
            "mean": float(mean),
            "msas": float(msas) if isinstance(msas, (int, float)) else None,
        }
    return out


def fmt_time(seconds: float) -> str:
    for scale, unit in ((1e-9, "ns"), (1e-6, "µs"), (1e-3, "ms"), (1.0, "s")):
        if seconds < scale * 1000:
            return f"{seconds / scale:.2f} {unit}"
    return f"{seconds:.2f} s"


def fmt_tput(msas: float) -> str:
    return f"{msas / 1000:.2f} GSa/s" if msas >= 1000 else f"{msas:.1f} MSa/s"


# ── loading ────────────────────────────────────────────────────────────────


def _load_build(version_dir, build):
    """One build of one release → {machine, doppler_build, suites} or None."""
    pj = os.path.join(version_dir, f"{build}.json")
    if not os.path.exists(pj):
        return None
    with open(pj) as fh:
        pdata = json.load(fh)
    suites = {"python": _parse(pdata, "python"), "c": {}}
    cj = os.path.join(version_dir, f"{build}-c.json")
    if os.path.exists(cj):
        with open(cj) as fh:
            suites["c"] = _parse(json.load(fh), "c")
    return {
        "machine": pdata.get("machine_info", {}),
        "meta": pdata.get("doppler_meta") or pdata.get("doppler_build", {}),
        "datetime": pdata.get("datetime", ""),
        "suites": suites,
    }


def load_published(root=PUBLISHED):
    """{version: {build: {...}}} for every committed release, sorted."""
    out = {}
    for d in sorted(glob.glob(os.path.join(root, "v*"))):
        version = os.path.basename(d)
        if _version_key(version) is None:
            continue
        builds = {b: _load_build(d, b) for b in BUILDS}
        builds = {b: v for b, v in builds.items() if v}
        if builds:
            out[version] = builds
    return dict(sorted(out.items(), key=lambda kv: _version_key(kv[0])))


# ── publish (stamp a release) ────────────────────────────────────────────────


def _build_info():
    """(compiler, flags) for the current build, from compile_commands.json."""
    db = "compile_commands.json"
    if not os.path.exists(db):
        return "", ""
    with open(db) as fh:
        entries = json.load(fh)
    core = next(
        (
            e
            for e in entries
            if e["file"].endswith("_core.c") and "vendor" not in e["file"]
        ),
        entries[0] if entries else None,
    )
    if not core:
        return "", ""
    cmd = core.get("command") or " ".join(core.get("arguments", []))
    cc = cmd.split()[0]
    ver = subprocess.run([cc, "--version"], capture_output=True, text=True)
    compiler = (
        ver.stdout.splitlines()[0].strip() if ver.returncode == 0 else cc
    )
    flags = " ".join(dict.fromkeys(FLAG_RE.findall(cmd)))  # de-dup, keep order
    return compiler, flags


def _readfile(path):
    try:
        with open(path) as fh:
            return fh.read().strip()
    except OSError:
        return ""


def collect_meta(machine, compiler, flags, commit, measured_at):
    """Reproducibility metadata for a published snapshot — what you want when a
    regression appears: when it ran, which commit, the compiler + flags, the
    CPU's *state* (governor/boost decide whether numbers are even repeatable),
    and the library versions that move DSP numbers (NumPy; glibc, whose libmvec
    owns vectorised sincos/exp)."""
    import platform

    cpu = machine.get("cpu")
    cpu = cpu.get("brand_raw") if isinstance(cpu, dict) else cpu
    try:
        import numpy

        numpy_v = numpy.__version__
    except Exception:
        numpy_v = ""
    cv = subprocess.run(["cmake", "--version"], capture_output=True, text=True)
    cmake_v = cv.stdout.split()[2] if cv.returncode == 0 else ""
    boost = _readfile("/sys/devices/system/cpu/cpufreq/boost")
    return {
        "measured_at": measured_at,
        "commit": commit,
        "compiler": compiler,
        "flags": flags,
        "cpu": cpu,
        "cores": os.cpu_count(),
        "cpu_governor": _readfile(
            "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
        )
        or None,
        "cpu_boost": {"1": "on", "0": "off"}.get(boost),
        "kernel": machine.get("release") or platform.release(),
        "python": machine.get("python_version") or platform.python_version(),
        "numpy": numpy_v,
        "glibc": " ".join(platform.libc_ver()).strip(),
        "cmake": cmake_v,
    }


def _git_short_sha():
    r = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"], capture_output=True, text=True
    )
    return r.stdout.strip() if r.returncode == 0 else ""


def cmd_publish(version, build) -> int:
    """Stamp this machine's ``make bench`` output into published/v<ver>/."""
    if build not in BUILDS:
        print(f"--build must be one of {BUILDS}")
        return 1
    ver = f"v{version.lstrip('v')}"
    py = sorted(
        (
            f
            for f in glob.glob(os.path.join(LOCAL, "*.json"))
            if not f.endswith("-c.json")
        ),
        key=os.path.getmtime,
    )
    if not py:
        print(f"No local snapshot in {LOCAL}/ — run `make bench` first.")
        return 1
    latest = py[-1]
    tag = os.path.basename(latest)[: -len(".json")]
    compiler, flags = _build_info()
    dst = os.path.join(PUBLISHED, ver)
    os.makedirs(dst, exist_ok=True)
    for src_name, out_name in (
        (f"{tag}.json", f"{build}.json"),
        (f"{tag}-c.json", f"{build}-c.json"),
    ):
        src = os.path.join(LOCAL, src_name)
        if not os.path.exists(src):
            continue
        with open(src) as fh:
            data = json.load(fh)
        data["doppler_meta"] = collect_meta(
            data.get("machine_info", {}),
            compiler,
            flags,
            _git_short_sha(),
            data.get("datetime", ""),
        )
        with open(os.path.join(dst, out_name), "w") as fh:
            json.dump(data, fh, indent=1)
            fh.write("\n")
    print(f"published {ver}/{build}  [{compiler}; {flags}]")
    return 0


# ── render the page ──────────────────────────────────────────────────────────


def _speedup(portable, native, throughput):
    """Native-vs-portable as a signed %: higher throughput / lower time = +."""
    if not portable or not native:
        return None
    return (
        (native / portable - 1.0) * 100.0
        if throughput
        else (portable / native - 1.0) * 100.0
    )


def _two_col_rows(rel, suite, throughput):
    """(disp, portable_val, native_val, speedup_pct) for the chosen metric."""
    keys = set()
    for b in BUILDS:
        if b in rel:
            for k, e in rel[b]["suites"].get(suite, {}).items():
                if (e["msas"] is not None) == throughput:
                    keys.add(k)
    rows = []
    for k in keys:
        p = rel.get("portable", {}).get("suites", {}).get(suite, {}).get(k)
        n = rel.get("native", {}).get("suites", {}).get(suite, {}).get(k)
        disp = (p or n)["disp"]
        pv = (p["msas"] if throughput else p["mean"]) if p else None
        nv = (n["msas"] if throughput else n["mean"]) if n else None
        rows.append((disp, pv, nv, _speedup(pv, nv, throughput)))
    # headline ordering: fastest throughput / slowest time first
    rows.sort(
        key=lambda r: (
            -(r[1] or r[2] or 0) if throughput else (r[1] or r[2] or 0)
        )
    )
    return rows


def cmd_page(published, out_path) -> int:
    if not published:
        msg = (
            f"No published snapshots under {PUBLISHED}/ — "
            "run `make bench && make bench-publish ...` first."
        )
        if not out_path:
            print(msg)
        else:
            with open(out_path, "w") as fh:
                fh.write("# Benchmarks\n\n_No published numbers yet._\n")
        return 1
    versions = list(published)
    latest = versions[-1]
    rel = published[latest]
    ref = rel.get("portable") or rel.get("native")
    cpu = ref["machine"].get("cpu")
    cpu = cpu.get("brand_raw") if isinstance(cpu, dict) else (cpu or "?")
    pyv = ref["machine"].get("python_version", "?")
    pb = rel.get("portable", {}).get("meta", {})
    nb = rel.get("native", {}).get("meta", {})
    m = pb or nb  # shared environment (same machine/session)

    L = [
        "<!-- generated by `make bench-docs`"
        " (scripts/bench_report.py --page);"
        " regenerate on a representative machine, don't hand-edit -->",
        "# Benchmarks",
        "",
        f"Representative single-machine numbers for **{latest}**,"
        " committed by hand — *not* from CI"
        " (shared runners aren't hardware-representative).",
        "",
        "Two builds are shown so you can see the from-source upside:",
        "",
        f"- **portable** — `{pb.get('flags', '?')}` — the shipped PyPI wheel.",
        f"- **native** — `{nb.get('flags', '?')}` — `-DDOPPLER_NATIVE=ON`, "
        "built from source for this CPU.",
        "",
        "**Environment** "
        f"— measured {m.get('measured_at', '?')[:19].replace('T', ' ')} UTC, "
        f"doppler `{m.get('commit', '?')}`, {m.get('compiler', '?')}.",
        "",
        f"- CPU: **{cpu}** — {m.get('cores', '?')} threads, governor "
        f"`{m.get('cpu_governor') or '?'}`, boost {m.get('cpu_boost') or '?'}",
        f"- OS: {m.get('kernel', '?')}, {m.get('glibc') or 'glibc ?'}",
        f"- Libs: Python {m.get('python', pyv)}, NumPy "
        f"{m.get('numpy') or '?'}, CMake {m.get('cmake') or '?'}",
        "",
        "Throughput is **MSa/s** (higher is better); latency ops are mean "
        "**time/call** (lower is better). *from src* = native vs portable.",
        "",
        "> The two builds are measured **interleaved** (alternating runs, "
        "per-benchmark best), so *from src* reflects the real build"
        " difference. Big gains are vectorizable kernels under AVX-512;"
        " overhead-bound benches (tiny per-call work) sit near 0% because"
        " the build can't help where Python-call overhead dominates.",
        "",
    ]
    for suite in SUITES:
        if not any(b in rel and rel[b]["suites"].get(suite) for b in BUILDS):
            continue
        L += [f"## {SUITE_LABEL[suite]}", ""]
        for throughput in (True, False):
            rows = _two_col_rows(rel, suite, throughput)
            if not rows:
                continue
            fmt = fmt_tput if throughput else fmt_time
            head = "throughput" if throughput else "time/call"
            L += [
                f"### {'Throughput' if throughput else 'Latency'}",
                "",
                f"| benchmark | portable {head} | native {head} | from src |",
                "| --- | ---: | ---: | ---: |",
            ]
            for disp, pv, nv, sp in rows:
                pc = fmt(pv) if pv else "—"
                nc = fmt(nv) if nv else "—"
                spc = f"{sp:+.0f}%" if sp is not None else "—"
                L.append(f"| `{disp}` | {pc} | {nc} | {spc} |")
            L.append("")

    if len(versions) >= 2:
        L += _history_section(published)

    text = "\n".join(L).rstrip("\n") + "\n"
    if out_path:
        with open(out_path, "w") as fh:
            fh.write(text)
        print(f"wrote {out_path} ({cpu}, {len(versions)} release(s))")
    else:
        print(text)
    return 0


def _history_section(published):
    """Portable-build MSa/s of throughput benches across releases (the wheel
    trend users actually track)."""
    versions = list(published)
    L = [
        "## Release history (portable build)",
        "",
        "Portable-build throughput across releases — the wheel numbers. "
        "Comparable only across releases measured on the same machine.",
        "",
    ]
    for suite in SUITES:
        # benchmarks present (throughput) in the latest portable build
        latest = published[versions[-1]].get("portable")
        if not latest:
            continue
        keys = [
            k
            for k, e in latest["suites"].get(suite, {}).items()
            if e["msas"] is not None
        ]
        if not keys:
            continue
        keys.sort(key=lambda k: -latest["suites"][suite][k]["msas"])
        L += [
            f"### {SUITE_LABEL[suite]}",
            "",
            "| benchmark | " + " | ".join(versions) + " |",
            "| --- |" + " ---: |" * len(versions),
        ]
        for k in keys:
            cells = []
            for v in versions:
                e = (
                    published[v]
                    .get("portable", {})
                    .get("suites", {})
                    .get(suite, {})
                    .get(k)
                )
                cells.append(fmt_tput(e["msas"]) if e and e["msas"] else "—")
            disp = latest["suites"][suite][k]["disp"]
            L.append(f"| `{disp}` | " + " | ".join(cells) + " |")
        L.append("")
    return L


# ── stdout comparison (portable trend, quick check) ──────────────────────────


def cmd_compare(published) -> int:
    versions = list(published)
    if len(versions) < 2:
        print(
            "Need ≥2 published releases to compare. "
            f"Have: {versions or 'none'}."
        )
        return 1
    prev, cur = versions[-2], versions[-1]
    for suite in SUITES:
        pcur = (
            published[cur].get("portable", {}).get("suites", {}).get(suite, {})
        )
        pprev = (
            published[prev]
            .get("portable", {})
            .get("suites", {})
            .get(suite, {})
        )
        rows = []
        for k, e in pcur.items():
            if e["msas"] and k in pprev and pprev[k]["msas"]:
                a, b = pprev[k]["msas"], e["msas"]
                rows.append((e["disp"], a, b, (b - a) / a * 100))
        if not rows:
            continue
        rows.sort(key=lambda r: -abs(r[3]))
        print(
            f"\n=== {SUITE_LABEL[suite]} portable: {cur} vs {prev} (MSa/s) ==="
        )
        for disp, a, b, pct in rows[:25]:
            flag = " ⚠" if pct < -10 else (" ✓" if pct > 10 else "")
            print(
                f"{disp:42}  {fmt_tput(a):>11}  {fmt_tput(b):>11}  "
                f"{pct:>+6.1f}%{flag}"
            )
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument(
        "--page",
        action="store_true",
        help="render the docs page from benchmarks/published/",
    )
    p.add_argument("--out", default=None, help="write --page here")
    p.add_argument(
        "--publish",
        metavar="VERSION",
        help="stamp THIS machine's latest run into published/",
    )
    p.add_argument("--build", choices=BUILDS, default="portable")
    a = p.parse_args()
    if a.publish:
        return cmd_publish(a.publish, a.build)
    if a.page:
        return cmd_page(load_published(), a.out)
    return cmd_compare(load_published())


if __name__ == "__main__":
    raise SystemExit(main())
