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

The page converts every release to one quantity, seconds per unit
(`seconds_per_unit`), and reads each row as a rate in its domain -- input
samples per second for a stream -- or as time per call (`rate_of`).

Workflow
--------
A release's numbers are published by the steps in docs/dev/release.md §2b,
their one home (`make bench-interleaved`, `make bench-labels-fix`, `make
bench-docs`). The entry points here are those targets' and:

    make bench-publish VERSION=X.Y.Z BUILD=portable   # stamp one build by hand
    make bench-report      # portable trend across releases, to stdout
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import subprocess
import sys
from pathlib import Path

PUBLISHED = "benchmarks/published"
LOCAL = "benchmarks/history"
#: Where the C benchmarks live: each ``bench_<x>_core.c`` is component ``x``.
BENCH_SRC = "native/benchmarks"
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
    """{fullname: {disp, msas}} for one snapshot's benchmarks: what
    `cmd_compare` reads. The page converts rows with `seconds_per_unit`.
    """
    out = {}
    for b in data.get("benchmarks", []):
        mean = b.get("stats", {}).get("mean")
        if not (isinstance(mean, (int, float)) and mean > 0):
            continue
        msas = b.get("extra_info", {}).get("MSa_s")
        out[b.get("fullname") or b["name"]] = {
            "disp": _display_name(suite, b),
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


def collect_meta(
    machine, compiler, flags, commit, measured_at, pinned_cpus=None
):
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
        # The ACPI platform profile sets the package power limit; it is NOT the
        # governor. v0.56.0's first measurement ran at governor `performance`
        # under profile `balanced`, and nothing here recorded it.
        "platform_profile": _readfile("/sys/firmware/acpi/platform_profile")
        or None,
        # The affinity the MEASUREMENT ran under, when it was narrowed: a
        # published number is not comparable without knowing which core class
        # produced it. None means unpinned (see `fastest_cpus`).
        "pinned_cpus": pinned_cpus,
        "kernel": machine.get("release") or platform.release(),
        "python": machine.get("python_version") or platform.python_version(),
        "numpy": numpy_v,
        "glibc": " ".join(platform.libc_ver()).strip(),
        "cmake": cmake_v,
    }


def _cpu_ranges(cpus):
    """`[0, 1, 2, 3, 10, 11]` -> `"0-3,10-11"`, the spelling taskset takes.

    Examples
    --------
    >>> _cpu_ranges([0, 1, 2, 3, 10, 11, 12, 13])
    '0-3,10-13'
    >>> _cpu_ranges([5])
    '5'
    """
    out, run = [], []
    for n in [*sorted(cpus), None]:
        if run and n is not None and n == run[-1] + 1:
            run.append(n)
            continue
        if run:
            out.append(str(run[0]) if len(run) == 1 else f"{run[0]}-{run[-1]}")
        run = [n]
    return ",".join(out)


def machine_not_ready(
    cpu_sysfs="/sys/devices/system/cpu",
    profile_path="/sys/firmware/acpi/platform_profile",
    loadavg_path="/proc/loadavg",
    max_load=1.0,
):
    r"""Why this machine must not measure a release, or [] if it may.

    A published number is only comparable if it was measured in the same
    peak, quiet state every release. Three things decide that, and a snapshot
    that recorded only the first was taken with the other two wrong (v0.56.0,
    first attempt): every core's governor `performance`; the ACPI platform
    profile `performance` where the machine has one (it sets the package
    power limit a sustained run is held to, independently of the governor);
    and nothing else running -- a 1-minute load average under ``max_load``
    (a docker stream stack had been up for nine days).

    Parameters
    ----------
    cpu_sysfs, profile_path, loadavg_path : str
        Where to read them; the defaults are the live machine.
    max_load : float
        The 1-minute load average above which the machine is not quiet.

    Returns
    -------
    list of str
        One line per problem, each naming the fix.

    Examples
    --------
    >>> import os, tempfile
    >>> d = tempfile.mkdtemp()
    >>> for c in ("cpu0", "cpu1"):
    ...     os.makedirs(f"{d}/{c}/cpufreq")
    ...     gov = f"{d}/{c}/cpufreq/scaling_governor"
    ...     _ = open(gov, "w").write("performance\n")
    >>> _ = open(f"{d}/profile", "w").write("balanced\n")
    >>> _ = open(f"{d}/loadavg", "w").write("0.20 0.30 0.40 1/500 1\n")
    >>> machine_not_ready(d, f"{d}/profile", f"{d}/loadavg")
    ['platform profile is balanced: powerprofilesctl set performance']
    """
    problems = []
    govs = {
        _readfile(g)
        for g in glob.glob(
            os.path.join(cpu_sysfs, "cpu[0-9]*/cpufreq/scaling_governor")
        )
    }
    if govs and govs != {"performance"}:
        problems.append(
            f"governor is {'/'.join(sorted(govs))}: "
            "sudo cpupower frequency-set -g performance"
        )
    profile = _readfile(profile_path)
    if profile and profile != "performance":
        problems.append(
            f"platform profile is {profile}: powerprofilesctl set performance"
        )
    load = _readfile(loadavg_path).split()
    if load and float(load[0]) >= max_load:
        problems.append(
            f"1-minute load is {load[0]} (>= {max_load}): stop what is "
            "running (docker ps) and wait for the machine to settle"
        )
    return problems


def fastest_cpus(sysfs="/sys/devices/system/cpu"):
    """Logical CPUs of the fastest core class; None if there is only one.

    A heterogeneous CPU schedules a benchmark onto whichever core is free, and
    the classes are not close: on the Ryzen AI 9 465 the published numbers come
    from (4 Zen 5 at 5.09 GHz, 6 Zen 5c at 3.35 GHz) the same binary measured
    3.5 us/call pinned to a Zen 5 core and 5.6 us/call pinned to a Zen 5c one.
    Unpinned, a benchmark is therefore BIMODAL, and min-of-K only usually finds
    the fast mode: across 465 benchmarks a handful lose the coin toss K times
    running and publish a 1.6x "regression" with no source change behind it
    (measured on the v0.53.0 snapshot -- `awgn` read slower native than
    portable, and re-ran identical in both builds).

    The class is read from ``cpuinfo_max_freq``, which the kernel reports per
    logical CPU and which does not move with load or governor. Returns None
    when every CPU reports the same ceiling, or none report one, so a uniform
    machine and a container without cpufreq are both left alone.

    Parameters
    ----------
    sysfs : str
        Root of the per-CPU sysfs tree; a parameter so the test can point it
        at a fake one.

    Returns
    -------
    list[int] | None
        Sorted logical CPU numbers of the fastest class, or None.

    Examples
    --------
    >>> import os, tempfile
    >>> root = tempfile.mkdtemp()
    >>> for n, khz in enumerate([5090000, 3350000, 5090000]):
    ...     d = os.path.join(root, f"cpu{n}", "cpufreq")
    ...     os.makedirs(d)
    ...     _ = open(os.path.join(d, "cpuinfo_max_freq"), "w").write(str(khz))
    >>> fastest_cpus(root)
    [0, 2]
    >>> os.remove(os.path.join(root, "cpu1", "cpufreq", "cpuinfo_max_freq"))
    >>> fastest_cpus(root) is None  # one class left: nothing to choose
    True
    """
    ceilings = {}
    for path in glob.glob(os.path.join(sysfs, "cpu[0-9]*")):
        khz = _readfile(os.path.join(path, "cpufreq", "cpuinfo_max_freq"))
        if khz and khz.isdigit():
            ceilings[int(os.path.basename(path)[3:])] = int(khz)
    if len(set(ceilings.values())) < 2:
        return None
    top = max(ceilings.values())
    return sorted(n for n, khz in ceilings.items() if khz == top)


