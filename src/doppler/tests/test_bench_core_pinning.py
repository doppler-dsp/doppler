"""The published benchmarks are measured on ONE core class.

`scripts/bench_report.fastest_cpus` exists because a heterogeneous CPU makes
an unpinned benchmark bimodal: on the machine the published numbers come from
(4 Zen 5 + 6 Zen 5c) one binary measured 3.5 us/call on a Zen 5 core and 5.6 on
a Zen 5c one, and the v0.53.0 snapshot published a 1.6x `awgn` "regression"
with no source change behind it. Nothing under `scripts/` has its doctests
run, so the behaviour is pinned here, where `make test-python` reaches it.
"""

from __future__ import annotations

import doctest
import importlib.util
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

SCRIPTS = repo_root() / "scripts"


def _load(name: str):
    """Import `scripts/<name>.py` by path (scripts/ is not a package)."""
    sys.path.insert(0, str(SCRIPTS))
    try:
        spec = importlib.util.spec_from_file_location(
            name, SCRIPTS / f"{name}.py"
        )
        assert spec and spec.loader
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        return mod
    finally:
        sys.path.remove(str(SCRIPTS))


def _fake_sysfs(root: Path, ceilings_khz: list[int | None]) -> str:
    """A per-CPU sysfs tree; None omits that CPU's cpufreq entirely."""
    for n, khz in enumerate(ceilings_khz):
        d = root / f"cpu{n}" / "cpufreq"
        d.mkdir(parents=True)
        if khz is not None:
            (d / "cpuinfo_max_freq").write_text(f"{khz}\n")
    return str(root)


def test_picks_the_fastest_class_in_the_published_machines_layout(tmp_path):
    """cpus 0-3 and 10-13 are Zen 5, 4-9 and 14-19 are Zen 5c."""
    fast, slow = 5_090_000, 3_350_000
    layout = [fast] * 4 + [slow] * 6 + [fast] * 4 + [slow] * 6
    report = _load("bench_report")
    cpus = report.fastest_cpus(_fake_sysfs(tmp_path, layout))
    assert cpus == [0, 1, 2, 3, 10, 11, 12, 13]
    assert report._cpu_ranges(cpus) == "0-3,10-13"


@pytest.mark.parametrize(
    "layout",
    [
        [4_000_000] * 8,  # one class: nothing to choose between
        [None] * 8,  # a container with no cpufreq to read
        [],  # no CPUs visible at all
    ],
)
def test_leaves_a_uniform_or_unreadable_machine_unpinned(tmp_path, layout):
    """None means "do not pin" -- never an empty affinity set, which
    sched_setaffinity refuses."""
    report = _load("bench_report")
    assert report.fastest_cpus(_fake_sysfs(tmp_path, layout)) is None


def test_the_snapshot_records_what_it_was_pinned_to():
    """A published number is not comparable without its core class."""
    report = _load("bench_report")
    meta = report.collect_meta({}, "cc", "-O3", "abc1234", "", [0, 1])
    assert meta["pinned_cpus"] == [0, 1]
    assert (
        report.collect_meta({}, "cc", "-O3", "abc1234", "")["pinned_cpus"]
        is None
    )


def test_the_doctests_in_bench_report_run():
    """scripts/ has no doctest gate of its own; this is it for this file."""
    result = doctest.testmod(_load("bench_report"))
    assert result.attempted >= 6
    assert result.failed == 0


def _fake_machine(
    root: Path, governors: list[str], profile: str | None, load: str
) -> tuple[str, str, str]:
    """A cpu sysfs, a platform-profile file (or none) and a loadavg file."""
    for i, g in enumerate(governors):
        d = root / "cpu" / f"cpu{i}" / "cpufreq"
        d.mkdir(parents=True)
        (d / "scaling_governor").write_text(g + "\n")
    prof = root / "platform_profile"
    if profile is not None:
        prof.write_text(profile + "\n")
    (root / "loadavg").write_text(f"{load} 0.30 0.40 1/500 1\n")
    return str(root / "cpu"), str(prof), str(root / "loadavg")


def test_a_ready_machine_has_no_problems(tmp_path):
    """Governor and profile `performance`, quiet: nothing stops the run."""
    report = _load("bench_report")
    paths = _fake_machine(tmp_path, ["performance"] * 4, "performance", "0.4")
    assert report.machine_not_ready(*paths) == []


def test_a_machine_without_a_platform_profile_is_not_refused_for_it(
    tmp_path,
):
    """A desktop has no /sys/firmware/acpi/platform_profile; that is fine."""
    report = _load("bench_report")
    paths = _fake_machine(tmp_path, ["performance"] * 2, None, "0.1")
    assert report.machine_not_ready(*paths) == []


@pytest.mark.parametrize(
    ("governors", "profile", "load", "names"),
    [
        (["performance", "powersave"], "performance", "0.2", "governor"),
        (["performance"] * 2, "balanced", "0.2", "platform profile"),
        (["performance"] * 2, "performance", "3.5", "load"),
    ],
)
def test_each_wrong_state_refuses_the_run(
    tmp_path, governors, profile, load, names
):
    """v0.56.0's first run had the governor right and the other two wrong.

    Each of the three is checked on its own, so fixing one cannot hide
    another, and each problem names its fix.
    """
    report = _load("bench_report")
    problems = report.machine_not_ready(
        *_fake_machine(tmp_path, governors, profile, load)
    )
    assert len(problems) == 1 and names in problems[0], problems
    assert ":" in problems[0]  # "<what is wrong>: <the fix>"
