# iqtools — the doppler downstream showcase

You're in `/iqtools`, a **complete project built on doppler** — already
compiled and with its full C + Python suite green (the image's build ran
`make test`). doppler is installed to `/usr/local`; this project found it with
a plain `find_package(doppler)`.

`iqtools` is a real IQ-capture reader — a Python package *and* a C library,
fully typed, documented and tested — in roughly **140 lines of C glue plus a
few manifest tables**. There is no hand-written CPython anywhere: `just-makeit`
generates every binding, `.pyi`, per-module `CMakeLists.txt` and `__init__.py`.

## It already works

```sh
python3 -c "from iqtools.capture import Capture, RawCapture; print(Capture, RawCapture)"
python3 -c "from iqtools.capture import Capture; help(Capture.summary)"   # docs derived from C
```

## The loop that makes it tick

```sh
just-makeit apply    # regenerate all glue from the manifest tables
make test            # rebuild + ctest + pytest — same green, zero drift
```

## Where to look

| File                                                               | What it is                                                                                       |
| ------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------ |
| `just-makeit.toml`, `objects/capture.toml`, `modules/capture.toml` | The manifest — the object, its `view`, its result record, and the *one line* that links doppler. |
| `native/inc/`, `native/src/`                                       | The hand-written C core (yours).                                                                 |
| `native/inc/capture/capture_summary.h`                             | The `///<` field docs that become `CaptureSummary`'s Python docstrings.                          |
| `src/iqtools/**.pyi`                                               | Generated stubs — every binding and docstring, not written by hand.                              |

See `README.md` for the full walkthrough and diagrams.