def _git_short_sha():
    r = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"], capture_output=True, text=True
    )
    return r.stdout.strip() if r.returncode == 0 else ""


def missing_components(c_report: dict, bench_dir: str = BENCH_SRC) -> list:
    """The tree's C benchmarks a C snapshot did not record, sorted.

    The full set is derived, never written down: every
    ``native/benchmarks/bench_<x>_core.c`` is component ``x``, and jm names
    each row it records ``x::<entry>``. (v0.65.0's published C snapshot held
    every one of them, so nothing in the tree is legitimately absent.) A
    snapshot short of that set came from a filtered or broken run -- ``make
    bench BENCH_ARGS="..."`` filters the C side, and a benchmark that
    crashed records nothing -- and publishing it would ship a release's
    numbers with components silently gone.

    >>> import os, tempfile
    >>> d = tempfile.mkdtemp()
    >>> for x in ("fir", "rs"):
    ...     open(os.path.join(d, f"bench_{x}_core.c"), "w").close()
    >>> missing_components({"benchmarks": [{"name": "fir::step"}]}, d)
    ['rs']
    >>> rows = [{"name": "fir::step"}, {"name": "rs::decode"}]
    >>> missing_components({"benchmarks": rows}, d)
    []
    """
    want = {
        os.path.basename(p)[len("bench_") : -len("_core.c")]
        for p in glob.glob(os.path.join(bench_dir, "bench_*_core.c"))
    }
    have = {b["name"].split("::")[0] for b in c_report.get("benchmarks", [])}
    return sorted(want - have)


def cmd_publish(version, build) -> int:
    """Stamp this machine's ``make bench`` output into published/v<ver>/.

    Refuses, publishing nothing, unless the run is whole: the newest snapshot
    must have its C half, and that half must hold every component the tree
    benchmarks (see :func:`missing_components`).
    """
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
    c_src = os.path.join(LOCAL, f"{tag}-c.json")
    if not os.path.exists(c_src):
        print(
            f"refusing to publish: snapshot {tag} has no C half"
            f" ({tag}-c.json). A Python-only run (--python-only,"
            " `make bench-save`) is half a release; run a full `make bench`."
        )
        return 1
    with open(c_src) as fh:
        missing = missing_components(json.load(fh))
    if missing:
        print(
            f"refusing to publish: {tag}-c.json is missing {len(missing)} of"
            f" the tree's C benchmarks: {', '.join(missing)}. A `make bench"
            ' BENCH_ARGS="..."` run, or a benchmark that failed, records only'
            " part of the set; publish a full `make bench`."
        )
        return 1
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


# ── the benchmarks page (#2372) ──────────────────────────────────────────────
#
# The page is generated from the committed snapshots alone: one row per
# benchmark that the NEWEST release still measures (a retired row is dropped),
# with its value in every release converted to ONE quantity -- seconds per
# unit -- so a trend is continuous across the releases that recorded it
# differently. docs/javascripts/benchmarks.js draws the cards, the family
# tables and the charts from the JSON this embeds; everything a reader needs
# without JavaScript (the headline numbers, the definitions) is plain
# markdown. `--check` re-renders and compares, so the page cannot drift from
# the data or from the label table.

#: The global the page's data lands in, which benchmarks.js reads. The data
#: is a script that ASSIGNS it, never an inert ``<script type=...
#: id=...>`` block found by id: the site's instant navigation re-creates
#: each inline script in a page it swaps in with its text alone, so an
#: attribute does not survive a click from another page (#2391).
PAGE_DATA = "dpBenchData"

#: Plain name, unit and one-sentence definition per row: the label table.
LABELS = "benchmarks/labels.toml"
#: Live rows that have no label yet. A RATCHET: it only shrinks.
LABELS_EXEMPT = "benchmarks/.labels-exempt"
#: C units for releases that never recorded one (#2314); it only empties.
LEGACY_UNITS = "benchmarks/legacy-units.toml"
#: The jm manifest: which module each object belongs to.
MANIFEST = "just-makeit.toml"

# The coverage gate's plausibility ceiling, in MSa/s: one definition of
# "too fast to be a real kernel" for both. A sibling module; this file may
# be loaded by path, not run.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _gitbase import (  # noqa: E402
    BaseUnreadableError,
    added_since_base,
    in_git_repo,
    resolve_base,
    show_at,
)
from check_bench_coverage import CEILING_MSA_S  # noqa: E402

#: The families the page groups rows into, in tab order.
FAMILIES = (
    ("spectral", "Spectra & measurement"),
    ("filter", "Filters & rate change"),
    ("track", "Tracking & sync"),
    ("acq", "Acquisition & DSSS"),
    ("wfm", "Waveforms & coding"),
    ("data", "Conversion, buffers & streams"),
)

#: Every jm module's family. A module missing here fails the label gate, so
#: a new module is filed the day it lands. Objects reach a module through
#: the manifest's `[module.*].objects`, not through a second list here.
MODULE_FAMILY = {
    "spectral": "spectral",
    "measure": "spectral",
    "analyzer": "spectral",
    "ber": "spectral",
    "snr": "spectral",
    "filter": "filter",
    "resample": "filter",
    "ddc": "filter",
    "delay": "filter",
    "interp": "filter",
    "agc": "filter",
    "track": "track",
    "detection": "track",
    "acquire": "acq",
    "dsss": "acq",
    "wfm": "wfm",
    "wfm_compose": "wfm",
    "wfm_plan": "wfm",
    "wfm_sink": "wfm",
    "source": "wfm",
    "coding": "wfm",
    "ccsds": "wfm",
    "mpsk": "wfm",
    "impairment": "wfm",
    "cvt": "data",
    "accumulator": "data",
    "arith": "data",
    "buffer": "data",
    "stream": "data",
    "wfm_reader": "data",
    "wfm_writer": "data",
    "telemetry": "data",
    "interrupt": "data",
    "sample_clock": "data",
    "util": "data",
}

#: C bench components that are neither a manifest object nor a module name:
#: c_deps and helpers benchmarked on their own. Exact names only -- a
#: substring rule filed `loop_filter` under filters and `carrier_acq` under
#: whatever matched first.
COMPONENT_MODULE = {
    "ccsds_tm": "ccsds",
    "conv": "coding",
    "resamp": "resample",
    "rs": "coding",
    "timing": "sample_clock",
}

