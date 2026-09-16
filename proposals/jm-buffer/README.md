# Proposal: make the ring buffer jm-owned

**Nothing here is wired into the build.** This directory is a declaration, the
glue jm generates from it, and a script that reproduces that glue — so the
binding can be diffed against the hand-written one before anything is deleted.

## Why

`native/src/buffer/buffer_ext.c` (930 lines) and
`src/doppler/buffer/buffer.pyi` (936 lines) are hand-written because the ring
could not be declared: jm had no way to express a component whose C is
header-only, nor a method that lends a pointer into memory the component owns.
It now does.

The cost of hand-writing shows up as **three faces disagreeing about one
method**, live on `main` today:

| face                                             | says `I16Buffer.wait` returns           |
| ------------------------------------------------ | --------------------------------------- |
| `buffer_ext.c:730`                               | 2-D `(n, 2)` `NPY_INT16`                |
| `buffer_ext.c:7` (the file's own header comment) | "int16 IQ pairs (**structured array**)" |
| `buffer.pyi:704`                                 | `NDArray[np.int16]`                     |

Nothing reconciles them, and `jm status --check` cannot see it because the
module is outside the manifest. Generated glue makes that state unreachable
rather than merely detectable.

## What maps cleanly

`DECLARE_DP_BUFFER` is a good fit, and the C **does not change**. jm binds the
symbols the macro already emits, via `fn = "dp_f32_wait"` and friends:

| ring op                           | manifest                                    |
| --------------------------------- | ------------------------------------------- |
| `dp_f32_create(size_t n_samples)` | `create_fn` + an `init_param`               |
| `type *dp_f32_wait(ab, n)`        | a method with `borrow = true`               |
| `dp_f32_consume(ab, n)`           | an ordinary method                          |
| `dp_f32_available/closed/close`   | ordinary methods                            |
| the header-only C, no `.c` file   | `header_only = "true"` → INTERFACE core lib |

`f32_buffer.toml` beside this file is the whole declaration — **58 lines**,
generating **347** lines of binding and **95** of `.pyi`. For the three
instances that is roughly **174 declared lines against 1,866 maintained ones**,
and the generated side is regenerated rather than maintained.

Verified: the generated binding calls the real symbols —

```c
self->handle = dp_f32_create(n_samples);
float _Complex *_p = dp_f32_wait(self->handle, n);
dp_f32_consume(self->handle, n);
```

## What needs a decision — the i16 instance

This is the part worth arguing about before adopting.

`dp_i16_wait` returns `int16_t *`, but a complex q15 sample is a **pair**.
numpy has no complex-integer dtype, so
[just-makeit#1310](https://github.com/just-buildit/just-makeit/issues/1310)
measured the four zero-copy spellings of those bytes and chose a **structured
array**, `[('i','<i2'),('q','<i2')]` — 1-D, one element per sample, byte order
stated. That is the shape `buffer_ext.c`'s own header comment already claims,
and the shape the code does not produce.

The measurement that settled it: the tempting packed-`int32` alternative is
also 1-D and one element per sample, and

```
d + 1   ->   [1, 1, 3, 3, 5, 5, 7, 7]
```

increments **I only**, silently, because int32 addition carries across the I/Q
boundary. `arr * 2` and `arr - dc` corrupt the same way. The structured form
raises `ufunc 'add' did not contain a loop` instead — a loud failure where the
packed form gives a quiet wrong answer.

To generate that, jm wants the kernel to return the record type. The smallest
change on doppler's side is a one-line sibling beside the macro:

```c
typedef struct { int16_t i; int16_t q; } dp_iq16_t;

static inline dp_iq16_t *
dp_i16_wait_iq (dp_i16_t *ab, size_t n)
{
  return (dp_iq16_t *) dp_i16_wait (ab, 2 * n);   /* n SAMPLES */
}
```

Note `2 * n`: the existing `wait` counts `int16` slots, the record view counts
samples. That factor is exactly the kind of thing three hand-written faces get
to disagree about, which is the argument for declaring it once.

**The alternative is to keep the 2-D `(n, 2)` shape** the code produces today.
It is defensible — it is also zero-copy and one row per sample — but it is not
what the header comment promises, and per-component arithmetic on it is a
different API from what `F32Buffer` offers. Either way, picking one and
declaring it is the point.

## Blocked on a just-makeit release

`pyproject.toml` pins `just-makeit==0.75.5`. The three features this needs all
landed **after** that release and are unreleased:

- `header_only` — just-buildit/just-makeit#1311
- `borrow` — #1312
- a borrowed `record_dtype` — #1310 decision B, implemented in #1317
- and the fix that makes them compose at all — #1321 / #1322

So this cannot be adopted until jm ships them. Treat this directory as
reviewable intent, not a change to apply today.

## Suggested order

1. Review the declaration and the generated glue here against
    `native/src/buffer/buffer_ext.c`.
1. Decide the i16 question above.
1. When jm ships, bump the pin, move `f32_buffer.toml` into `objects/`, add the
    f64 and i16 siblings, and run `just-makeit apply`.
1. Delete the hand-written binding and `.pyi` only once the generated faces are
    confirmed equivalent — `jm status --check` then guards them.

## Reproducing

```sh
./regenerate.sh
diff -u ../../native/src/buffer/buffer_ext.c generated/buffer_ext_f32_buffer.c
```

The committed copy is in **this repo's GNU style**, not jm's. The script does
not run a formatter itself — the Makefile is the SSOT for how tools run here,
and the style already has two homes that agree: jm's `c_format_command` on
`apply`, and this repo's pre-commit hook on commit. So regenerate, then commit,
and the hook normalises it:

```sh
./regenerate.sh && git add -A && git commit
```

Skip that and the fresh output differs from the committed copy by ~500 lines of
pure style, because jm emits its own brace and spacing conventions — which is
itself worth seeing once, and is why adoption should set `c_format_command`.
