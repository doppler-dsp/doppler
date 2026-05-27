# C Return-Code Convention

doppler uses a two-tier return convention.  The tier is determined by the
**return type**, not by the function name.

---

## `int`-returning functions — status codes

Functions that return `int` use the named constants from `clib_common.h`:

| Constant | Value | Meaning |
|----------|-------|---------|
| `DP_OK` | `0` | Success |
| `DP_ENOMEM` | `−1` | Memory allocation failure |
| `DP_EINVAL` | `−2` | Invalid argument |

```c
#include "clib_common.h"

float complex out[1024];
if (awgn(42, 1.0f, 1024, out) != DP_OK) {
    /* handle OOM */
}
```

0 is always success — consistent with the C standard library and POSIX.
Never compare against the raw integer literals; use the named constants.

---

## `size_t`-returning functions — sample/byte counts

Functions that return `size_t` report how many samples (or bytes) were
written.  They operate on **already-created** objects and cannot fail
internally — malloc errors belong to the `create()` call, not to the
hot-path execute call.

```c
RateConverter_state_t *rc = RateConverter_create(0.5, 0);   /* NULL = OOM */
size_t n = RateConverter_execute(rc, in, 4096, out, 4096);  /* always succeeds */
```

One-shot count-returning functions (e.g. `RateConverter_convert`) return
0 only if allocation failed or `n_in == 0`.  A positive return always
means success.

---

## Pointer-returning functions — NULL on failure

`create()` functions return a heap-allocated state pointer, or `NULL` on
allocation failure or invalid arguments.

```c
awgn_state_t *g = awgn_create(0, 1.0f);
if (!g) { /* OOM or invalid amplitude */ }
```

---

## What just-makeit generates

jm generates `_ext.c` (Python glue) and stubs for `_core.h`/`_core.c`.
It never reads or modifies `_core.c` after the initial stub.

- **`create()` calls** in `_ext.c`: jm generates a `NULL` check and raises
  `MemoryError` — correct for the pointer convention above.
- **`execute()` calls** in `_ext.c`: jm generates no error check — correct
  because execute cannot fail post-create.
- **One-shot pure functions** (`awgn()`, `RateConverter_convert()`): always
  hand-written in `_core.c`; never appear in jm-generated code.

No changes to jm are needed or expected.

---

## Where the constants live

`DP_OK`, `DP_ENOMEM`, and `DP_EINVAL` are defined in `native/inc/clib_common.h`,
which every `_core.c` and `_core.h` includes transitively.  The streaming
layer (`native/inc/stream/stream.h`) defines its own `DP_ERR_*` codes for
network/I/O failures; those are distinct and not used in the DSP algorithm
layer.
