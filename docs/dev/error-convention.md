# C Return-Code Convention

doppler uses a three-tier return convention. The tier is determined by the
**return type**, not by the function name.

______________________________________________________________________

## `int`-returning functions — status codes

Functions that return `int` use the named constants from `clib_common.h`.
The core DSP algorithm path only ever returns three of these; the rest are
meaningful to the streaming layer (below):

| Constant           | Value | Meaning                                |
| ------------------ | ----- | -------------------------------------- |
| `DP_OK`            | `0`   | Success                                |
| `DP_ERR_INIT`      | `−1`  | Initialisation failed (context/socket) |
| `DP_ERR_SEND`      | `−2`  | Send failed                            |
| `DP_ERR_RECV`      | `−3`  | Receive failed or timed out (EAGAIN)   |
| `DP_ERR_INVALID`   | `−4`  | Invalid argument                       |
| `DP_ERR_TIMEOUT`   | `−5`  | Operation timed out                    |
| `DP_ERR_MEMORY`    | `−6`  | Memory allocation failure              |
| `DP_ERR_TOO_LARGE` | `−7`  | Frame exceeds transport max payload    |

All eight live in one unified enum in `clib_common.h` — `stream.h` doesn't
define its own codes, it just includes this header, so a value never means
two things in one TU.

```c
#include <awgn/awgn_core.h>
#include <complex.h>

int main(void)
{
  float complex out[1024];
  if (awgn(42, 1.0f, 1024, out) != DP_OK) {
      /* handle OOM */
      return 1;
  }
  return 0;
}
```

0 is always success — consistent with the C standard library and POSIX.
Never compare against the raw integer literals; use the named constants.

______________________________________________________________________

## `size_t`-returning functions — sample/byte counts

Functions that return `size_t` report how many samples (or bytes) were
written. They operate on **already-created** objects and cannot fail
internally — malloc errors belong to the `create()` call, not to the
hot-path execute call.

```c
#include <RateConverter/RateConverter_core.h>
#include <complex.h>

int main(void)
{
  float complex in[4096]  = { 0 };
  float complex out[4096];

  RateConverter_state_t *rc = RateConverter_create(0.5, 0);   /* NULL = OOM */
  if (!rc) return 1;
  size_t n = RateConverter_execute(rc, in, 4096, out, 4096);  /* always succeeds */
  (void) n;
  return 0;
}
```

One-shot count-returning functions (e.g. `RateConverter_convert`) return
0 only if allocation failed or `n_in == 0`. A positive return always
means success.

______________________________________________________________________

## Pointer-returning functions — NULL on failure

`create()` functions return a heap-allocated state pointer, or `NULL` on
allocation failure or invalid arguments.

```c
#include <awgn/awgn_core.h>

int main(void)
{
  awgn_state_t *g = awgn_create(0, 1.0f);
  if (!g) { /* OOM or invalid amplitude */ return 1; }
  awgn_destroy(g);
  return 0;
}
```

______________________________________________________________________

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

______________________________________________________________________

## Where the constants live

All eight codes are defined once in `native/inc/clib_common.h`, which every
`_core.c`/`_core.h` includes transitively. `native/inc/stream/stream.h`
includes `clib_common.h` for the same codes rather than defining its own —
one scheme everywhere. The DSP algorithm layer only ever returns
`DP_OK`/`DP_ERR_MEMORY`/`DP_ERR_INVALID`; the rest
(`DP_ERR_INIT`/`SEND`/`RECV`/`TIMEOUT`/`TOO_LARGE`) are meaningful to the
streaming transport.
