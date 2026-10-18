"""The parallel record's toolchain stamp: what it reads, and what it refuses.

`docs/design/async-dsss-parallel-measurements/record.py` stamps every run it
writes (the validator's, the stories', the bench's) with the toolchain the
build used, because the validator's numbers are toolchain-bound: the same
commit gives different rows under another compiler, C library, build type or
`-march`. Two things keep that stamp honest, and this file pins both.

- `_require_toolchain` refuses a schema-2 stamp whose toolchain is missing,
  `null`, empty or short of a field. A presence check alone let
  `"toolchain": null` through, and it rendered "—" as if it were a
  schema-1 stamp that predates the field (doppler-reviewer on #2419).
- `_toolchain` reads the compiler CMake detected and the flags the build
  type compiles with, from the build tree. It used to run `cc --version` at
  stamp time, which can name a compiler the build never used, and it read
  `CMAKE_C_FLAGS` alone, which misses the release `-O3` and says nothing of
  `DOPPLER_NATIVE`.

The record script is imported rather than run: its data paths are fixed to
its own directory, and these are its pure helpers.
"""

from __future__ import annotations

import importlib.util
import os
import sys
from typing import TYPE_CHECKING, Any

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path
    from types import ModuleType

REPO = repo_root(__file__)
SCRIPT = REPO / "docs/design/async-dsss-parallel-measurements/record.py"

#: A complete schema-2 toolchain, as `_toolchain` writes one.
FULL = {
    "compiler": "GNU 16.2.1",
    "compiler_path": "/usr/bin/cc",
    "libc": "glibc 2.44",
    "build_type": "Release",
    "c_flags": "-O3 -DNDEBUG",
    "native": "OFF",
}


@pytest.fixture(scope="module")
def record() -> ModuleType:
    """The record script as a module (it finds `_record_blocks` itself)."""
    spec = importlib.util.spec_from_file_location("parallel_record", SCRIPT)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


def _without(key: str) -> dict[str, Any]:
    return {k: v for k, v in FULL.items() if k != key}


@pytest.mark.parametrize(
    "toolchain",
    [
        pytest.param(None, id="null"),
        pytest.param({}, id="empty"),
        pytest.param("GNU 16.2.1", id="not-a-dict"),
        pytest.param(_without("native"), id="no-native"),
        pytest.param(_without("compiler"), id="no-compiler"),
        pytest.param({**FULL, "c_flags": ""}, id="blank-flags"),
    ],
)
def test_a_schema_2_stamp_short_of_its_toolchain_is_refused(
    record: ModuleType, toolchain: object
) -> None:
    stamp = {"stamp_schema": 2, "toolchain": toolchain}
    with pytest.raises(SystemExit, match="full toolchain"):
        record._require_toolchain("lock-meta.json", stamp)


def test_a_schema_2_stamp_without_the_key_is_refused(
    record: ModuleType,
) -> None:
    with pytest.raises(SystemExit, match="full toolchain"):
        record._require_toolchain("bench.json", {"stamp_schema": 2})


def test_a_full_schema_2_stamp_passes_and_renders(record: ModuleType) -> None:
    stamp = {"stamp_schema": 2, "toolchain": dict(FULL)}
    record._require_toolchain("lock-meta.json", stamp)
    cell = record._tc(stamp)
    assert "GNU 16.2.1" in cell and "-O3 -DNDEBUG" in cell
    assert "native OFF" in cell


def test_a_schema_1_stamp_predates_the_field(record: ModuleType) -> None:
    """The committed data before the stamp: accepted, rendered as "—"."""
    stamp = {"commit": "abc"}
    record._require_toolchain("lock-meta.json", stamp)
    assert record._tc(stamp) == "—"


def _build_tree(
    root: Path, *, compiler_path: str, native: str = "OFF"
) -> Path:
    """A configured build tree with what CMake records, and nothing else."""
    build = root / "build"
    (build / "CMakeFiles" / "3.31.6").mkdir(parents=True)
    (build / "CMakeCache.txt").write_text(
        "# This is the CMakeCache file.\n"
        "CMAKE_CACHE_MAJOR_VERSION:INTERNAL=3\n"
        "CMAKE_CACHE_MINOR_VERSION:INTERNAL=31\n"
        "CMAKE_CACHE_PATCH_VERSION:INTERNAL=6\n"
        f"CMAKE_C_COMPILER:FILEPATH={compiler_path}\n"
        "CMAKE_BUILD_TYPE:STRING=Release\n"
        "CMAKE_C_FLAGS:STRING=-pipe\n"
        "CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG\n"
        f"DOPPLER_NATIVE:BOOL={native}\n",
        encoding="utf-8",
    )
    (build / "CMakeFiles" / "3.31.6" / "CMakeCCompiler.cmake").write_text(
        'set(CMAKE_C_COMPILER "/usr/bin/cc")\n'
        'set(CMAKE_C_COMPILER_ID "GNU")\n'
        'set(CMAKE_C_COMPILER_VERSION "16.2.1")\n',
        encoding="utf-8",
    )
    return build


def test_the_compiler_is_the_one_cmake_detected(
    record: ModuleType, tmp_path: Path
) -> None:
    """A compiler path that cannot run: the stamp still names the compiler,
    so it read CMake's record of it and ran nothing."""
    build = _build_tree(tmp_path, compiler_path=str(tmp_path / "no-such-cc"))
    tc = record._toolchain(build)
    assert tc["compiler"] == "GNU 16.2.1"


def test_the_flags_include_the_build_types_and_native(
    record: ModuleType, tmp_path: Path
) -> None:
    build = _build_tree(tmp_path, compiler_path="/usr/bin/cc", native="ON")
    tc = record._toolchain(build)
    assert tc["build_type"] == "Release"
    assert tc["c_flags"] == "-pipe -O3 -DNDEBUG"
    assert tc["native"] == "ON"
    # What it writes, it accepts.
    record._require_toolchain("x", {"stamp_schema": 2, "toolchain": tc})


def _refuse(name: str) -> str:
    raise ValueError(f"unrecognized configuration name: {name}")


@pytest.mark.parametrize(
    "platform_shape",
    [
        pytest.param("no-confstr", id="windows"),
        pytest.param("confstr-refuses", id="macos"),
    ],
)
def test_the_libc_is_unknown_where_none_is_named(
    record: ModuleType,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    platform_shape: str,
) -> None:
    """Windows has no os.confstr; macOS refuses the GNU name. Either way
    the stamp still forms, says "unknown", and is accepted."""
    if platform_shape == "no-confstr":
        monkeypatch.delattr(os, "confstr", raising=False)
    else:
        monkeypatch.setattr(os, "confstr", _refuse, raising=False)
    build = _build_tree(tmp_path, compiler_path="/usr/bin/cc")
    tc = record._toolchain(build)
    assert tc["libc"] == "unknown"
    record._require_toolchain("x", {"stamp_schema": 2, "toolchain": tc})
