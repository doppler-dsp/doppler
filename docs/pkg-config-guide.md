# pkg-config & CMake Package Config: A Comprehensive Guide

A complete reference for shipping installable C libraries that consumers
can find with both `pkg-config` and CMake's `find_package`.

---

## Why Both?

Two different toolchain ecosystems consume installed C libraries:

| Tool | Used by |
|------|---------|
| `pkg-config` | Makefiles, Meson, Python cffi/ctypes, autoconf, shell scripts |
| CMake config | Any project using `find_package(MyLib REQUIRED)` |

They are independent systems. Supporting both takes about 30 extra lines of
CMake and two small template files — always do both.

---

## File Layout

```
myproject/
├── CMakeLists.txt
└── cmake/
    ├── mylib.pc.in              # pkg-config template
    └── mylibConfig.cmake.in     # CMake find_package template
```

Keep the templates in `cmake/` next to the `CMakeLists.txt` that references
them. If your project uses subdirectories (e.g. a `c/` subdirectory for the
library), put `cmake/` there — CMake resolves the path relative to the
`CMakeLists.txt` that calls `configure_file`.

---

## The pkg-config Template (`mylib.pc.in`)

```ini
prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@

Name: mylib
Description: One-line description of what the library does
Version: @PROJECT_VERSION@
Requires: libzmq
Requires.private: libfftw3
Libs: -L${libdir} -lmylib
Libs.private: -lm -lpthread
Cflags: -I${includedir}
```

### Anatomy

- **`prefix`** — set from `@CMAKE_INSTALL_PREFIX@` at configure time.
  The `${prefix}` variable lets consumers use
  `pkg-config --define-variable=prefix=...` to relocate the package
  without regenerating it.
- **`libdir` / `includedir`** — built from `prefix` + the relative path
  from `GNUInstallDirs`. Never hardcode these.
- **`Requires`** — public dependencies: consumers need them at link time.
- **`Requires.private`** — private dependencies: only needed when linking
  statically against your library. Omit from `Requires` to keep consumer
  link lines clean.
- **`Libs.private`** — same idea for `-l` flags. `-lm` and `-lpthread`
  almost always belong here, not in `Libs`.

### Common Pitfalls

**Using absolute paths directly.**
```ini
# WRONG — not relocatable
libdir=/usr/local/lib
includedir=/usr/local/include

# RIGHT — use variables
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@
```

**Using `@CMAKE_INSTALL_FULL_LIBDIR@` (the expanded absolute path).**
This bakes the prefix in at configure time. The file will be wrong if
the package is installed to a different prefix later (common with
`DESTDIR` staging for distribution packages).

**Over-populating `Requires`.**
If your shared library links `libfftw3` with `PRIVATE` visibility, the
symbol is already resolved inside your `.so`. Consumer executables do not
need `-lfftw3` on their link line. Move it to `Requires.private` or omit
it entirely.

**Missing `Requires` for truly public deps.**
If a public header of yours `#include`s a header from another library,
that library belongs in `Requires` (or `Cflags` if header-only). The
consumer's compiler must find it.

---

## The CMake Config Template (`mylibConfig.cmake.in`)

```cmake
@PACKAGE_INIT@

include("${CMAKE_CURRENT_LIST_DIR}/mylibTargets.cmake")

check_required_components(mylib)
```

That is the complete correct minimal form. Do not add more unless you
have public CMake dependencies (see below).

### `@PACKAGE_INIT@`

This macro, provided by `CMakePackageConfigHelpers`, inserts the
`set_and_check()` and `check_required_components()` helpers and sets up
`PACKAGE_PREFIX_DIR` so relative paths work after install. Without it,
your config file will break whenever the install prefix changes.

### Public CMake Dependencies

If your public headers expose types from another CMake package, consumers
need to `find_package` that dependency too. Add it explicitly:

```cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)
find_dependency(ZeroMQ REQUIRED)    # public dep — headers expose zmq types
find_dependency(FFTW3)              # optional dep

include("${CMAKE_CURRENT_LIST_DIR}/mylibTargets.cmake")

check_required_components(mylib)
```