#: How many INPUT SAMPLES one recorded unit consumes (#2372): the page
#: reads every stream row in samples per second, because a real-time chain
#: has to eat its input faster than the sample rate brings it. An FFT bin is
#: one input sample (an n-point transform eats n); an input pair is two. A
#: frame consumes its `nfft`, read from the row's name.
UNIT_SAMPLES = {
    "sample": 1,
    "in_sample": 1,
    "bin": 1,
    "elem": 1,
    "in_pair": 2,
}
#: Units whose input is not samples read in their own rate.
UNIT_DOMAIN = {
    "symbol": "sym",
    "sym": "sym",
    "bit": "bit",
    "codeword": "codeword",
    "chip": "chip",
    "event": "event",
    "eval": "eval",
    "tap": "tap",
    "boundary": "boundary",
    "record": "record",
}
#: Units that are setup, not a stream: read as time per call.
CONTROL_UNITS = ("create", "arm")
#: Every unit word the page knows how to read; a recorded unit outside
#: it fails the label gate. `call` and `frame` are read by rule, not table.
KNOWN_UNITS = {
    *UNIT_SAMPLES,
    *UNIT_DOMAIN,
    *CONTROL_UNITS,
    "call",
    "frame",
}
#: Operation names that are setup, control or per-call bookkeeping -- they
#: leave the throughput view whatever unit they recorded as `call`.
CONTROL_OP = re.compile(
    r"(^|[^a-z])(create|destroy|reset|retune|configure|reseed|set_|"
    r"get_state|set_state|state_bytes|dump|get$|get\[|value|baseline|init|"
    r"open|close|alloc|clear|plan)",
    re.I,
)
#: A stream verb, for a row whose unit nothing records yet.
STREAM_OP = re.compile(
    r"steps?|execute|generate|push|accumulate|decimate|interp|filter|"
    r"convert|madd|add2d|search|process|run|demod|despread|rotate|prod|"
    r"fold|correlate|mix|write|read|analy[sz]e|encode|decode|map|find",
    re.I,
)


def rate_of(rid: str, unit, label=None):
    """``(count, domain, inferred)`` for one row.

    ``count`` units of ``domain`` per recorded unit: ``(1, "Sa", False)``
    for a sample row, ``(2040, "bit", False)`` for an RS codeword read in
    coded bits. ``domain`` None is setup, control or a read-out, which reads
    as time per call, and ``count`` is then how many recorded units one call
    is (a read-out of 4,096 bins recorded per bin is 4,096).

    A label's ``samples`` or ``domain``/``count`` decides; else the recorded
    unit; else the operation's name. A count read from the NAME -- a
    frame's ``nfft``, a Python call's ``64k``, a one-sample ``step`` -- is
    ``inferred``, because nothing the snapshot records states it.

    >>> rate_of("c:fft::execute_cf32[n=4096]", "bin")
    (1, 'Sa', False)
    >>> rate_of("c:psd::fft[nfft=256]", "frame")
    (256, 'Sa', True)
    >>> rs = {"domain": "bit", "count": 2040}
    >>> rate_of("c:rs::decode[e=8]", "codeword", rs)
    (2040, 'bit', False)
    >>> rate_of("c:acc_f32::get", "call")
    (1, None, False)
    >>> rate_of("c:psd::power_onesided[nfft=1024]", "call")
    (1, None, False)
    >>> rate_of("c:acc_trace::value", "bin", {"domain": "call", "count": 4096})
    (4096, None, False)
    >>> rate_of("c:acc_f32::step", None)
    (1, 'Sa', True)
    >>> rate_of("py:i16_to_f32::test_bench_steps_64k", "call")
    (65536, 'Sa', True)
    """
    label = label or {}
    if label.get("domain") == "call":
        return int(label.get("count", 1)), None, False
    if "samples" in label:
        return int(label["samples"]), "Sa", False
    if label.get("domain"):
        return int(label.get("count", 1)), label["domain"], False
    op = rid.split("::", 1)[1] if "::" in rid else rid
    if unit in UNIT_SAMPLES:
        return UNIT_SAMPLES[unit], "Sa", False
    if unit in UNIT_DOMAIN:
        return 1, UNIT_DOMAIN[unit], False
    if unit in CONTROL_UNITS or CONTROL_OP.search(op):
        return 1, None, False
    # Past here the count comes from the row's name, never from the
    # snapshot, so every result says so.
    if unit == "frame":
        nfft = re.search(r"nfft\s*=\s*(\d+)", op)
        return (
            (int(nfft.group(1)), "Sa", True) if nfft else (1, "frame", False)
        )
    blk = re.search(r"(\d+)k(?![a-z])", op, re.I)
    if rid.startswith("py:") and blk and STREAM_OP.search(op):
        return int(blk.group(1)) * 1024, "Sa", True
    if re.search(r"(^|_)step(_|$|\[)", op) or (
        unit is None and re.search(r"(^|_)steps(_|$|\[)", op)
    ):
        return 1, "Sa", True
    if unit is None and STREAM_OP.search(op):
        return 1, "Sa", True
    return 1, None, False


def label_count_problem(rid: str, label: dict, unit):
    """Why a label's count contradicts the row's recorded unit, or None."""
    if "samples" not in label and not label.get("domain"):
        return (
            f"{rid}: label declares no count -- `samples = N`, or a "
            "`domain` and `count`"
        )
    if unit is None:
        return None
    want = UNIT_SAMPLES.get(unit)
    if (
        "samples" in label
        and want is not None
        and int(label["samples"]) != want
    ):
        return (
            f"{rid}: label counts {label['samples']} samples per unit, but "
            f"the snapshot's unit `{unit}` is {want}"
        )
    if label.get("domain") not in (None, "call") and want is not None:
        return (
            f"{rid}: label reads in `{label['domain']}`, but the snapshot's "
            f"unit `{unit}` is samples"
        )
    if unit in CONTROL_UNITS and (
        "samples" in label or label.get("domain") not in (None, "call")
    ):
        return f"{rid}: label gives a rate, but `{unit}` is setup"
    return None


try:  # 3.11+; tomli is the same parser, marker-gated in the dev group
    import tomllib as _toml
except ImportError:  # pragma: no cover - the floor Python
    import tomli as _toml  # type: ignore[no-redef]


def row_id(suite: str, bench: dict) -> str:
    """The page's stable key for one benchmark row.

    ``c:<component>::<row>`` for a C row (jm_bench already names it
    ``component::row``) and ``py:<module>::<test>`` for a Python one, the
    bench file's stem without its ``bench_`` prefix.

    >>> row_id("c", {"name": "costas::steps"})
    'c:costas::steps'
    >>> row_id("python", {"name": "bench_i16_to_f32::test_bench_steps_64k"})
    'py:i16_to_f32::test_bench_steps_64k'
    """
    if suite == "c":
        return "c:" + bench["name"]
    return "py:" + bench["name"].removeprefix("bench_")


#: The rate a Python bench records, by ``extra_info`` key, and the unit it
#: counts -- in precedence order, because a row may record two. A generator
#: of bits or chips that ALSO wrote ``MSa_s`` (pn, gold) produces no samples,
#: so its own key wins; a receiver that records ``Msym_s`` beside ``MSa_s``
#: (carrier_mpsk) eats samples, so ``MSa_s`` wins over symbols out.
PY_RATE_KEYS = (
    ("Mbit_s", "bit"),
    ("Mchip_s", "chip"),
    ("MSa_s", "sample"),
    ("Msym_s", "symbol"),
    ("Mevent_s", "event"),
    ("Mframe_s", "frame"),
    ("Mrecord_s", "record"),
)


