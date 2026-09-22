"""The release freshness gate: stale plots and stale benchmarks refuse a tag.

`docs/dev/release.md` §2 and §2b both say "regenerate it if it changed since
the last release" and nothing checked either. The failure mode is not that
someone forgets once -- it is that forgetting is invisible: a gallery page
shows a PNG of code that no longer exists, and a release publishes numbers
measured against a different tree, both of which read as correct.

The cases below are the ones where a weaker gate still looks right: one that
passes because nothing relevant changed (vacuous), one that cannot tell a
re-rendered plot from an un-rendered one, and one that fires on a repo with
no tag to compare against.

The real script runs against a scratch git repo -- the artifact, not a
re-implementation of it.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_release_freshness.py"
GALLERY = "src/doppler/examples/demo.py"


def _git(cwd: Path, *args: str) -> None:
    subprocess.run(
        ["git", "-c", "user.email=t@t", "-c", "user.name=t", *args],
        cwd=cwd,
        check=True,
        capture_output=True,
    )


def _fixture(tmp_path: Path, *, tag: bool = True) -> Path:
    """A repo with one gallery script and one C file, tagged v1.0.0."""
    (tmp_path / "src/doppler/examples").mkdir(parents=True)
    (tmp_path / "native/src").mkdir(parents=True)
    (tmp_path / "docs/assets").mkdir(parents=True)
    (tmp_path / GALLERY).write_text("print('plot')\n")
    (tmp_path / "native/src/k.c").write_text("int k(void){return 0;}\n")
    (tmp_path / "docs/assets/demo.png").write_text("PNG\n")
    _git(tmp_path, "init", "-q", "-b", "main")
    _git(tmp_path, "add", "-A")
    _git(tmp_path, "commit", "-qm", "base")
    if tag:
        _git(tmp_path, "tag", "v1.0.0")
    return tmp_path


def _run(repo: Path, version: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--version",
            version,
            "--repo",
            str(repo),
            GALLERY,
        ],
        capture_output=True,
        text=True,
        check=False,
    )


def test_clean_tree_passes(tmp_path: Path) -> None:
    repo = _fixture(tmp_path)
    # A file, not just the directory: git does not track an empty one, so
    # `add -A` would stage nothing and the commit would fail.
    (repo / "benchmarks/published/v1.0.1").mkdir(parents=True)
    (repo / "benchmarks/published/v1.0.1/portable.json").write_text("{}\n")
    _git(repo, "add", "-A")
    _git(repo, "commit", "-qm", "snapshot")
    r = _run(repo, "1.0.1")
    assert r.returncode == 0, r.stdout


def test_a_changed_gallery_script_with_no_new_plot_is_refused(
    tmp_path: Path,
) -> None:
    """The committed PNG is what the page shows beside the script."""
    repo = _fixture(tmp_path)
    (repo / GALLERY).write_text("print('plot v2')\n")
    _git(repo, "commit", "-qam", "tweak the plot script")
    r = _run(repo, "1.0.1")
    assert r.returncode != 0
    assert "gallery script(s) changed" in r.stdout
    assert GALLERY in r.stdout


def test_regenerating_the_plot_clears_it(tmp_path: Path) -> None:
    """Touching docs/assets is what a `make gallery` run leaves behind."""
    repo = _fixture(tmp_path)
    (repo / GALLERY).write_text("print('plot v2')\n")
    (repo / "docs/assets/demo.png").write_text("PNG v2\n")
    _git(repo, "commit", "-qam", "tweak the script and re-render")
    r = _run(repo, "1.0.1")
    assert r.returncode == 0, r.stdout


def test_perf_code_without_a_snapshot_is_refused(tmp_path: Path) -> None:
    """release.md §2b's "skip only if nothing perf-relevant moved"."""
    repo = _fixture(tmp_path)
    (repo / "native/src/k.c").write_text("int k(void){return 1;}\n")
    _git(repo, "commit", "-qam", "change a kernel")
    r = _run(repo, "1.0.1")
    assert r.returncode != 0
    assert "benchmarks/published/v1.0.1" in r.stdout
    assert "native/src/k.c" in r.stdout


