# Contributing to doppler

The short version: **C first, then Python, then Rust.**
Every algorithm lives in the C library exactly once.
Language bindings are interface — they call C, they don't
reimplement it.

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

Document the API thoroughly here — this is the C source of truth.
The semantics described in this header will be translated into the
Python stub (see below), which becomes the public-facing documentation.

```c
#ifndef DP_FOO_H
#define DP_FOO_H

#include <dp/stream.h>   /* dp_cf32_t and friends */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief One-line description of what foo does.
 *
 * Longer description covering the algorithm, its assumptions, and any
 * key implementation choices (e.g. phase encoding, coefficient layout).
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

**Mandatory.**  A minimal, runnable program showing the typical use
case with console output so users can see expected results.

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

> [!IMPORTANT] **No `dp_` PREFIX!**
>
> Python has dotted module names `doppler.foo`.

Keep it thin: type conversion, error translation, lifetime bridging.
Logic lives in C, not here.  If the wrapper is a direct pass-through,
skip module-level docstrings and let the stub own the documentation
(see step 4).  Docstrings in `.py` files belong to pure-Python
modules (polyphase design tools, optimisation helpers, etc.) where
there is no stub.

### 4. Type stub — `python/doppler/dp_foo.pyi`

> [!IMPORTANT] **THIS IS THE CANONICAL DOCUMENTATION FOR THE MODULE.**

The stub is what mkdocstrings reads to generate the API reference
page.  Write it as if it were the primary user-facing documentation,
because it is.  Full NumPy-style docstrings with runnable examples
are mandatory on every public class and method.

```python
"""Type stubs for the dp_foo C extension."""

from __future__ import annotations

import numpy as np
from numpy.typing import NDArray

class Foo:
    """One-line description.

    Longer description capturing the key behaviour, any invariants,
    and how this maps to the underlying C type (dp_foo_t).

    Parameters
    ----------
    param:
        Description; units and valid range.
    """

    def __init__(self, param: float) -> None: ...

    def execute(
        self, x: NDArray[np.complex64]
    ) -> NDArray[np.complex64]:
        """Process a block of cf32 samples.

        Parameters
        ----------
        x:
            Input samples, dtype=complex64.

        Returns
        -------
        NDArray[np.complex64]
            Output samples.  Length depends on the configured rate.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dp_foo import Foo
        >>> f = Foo(param=0.5)
        >>> y = f.execute(np.ones(64, dtype=np.complex64))
        >>> y.dtype
        dtype('complex64')
        """
        ...

    def reset(self) -> None:
        """Reset internal state without reallocating."""
        ...

    def destroy(self) -> None:
        """Release the underlying C resource (also called on GC)."""
        ...

    def __enter__(self) -> "Foo": ...
    def __exit__(self, *args: object) -> None: ...
```

### 5. Tests — `python/doppler/tests/test_<module>.py`

**Mandatory.**  pytest.  Exercise the round-trip through the C library —
don't just test the Python layer in isolation.

### 6. Documentation page — `docs/api/python-<module>.md`

Create a page with a brief intro and an autodoc directive:

```markdown
# Python Foo API

One sentence describing what this does and which C functions back it.

Source: [`python/doppler/dp_foo.pyi`](https://github.com/doppler-dsp/doppler/blob/main/python/doppler/dp_foo.pyi)

---

::: doppler.dp_foo.Foo
```

Then add it to the nav in `zensical.toml`:

```toml
nav = [
  ...
  { "API: Foo" = "api/python-foo.md" },
]
```

Run `make docs-build` to verify it renders.  No hand-written API table
is needed — the stub docstrings are the source of truth.

### Verify

```sh
make pyext && make python-test && make docs-build
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

Docstrings are derived from the Python stub — translate parameter
names and types to Rust idioms; keep the description parallel.

### 3. Tests / examples

Add tests inline or in `ffi/rust/tests/`.

### Verify

```sh
make rust-test    # single-threaded — see Gotchas
```

---

## Cross-language testing

There is no special cross-language test framework.

Pure Python in doppler is rare by design — it exists only for things
that are genuinely better expressed in Python: filter design,
polynomial fitting, LP optimisation.  All of it produces **parameters
that get handed to a C runtime**.  The integration test is therefore
just a normal pytest that exercises the full path:

```python
_, bank = kaiser_prototype(...)      # pure Python — design
r = Resamp(L, N, bank, rate=2.0)    # C extension  — create
y = r.execute(x)                     # C extension  — execute
assert stopband_attenuation(y) > 60  # validate end-to-end
```

This lives in the module's regular `test_<module>.py`.  No subprocess,
no golden vectors, no special harness.

Rust wraps C directly, so Rust tests already are the Rust-C
integration tests.  Nothing extra needed there either.

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
Always `cargo test -- --test-threads=1` and `ctest`
(sequential by default).

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
