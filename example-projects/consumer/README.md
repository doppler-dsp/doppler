# consumer — the smallest project that links doppler

<!-- --8<-- [start:body] -->

One source file, one `find_package(doppler)`, two link modes. If you are
adding doppler to an existing build, start here: everything else in
`example-projects/` is this plus a subject.

It runs the local oscillator at a quarter of the sample rate, where the output
is known exactly (1, j, -1, -j, repeating), and exits 0 only if the samples
match. It uses the pure-C core alone, so it is the same program on Linux,
macOS and Windows. The oscillator is not the point — the link line is.

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

If doppler came from the `.deb` or `.rpm` packages (or any system install), it
is already on the compiler's paths and this is all of it. You need `cmake`,
`gcc`, `make` and `pkg-config`; on Debian or Ubuntu that is
`sudo apt install cmake gcc make pkg-config`.

```sh
make run
```

If doppler is in a prefix of your own (the release tarball, `jbx get-doppler`,
`~/.local`), say where:

```sh
make PREFIX=~/.local run
```

From a source tree, install it into a prefix first, then the same:

```sh
cmake --install /path/to/doppler/build --prefix ~/.local
make PREFIX=~/.local run
```

`make help` lists the knobs; `make clean` removes `build/`.

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

<!-- --8<-- [end:body] -->