Use `find_dependency` (not `find_package`) inside config files — it
propagates `REQUIRED`/`QUIET` correctly to the caller.

### Common Pitfalls

**Using plain `configure_file` instead of `configure_package_config_file`.**
```cmake
# WRONG — paths baked in, not relocatable
configure_file(cmake/mylibConfig.cmake.in ...)

# RIGHT
configure_package_config_file(cmake/mylibConfig.cmake.in ...)
```

`configure_package_config_file` handles `PACKAGE_PREFIX_DIR` and the path
helper macros. Plain `configure_file` does string substitution only.

**Not using imported targets (the old variables pattern).**
```cmake
# OUTDATED — avoid
set(MYLIB_INCLUDE_DIRS "${PACKAGE_PREFIX_DIR}/include")
set(MYLIB_LIBRARIES    "${PACKAGE_PREFIX_DIR}/lib/libmylib.so")

# CORRECT — modern CMake, exported targets
include("${CMAKE_CURRENT_LIST_DIR}/mylibTargets.cmake")
# Consumer now uses:  target_link_libraries(app PRIVATE mylib::mylib)
```

Imported targets carry include paths, compile definitions and transitive
dependencies automatically. Consumers do not need to set anything manually.

---

## CMakeLists.txt Wiring

### Includes (top-level CMakeLists.txt)

```cmake
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
```

`GNUInstallDirs` defines `CMAKE_INSTALL_LIBDIR`, `CMAKE_INSTALL_INCLUDEDIR`,
`CMAKE_INSTALL_BINDIR` etc. as relative paths that follow platform
conventions. Always include it before any `install()` call.

### Build vs Install Include Paths

```cmake
target_include_directories(mylib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
```

The generator expressions give the build tree the source path and give
the installed package the install path. Without this, consumers who
call `find_package` get your source tree path baked into their builds —
a hard-to-debug failure on other machines.

### Install Rules

```cmake
# 1. Install the library and export the target
install(TARGETS mylib
    EXPORT mylibTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 2. Install public headers
install(FILES include/mylib.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 3. Install the export set (the Targets.cmake file)
install(EXPORT mylibTargets
    FILE        mylibTargets.cmake
    NAMESPACE   mylib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mylib
)

# 4. Generate and install the Config and ConfigVersion files
configure_package_config_file(
    cmake/mylibConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/mylibConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mylib
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/mylibConfigVersion.cmake
    VERSION     ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mylibConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/mylibConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mylib
)

# 5. Generate and install the pkg-config file
configure_file(
    cmake/mylib.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/mylib.pc
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mylib.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
```

### Version Compatibility Modes

`write_basic_package_version_file` takes a `COMPATIBILITY` argument:

| Mode | Meaning |
|------|---------|
| `SameMajorVersion` | 1.x satisfies requests for 1.y (safe default for semver) |
| `SameMinorVersion` | 1.2.x satisfies 1.2.y (stricter, use for unstable APIs) |
| `AnyNewerVersion` | 2.0 satisfies a request for 1.0 (dangerous, avoid) |
| `ExactVersion` | Only exact match (too strict for most use) |

`SameMajorVersion` is the right default for any library following semver.

### NAMESPACE Convention

Always pass `NAMESPACE mylib::` to `install(EXPORT ...)`. This means
consumers write:

```cmake
find_package(mylib REQUIRED)
target_link_libraries(app PRIVATE mylib::mylib)
```

The double-colon makes it clear the target is an imported CMake target
(not a raw library name), and prevents name collisions across packages.

---

## What Gets Installed Where

After `cmake --install build --prefix /usr/local`:

```
/usr/local/
├── include/
│   └── mylib.h
├── lib/
│   ├── libmylib.so
│   ├── libmylib.a
│   ├── cmake/mylib/
│   │   ├── mylibConfig.cmake
│   │   ├── mylibConfigVersion.cmake
│   │   ├── mylibTargets.cmake
│   │   └── mylibTargets-release.cmake
│   └── pkgconfig/
│       └── mylib.pc
```

---

## Verifying the Install

Always write a post-install smoke test. Minimum checks:

