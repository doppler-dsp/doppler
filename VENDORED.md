# Vendored Dependencies Policy

This document describes how we manage vendored third-party code in the doppler project.

## Philosophy

**Why we vendor:** We statically link libzmq into the Python extension to eliminate runtime dependencies and ensure `pip install doppler` just works on any system without requiring users to install libzmq separately.

**When to vendor:** Only when absolutely necessary for ease-of-use and cross-platform compatibility. We prefer system libraries when reasonable.

---

## Current Vendored Dependencies

### libzmq 4.3.5

- **Location:** `python/vendor/libzmq/`
- **License:** MPL-2.0 (Mozilla Public License 2.0)
- **Upstream:** https://github.com/zeromq/libzmq
- **Version:** 4.3.5 (released 2023-10-09)
- **SHA256:** (see below)
- **Used in:** Python `dp_stream` extension (static link only)
- **Size:** ~2.5 MB source, builds to ~2.1 MB static lib (PIC)

**Why vendored:**
- Statically linked into the `dp_stream` Python extension for
  zero-dependency `pip install doppler` (no system libzmq needed)
- Vendored source is built with `-fPIC` and three compiler-flag
  workarounds for GCC ≥ 13 PCH include-order bugs — see
  Build System Integration below

**What we modified:**
1. `CMakeLists.txt:2` - Changed `cmake_minimum_required(VERSION 2.8.12)` → `VERSION 3.5` for modern CMake compatibility
2. No source code changes (clean vendor)

---

## Update Process

### Checking for Updates

```bash
# Check latest libzmq release
curl -s https://api.github.com/repos/zeromq/libzmq/releases/latest | jq -r '.tag_name'

# Current: v4.3.5
# Latest as of 2026-02-23: v4.3.5 (no updates needed)
```

### Updating libzmq

**Before updating:**
1. Read the upstream changelog for breaking changes
2. Check if any CMake configuration needs updating
3. Review security advisories (if applicable)

**Update steps:**

```bash
# 1. Download new version
cd python/vendor
rm -rf libzmq
curl -L https://github.com/zeromq/libzmq/releases/download/v4.3.6/zeromq-4.3.6.tar.gz -o zmq.tar.gz

# 2. Verify SHA256 (get from GitHub release page)
echo "EXPECTED_SHA256  zmq.tar.gz" | sha256sum --check

# 3. Extract
tar xzf zmq.tar.gz
mv zeromq-4.3.6 libzmq
rm zmq.tar.gz

# 4. Apply our CMakeLists.txt patch
sed -i '1,10s/cmake_minimum_required(VERSION 2.8.12)/cmake_minimum_required(VERSION 3.5)/' \
    libzmq/CMakeLists.txt

# 5. Test build
cd ../..
make clean
make pyext

# 6. Run tests
make python-test

# 7. Verify symbol hiding
nm -D build/python/dp_stream*.so | grep zmq
# Should be empty (all zmq symbols hidden)

# 8. Verify size
ls -lh python/doppler/dp_stream*.so
# Should be ~270-280 KB

# 9. Update this file with new version/SHA256/date
```

**After updating:**
1. Update `VENDORED.md` with new version, SHA256, date
2. Update `python/CMakeLists.txt` comments if build config changed
3. Commit with message: `vendor: update libzmq 4.3.5 → 4.3.6`
4. Run full CI to verify all platforms

---

## Maintenance Guidelines

### DO:
✅ Keep vendored code in a dedicated `vendor/` subdirectory
✅ Document the exact version, license, and upstream URL
✅ Minimize modifications (prefer build-time configuration)
✅ Keep modifications documented in this file
✅ Verify checksums when updating
✅ Test thoroughly after updates
✅ Use `.gitattributes` to exclude vendored code from language stats

### DON'T:
❌ Modify vendored source code (except CMakeLists.txt for compatibility)
❌ Vendor code with GPL licenses (incompatible with MPL-2.0/MIT)
❌ Vendor without clear necessity (prefer system deps when reasonable)
❌ Forget to update documentation when upgrading versions
❌ Commit vendored code without verifying the source

