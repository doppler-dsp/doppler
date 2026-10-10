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
#include <doppler/awgn/awgn_core.h>
#include <complex.h>

int main(void)
{
  float complex out[1024];
  if (dp_awgn(42, 1.0f, 1024, out) != DP_OK) {
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
#include <doppler/RateConverter/RateConverter_core.h>
#include <complex.h>

int main(void)
{
  float complex in[4096]  = { 0 };
  float complex out[4096];

  dp_RateConverter_state_t *rc = dp_RateConverter_create(0.5, 0);   /* NULL = OOM */
  if (!rc) return 1;
  size_t n = dp_RateConverter_execute(rc, in, 4096, out, 4096);  /* always succeeds */
  (void) n;
  return 0;
}
```

One-shot count-returning functions (e.g. `dp_RateConverter_convert`) return
0 only if allocation failed or `n_in == 0`. A positive return always
means success.

______________________________________________________________________

## Pointer-returning functions — NULL on failure

`create()` functions return a heap-allocated state pointer, or `NULL` on
allocation failure or invalid arguments.

```c
#include <doppler/awgn/awgn_core.h>

int main(void)
{
  dp_awgn_state_t *g = dp_awgn_create(0, 1.0f);
  if (!g) { /* OOM or invalid amplitude */ return 1; }
  dp_awgn_destroy(g);
  return 0;
}
```

______________________________________________________________________

## Refusal reasons — `const char **why`

A function that can refuse its input and say why takes the reason as one
parameter, always spelled the same way:

<!-- docs-snippet: skip=a signature-only excerpt; the program below runs it -->

```c
size_t dp_field_bits (const char *spec, uint8_t *out, const char **why);
```

- **Static.** On refusal the function stores a pointer to a string literal:
    a fixed sentence that outlives the call.
- **Written only on refusal.** On success `*why` is left as the caller set
    it, so initialise it if you read it unconditionally.
- **Optional.** `why` may be `NULL`; the function then refuses silently, by
    its return value alone.
- **Never freed.** The caller does not own the string.

```c
#include <doppler/wfm/wfm_core.h>
#include <stdio.h>

int main(void)
{
  const char *why = NULL;
  if (dp_field_bits("pn::10", NULL, &why) != 0 || !why)
    return 1;                      /* must refuse, and say why */
  printf("refused: %s\n", why);    /* a static sentence: never free() it */
  return 0;
}
```

Why this shape and no other:

- **One shape for every binding.** jm binds exactly this one: a function's
    `why = true`, a composer's `from_json_why` / `from_file_why`, and an
    owned-pointer source field's `parse_why` all raise `*why` as the
    `ValueError`'s message. A second shape, such as a
    caller-owned `char *why, size_t why_cap` buffer, needs its own binding
    and splits the convention in two.
- **No allocation on an error path.** A pointer to a literal cannot fail. A
    formatted buffer can truncate, and a heap string needs an owner.
- **A reason is a fixed sentence.** If a caller needs a number (which byte,
    how far out of range), give it an accessor. Don't format it into the
    reason.

`make lint-why-param` enforces this. It runs `scripts/check_why_param.py`
over every header under `native/inc/`, with no list to register in, and
fails on any function parameter named `why` or ending `_why` that is not
exactly `const char **`.

______________________________________________________________________

## What just-makeit generates

jm generates `_ext.c` (Python glue) and stubs for `_core.h`/`_core.c`.
It never reads or modifies `_core.c` after the initial stub.

- **`create()` calls** in `_ext.c`: jm generates a `NULL` check and raises
    `MemoryError` — correct for the pointer convention above, where `NULL`
    covers an invalid argument as well as an allocation failure. An object
    whose refusals are mostly invalid arguments declares `create_error` and
    `create_error_message` in its manifest (jm gh-482), and the check raises
    that exception with the rules instead: `PSD` and `AccTrace` raise
    `ValueError` (#1986), as `ddc`, `RateConverter` and `burst_acq` do.
- **`execute()` calls** in `_ext.c`: jm generates no error check — correct
    because execute cannot fail post-create.
- **One-shot pure functions** (`dp_awgn()`, `dp_RateConverter_convert()`): always
    hand-written in `_core.c`; never appear in jm-generated code.

No changes to jm are needed or expected.

______________________________________________________________________

## Where the constants live

All eight codes are defined once in `native/inc/doppler/clib_common.h`, which every
`_core.c`/`_core.h` includes transitively. `native/inc/doppler/stream/stream.h`
includes `clib_common.h` for the same codes rather than defining its own —
one scheme everywhere. The DSP algorithm layer only ever returns
`DP_OK`/`DP_ERR_MEMORY`/`DP_ERR_INVALID`; the rest
(`DP_ERR_INIT`/`SEND`/`RECV`/`TIMEOUT`/`TOO_LARGE`) are meaningful to the
streaming transport.
