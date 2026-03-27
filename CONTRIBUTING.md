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

The C header is the **single source of truth** for the entire API.
The Python and Rust docstrings are derived from it — write it once,
write it well.

```c
#ifndef DP_FOO_H
#define DP_FOO_H

#include <dp/stream.h>   /* dp_cf32_t and friends */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file foo.h
 * @brief One-line description of what foo does.
 *
 * Longer description covering the algorithm, its assumptions, and any
 * key implementation choices (e.g. phase encoding, coefficient layout).
 * Include a usage example:
 *
 * @code
 * dp_foo_t *h = dp_foo_create(0.5f);
 * dp_cf32_t out[1024];
 * dp_foo_execute(h, in, 1024, out, 1024);
 * dp_foo_destroy(h);
 * @endcode
 */

/** @brief Opaque foo state. */
typedef struct dp_foo dp_foo_t;

/**
 * @brief Create a foo processor.
 * @param param  Description of param; units, valid range.
 * @return       Heap-allocated handle, or NULL on failure.
 */
dp_foo_t *dp_foo_create(float param);

/** @brief Free a foo processor.  May be NULL (no-op). */
void dp_foo_destroy(dp_foo_t *h);

/** @brief Reset internal state without reallocating. */
void dp_foo_reset(dp_foo_t *h);

/**
 * @brief Process a block of cf32 samples.
 * @param h        Must be non-NULL.
 * @param in       Input samples (may be NULL if n == 0).
 * @param n        Number of input samples.
 * @param out      Output buffer (must hold >= max_out samples).
 * @param max_out  Capacity of out.
 * @return         Number of output samples written.
 */
size_t dp_foo_execute(dp_foo_t *h,
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
- Add `#include <dp/foo.h>` to `c/include/doppler.h`.

### 2. Implementation — `c/src/<module>.c`

Plain C99, no VLAs in hot paths.
Helpers at the top, public API at the bottom.

### 3. Register in CMake — `c/CMakeLists.txt`

```cmake
set(DOPPLER_SOURCES
    ...
    src/foo.c       # ← add here
)
```

### 4. Tests — `c/tests/test_<module>.c`

**Mandatory.**  Self-contained: embed any coefficients or design helpers
you need.  Use the same `pass/fail` counter pattern as `test_nco.c`.

Register in `c/CMakeLists.txt`:

```cmake
add_executable(test_foo tests/test_foo.c)
target_link_libraries(test_foo PRIVATE doppler_static m)
add_test(NAME foo_unit_tests COMMAND test_foo)
```

### 5. Benchmark — `c/bench/bench_<module>.c`

**Mandatory.**  Reports input MSamples/s at representative block sizes
and rates.  Self-contained — no external setup required.
Register as `bench_foo_c` in CMakeLists.txt.

### 6. Example — `c/examples/<module>_demo.c`

**Mandatory.**  A minimal, runnable program showing the typical use case.
The same scenario should be ported to Python and Rust (see below) — the
three examples should be recognisably parallel.

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

### 3. Python wrapper — `python/doppler/<module>/`

Keep it thin: type conversion, error translation, lifetime bridging.
Logic lives in C, not here.

**Docstrings are mandatory, NumPy style, with runnable examples.**
The C header is the source of truth — translate it, don't replace it:

```python
def execute(self, x: np.ndarray) -> np.ndarray:
    """Process a block of cf32 samples.

    Parameters
    ----------
    x : np.ndarray, dtype=complex64
        Input samples.

    Returns
    -------
    np.ndarray, dtype=complex64
        Output samples.  Length depends on the configured rate.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.foo import Foo
    >>> f = Foo(param=0.5)
    >>> y = f.execute(np.ones(64, dtype=np.complex64))
    >>> y.dtype
    dtype('complex64')
    """
```

All examples must pass `pytest --doctest-modules` without skipping.

### 4. Type stubs — `python/doppler/<module>/__init__.pyi`

**Mandatory, fully typed.**  Every public function and class needs a
stub.  Use `np.ndarray` with `np.dtype` comments where array shapes
and dtypes matter.

```python
import numpy as np

class Foo:
    def __init__(self, param: float) -> None: ...
    def execute(self, x: np.ndarray) -> np.ndarray: ...
    def reset(self) -> None: ...
```

### 5. Tests — `python/doppler/tests/test_<module>.py`

**Mandatory.**  pytest.  Exercise the round-trip through the C library —
don't just test the Python layer in isolation.

### 6. Example — `python/doppler/examples/<module>_demo.py`

**Mandatory.**  Port of the C example.  Should produce the same
observable result with the same algorithm, demonstrating the Python
ergonomics over the C ABI.

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

**Docstrings are derived from the Python docstrings** (which are
themselves derived from the C header).  Translate parameter names and
types to Rust idioms; keep the description and example parallel:

```rust
/// Process a block of cf32 samples.
///
/// # Arguments
///
/// * `input`  – Input samples as `&[DpCf32]`.
/// * `output` – Output buffer (must be pre-allocated).
///
/// # Returns
///
/// Number of output samples written.
///
/// # Example
///
/// ```no_run
/// use doppler::foo::Foo;
/// use doppler::DpCf32;
///
/// let mut f = Foo::new(0.5);
/// let input  = vec![DpCf32 { i: 1.0, q: 0.0 }; 64];
/// let mut output = vec![DpCf32::default(); 128];
/// let n = f.execute(&input, &mut output);
/// ```
```

### 3. Tests / examples

Add tests inline or in `ffi/rust/tests/`.

**Example — `ffi/rust/examples/<module>_demo.rs` — mandatory.**
Register in `Cargo.toml` as `[[example]]`.
Port of the C and Python examples: same scenario, Rust idioms.

### Verify

```sh
make rust-test    # single-threaded — see Gotchas
```

---

## Cross-language testing

There is no special cross-language test framework.

Pure Python in doppler is rare by design — it exists only for things
that are genuinely better expressed in Python: filter design, polynomial
fitting, LP optimisation.  All of it produces **parameters that get
handed to a C runtime**.  The integration test is therefore just a
normal pytest that exercises the full path:

```python
_, bank = kaiser_prototype(...)      # pure Python — design
r = Resamp(L, N, bank, rate=2.0)    # C extension  — create
y = r.execute(x)                     # C extension  — execute
assert stopband_attenuation(y) > 60  # validate end-to-end
```

This lives in the module's regular `test_<module>.py`.  No subprocess,
no golden vectors, no special harness.

Rust wraps C directly, so Rust tests already are the Rust-C integration
tests.  Nothing extra needed there either.

---

## Build commands

| Command | What it does |
|---------|-------------|
| `make build` | cmake configure + build (RelWithDebInfo) |
| `make blazing` | Release + `-march=native` |
| `make test` | CTest (C tests) |
| `make pyext` | Build + copy Python C extensions |
| `make python-test` | pytest with coverage + doctest |
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