def _py_rate(bench: dict):
    """``(per second in millions, unit)`` a Python row records, or None.

    >>> _py_rate({"extra_info": {"MSa_s": 2.0, "Mbit_s": 2.0}})
    (2.0, 'bit')
    >>> _py_rate({"extra_info": {"MSa_s": 4.0, "Msym_s": 1.0}})
    (4.0, 'sample')
    >>> _py_rate({"extra_info": {}}) is None
    True
    """
    extra = bench.get("extra_info") or {}
    for key, unit in PY_RATE_KEYS:
        v = extra.get(key)
        if isinstance(v, (int, float)) and v > 0:
            return float(v), unit
    return None


def seconds_per_unit(suite: str, bench: dict):
    """(seconds per unit, basis) for one row, or None.

    The one quantity every release is converted to (#2372). ``MSa_s``, where
    a C row carries it, is samples per second, so a unit is a sample. A C
    row without it is ``stats.mean`` over ``stats.iterations`` -- what
    jm_bench's ``mean_per_unit`` is, and what no release before v0.73.0
    wrote. A Python row is per unit of the rate it records (`_py_rate`),
    else per call: pytest-benchmark's mean is already per call, whatever
    its ``iterations`` says. ``basis`` is what the value is per; `breaks`
    says which change of it, and of the recorded unit, breaks a trend.

    >>> rnd = {"mean": 4e-5, "iterations": 8}
    >>> seconds_per_unit("c", {"stats": rnd, "extra_info": {}})
    (5e-06, ('it', 8))
    >>> seconds_per_unit("c", {"stats": rnd, "extra_info": {"MSa_s": 0.2}})
    (5e-06, ('it', 8))
    >>> seconds_per_unit("python", {"stats": {"mean": 2e-6}, "extra_info": {}})
    (2e-06, ('call',))
    >>> py = {"stats": {"mean": 2.288e-8, "iterations": 220}}
    >>> seconds_per_unit("python", {**py, "extra_info": {}})
    (2.288e-08, ('call',))
    >>> seconds_per_unit("python", {**py, "extra_info": {"Mbit_s": 500.0}})
    (2e-09, ('bit',))
    """
    stats = bench.get("stats", {})
    mean = stats.get("mean")
    if not (isinstance(mean, (int, float)) and mean > 0):
        return None
    msas = (bench.get("extra_info") or {}).get("MSa_s")
    if suite == "c":
        iters = stats.get("iterations") or 1
        basis = ("it", iters)
        if isinstance(msas, (int, float)) and msas > 0:
            return 1.0 / (msas * 1e6), basis
        return mean / iters, basis
    rate = _py_rate(bench)
    if rate:
        return 1.0 / (rate[0] * 1e6), (rate[1],)
    return float(mean), ("call",)


def _recorded_unit(suite: str, bench: dict):
    """The unit the snapshot itself states, or None (#2314)."""
    if suite == "c":
        return (bench.get("extra_info") or {}).get("unit")
    rate = _py_rate(bench)
    return rate[1] if rate else "call"


def _component(suite: str, bench: dict) -> str:
    if suite == "c":
        return bench["name"].split("::", 1)[0]
    m = re.search(r"src/doppler/([^/]+)/", bench.get("fullname", ""))
    return m.group(1) if m else ""


def module_of(suite: str, component: str, obj2mod: dict, modules) -> str:
    """The jm module a row belongs to, or '' when nothing claims it.

    A Python row's component IS its module (the bench file's package). A C
    component is a manifest object, a module name, or an exact
    COMPONENT_MODULE entry, in that order.
    """
    if suite != "c":
        return component if component in modules else ""
    if component in obj2mod:
        return obj2mod[component]
    if component in modules:
        return component
    return COMPONENT_MODULE.get(component, "")


def read_manifest(root: str = ".") -> tuple[dict, list]:
    """({object: module}, [modules]) from just-makeit.toml."""
    with open(os.path.join(root, MANIFEST), "rb") as fh:
        mods = _toml.load(fh).get("module", {})
    obj2mod = {
        o: m
        for m, v in mods.items()
        if isinstance(v, dict)
        for o in v.get("objects", [])
    }
    return obj2mod, list(mods)


def read_labels(root: str = ".") -> dict:
    """{row id: {name, unit, definition, headline}} from LABELS, in order."""
    path = os.path.join(root, LABELS)
    if not os.path.exists(path):
        return {}
    with open(path, "rb") as fh:
        return dict(_toml.load(fh))


def read_legacy_units(root: str = ".") -> dict:
    """{row id: unit} from LEGACY_UNITS; absent is empty."""
    path = os.path.join(root, LEGACY_UNITS)
    if not os.path.exists(path):
        return {}
    with open(path, "rb") as fh:
        return dict(_toml.load(fh))


def read_exempt(text: str) -> list:
    """The row ids an exempt list holds; `#` comments and blanks out."""
    return [
        s.strip()
        for s in text.splitlines()
        if s.strip() and not s.lstrip().startswith("#")
    ]


def _raw_releases(root: str = PUBLISHED) -> list:
    """[(version, {build: {suite: [bench, ...]}, meta})] oldest first."""
    out = []
    for d in glob.glob(os.path.join(root, "v*")):
        v = os.path.basename(d)
        if _version_key(v) is None:
            continue
        builds, meta = {}, {}
        for b in BUILDS:
            suites = {}
            pj = os.path.join(d, f"{b}.json")
            if os.path.exists(pj):
                with open(pj, encoding="utf-8") as fh:
                    data = json.load(fh)
                suites["python"] = data.get("benchmarks", [])
                if not meta:
                    m = data.get("doppler_meta") or data.get("doppler_build")
                    cpu = data.get("machine_info", {}).get("cpu")
                    meta = dict(m or {})
                    meta["cpu"] = (
                        cpu.get("brand_raw") if isinstance(cpu, dict) else cpu
                    )
            cj = os.path.join(d, f"{b}-c.json")
            if os.path.exists(cj):
                with open(cj, encoding="utf-8") as fh:
                    suites["c"] = json.load(fh).get("benchmarks", [])
            if suites:
                builds[b] = suites
        if builds:
            out.append((v, builds, meta))
    return sorted(out, key=lambda t: _version_key(t[0]))


def _sig(x):
    """Four significant digits: what a chart can show, a third the bytes."""
    return None if x is None else float(f"{x:.4g}")


def breaks(bases: list, units: list) -> list:
    """Indices of the releases where a row's value changed meaning.

    ``bases[i]`` is what release i's value is per (`seconds_per_unit`),
    ``units[i]`` the unit that release recorded, None for a C row from
    before #2314 (every release before v0.73.0). What breaks depends on
    the row:

    - a C row between two releases that both recorded a unit breaks where
      the UNIT changed. A round that only grew, same unit, is the same
      quantity, and a break there would hide a real since-last change;
    - a C row where either release recorded no unit -- any two before
      v0.73.0, or across into the first set with units, v0.73.0 -- has only
      its round to go on, so a change in ``iterations`` breaks. (v0.73.0
      changes no row's ``iterations``, so it breaks nothing there.)
    - a Python row breaks where its rate key changed: per call one release,
      per bit (``Mbit_s``) the next.

    >>> c = [("it", 4096), ("it", 65536), ("it", 65536), ("it", 1)]
    >>> breaks(c, [None, None, "sample", "sample"])
    [1]
    >>> breaks(c[2:], ["sample", "call"])
    [1]
    >>> breaks([("it", 8), ("it", 8)], [None, "call"])
    []
    >>> breaks([("call",), ("bit",)], ["call", "bit"])
    [1]
    """
    out, prev = [], None
    for i, basis in enumerate(bases):
        if basis is None:
            continue
        if prev is not None:
            pb, pu = prev
            if basis[0] == "it" and pu is not None and units[i] is not None:
                changed = pu != units[i]
            else:
                changed = basis != pb
            if changed:
                out.append(i)
        prev = basis, units[i]
    return out


