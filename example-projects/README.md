# example-projects — doppler seen from the outside

Three self-contained projects that consume doppler the way you would: their
own `CMakeLists.txt`, their own `Makefile`, `find_package(doppler)` against an
install rather than a path into this source tree.

That is what separates them from [`native/examples/`](../native/examples),
which builds against this repo's headers and links its targets directly.
Those demonstrate the API; these demonstrate *depending on it*.

| project                             | what it is for                                                                                                          |
| ----------------------------------- | ----------------------------------------------------------------------------------------------------------------------- |
| [`consumer/`](consumer)             | The smallest project that links doppler. Start here if you are adding doppler to an existing build.                     |
| [`standalone/`](standalone)         | One AWGN call, in C and in Python. The only one that can build against a doppler **build tree**, with no install step.  |
| [`burst-pipeline/`](burst-pipeline) | A burst waveform end to end: describe a frame, generate the train, sweep it through a `Plan`, write BLUE, read it back. |

Each has a `README.md` and takes the same three commands:

```sh
make build      # configure and compile
make run        # build, then run it
make clean
```

`make help` in any of them lists the knobs. `PREFIX=<dir>` points at an
install; `standalone` additionally takes `DOPPLER_BUILD_DIR=<dir>`.

## They are gated, not merely present

Each is built and run in CI, so a directory users are told to copy keeps
building the way they will build it:

| project           | gate                        | how                                                            |
| ----------------- | --------------------------- | -------------------------------------------------------------- |
| `consumer/`       | `make link-check`           | bare `cc` against `libdoppler.a` with `-lm -lpthread`          |
| `standalone/`     | `make test-examples-c`      | `make -C example-projects/standalone run`                      |
| `burst-pipeline/` | `make burst-pipeline-check` | `make -C example-projects/burst-pipeline run`, both link modes |

The last two run the same `make` command this file tells you to use, so the
documented path is the tested one — a gate that configured CMake its own way
would prove something adjacent to what you are about to type.

`consumer/` is deliberately the exception. `link-check` spells the compiler
line out by hand because its whole question is *what does a downstream typing
the documented command get* — routing it through a Makefile would make it
agree with itself rather than with the docs. `tests/install/release-smoke.sh`
additionally builds this same program all three ways (CMake, `cc`,
`pkg-config`) against a published release tarball.