def test_publishing_the_snapshot_clears_it(tmp_path: Path) -> None:
    repo = _fixture(tmp_path)
    (repo / "native/src/k.c").write_text("int k(void){return 1;}\n")
    (repo / "benchmarks/published/v1.0.1").mkdir(parents=True)
    (repo / "benchmarks/published/v1.0.1/portable.json").write_text("{}\n")
    _git(repo, "add", "-A")
    _git(repo, "commit", "-qm", "change a kernel and publish numbers")
    r = _run(repo, "1.0.1")
    assert r.returncode == 0, r.stdout


def test_docs_only_change_needs_no_snapshot(tmp_path: Path) -> None:
    """The gate must not fire on a release that moved no kernel."""
    repo = _fixture(tmp_path)
    (repo / "docs/assets/note.md").write_text("prose\n")
    _git(repo, "add", "-A")
    _git(repo, "commit", "-qm", "docs only")
    r = _run(repo, "1.0.1")
    assert r.returncode == 0, r.stdout


def test_a_repo_with_no_tag_does_not_fire(tmp_path: Path) -> None:
    """Nothing to compare against is not the same as something stale."""
    repo = _fixture(tmp_path, tag=False)
    r = _run(repo, "1.0.1")
    assert r.returncode == 0
    assert "no tag yet" in r.stdout


def _with_cmake(repo: Path) -> Path:
    """Add a CMakeLists.txt under native/src to the tagged baseline."""
    (repo / "native/src/CMakeLists.txt").write_text(
        "# the stream layer\nadd_library(k OBJECT k.c)\n"
    )
    _git(repo, "add", "-A")
    _git(repo, "commit", "-qm", "cmake")
    _git(repo, "tag", "-f", "v1.0.0")
    return repo


def test_a_comment_only_cmake_edit_needs_no_snapshot(
    tmp_path: Path,
) -> None:
    """v0.51.1's refusal: two CMake comments rewritten, 66 min asked."""
    repo = _with_cmake(_fixture(tmp_path))
    (repo / "native/src/CMakeLists.txt").write_text(
        "# the NATS stream layer, not ported to Windows\n"
        "add_library(k OBJECT k.c)\n"
    )
    _git(repo, "commit", "-qam", "reword a comment")
    r = _run(repo, "1.0.1")
    assert r.returncode == 0, r.stdout


def test_a_cmake_flag_change_still_needs_one(tmp_path: Path) -> None:
    """A flag can move performance: the path filter still applies."""
    repo = _with_cmake(_fixture(tmp_path))
    (repo / "native/src/CMakeLists.txt").write_text(
        "# the stream layer\nadd_library(k OBJECT k.c)\n"
        "target_compile_options(k PRIVATE -O3)\n"
    )
    _git(repo, "commit", "-qam", "tune")
    r = _run(repo, "1.0.1")
    assert r.returncode != 0
    assert "native/src/CMakeLists.txt" in r.stdout


def test_a_c_comment_only_edit_needs_no_snapshot(tmp_path: Path) -> None:
    repo = _fixture(tmp_path)
    (repo / "native/src/k.c").write_text(
        "/* why k returns zero */\nint k(void){return 0;}\n"
    )
    _git(repo, "commit", "-qam", "document k")
    r = _run(repo, "1.0.1")
    assert r.returncode == 0, r.stdout


def test_a_c_line_that_only_looks_like_a_comment_counts(
    tmp_path: Path,
) -> None:
    """Conservative: `*p = 1;` is code, so it still asks for numbers."""
    repo = _fixture(tmp_path)
    (repo / "native/src/k.c").write_text(
        "int k(void){return 0;}\nvoid z(int *p){\n*p = 1;\n}\n"
    )
    _git(repo, "commit", "-qam", "add z")
    r = _run(repo, "1.0.1")
    assert r.returncode != 0


# ── waivers: answering the gate in writing (doppler#1457's release) ─────────
#
# The gate is path-granular, so it cannot see that a change is confined to a
# platform this release does not measure. A waiver names ONE item, carries a
# reason, and is printed -- the cases below are the ones that decide whether
# it is a record or a bypass.


def _waive(repo: Path, version: str, body: str) -> None:
    d = repo / "release-waivers"
    d.mkdir(parents=True, exist_ok=True)
    (d / f"v{version}.md").write_text(body, encoding="utf-8")
    _git(repo, "add", "-A")
    _git(repo, "commit", "-qm", "waiver")


