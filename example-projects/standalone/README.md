# standalone — one AWGN call, in C and in Python

The smallest useful doppler program, written twice. `main.c` draws 4096
complex Gaussian samples and prints their mean and standard deviation;
`example.py` does the same thing through the wheel. Between them they show
the two different ways to *have* doppler.

Output, from the C side:

```
samples : 4096
mean    : 0.0237 + -0.0007i  (expect ≈ 0)
std dev : 0.9847 (Re)  0.9858 (Im)  (expect ≈ 1.0)
```

## What it demonstrates

- **Building against a doppler BUILD TREE**, with no install step. This is
    the one example here that can do that, and it is what you reach for while
    hacking on doppler itself.
- **Static or shared**, selected at configure time, with the build-tree rpath
    baked so the shared binary runs without `LD_LIBRARY_PATH`.
- **The C and Python faces side by side.** They are two distributions of one
    library: the wheel carries the Python extension and no C headers, a source
    build or an install carries the headers and the libraries. The C path does
    not depend on the Python one working, or the reverse.

## Build and run

Against a doppler build tree — no install needed:

```sh
make DOPPLER_BUILD_DIR=../../build run
```

Add `LINK=shared` for the shared library instead of the static one. Against
an installed doppler:

```sh
make PREFIX=~/.local run
```

`make help` lists the knobs; `make clean` removes `build/`.

The Python sibling needs the wheel rather than the headers:

```sh
pip install doppler-dsp
make run-python
```

### Without CMake

The core is pure C, so `-lm` is the only extra:

```sh
cc main.c -o awgn_example -I ../../native/inc -I ../../build/native/inc \
   ../../build/libdoppler.a -lm
```

## Copying it

`CMakeLists.txt` is the interesting file: its `DOPPLER_BUILD_DIR` branch is
worth keeping if your project is developed alongside doppler, and the
`find_package(doppler)` fallback is what you ship.
