# iqtools — a downstream project built on doppler

An example of consuming doppler from *another* just-makeit project. It does two
things that are awkward to work out from the docs alone:

1. **Links `libdoppler.a`** — the static library and the public C headers, from
    a build tree or an install. No doppler source is vendored, copied or
    rebuilt here.
1. **Declares a just-makeit `view`** over doppler's reader, exposing a second
    Python class with a *different constructor* over the same C core — with no
    hand-written CPython anywhere in the project.

`jm apply` generates the bindings, the per-module `CMakeLists.txt`, and the
`__init__.py`/`.pyi` under `src/iqtools/` from **three** manifest files:
`just-makeit.toml` (project + the shared enum), `objects/capture.toml` (the
object and its view) and `modules/capture.toml` (the module — and the one line
that links doppler). The C core and the tests are hand-written; see
[Hand-owned files](#hand-owned-files) for the full map.

______________________________________________________________________

## What it exposes, and why

doppler's `Reader` detects a capture's file type from its content — right for a
self-describing capture (BLUE, SigMF), and *impossible* for a headerless one.
`raw` and `csv` carry no sample type, no sample rate and no centre frequency,
so the reader has to fall back to defaults and cannot tell you it did.

That asymmetry is a constructor problem, which is exactly what a view is for:

| class        | constructor                                             | for                                                         |
| ------------ | ------------------------------------------------------- | ----------------------------------------------------------- |
| `Capture`    | `Capture(path)`                                         | a self-describing capture — let the file speak              |
| `RawCapture` | `RawCapture(path, sample_type=…, endian=…, fs=…, fc=…)` | a headerless capture — you supply what the file cannot hold |

One C core, one `.so`, one set of methods. Only the way in differs.

A third property, `metadata_source`, reports where `fs`/`fc` actually came
from — `"file"`, `"supplied"` or `"none"` — so a default can never be mistaken
for a reading.

### The difference is not cosmetic

Both classes opening the *same* 8192-sample ci16 raw file:

```text
$ ./build/tools/iq_info capture.raw ci16 2.4e6
  Capture      fs=         0.0 Hz  fc=  0.0 Hz  n=4096  source=none
  RawCapture   fs=   2400000.0 Hz  fc=  0.0 Hz  n=8192  source=supplied
```

`Capture` reports **half the samples** and no error: nothing in a raw file
states its sample type, so it decodes ci16 at the cf32 stride. `RawCapture`,
told the truth, recovers the real length. Both behaviours are pinned by tests
(`test_capture_gets_the_stride_wrong_on_a_headerless_file` is deliberate).

______________________________________________________________________

## Getting doppler

**You need neither doppler's source nor a doppler build.** Every release ships
a self-contained C SDK for each platform — about 1.8 MB — and that is the
normal way to consume it:

```bash
PLAT=linux-x86_64          # or linux-aarch64, macos-arm64

# Resolve the current release rather than hard-coding one: a version typed
# into a page is stale at the next tag.
VER=$(curl -fsSL https://api.github.com/repos/doppler-dsp/doppler/releases/latest \
      | sed -n 's/.*"tag_name": *"v\([^"]*\)".*/\1/p')

curl -fsSL -O https://github.com/doppler-dsp/doppler/releases/download/v$VER/doppler-$VER-$PLAT.tar.gz
mkdir -p ~/.local/doppler
tar xzf doppler-$VER-$PLAT.tar.gz -C ~/.local/doppler
```

With the GitHub CLI it is one line, on the same principle — no version named:

```bash
gh release download --repo doppler-dsp/doppler \
   --pattern "doppler-*-$PLAT.tar.gz" --dir /tmp
```

What is in it:

```text
lib/libdoppler.a          the static library this example links
lib/libdoppler.so         the shared one, if you prefer it
lib/cmake/doppler/        the find_package(doppler) package config
lib/pkgconfig/doppler.pc  for builds that are not CMake
include/                  the public headers
bin/wfmgen                the waveform generator CLI
```

Nothing else is required — no Python, no toolchain beyond a C compiler, and no
doppler checkout.

> **If your doppler is too old, `cmake` says so and stops.** This example needs
> a reader that resolves a capture's centre frequency, which arrived after the
> release current when this page was written. You do not have to know that: the
> configure step probes for the capability and fails with both the reason and
> the fix. Nothing here names a version, so nothing here goes stale — the check
> starts passing by itself once a release carries the feature.
>
> The probe is by symbol rather than by version, because a too-old doppler
> **links perfectly well**: `wfm_reader_get_fc` has existed for ages and simply
> returns 0.0. Before the check, the example built clean and failed a test at
> `capture_get_fc (obj) == FC`, which tells a newcomer nothing.

### Building the example against it

From this directory:

```bash
cmake -B build . -DCMAKE_PREFIX_PATH=$HOME/.local/doppler
cmake --build build -j
```

Untar into `/usr/local` instead and even that flag goes away —
`find_package(doppler REQUIRED)` searches the default prefixes.

### From source, or against a doppler build tree

For doppler developers, and currently the only way to run this example's full
suite:

```bash
# an install, from a doppler checkout
cmake -B build -S . -DCMAKE_INSTALL_PREFIX=$HOME/.local/doppler
cmake --build build -j && cmake --install build

# or point straight at the build tree — doppler exports a build-tree config
cmake -B build . -Ddoppler_DIR=/path/to/doppler/build
```

`CMAKE_PREFIX_PATH` / `doppler_DIR` is all that changes between the modes — the
manifest, the generated CMake and the code are identical.

### If your project is not CMake

The tarball ships a pkg-config file, so nothing here is CMake-specific:

```bash
export PKG_CONFIG_PATH=$HOME/.local/doppler/lib/pkgconfig

# shared
cc myapp.c $(pkg-config --cflags --libs doppler) -o myapp

# static — name the archive to get a binary with no doppler runtime dependency
cc myapp.c -I$HOME/.local/doppler/include \
   $HOME/.local/doppler/lib/libdoppler.a -lm -o myapp
```

`-ldoppler` resolves to the **shared** library when both are present, so naming
`libdoppler.a` explicitly is how you link it statically. `-lm` is doppler's only
runtime dependency.

## Running it

```bash
ctest --test-dir build            # the C tests
PYTHONPATH=src python -m pytest src/iqtools/capture/tests/
./build/tools/iq_info capture.blue
```

______________________________________________________________________

## How the wiring works

**Linking doppler is one line, in the manifest** (`modules/capture.toml`):

```toml
[module.capture]
objects = ["capture"]
extra_link_libs = ["doppler::doppler-static", "m"]
```

`doppler::doppler-static` is an imported CMake target from the
`find_package(doppler REQUIRED)` that `[project] find_packages = ["doppler"]`
generates into the root `CMakeLists.txt`. It carries its own
`INTERFACE_INCLUDE_DIRECTORIES`, so there is no include path to keep in sync.
Swap it for `doppler::doppler` to link the shared library instead.

**The view is one table** (`objects/capture.toml`):

```toml
[[capture.views]]
class_name = "RawCapture"
create_fn = "capture_open_raw"
init_params = [
  { name = "path", type = "path", required = true },
  { name = "sample_type", type = "string_enum:cf32,cf64,ci32,ci16,ci8", default = "ci16" },
  ...
]
```

jm injects the `capture_open_raw` prototype into `capture_core.h` itself; you
write that one C function, and the second Python class appears.

**The C core owns no DSP.** `capture_core.c` is ~140 lines, all of it
forwarding into `wfm_reader_*`. The state struct holds doppler's opaque handle
plus the supplied metadata — `no_state = "true"` in the manifest is what hands
that struct to you rather than having jm infer one.

### Hand-owned files

| path                                                                                                    | owner                                      |
| ------------------------------------------------------------------------------------------------------- | ------------------------------------------ |
| `objects/*.toml`, `modules/*.toml`, `just-makeit.toml`                                                  | you — the source of truth                  |
| `native/inc/capture/capture_core.h`                                                                     | you (jm injects prototypes into it)        |
| `native/src/capture/capture_core.c`                                                                     | you — sacred, `jm apply` never rewrites it |
| `native/tests/`, `tools/`                                                                               | you                                        |
| `native/src/capture/capture_ext*.c`, `src/iqtools/**/*.pyi`, `__init__.py`, per-module `CMakeLists.txt` | **jm** — regenerated; do not edit          |

`add_subdirectory(tools)` sits outside every jm marker block in the root
`CMakeLists.txt` and survives `jm apply` untouched (verified: a re-apply on a
clean tree reports *"Project already matches"* and leaves the file
byte-identical). Content *inside* the `# ── External deps` markers does not —
that region is regenerated from `find_packages`, which is why doppler is
located declaratively rather than by hand-editing CMake.

______________________________________________________________________

## Three just-makeit gotchas this example hit

Worth knowing before you write your own manifest — each cost real time here:

- **A property `doc` must be a single physical line.** jm 0.33.15 escapes
    embedded quotes when it emits the doc into the C `PyGetSetDef`, but not
    newlines, so a genuinely multi-line `doc = """…"""` produces an
    unterminated string literal and the module will not compile. The
    `metadata_source` doc in `objects/capture.toml` is kept on one line for
    this reason. Filed as
    [jm#633](https://github.com/just-buildit/just-makeit/issues/633).
- **`_ext_<obj>.c` fragments are sacred**, so editing a `doc` (or anything else
    that only affects them) and re-running `jm apply` changes nothing. Delete
    the fragment and re-apply to pick the change up.
- **`[project] c_style = "clang-format"` is incompatible with
    `jm status --check`.** `jm apply` formats the regenerated `*_ext.c`
    aggregator, but `status` compares against an *unformatted* regeneration, so
    the formatted file it just wrote reads as permanently stale and the drift
    gate can never pass. This example therefore does **not** set `c_style`; the
    aggregator stays in jm's own style and is excluded from doppler's
    clang-format (`_ext\.c$`), which is the same posture doppler itself takes.
    Filed as
    [jm#635](https://github.com/just-buildit/just-makeit/issues/635).
    The per-object `_ext_<obj>.c` fragments *are* formatted — jm treats
    fragment bodies as sacred and does not diff them, so that causes no drift.

______________________________________________________________________

## How this is gated

The example is built and tested by doppler's own CI, so it cannot rot:

| gate                                  | what it covers                                          | where it runs                                                                                      |
| ------------------------------------- | ------------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `make test-example-downstream`        | configure + build + CTest, **`BUILD_PYTHON=OFF`**       | inside `make test-examples` — ubuntu, macOS, **and the glibc 2.28 container**, which has no Python |
| `make test-example-downstream-python` | builds the extension, runs this project's pytest        | inside `make test-examples-python` — the Python matrix job                                         |
| `make drift-check`                    | `jm status --check` **for this project's own manifest** | the `jm manifest drift gate` job                                                                   |

The C half is deliberately Python-free so the question that matters most —
*can a downstream project link `libdoppler.a`?* — is answered on three
platforms including an ancient glibc, not just where NumPy happens to exist.

The drift gate uses **doppler's pinned jm** (`uv run --project <repo>`), so
this example cannot silently document a jm version doppler is not on.

______________________________________________________________________

## Layout

```text
just-makeit.toml           project + the metadata_source enum
modules/capture.toml       the module -> links doppler::doppler-static
objects/capture.toml       the object + the RawCapture VIEW
native/inc/capture/        hand-written header (state typedef + jm's decls)
native/src/capture/        hand-written core; jm-generated bindings
native/tests/              C tests (fixtures written with doppler's writer)
tools/iq_info.c            C demo — links libdoppler.a, no Python
src/iqtools/capture/       generated package + .pyi; hand-written tests
```