def test_a_waiver_clears_the_named_gallery_script(tmp_path: Path) -> None:
    repo = _fixture(tmp_path)
    (repo / GALLERY).write_text("print('plot v2')\n")
    _git(repo, "commit", "-qam", "tweak the plot script")
    _waive(
        repo, "1.0.1", f"- gallery: {GALLERY} -- windows-only, art identical\n"
    )
    r = _run(repo, "1.0.1")
    assert r.returncode == 0, r.stdout
    # Printed, so a skipped check appears in the release log.
    assert "WAIVED gallery" in r.stdout
    assert "art identical" in r.stdout


def test_a_waiver_clears_the_named_perf_path(tmp_path: Path) -> None:
    repo = _fixture(tmp_path)
    (repo / "native/src/k.c").write_text("int k(void){return 1;}\n")
    _git(repo, "commit", "-qam", "tweak the kernel")
    _waive(
        repo, "1.0.1", "- benchmarks: native/src/k.c -- inside #ifdef _WIN32\n"
    )
    r = _run(repo, "1.0.1")
    assert r.returncode == 0, r.stdout
    assert "WAIVED benchmarks native/src/k.c" in r.stdout


def test_a_waiver_for_one_item_does_not_clear_another(tmp_path: Path) -> None:
    """The point of naming the item: it cannot widen into a blanket skip."""
    repo = _fixture(tmp_path)
    (repo / GALLERY).write_text("print('plot v2')\n")
    (repo / "native/src/k.c").write_text("int k(void){return 1;}\n")
    _git(repo, "commit", "-qam", "tweak both")
    _waive(repo, "1.0.1", f"- gallery: {GALLERY} -- windows-only\n")
    r = _run(repo, "1.0.1")
    assert r.returncode != 0
    assert "perf-relevant code changed" in r.stdout


def test_a_waiver_without_a_reason_waives_nothing(tmp_path: Path) -> None:
    repo = _fixture(tmp_path)
    (repo / GALLERY).write_text("print('plot v2')\n")
    _git(repo, "commit", "-qam", "tweak the plot script")
    _waive(repo, "1.0.1", f"- gallery: {GALLERY}\n")
    r = _run(repo, "1.0.1")
    assert r.returncode != 0
    assert "gallery script(s) changed" in r.stdout


def test_a_waiver_matching_nothing_stale_is_refused(tmp_path: Path) -> None:
    """A waiver that outlives its reason is how one widens unnoticed."""
    repo = _fixture(tmp_path)
    (repo / "benchmarks/published/v1.0.1").mkdir(parents=True)
    (repo / "benchmarks/published/v1.0.1/portable.json").write_text("{}\n")
    _waive(repo, "1.0.1", "- benchmarks: native/src/k.c -- stale waiver\n")
    r = _run(repo, "1.0.1")
    assert r.returncode != 0
    assert "match nothing stale" in r.stdout


def test_a_waiver_for_another_version_is_not_read(tmp_path: Path) -> None:
    repo = _fixture(tmp_path)
    (repo / GALLERY).write_text("print('plot v2')\n")
    _git(repo, "commit", "-qam", "tweak the plot script")
    _waive(repo, "9.9.9", f"- gallery: {GALLERY} -- other release\n")
    r = _run(repo, "1.0.1")
    assert r.returncode != 0
    assert "gallery script(s) changed" in r.stdout


def test_a_wrapped_reason_is_printed_whole(tmp_path: Path) -> None:
    """mdformat reflows the waiver file, so a long reason arrives wrapped.
    Printing only its first line would make the record look like a shrug."""
    repo = _fixture(tmp_path)
    (repo / GALLERY).write_text("print('plot v2')\n")
    _git(repo, "commit", "-qam", "tweak the plot script")
    _waive(
        repo,
        "1.0.1",
        f"- gallery: {GALLERY} -- windows-only early return,\n"
        "  and the art re-renders byte-identically\n",
    )
    r = _run(repo, "1.0.1")
    assert r.returncode == 0, r.stdout
    assert "byte-identically" in r.stdout
    # One line, not a fragment followed by an orphaned continuation.
    waived = [ln for ln in r.stdout.splitlines() if "WAIVED" in ln]
    assert len(waived) == 1
    assert waived[0].endswith("byte-identically")