def since_last(p: list, brk: list) -> tuple:
    """``(change, broke)``: the newest value against the release before it.

    ``p`` is seconds per unit per release, None where unmeasured, and
    ``change`` is how much faster the newest is than the previous release
    that measured the row (``prev / newest - 1``, four significant digits).
    A break between the two (``brk``, from `breaks`) makes them different
    quantities, so there is no change to report: ``(None,
    True)``. A row the newest release measures for the first time is
    ``(None, False)``.

    >>> since_last([2e-9, None, 1e-9], [])
    (1.0, False)
    >>> since_last([8e-4, 8e-4, 8e-9], [2])
    (None, True)
    >>> since_last([None, 1e-9], [])
    (None, False)
    """
    last = len(p) - 1
    if last < 0 or p[last] is None:
        return None, False
    prev = next((j for j in range(last - 1, -1, -1) if p[j] is not None), -1)
    if prev < 0:
        return None, False
    if any(prev < b <= last for b in brk):
        return None, True
    return _sig(p[prev] / p[last] - 1), False


def page_data(root: str = ".") -> dict:
    """Everything the page draws, from the committed snapshots.

    ``rows`` holds every row the newest release measures, each with its
    seconds per unit in every release (``p`` portable, ``n`` native; None
    where a release did not measure it), the indices where its value changed
    meaning (``brk``, from `breaks`), its since-last change (``sl``, from
    `since_last`), its family, unit and, when the label table has one, its
    label. ``problems`` lists what the label gate refuses: a row no family
    claims, a label whose unit contradicts the snapshot.
    """
    releases = _raw_releases(os.path.join(root, PUBLISHED))
    versions = [v for v, _, _ in releases]
    obj2mod, modules = read_manifest(root)
    labels = read_labels(root)
    legacy = read_legacy_units(root)
    if not releases:
        return {"versions": [], "rows": [], "problems": []}
    _, newest, _ = releases[-1]
    live = {}
    for b in BUILDS:
        for suite, benches in newest.get(b, {}).items():
            for bench in benches:
                live.setdefault(row_id(suite, bench), (suite, bench))

    rows, problems, stale_legacy = [], [], []
    for rid in sorted(live):
        suite, bench = live[rid]
        comp = _component(suite, bench)
        mod = module_of(suite, comp, obj2mod, modules)
        fam = MODULE_FAMILY.get(mod, "")
        if not fam:
            problems.append(
                f"{rid}: no family -- component `{comp}` is no manifest "
                "object or module and not in COMPONENT_MODULE "
                "(scripts/bench_report.py)"
                if not mod
                else f"{rid}: module `{mod}` is not in MODULE_FAMILY "
                "(scripts/bench_report.py): file it under a page family"
            )
        series = {b: [None] * len(versions) for b in BUILDS}
        bases = [None] * len(versions)
        units = [None] * len(versions)
        recorded = None
        for i, (_, builds, _) in enumerate(releases):
            for b in BUILDS:
                for x in builds.get(b, {}).get(suite, []):
                    if row_id(suite, x) != rid:
                        continue
                    got = seconds_per_unit(suite, x)
                    # A name recorded more than once in one set (acq's
                    # rows, once per Pd point) keeps its FIRST value, as
                    # `live` keeps its first row.
                    if got is None or series[b][i] is not None:
                        continue
                    series[b][i] = _sig(got[0])
                    if b == "portable" or bases[i] is None:
                        bases[i] = got[1]
                        units[i] = _recorded_unit(suite, x)
                    recorded = _recorded_unit(suite, x) or recorded
        label = labels.get(rid)
        if rid in legacy and suite == "c" and recorded:
            stale_legacy.append(rid)
            problems.append(
                f"{LEGACY_UNITS}: `{rid}` records its own unit now {FIXABLE}"
            )
        known = recorded or legacy.get(rid)
        unit = known or (label or {}).get("unit")
        if label and known and label.get("unit") != known:
            problems.append(
                f"{rid}: labelled unit `{label.get('unit')}` but the "
                f"snapshot records `{known}`"
            )
        if label:
            why = label_count_problem(rid, label, known)
            if why:
                problems.append(why)
        reads = label and ("samples" in label or label.get("domain"))
        if known and known not in KNOWN_UNITS and not reads:
            # jm_bench takes any unit word; one the page has no reading for
            # would otherwise fall through to "time per call" unannounced.
            # A label with a count reads the row whatever the word is.
            problems.append(
                f"{rid}: the snapshot records unit `{known}`, which the page "
                f"cannot read -- give the row a label in {LABELS} with "
                "`samples`, or `domain` and `count`; or, for a unit every "
                "bench should share, add it to UNIT_SAMPLES or UNIT_DOMAIN "
                "in scripts/bench_report.py"
            )
        rate = rate_of(rid, unit, label)
        if rate[1] == "Sa":
            # A sample rate past the coverage gate's ceiling is a hollow
            # benchmark, not a fast one (#1918): py:corr read 203 GSa/s
            # before v0.65.0. The point is dropped, so the trend neither
            # draws the jump nor measures "since last" against it.
            floor = rate[0] / (CEILING_MSA_S * 1e6)
            for i in range(len(versions)):
                for b in BUILDS:
                    s = series[b][i]
                    if s is not None and s < floor:
                        series[b][i] = None
                if all(series[b][i] is None for b in BUILDS):
                    bases[i] = None
        brk = breaks(bases, units)
        sl, slb = since_last(series["portable"], brk)
        unit = unit or "iter"
        last_basis = next(
            (b for b in reversed(bases) if b is not None), ("call",)
        )
        rows.append(
            {
                "id": rid,
                "k": "c" if suite == "c" else "py",
                "comp": comp,
                "mod": mod,
                "fam": fam,
                "unit": unit,
                # `cnt` of `dom` per unit; dom None = setup/control, and
                # then `cnt` is how many units one call is
                "cnt": rate[0],
                "dom": rate[1],
                "inf": rate[2],
                "it": last_basis[1] if last_basis[0] == "it" else 1,
                "p": series["portable"],
                "n": series["native"],
                "brk": brk,
                # since last: portable speed-up over the previous release
                # that measured the row; None, with `slb`, across a break
                "sl": sl,
                "slb": slb,
            }
        )
    for rid in sorted(set(legacy) - set(live)):
        problems.append(f"{LEGACY_UNITS}: `{rid}` is no live row {FIXABLE}")
    for m in modules:
        if m not in MODULE_FAMILY:
            problems.append(
                f"module `{m}` is not in MODULE_FAMILY "
                "(scripts/bench_report.py): file it under a page family"
            )
    meta = {}
    for v, _, m in releases:
        meta[v] = {
            k: m.get(k)
            for k in ("measured_at", "commit", "compiler", "cpu", "kernel")
        }
    head = [
        rid
        for rid, lab in labels.items()
        if lab.get("headline") and rid in live
    ]
    return {
        "versions": versions,
        "meta": meta,
        "families": [{"id": f, "title": t} for f, t in FAMILIES],
        "labels": {
            rid: {
                "name": lab.get("name", ""),
                "def": lab.get("definition", ""),
            }
            for rid, lab in labels.items()
            if rid in live
        },
        "head": head,
        # the rate suffix per domain, so the script prints what fmt_rate does
        "suffix": DOMAIN_SUFFIX,
        "rows": rows,
        "problems": problems,
        # for fix_labels; never embedded in the page
        "_stale_legacy": stale_legacy,
    }


