# consumer — the smallest project that links doppler

One source file, one `find_package(doppler)`, two link modes. If you are
adding doppler to an existing build, start here: everything else in
`example-projects/` is this plus a subject.

It calls `doppler_wfmgen()` to generate a short waveform to `consumer.cf32`
and prints the return code and the path. The waveform is not the point — the
link line is.

## What it demonstrates

- **`find_package(doppler)` against an install prefix**, which is how a
    downstream consumes a release rather than a source tree.
- **Both link targets.** `doppler::doppler` is the shared library;
    `doppler::doppler-static` is the self-contained static one. Both binaries
    print the same thing, which is the point of building the pair — a
    difference between them is a packaging bug, not a program one.
- **Three build faces, all equivalent.** CMake is below; `cc` and
    `pkg-config` are in the next section, and CI asserts all three produce
    identical output (`make consumer-faces-check`).

## Build and run

You need doppler installed somewhere CMake can find it. From a source tree:

```sh
cmake --install /path/to/doppler/build --prefix ~/.local
```

Then:

```sh
make PREFIX=~/.local run
```

Drop `PREFIX=` if doppler is installed system-wide. `make help` lists the
knobs; `make clean` removes `build/`.

The Makefile is a thin wrapper — the two commands it runs are:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=~/.local
cmake --build build
```

### Without CMake

The same program, compiled directly:

```sh
cc main.c -o consumer -I ~/.local/include -L ~/.local/lib -ldoppler -lm
```

or letting pkg-config answer:

```sh
cc main.c -o consumer $(PKG_CONFIG_PATH=~/.local/lib/pkgconfig pkg-config --cflags --libs doppler)
```

## Copying it

Take `CMakeLists.txt` and the `find_package` block. `main.c` is a placeholder
for your program.