```bash
#!/usr/bin/env bash
set -euo pipefail
PREFIX="${1:-/usr/local}"
export PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

# pkg-config
pkg-config --exists mylib
pkg-config --modversion mylib
pkg-config --cflags mylib
pkg-config --libs mylib

# headers and library files
test -f "${PREFIX}/include/mylib.h"
ls "${PREFIX}/lib/libmylib"*

# CMake config files
test -f "${PREFIX}/lib/cmake/mylib/mylibConfig.cmake"
test -f "${PREFIX}/lib/cmake/mylib/mylibTargets.cmake"

# compile smoke test
cat > /tmp/smoke.c <<'EOF'
#include <mylib.h>
int main(void) { return 0; }
EOF
gcc -o /tmp/smoke /tmp/smoke.c \
    $(pkg-config --cflags --libs mylib) \
    "-Wl,-rpath,${PREFIX}/lib"
/tmp/smoke
```

Run it as `bash test_install.sh "$HOME/.local"` for non-root installs.

---

## Platform Notes

### Non-system Prefix (any platform)

pkg-config only scans standard system paths by default. For any prefix
other than `/usr` or `/usr/local`:

```sh
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH"
```

For CMake:
```sh
cmake -DCMAKE_PREFIX_PATH="$HOME/.local" ..
```

Or at install/configure time, pass `--prefix` or `-DCMAKE_INSTALL_PREFIX`.

### Debian / Ubuntu (multiarch)

`GNUInstallDirs` sets `CMAKE_INSTALL_LIBDIR` to `lib/x86_64-linux-gnu`
on Debian/Ubuntu. This is correct — pkg-config scans that path
automatically. Do not hardcode `lib`; always use `${CMAKE_INSTALL_LIBDIR}`.

The smoke test above should probe both paths:
```sh
PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${PREFIX}/lib/$(gcc -dumpmachine)/pkgconfig"
```

### macOS (Homebrew)

- Intel: prefix `/usr/local`, Apple Silicon: `/opt/homebrew`
- No multiarch — `CMAKE_INSTALL_LIBDIR` is just `lib`
- CMake's `find_package` searches `$(brew --prefix)/lib/cmake` automatically

### MSYS2 / Windows (UCRT64)

- Always use the UCRT64 shell; MSYS shell uses a POSIX-emulation GCC
  with an incompatible ABI
- Prefix: `/ucrt64`; pkg-config path: `/ucrt64/lib/pkgconfig`
- Stage runtime DLLs next to executables or add the prefix `bin/` to
  `PATH` — Windows has no `rpath` equivalent
- Add MSYS2 prefixes to `CMAKE_PREFIX_PATH` via filesystem probe, not
  `$ENV{MSYSTEM}` (cmake may not inherit shell environment variables):

```cmake
foreach(_pfx /ucrt64 /mingw64 /clang64 /mingw32)
    if(IS_DIRECTORY "${_pfx}/lib")
        list(APPEND CMAKE_PREFIX_PATH "${_pfx}")
    endif()
endforeach()
```

- If `find_library` returns `NOTFOUND` after fixing cmake files, delete
  the stale `CMakeCache.txt` and reconfigure — cached `NOTFOUND` values
  are not automatically re-evaluated.

---

## Quick Checklist

- [ ] `include(GNUInstallDirs)` and `include(CMakePackageConfigHelpers)` at top
- [ ] `$<BUILD_INTERFACE:...>` / `$<INSTALL_INTERFACE:...>` on include paths
- [ ] `install(TARGETS ... EXPORT ...)` with `LIBRARY`, `ARCHIVE`, `RUNTIME`
- [ ] `install(EXPORT ... NAMESPACE mylib:: ...)` for the Targets file
- [ ] `configure_package_config_file` (not `configure_file`) for the Config
- [ ] `write_basic_package_version_file` with `SameMajorVersion`
- [ ] `cmake/mylib.pc.in` uses `${prefix}` variables, not absolute paths
- [ ] Private deps in `Requires.private` / `Libs.private`, not `Requires`
- [ ] Post-install smoke test that covers both pkg-config and CMake consumers
- [ ] Document the `PKG_CONFIG_PATH` and `CMAKE_PREFIX_PATH` overrides for
  non-system installs