def label_problems(
    root: str = ".", base: str | None = None, history: bool = True
) -> list:
    """What the label gate refuses (#2372), as messages.

    ``history`` False skips the one check that asks git -- whether a label
    for an unpublished row is pending or dead (`unpublished_labels`) --
    for ``cmd_page --check`` only: the Docs job checks the page out at
    depth 1, where the newest release's commit cannot be read, and the
    gate's home is `make lint` (bench-coverage-check), whose checkout has
    the history. The page draws no unpublished row, so it does not need
    the answer; the writer, `make bench-docs`, asks it.

    Every row the newest release measures has a label in LABELS or a line in
    LABELS_EXEMPT; a label carries a name, a unit, a definition and a
    count, and is for a live row or a pending one (`unpublished_labels`);
    an exempt line names a live, unlabelled row; and, with ``base``, the
    exempt list holds nothing its merge base did not.
    """
    data = page_data(root)
    bad = list(data["problems"])
    live = {r["id"] for r in data["rows"]}
    labels = read_labels(root)
    newest = data["versions"][-1] if data["versions"] else None
    commit = (data.get("meta", {}).get(newest) or {}).get("commit")
    if history:
        _, dead = unpublished_labels(root, labels, live, newest, commit)
        bad += dead
    for rid, lab in labels.items():
        for key in ("name", "unit", "definition"):
            if not str(lab.get(key, "")).strip():
                bad.append(f"{LABELS}: `{rid}` has no {key}")
    path = os.path.join(root, LABELS_EXEMPT)
    text = ""
    if os.path.exists(path):
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
    exempt = read_exempt(text)
    for rid in exempt:
        if rid in labels:
            bad.append(f"{LABELS_EXEMPT}: `{rid}` is labelled now {FIXABLE}")
        elif rid not in live:
            bad.append(f"{LABELS_EXEMPT}: `{rid}` is no live row {FIXABLE}")
    for rid in sorted(live - set(labels) - set(exempt)):
        bad.append(
            f"`{rid}` has no label: give it a name, unit, definition and "
            f"count (`samples`, or `domain` and `count`) in {LABELS}"
        )
    if base:
        for rid in added_since_base(
            Path(root),
            base,
            LABELS_EXEMPT,
            exempt,
            read_exempt,
            since=LABELS,
        ):
            bad.append(
                f"{LABELS_EXEMPT}: `{rid}` was added -- the list only "
                "shrinks; label the row instead"
            )
    fixable = sum(1 for b in bad if b.endswith(FIXABLE))
    if fixable:
        bad.append(
            f"{fixable} of these only need a line deleted, which `make "
            "bench-labels-fix` does; the rest need a label written"
        )
    return bad


#: The tail of every message `fix_labels` clears by itself.
FIXABLE = "-- remove the line"


def unpublished_labels(root, labels, live, newest, commit) -> tuple:
    """``(pending, problems)`` for labels whose row the newest release
    does not measure.

    A row is labelled in the PR that adds it, which is before any release
    has published it (#2373 review: v0.73.0 publishes 31 C rows v0.72.0
    never had, and the exempt list may not grow to take them). Such a
    label is PENDING if ``labels.toml`` at the commit the newest release was
    measured at did not hold it: the row was added since. A label that
    release WAS measured with, for a row it did not record, is dead -- a
    retired row or a wrong ID -- and so is every non-live label in a tree
    with no history to ask (a test's synthetic tree).
    """
    waiting = [rid for rid in labels if rid not in live]
    if not waiting:
        return [], []
    then = None
    if commit and in_git_repo(Path(root)):
        try:
            rev = resolve_base(Path(root), commit)
        except BaseUnreadableError:
            return [], [
                f"{LABELS}: {len(waiting)} label(s) for rows no release "
                f"measures, and {newest}'s commit `{commit}` cannot be "
                "read to tell new from dead: fetch the full history"
            ]
        text = show_at(Path(root), rev, LABELS)
        then = set(_toml.loads(text)) if text else set()
    pending, dead = [], []
    for rid in waiting:
        if then is not None and rid not in then:
            pending.append(rid)
        else:
            dead.append(
                f"{LABELS}: `{rid}` is no live row -- {newest} was measured "
                "with this label and recorded no such row: remove the "
                "label, or correct its ID"
            )
    return pending, dead


def fix_labels(root: str = ".") -> list:
    """Delete every line the label gate only wants deleted; say which.

    A ``legacy-units.toml`` entry whose row now records its own unit, or is
    no longer published, and a ``.labels-exempt`` line whose row is
    labelled or no longer published. The legacy file goes once it holds no
    entry -- the first release measured after #2314 empties it. Labels are
    never touched: a dead one may be a new row's mistyped ID, which needs a
    person.
    """
    data = page_data(root)
    live = {r["id"] for r in data["rows"]}
    labels = read_labels(root)
    done = []
    stale = set(data.get("_stale_legacy", []))
    path = os.path.join(root, LEGACY_UNITS)
    if os.path.exists(path):
        with open(path, encoding="utf-8") as fh:
            lines = fh.read().splitlines(keepends=True)
        keep, entries = [], 0
        for ln in lines:
            m = re.match(r'^"([^"]+)"\s*=', ln)
            if m and (m.group(1) in stale or m.group(1) not in live):
                done.append(f"{LEGACY_UNITS}: removed `{m.group(1)}`")
                continue
            entries += bool(m)
            keep.append(ln)
        if not entries:
            os.remove(path)
            done.append(f"{LEGACY_UNITS}: no entry left, deleted the file")
        elif len(keep) != len(lines):
            with open(path, "w", encoding="utf-8") as fh:
                fh.writelines(keep)
    path = os.path.join(root, LABELS_EXEMPT)
    if os.path.exists(path):
        with open(path, encoding="utf-8") as fh:
            lines = fh.read().splitlines(keepends=True)
        keep = []
        for ln in lines:
            rid = ln.strip()
            if (
                rid
                and not rid.startswith("#")
                and (rid in labels or rid not in live)
            ):
                done.append(f"{LABELS_EXEMPT}: removed `{rid}`")
                continue
            keep.append(ln)
        if len(keep) != len(lines):
            with open(path, "w", encoding="utf-8") as fh:
                fh.writelines(keep)
    return done


#: Rate suffixes per domain, as the page prints them.
DOMAIN_SUFFIX = {
    "Sa": "Sa/s",
    "sym": "sym/s",
    "bit": "bit/s",
    "codeword": "cw/s",
    "frame": "frames/s",
}


