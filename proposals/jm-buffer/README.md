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

`f32_buffer.toml` beside this file is the whole declaration. It **compiles**
against this repo's real `buffer.h` — see the section below — and calls the
real symbols:

```c
self->handle    = dp_f32_create(n_samples);
int y           = dp_f32_write_cf(self->handle, x, x_len);
float _Complex *_p = dp_f32_wait_cf(self->handle, n);
dp_f32_consume(self->handle, n);
return PyLong_FromUnsignedLongLong(...(self->handle->capacity));
```

## It compiles, links, and the check runs itself

The first version of this proposal said *"verified — the generated binding
calls the real functions"*. It did call them, and that was verified by
**reading**. Compiling it found four defects reading could not.

The probe that found them then had the same problem one level down: it lived
in this README as a command naming `shim.h` and `frag.c`, **neither of which
is in the tree**, so it could not be run — and `regenerate.sh` could not
reproduce `generated/` either (`jm new` does not create `objects/`, and the
object was never registered with the module, so `apply` generated nothing and
said nothing about why). A claim nothing runs is prose, which is this repo's
own tier-1 shape.

So the probe now runs as part of regeneration, against the files as they
actually exist:

```sh
./regenerate.sh          # regenerate, then compile AND link the result
# probe: compiles and links clean
```

Compile catches a signature mismatch or an undeclared callee; link catches a
symbol that is declared and never defined. Both run, because they see
different defects.

What it caught:

|                                         |                                                                                    |
| --------------------------------------- | ---------------------------------------------------------------------------------- |
| `float _Complex *_p = dp_f32_wait(...)` | `wait` returns `type *` = `float *`. Hard error.                                   |
| `dp_f32_write(ab, x, n)`                | same mismatch on argument 2 — **`write` needs the sibling too**                    |
| `f32_buffer_reset()`                    | jm generated a `reset()` for a symbol the ring has no equivalent of → `--no-reset` |
| `state->capacity` in a property         | jm splices `expr` into a getter whose variable is `self->handle`, not `state`      |
| `f32_buffer_destroy()`                  | **an undeclared destroyer** — see below; it was being `#define`d away              |

## create_fn has no counterpart, and the probe was hiding it

`create_fn = "dp_f32_create"` names the C jm calls to construct. There is no
`destroy_fn` on an object — it is a capsule-module key, and jm warns if you
put one there — so jm emits `<comp>_destroy` for the dealloc path regardless:

```c
self->handle = dp_f32_create (n_samples);   /* mmaps a mirrored region */
...
f32_buffer_destroy (self->handle);          /* nothing defines this */
```

The compiler says so plainly:

```
error: implicit declaration of function 'f32_buffer_destroy'
```

An earlier probe carried `#define f32_buffer_destroy dp_f32_destroy`, which
silenced exactly that diagnostic. That is the hazard worth naming: the
workaround lived in the **harness**, so the probe passed while the
*declaration* stayed asymmetric — and had jm's scaffolded `<comp>_destroy`
existed under that name, it is a plain `free(state)` that never unmaps the
mirrored region.

`proposed-siblings.h` now forwards it like the other two, so create and
destroy are symmetric by construction. Verified by sabotage: rename the
forwarder and the build fails on the implicit declaration again.

Filed upstream as the asymmetry it is — an object honouring `create_fn` with
no way to say who destroys. If jm closes it, the forwarder collapses into a
manifest key.

## The siblings: ONE decision, all three instances

The ring stores **scalars** (`type *data`) while a jm borrow declares the
**element** the Python view has. So the mismatch is not about integer IQ at
all — `f32` and `f64` need the identical pair for the identical reason, and
`i16` is only the instance whose element type is unfamiliar.

`proposed-siblings.h` holds them. Both are **pure casts with no factor
change**: `n` is already in complex samples, because `wait` returns
`&data[(t & ab->mask) * 2]` — the `* 2` is already inside the ring.

```c
static inline float _Complex *
dp_f32_wait_cf (dp_f32_t *ab, size_t n)
{ return (float _Complex *) dp_f32_wait (ab, n); }

static inline int
dp_f32_write_cf (dp_f32_t *ab, const float _Complex *src, size_t n)
{ return dp_f32_write (ab, (const float *) src, n); }
```

*(An earlier draft of this file claimed the i16 sibling needed `2 * n`. It does
not — that factor was invented, and the compile is what settled it.)*

That reframing is the point: the sibling stops looking like a workaround for
integer IQ and becomes **the ring's actual borrow surface**, declared once per
instance in the same shape.

## What is still a decision — the i16 element type

With the siblings in place, `dp_i16_wait_iq` can return either shape, and this
is the substantive choice:

- **a structured array** `[('i','<i2'),('q','<i2')]` — 1-D, one element per
    sample, byte order stated. What `buffer_ext.c`'s own header comment already
    claims, and what
    [just-makeit#1310](https://github.com/just-buildit/just-makeit/issues/1310)
    chose after measuring all four zero-copy spellings.
- **today's 2-D `(n, 2)` int16** — also zero-copy, also one row per sample, but
    not what the comment promises and a different API from `F32Buffer`'s.

The measurement that rejected the third candidate is worth keeping either way:
packed `int32` is *also* 1-D and one element per sample, and

```
d + 1   ->   [1, 1, 3, 3, 5, 5, 7, 7]
```

increments **I only**, silently, because int32 addition carries across the I/Q
boundary. The structured form raises instead.

## The surface is now like-for-like

An earlier draft declared five methods and compared them to a strictly larger
hand-written API, which made the headline ratio dishonest. Declared now:

|            |                                              |
| ---------- | -------------------------------------------- |
| methods    | `write`, `wait`, `consume`, `close`          |
| properties | `capacity`, `available`, `dropped`, `closed` |

`available` and `closed` stay **properties**, so `buf.available` does not
silently become `buf.available()` for existing callers. `destroy` is jm's own
lifecycle (`tp_dealloc` plus an explicit `destroy()`), so it is not declared.

**69 declared lines** produce **341** of binding and **98** of `.pyi`.

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
