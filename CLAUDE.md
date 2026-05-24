# doppler — codebase guide for Claude

## Project

C-first DSP library. Every algorithm lives in C exactly once; Python, Rust, and
other language bindings are thin wrappers over the C ABI. The Python layer is a
CPython C extension generated and managed by **just-makeit** (jm).

See `~/.claude/skills/just-makeit.md` for the full jm reference.

---

## Repository layout

```
native/inc/<obj>/<obj>_core.h   — public C API + state struct (hand-written)
native/src/<obj>/<obj>_core.c   — algorithm implementation (hand-written)
native/src/<module>/<mod>_ext.c — CPython glue (jm-generated, patch sparingly)
native/src/<module>/CMakeLists.txt
native/tests/test_<obj>_core.c  — C-level unit tests
native/benchmarks/bench_<obj>_core.c

objects/<obj>.toml              — jm fragment: the interface declaration (source)
templates/                      — starter fragments for the two non-default patterns
scaffold/                       — legacy CLI scripts (historical; do not re-run)

src/doppler/<module>/__init__.py    — re-export only; no logic
src/doppler/<module>/tests/         — Python integration tests
src/doppler/<module>/benchmarks/
```

---

## Adding a new module or object

### Step 0 — characterise the algorithm

Ask one question:

> Can a single input sample be processed independently with small fixed state?

- **Yes** → use the CLI default (`jm object name --state ...`). No template needed.
- **No** → the C API owns block I/O. Pick a template:
  - Block processor / decimator / resampler / FFT / correlator → `templates/block_nostate.toml`
  - Signal generator (void input, array output) → `templates/void_source.toml`

### Step 1 — declare the interface

**Entry point A: CLI (exploratory, step/steps objects)**

```sh
source /tmp/jm-venv/bin/activate       # or install: bash ~/just-makeit/install.sh
jm object myobj --module mymodule --state gain:double:1.0 --mutable
jm property myobj gain --module mymodule --type double --writable --field
# CLI mutations route to objects/myobj.toml automatically (split layout)
```

**Entry point B: template (block I/O objects)**

```sh
cp templates/block_nostate.toml objects/myobj.toml  # or void_source.toml
# Edit objects/myobj.toml — replace <<OBJ>> and <<CLASS>>, tune init_params,
# methods, and properties for the algorithm.
```

### Step 2 — register in the module manifest

```toml
# just-makeit.toml
[module.mymodule]
objects = ["existing_obj", "myobj"]
```

### Step 3 — apply

```sh
jm apply objects/myobj.toml
# Materialises: _core.h stub, _core.c stub, _ext.c (regenerated),
# CMakeLists.txt, __init__.py, .pyi, C test, C bench, Python test, Python bench.
```

### Step 4 — implement the C core (the only phase requiring algorithm knowledge)

1. `native/inc/<obj>/<obj>_core.h` — add state struct fields; declare extra
   lifecycle functions (`reconfigure`, secondary constructors, etc.)
2. `native/src/<obj>/<obj>_core.c` — implement:
   - `<obj>_create()`: allocate, validate args, compute derived values
   - `<obj>_destroy()`: free heap members then struct
   - `<obj>_reset()`: zero state, reset counters
   - Method bodies: hot loops, format conversions, modular arithmetic
3. `native/src/<module>/<mod>_ext.c` — patch **only** for:
   - Dtype dispatch (float32 vs complex64 → different create function)
   - Non-trivial argument validation the generated code cannot infer
   - Lazy-alloc grow-on-demand when block size may change across calls

### Step 5 — build and test

```sh
cmake --build build --target <module>   # rebuild just this .so
pytest src/doppler/<module>/tests/      # Python integration tests
ctest --test-dir build -R <obj>         # C-level tests
```

### Step 6 — reconcile (if TOML changed after apply)

```sh
jm apply   # idempotent; reconciles CMakeLists, __init__.py, .pyi
```

### Step 7 — commit the fragment, not the commands

```sh
git add objects/<obj>.toml native/inc/<obj>/ native/src/<obj>/
git add src/doppler/<module>/tests/ src/doppler/<module>/<module>.pyi
```

---

## Known post-apply patches

These apply to all block-I/O objects (template users):

| Issue | Fix |
|---|---|
| `variable_output` lazy-alloc: buffer NULL until first call | Generated `if (!self->_buf) { _max = n; ... }` is correct since jm 0.13.6 — verify it's present |
| Init params all optional (`\|kkk` format string) | Expected — `create(0,0,0)` → NULL → `MemoryError`. Add validation in `_core.c` for human-readable errors |
| Reconfigure method doctest silently no-ops on invalid args | Hand-fix the docstring; the C API intentionally ignores invalid params |

---

## Build

```sh
# First time (from repo root):
cmake -B build -DBUILD_PYTHON=ON
cmake --build build

# Rebuild a single module:
cmake --build build --target filter

# Full test suite:
pytest src/doppler/
ctest --test-dir build
```

---

## Code style

- **C**: `clang-format` with `.clang-format` at repo root
- **Python**: `uvx ruff format --line-length=79`
- Line width: 79 characters in all languages
- Docstrings: numpy-style, verbose — explain how it works, not just what
- Inline comments: explain intent and non-obvious mechanics only