def fmt_rate(per_sec: float, dom: str) -> str:
    """``per_sec`` units of ``dom`` per second, SI-prefixed.

    >>> fmt_rate(6.63e8, "Sa")
    '663 MSa/s'
    >>> fmt_rate(9.2e7, "bit")
    '92.0 Mbit/s'
    """
    scale, p = next(
        (sc, px)
        for sc, px in ((1e9, "G"), (1e6, "M"), (1e3, "k"), (1, ""))
        if per_sec >= sc or sc == 1
    )
    v = per_sec / scale
    num = f"{v:.0f}" if v >= 100 else f"{v:.1f}" if v >= 10 else f"{v:.2f}"
    return f"{num} {p}{DOMAIN_SUFFIX.get(dom, dom + '/s')}"


def fmt_pct(x: float) -> str:
    """A change as the page prints it, the same in the static table and in
    benchmarks.js's ``pct``: a true minus sign, one decimal under 10%.

    >>> fmt_pct(-0.003)
    '−0.3%'
    >>> fmt_pct(0.42)
    '+42%'
    >>> fmt_pct(0.0)
    '+0.0%'
    """
    sign = "+" if x >= 0 else "−"
    digits = 1 if abs(x) < 0.1 else 0
    return f"{sign}{abs(x) * 100:.{digits}f}%"


def _fmt_row(row: dict, secs) -> str:
    """One value as the static table shows it: its rate, or time per call.

    ``secs`` is per recorded unit; a setup or read-out row's ``cnt`` is how
    many of those one call is, so its time per call is ``secs * cnt``.

    >>> _fmt_row({"dom": "Sa", "cnt": 1}, 1.5e-9)
    '667 MSa/s'
    >>> _fmt_row({"dom": None, "cnt": 4096}, 1e-10)
    '409.60 ns / call'
    >>> _fmt_row({"dom": None, "cnt": 1}, None)
    '—'
    """
    if secs is None:
        return "—"
    if row["dom"]:
        return fmt_rate(row["cnt"] / secs, row["dom"])
    return f"{fmt_time(secs * row['cnt'])} / call"


def render_page(root: str = ".") -> str:
    """The whole of docs/benchmarks.md, as text."""
    data = page_data(root)
    versions = data["versions"]
    if not versions:
        return "# Benchmarks\n\n_No published numbers yet._\n"
    latest = versions[-1]
    m = data["meta"][latest]
    rows = {r["id"]: r for r in data["rows"]}
    pmeta = _raw_releases(os.path.join(root, PUBLISHED))[-1][2]
    L = [
        "<!-- generated by `make bench-docs`"
        " (scripts/bench_report.py --page); don't hand-edit. The labels"
        " come from benchmarks/labels.toml -->",
        "# Benchmarks",
        "",
        "How fast is doppler, and is it getting faster? These are"
        " single-machine numbers measured by hand for each release, not in"
        " CI. Each release is measured in two builds: **portable** is the"
        " PyPI wheel, and **native** is built from source for this CPU"
        " ([builds](#builds)). The trend under each number covers every"
        " release measured on this machine.",
        "",
        f"**{latest}** — measured"
        f" {(m.get('measured_at') or '?')[:10]} on **{m.get('cpu') or '?'}**,"
        f" doppler `{m.get('commit') or '?'}`, {m.get('compiler') or '?'},"
        f" {m.get('kernel') or '?'}"
        + (
            f"; fastest core class only (cpus"
            f" {_cpu_ranges(pmeta['pinned_cpus'])})"
            if pmeta.get("pinned_cpus")
            else ""
        )
        + ".",
        "",
        '<div class="dp-bench" id="dp-bench-rate"></div>',
        "",
        "## At a glance",
        "",
        '<div class="dp-bench" id="dp-bench-cards"></div>',
        "",
        "| benchmark | portable | native | %Δ |",
        "| --- | ---: | ---: | ---: |",
    ]
    for i, rid in enumerate(data["head"], 1):
        r, lab = rows[rid], data["labels"][rid]
        p, n = r["p"][-1], r["n"][-1]
        src = fmt_pct(p / n - 1) if p and n else "—"
        L.append(
            f"| [{lab['name']}](#d{i}) | {_fmt_row(r, p)} |"
            f" {_fmt_row(r, n)} | {src} |"
        )
    L += [
        "",
        "## Every benchmark, by family",
        "",
        f"{len(rows)} benchmarks, grouped by family. Click a row for its"
        " chart across releases, its unit and how to reproduce it.",
        "",
        '<div class="dp-bench" id="dp-bench-browser"></div>',
        "",
        "<noscript>The family tables and charts need JavaScript. The numbers"
        " themselves are in <code>benchmarks/published/</code>.</noscript>",
        "",
    ]
    L += _transport_section(latest, root)
    L += [
        "## Notes and definitions",
        "",
        "### Headline kernels",
        "",
    ]
    for i, rid in enumerate(data["head"], 1):
        lab = data["labels"][rid]
        L.append(
            f'{i}. <span id="d{i}"></span>**{lab["name"]}.** {lab["def"]}'
            f" `{rid.split(':', 1)[1]}`"
        )
    L += [
        "",
        "### Terms",
        "",
        '<span id="builds"></span>**Builds.** *portable* is the shipped'
        " wheel; *native* is `-DDOPPLER_NATIVE=ON` (`-march=native`), built"
        " from source for this CPU. The two are measured interleaved,"
        " alternating runs, keeping each benchmark's best.",
        "",
        '<span id="pct-delta"></span>**%Δ.** How much faster the native'
        " build is than the wheel on the same benchmark: portable time ÷"
        " native time − 1. Kernels that vectorize gain the most;"
        " call-overhead-bound rows sit near 0%.",
        "",
        '<span id="since-last"></span>**Since last.** Portable speed against'
        " the previous release that measured the row. Changes under 3% are"
        " grey: repeat runs of an unchanged tree move about that much. Across"
        " a [break](#trend) the two are different quantities, and it reads"
        " *break*.",
        "",
        '<span id="trend"></span>**Trend.** One point per release, portable'
        " build, drawn as speed (higher is faster). A gap is a release that"
        " did not measure the row, or measured it faster than"
        f" {CEILING_MSA_S / 1000:.0f} GSa/s: a hollow benchmark, not a fast"
        " kernel. A dashed line is a *break*: a release where the row's value"
        " changed meaning, so the line breaks rather than draw a jump that is"
        " not real. A C row breaks where its recorded unit changed; a release"
        " from before C rows recorded one (before v0.73.0) has only its"
        " round to go on, so there a round that changed size breaks. A"
        " Python row breaks where the rate it records changed, per call one"
        " release and per bit the next.",
        "",
        '<span id="units"></span>**Units.** Every row that processes a'
        " stream reads in **samples per second**: input samples consumed,"
        " whatever the row's own unit. An FFT bin, a one-sample `step()` and"
        " an input sample count 1; an input pair counts 2; a frame counts its"
        " `nfft`; a Python call over a 64k block counts 65,536. Rows whose"
        " input is not samples read in their own rate: symbols, bits or"
        " codewords per second. Setup and control calls (create, reset,"
        " retune, configure) and read-outs (a spectrum read once per display"
        " refresh, a window built once and cached) are not stream rates and"
        " read as time per call. The count comes from the row's label, else"
        " its recorded unit, else its name -- a frame's `nfft`, a `64k`"
        " block, a one-sample `step` -- and a count read from the name says"
        " *inferred*.",
        "",
        '<span id="headroom"></span>**Headroom.** Throughput ÷ your sample'
        " rate, for stream rows. ×1 is exactly real time on this machine,"
        " portable build, on one core unless the row says otherwise (a `_t4`"
        " row fans across 4 threads): green is ×1.5 or more, amber between"
        " ×1 and ×1.5, red falls behind. A receiver is a chain: its headroom"
        " is set by its slowest stage, and the stages share the core.",
        "",
        '<span id="row-ids"></span>**Row IDs.** The small monospace line'
        " under each name is the benchmark's own ID: what"
        ' `make bench BENCH_ARGS="--c-only <component>"` (C) or'
        " `make bench-python` (Python) reports it as.",
        "",
    ]
    blob = {
        k: v
        for k, v in data.items()
        if k != "problems" and not k.startswith("_")
    }
    # ASCII-only JSON is a JavaScript expression (U+2028 and U+2029 arrive
    # as \u escapes), so the assignment needs no attribute; see PAGE_DATA.
    payload = json.dumps(blob, separators=(",", ":"), sort_keys=True)
    # `</` cannot appear inside a <script> element, and JSON allows `\/`.
    payload = payload.replace("</", "<\\/")
    L.append(f"<script>window.{PAGE_DATA} = {payload};</script>")
    return "\n".join(L).rstrip("\n") + "\n"


