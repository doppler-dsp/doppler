# Contributing to doppler

The short version: **C first, then Python, then Rust.**
Every algorithm lives in the C library exactly once.
Language bindings are glue — they call C, they don't reimplement it.
See [CLAUDE.md](CLAUDE.md) for the full design philosophy.

## Table of contents

- [Adding a new C module](#adding-a-new-c-module)
- [Adding a Python binding](#adding-a-python-binding)
- [Adding a Rust FFI binding](#adding-a-rust-ffi-binding)
- [Build commands](#build-commands)
- [Code style](#code-style)
- [Gotchas](#gotchas)

---

## Adding a new C module

### 1. Header — `c/include/dp/<module>.h`

```c
#ifndef DP_FOO_H
#define DP_FOO_H

#include <dp/stream.h>   /* dp_cf32_t and friends */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque foo state. */
typedef struct dp_foo dp_foo_t;

dp_foo_t *dp_foo_create(float param);
void      dp_foo_destroy(dp_foo_t *h);  /* NULL-safe */
void      dp_foo_reset(dp_foo_t *h);
size_t    dp_foo_execute(dp_foo_t *h,
                         const dp_cf32_t *in, size_t n,
                         dp_cf32_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DP_FOO_H */
```

Rules:
- All public symbols use the `dp_` prefix.
- Opaque handles hide the `struct` definition.
- Coefficients/state passed to `_create` are **copied internally** — the
  caller may free them immediately.
- Document the header with Doxygen `@brief` / `@param` / `@return`.
- Add `#include <dp/foo.h>` to `c/include/doppler.h`.

### 2. Implementation — `c/src/<module>.c`

Plain C99, no VLAs in hot paths.
Follow the existing style: helpers at the top, public API at the bottom.

### 3. Register in CMake — `c/CMakeLists.txt`

```cmake
set(DOPPLER_SOURCES
    ...
    src/foo.c       # ← add here
)
```

### 4. Tests — `c/tests/test_<module>.c`

Self-contained: embed any coefficients or design helpers you need.
Use the same `pass/fail` counter pattern as `test_nco.c`.

Register in `c/CMakeLists.txt`:

```cmake
add_executable(test_foo tests/test_foo.c)
target_link_libraries(test_foo PRIVATE doppler_static m)
add_test(NAME foo_unit_tests COMMAND test_foo)
```

### 5. Benchmark (optional) — `c/bench/bench_<module>.c`

Same self-contained pattern.  Register as `bench_foo_c`.

### Verify

```sh
make build && make test
```

---

## Adding a Python binding

### 1. Generate the extension skeleton

```sh
python tools/gen_pyext.py c/include/dp/foo.h
# writes python/ext/dp_foo.c — review and hand-tune
```

The generator produces the boilerplate `PyObject` wrapper,
`__init__` / `__dealloc__`, method table, and module init.
Hand-tune for zero-copy NumPy I/O and GIL release on blocking calls.

### 2. Register in `python/CMakeLists.txt`

```cmake
add_library(dp_foo MODULE ext/dp_foo.c)
target_include_directories(dp_foo PRIVATE
    ${NumPy_INCLUDE_DIR} ${Python3_INCLUDE_DIRS})
target_link_libraries(dp_foo PRIVATE doppler m)
set_target_properties(dp_foo PROPERTIES
    LANGUAGE C PREFIX "" SUFFIX "${PY_EXT_SUFFIX}")
```

Add `dp_foo` to the `DEPENDS` list of the `pyext` target and add its
`copy` command alongside the others.

### 3. Python wrapper (if needed) — `python/doppler/<module>/`

Keep it thin.  Import the C extension and re-export with a Pythonic
interface.  Type annotations and docstrings live here; logic does not.

### 4. Tests — `python/doppler/tests/test_<module>.py`

pytest.  Exercise the round-trip through the C library — don't just
test the Python layer in isolation.

### Verify

```sh
make pyext && make python-test
```

---

## Adding a Rust FFI binding

### 1. Raw binding — `ffi/rust/src/lib.rs`

Add an `extern "C"` block and any `#[repr(C)]` types needed:

```rust
/// Opaque handle for dp_foo.
#[repr(C)]
pub struct DpFooRaw {
    _private: [u8; 0],
}

extern "C" {
    pub fn dp_foo_create(param: f32) -> *mut DpFooRaw;
    pub fn dp_foo_execute(
        h: *mut DpFooRaw,
        input: *const DpCf32, n: usize,
        output: *mut DpCf32, max_out: usize,
    ) -> usize;
    pub fn dp_foo_destroy(h: *mut DpFooRaw);
}
```

### 2. Safe wrapper — `ffi/rust/src/foo.rs`

```rust
use super::{DpCf32, DpFooRaw};

pub struct Foo(*mut DpFooRaw);

impl Foo {
    pub fn new(param: f32) -> Self {
        let h = unsafe { super::dp_foo_create(param) };
        assert!(!h.is_null(), "dp_foo_create returned null");
        Foo(h)
    }

    pub fn execute(&mut self, input: &[DpCf32],
                   output: &mut [DpCf32]) -> usize {
        unsafe {
            super::dp_foo_execute(
                self.0,
                input.as_ptr(), input.len(),
                output.as_mut_ptr(), output.len(),
            )
        }
    }
}

impl Drop for Foo {
    fn drop(&mut self) {
        unsafe { super::dp_foo_destroy(self.0) }
    }
}
```

Declare the module in `lib.rs`: `pub mod foo;`

### 3. Tests / examples

Add tests inline or in `ffi/rust/tests/`.
Register examples in `Cargo.toml` as `[[example]]` entries.

### Verify

```sh
make rust-test    # single-threaded — see Gotchas
```

---

## Build commands

| Command | What it does |
|---------|-------------|
| `make build` | cmake configure + build (RelWithDebInfo) |
| `make blazing` | Release + `-march=native` |
| `make test` | CTest (C tests) |
| `make pyext` | Build + copy Python C extensions |
| `make python-test` | pytest with coverage |
| `make rust-test` | `cargo test -- --test-threads=1` |
| `make install` | System install |
| `make docs-build` | Build documentation site |

---

## Code style

| Language | Tool | Config |
|----------|------|--------|
| C | `clang-format` | `.clang-format` |
| Python | `uvx ruff format --line-length=79` | — |
| Rust | `rustfmt` | `rustfmt.toml` |

Line width: **79 characters** across all languages.

---

## Gotchas

**Build artifacts stay out of the source tree.**
Always `make build` (outputs to `build/`).
Never `cmake -B . -S c`.

**FFT tests must be single-threaded.**
The global plan state is not thread-safe.
Always `cargo test -- --test-threads=1` and `ctest` (sequential by default).

**Python extensions are auto-generated scaffolding.**
`gen_pyext.py` produces a starting point — review and hand-tune every
generated file before committing.

**Thin means thin.**
A wrapper file growing past a few hundred lines is probably
reimplementing C logic.  Move it to C instead.

**Never redefine wire formats.**
One `dp_header_t`, one magic value, one framing convention.
Don't define custom structs for data that crosses the C boundary.

**Coefficient arrays passed to `_create` are copied.**
The caller owns them and may free immediately after `_create` returns.