---

## Licensing Compliance

### libzmq License

libzmq is licensed under **MPL-2.0** (Mozilla Public License 2.0):
- ✅ Allows static linking in closed-source software
- ✅ Allows commercial use
- ✅ No copyleft requirements (unlike GPL)
- ✅ Compatible with MIT/BSD licenses

**Our obligation:** Distribute `python/vendor/libzmq/LICENSE` with the Python package (already done via `MANIFEST.in`).

### Full License Text

See: `python/vendor/libzmq/LICENSE`

---

## Build System Integration

### Current Approach: static link from vendored source

`dp_stream` links against `doppler_static` (the static C library archive)
and `zmq_vendor_static` (a PIC static archive built from this vendor tree).
The result is a single self-contained `.so` with no external zmq runtime
dependency — `pip install doppler` just works with no system packages needed.

```bash
make pyext   # builds vendored libzmq + all extensions
```

The build is a two-pass configure to work around GCC ≥ 13:

**Pass 1** — plain `cmake` configure, no extra flags.  This lets CMake's
internal compiler tests run cleanly and generates `platform.hpp` in the
build directory.

**Pass 2** — reconfigure with three forced includes that restore the
include order the PCH used to guarantee:
- `-include precompiled.hpp` — pulls in `platform.hpp` + `zmq.h`
- `-include macros.hpp` — defines `ZMQ_NON_COPYABLE_NOR_MOVABLE`
- `-include command.hpp` — forward-declares `command_t` for `i_mailbox.hpp`

No source files are modified.  All fixes are at the compiler-flag level.

**What we modified in the vendor tree:**
1. `CMakeLists.txt:2` — `cmake_minimum_required(VERSION 2.8.12)` →
   `VERSION 3.5` for modern CMake compatibility (unchanged from 4.3.5)

### Verification

```bash
# No zmq dynamic dependency (fully static):
ldd python/doppler/dp_stream*.so | grep zmq
# (no output expected)

# Confirm zmq symbols are present (baked in):
nm -D python/doppler/dp_stream*.so | grep zmq_ctx_new
```

---

## Alternatives Considered

### Why not use system libzmq?

**Rejected because:**
- Users would need to install `libzmq-dev` before `pip install doppler`
- Version mismatches cause subtle bugs (libzmq 4.2 vs 4.3 API differences)
- Cross-platform builds become complex (Windows DLLs, macOS dylibs, Linux .so)
- Wheels wouldn't be self-contained

### Why not use PyZMQ (Python bindings)?

**Rejected because:**
- PyZMQ adds Python overhead (~500ns per call)
- Can't do zero-copy (PyZMQ copies data into Python objects)
- Adds 15 MB dependency for functionality we only need 5% of
- We need direct C-level ZMQ access for performance

---

## Future Considerations

### Other Vendoring Candidates?

**pocketfft** (MIT license) - already vendored at `c/src/pocketfft.cc`
- Used as fallback FFT backend when FFTW not available
- Single-file vendor, minimal maintenance burden
- License: BSD-3-Clause ✅

**Not planning to vendor:**
- FFTW - Too large, GPL license, prefer system install
- NumPy - Already a Python dependency, no need to vendor

---

## SHA256 Checksums

### libzmq 4.3.5

```
6653eda1046088d5f317253c693c9ce56e1b85b4d1d8dcc59f708f4a8fb76e46  zeromq-4.3.5.tar.gz
```

**Verify:**
```bash
curl -L https://github.com/zeromq/libzmq/releases/download/v4.3.5/zeromq-4.3.5.tar.gz \
  | sha256sum
# Should match above
```

---

## Questions?

If you're updating vendored code or adding new vendored dependencies, review this policy first. When in doubt:

1. Check if system libraries can be used instead
2. Verify license compatibility
3. Document the decision in this file
4. Ask in PR review before committing large vendored codebases

**Contact:** See [CLAUDE.md](CLAUDE.md) for project context.