def cmd_page(out_path, check=False, root=".") -> int:
    """Write docs/benchmarks.md, or with ``check`` compare against it.

    Writing (`make bench-docs`, at a release) runs the whole label gate,
    so a dead label -- a retired row, a renamed one -- shows there. The
    ``check`` path (`docs-check`, the Docs job's depth-1 checkout) skips
    the one question that needs history; `make lint` asks it.
    """
    bad = label_problems(root, history=not check)
    if bad:
        for b in bad:
            print(f"bench-labels: {b}")
        return 1
    text = render_page(root)
    if check:
        target = out_path or "docs/benchmarks.md"
        try:
            with open(os.path.join(root, target), encoding="utf-8") as fh:
                have = fh.read()
        except OSError:
            have = None
        if have == text:
            print(f"bench-docs: {target} matches the published snapshots")
            return 0
        print(
            f"bench-docs: {target} is stale against benchmarks/published "
            "or benchmarks/labels.toml -- run `make bench-docs`"
        )
        return 1
    if out_path:
        with open(os.path.join(root, out_path), "w", encoding="utf-8") as fh:
            fh.write(text)
        print(f"wrote {out_path}")
    else:
        print(text, end="")
    return 0


def _transport_section(version, root="."):
    """Render the wire-transport benchmark (scripts/bench_stream.py) if a
    ``stream.json`` was captured for this release. Absent on releases predating
    the transport bench — silently skipped.

    Releases through v0.27.0 measured ZMQ alongside NATS ("zmq" key present);
    ZMQ was subsequently removed, so newer snapshots carry only "nats". Both
    shapes render here — the table's columns follow whatever transports are
    actually present in the snapshot."""
    path = os.path.join(root, PUBLISHED, version, "stream.json")
    if not os.path.exists(path):
        return []
    with open(path) as fh:
        snap = json.load(fh)
    t = snap.get("transports", {})
    if not t:
        return []
    cfg = snap.get("config", {})

    def cell(transport, sec, key):
        v = t.get(transport, {}).get(sec, {}).get(key)
        if v is None:
            return "—"
        return f"{v:,.1f}" if v < 1000 else f"{v:,.0f}"

    has_zmq = "zmq" in t
    has_nats = "nats" in t
    rows = [
        ("firehose throughput — MSa/s", "firehose", "tput_msa_s"),
        ("firehose throughput — MB/s", "firehose", "tput_mb_s"),
        ("status-plane RTT mean — µs", "reqrep", "rtt_mean_us"),
        ("status-plane RTT p99 — µs", "reqrep", "rtt_p99_us"),
    ]
    L = [
        "## Transport (P0)",
        "",
        "Wire throughput and control-plane latency, measured by the "
        "`bench_stream` C harness "
        f"(block = {cfg.get('block_size', '?')} CF32 samples, "
        f"{cfg.get('num_blocks', '?')} frames, best of "
        f"{cfg.get('passes', '?')} passes).",
        "",
    ]
    if has_zmq:
        L.append(
            "- **ZMQ** — brokerless PUSH/PULL over TCP loopback (removed "
            "after v0.27.0; retained here only for releases that measured "
            "it)."
        )
    L.append(
        "- **NATS** — the durable JetStream work-queue tier (server-acked "
        "publish + explicit-ack pull). Its throughput reflects the cost of "
        "exactly-once durability, not overhead."
    )
    L.append("")
    if has_zmq:
        L.append(
            f"| metric | ZMQ | {'NATS (JetStream)' if has_nats else 'NATS'} |"
        )
        L.append("| --- | ---: | ---: |")
        for label, sec, key in rows:
            nats = cell("nats", sec, key) if has_nats else "—"
            L.append(f"| {label} | {cell('zmq', sec, key)} | {nats} |")
    else:
        L.append("| metric | NATS (JetStream) |")
        L.append("| --- | ---: |")
        for label, sec, key in rows:
            L.append(f"| {label} | {cell('nats', sec, key)} |")
    L += [
        "",
        "> JetStream is store-and-forward, so the harness one-way *firehose* "
        "latency degenerates to queue residency there and is omitted; the "
        "status-plane REQ/REP RTT is the comparable latency figure. Captured "
        "with `make bench-stream` (a broker is started and torn down "
        "automatically).",
        "",
    ]
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
        "--check",
        action="store_true",
        help="with --page: fail unless --out already holds the page",
    )
    p.add_argument(
        "--check-labels",
        action="store_true",
        help="fail unless every published row is labelled or exempt",
    )
    p.add_argument(
        "--base",
        default=None,
        help="with --check-labels: the exempt list may not grow past "
        "this ref's merge base",
    )
    p.add_argument(
        "--publish",
        metavar="VERSION",
        help="stamp THIS machine's latest run into published/",
    )
    p.add_argument("--build", choices=BUILDS, default="portable")
    p.add_argument(
        "--fix-labels",
        action="store_true",
        help="delete the legacy-unit and exempt lines the label gate "
        "only wants deleted",
    )
    a = p.parse_args()
    if a.fix_labels:
        for line in fix_labels(".") or ["nothing to delete"]:
            print(f"bench-labels-fix: {line}")
        return 0
    if a.publish:
        return cmd_publish(a.publish, a.build)
    if a.check_labels:
        bad = label_problems(".", a.base)
        for b in bad:
            print(f"bench-labels: {b}")
        if not bad:
            data = page_data(".")
            v = data["versions"][-1] if data["versions"] else None
            pending, _ = unpublished_labels(
                ".",
                read_labels("."),
                {r["id"] for r in data["rows"]},
                v,
                (data["meta"].get(v) or {}).get("commit"),
            )
            print(
                "bench-labels: every published row is labelled or exempt"
                + (
                    f"; {len(pending)} label(s) wait for the first release"
                    " that measures their row"
                    if pending
                    else ""
                )
            )
        return 1 if bad else 0
    if a.page:
        return cmd_page(a.out, check=a.check)
    return cmd_compare(load_published())


if __name__ == "__main__":
    raise SystemExit(main())
