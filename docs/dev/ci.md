# Continuous Integration

What CI is made of, and how to run it yourself.

The shape is one sentence: **CI runs `make` targets inside a pinned
container, and every gate it runs is reachable from `make gates`.** Both
halves are enforced rather than described — `gates-check` fails when CI
invokes a target `gates` cannot reach, and `ci-image-check` fails when the
tree stops describing the image CI runs in.

______________________________________________________________________

## Run it the way CI runs it

The toolchain lives in an image, so "works on my machine" and "works in CI"
can be the same sentence. Three targets:

```sh
make ci-shell                          # a shell in the pinned image
make ci-run TARGET='build test-rust'   # any goals, in CI's environment
make ci-gates                          # the whole gate set, in CI's environment
```

`ci-gates` is the pre-push check. It composes `gates` rather than listing
gates again: `gates` is already *every gate CI runs* — `gates-check` enforces
that against `ci.yml` — so the only thing the container adds is the
environment.

It is deliberately **not** a git pre-push hook. `gates` includes `coverage` at
roughly ten minutes, and a hook that slow is one people pass `--no-verify` to.
A gate that is routinely bypassed is decoration.

!!! warning "Container builds and host builds do not mix"

    `ci-run` builds into `build-ci/`, not `build/`, and sets both `BUILD_DIR`
    and `DOPPLER_BUILD_DIR`. The container and the host target different
    glibc versions, so handing one's build tree to the other fails at link
    time with something like `undefined reference: atan2f@GLIBC_2.43` — which
    reads as a code bug and is not one. `ffi/rust/build.rs` locates the
    library itself and defaults to the host tree, which is why the second
    variable is needed as well as the first. Same separation, same reason, as
    `glibc-gate`'s `build-glibc228/`.

______________________________________________________________________

## The toolchain image

`deploy/docker/Dockerfile.ci` builds it; `.github/ci-images.env` pins it.

Every Linux job used to open with `make install-deps-ci`, which apt-installs
the dev group — about 112 MB per job, ten jobs a run. Most of it was already
on the runner under a different owner: cmake and cargo live in `/usr/local`,
outside dpkg, so apt did not know they were there and fetched the distro
copies anyway. That download was the entire exposure to mirror weather, and
on one bad day it stalled five runs of a single PR, one job trickling for
21 minutes against a 25-minute ceiling.

**The image has no package list of its own.** It copies `bootstrap.toml` and
runs the same two `jbx install-deps` commands `make install-deps` and
`make install-docs-deps` run. A second list is exactly what `bootstrap.toml`
exists to prevent, and it would rot in the way hardest to notice: the image
would keep working while no longer being what a developer gets.

Two things are installed *outside* that list, each for a reason one
cross-distro package list cannot express:

| what                  | why it cannot come from `bootstrap.toml`                                                                                                                                                                                                                                                                                 |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `libclang-rt-<n>-dev` | Whether clang bundles its profile runtime is a property of the distro *release*. 22.04 bundles it and has no such package at all — naming one fails apt outright — while 24.04 splits it out and clang does not depend on it. The image asks apt, tolerates a miss, and then *compiles the probe* as the real assertion. |
| `nats-server`         | `make nats-up` shells out to `docker run`, and there is no docker daemon inside a container job.                                                                                                                                                                                                                         |

That last one matters more than it looks. The `nats://` stream tests
**self-skip** when 127.0.0.1:4222 is unreachable, so without a broker the
suite would stay green while silently dropping the whole NATS path — and the
coverage number with it. `scripts/start-nats.sh` prefers the binary and falls
back to docker, so a dev box without docker *gains* those tests rather than
skipping them.

### What is deliberately *not* in it

A rustup toolchain was, briefly — 613 MB, the single largest thing in the
image — carried only because `ffi/rust/Cargo.lock` had drifted to format v4,
which cargo refuses below 1.78 while apt ships 1.75 on both LTSes. That was a
workaround for a defect, not a requirement: the crate is edition 2021 with two
dependencies, and 1.75 compiles the whole tree in under four seconds.

The lockfile is back at v3, `Cargo.toml` declares `rust-version = "1.75"`, and
`make cargo-floor-check` fails if either leaves the floor — because cargo
rewrites the lockfile to v4 the first time a modern one resolves anything, and
a lockfile is not a file anyone reads. Removing the workaround took the image
from 3.17 GB to 2.31 GB.

That is the shape to copy when this image grows: ask whether the thing being
added is a *requirement* or a *workaround*, because a workaround baked into
the environment is one nobody sees again.

### Two bases, on purpose

`build-and-test` runs ubuntu-22.04 and ubuntu-24.04 because they are two
different glibcs. One image for both legs would leave that matrix naming two
environments while testing one, so `BASE` is a build argument and both are
published.

!!! note "The glibc 2.28 floor is a separate question"

    The floor is not what this image answers. `make glibc-gate` builds the
    tree in Debian 10 and `glibc-check` reads `libdoppler.so` plus every
    example binary, failing closed if it reads nothing; release wheels are
    built in `manylinux_2_28`. A modern base cannot answer a floor question,
    and the CI image does not try to.

### Pinning, and the nightly refresh

Jobs pin the image **by digest**, so an image rebuild cannot change what an
in-flight PR was tested against.

**The digest is written in exactly one place: `.github/ci-images.env`.** No
workflow names one. A `pin` job reads that file and publishes the two refs as
job outputs, and every containerised job consumes
`${{ needs.pin.outputs.image_2404 }}`.

It was not always so, and the reason is worth keeping. The digest used to live
in the pin file *and* in six literal `container:` refs in `ci.yml`, which
`ci-image-check` required to agree — while nothing could move both.
`ci-image.yml` writes the pin file, and the platform refuses any
`GITHUB_TOKEN` push touching `.github/workflows/**`. So the nightly's repin
branch was **born failing lint**, and the repin path had never once completed
end to end ([#1215](https://github.com/doppler-dsp/doppler/issues/1215)).

A whole job for two `echo`s is the price of `container:` being resolved
*before* any of its job's steps run: nothing a step sets can reach it, so the
value must arrive from a job that already finished. The jobs still fan out in
parallel behind it, so it costs one hop, not one per job.

`ci-image.yml` rebuilds nightly, compares the *package fingerprint* baked into
the image, and publishes and pushes the refreshed pin to `ci/repin-image` only
when the content actually moved. Rebuilding nightly is not the same as
consuming a nightly image: a run stays reproducible, while drift still
surfaces within a day.

**The nightly pushes a branch; it does not open a PR.** It used to try, and
could never succeed — the `doppler-dsp` org forbids GitHub Actions from
creating pull requests, so the step force-pushed the branch and then died on
`gh pr create`. That left the workflow red on every `main` run for three
releases; because it feeds no aggregator, its red gated nothing and was
indistinguishable from the image genuinely breaking
([#1212](https://github.com/doppler-dsp/doppler/issues/1212)).

So the branch is the deliverable, and a **gate** reads it rather than a human
noticing a PR. `make ci-image-repin-check` — its own required job in `ci.yml`
— fails while `ci/repin-image` carries a pin the tree does not:

```sh
gh pr create --head ci/repin-image --fill   # land it; the gate goes green
```

It compares the two `CI_IMAGE_FINGERPRINT_*` values and
`CI_IMAGE_SOURCE_HASH`, and deliberately **not** the digests: a rebuild
changes the digest every time while the content is identical, so a file diff
would be red every morning and everyone would learn to ignore the one morning
that mattered. Comparing values also makes it self-clearing — once the repin
lands, the tree's fingerprints equal the branch's and the gate goes green on
its own.

This is the half `ci-image-check` structurally cannot cover. That one is
offline, so it compares *our* inputs; only the nightly learns that *upstream*
moved under an unchanged Dockerfile. Blocking is the point: an unmerged repin
means every Linux job is running in an image the repo no longer describes.

The fingerprint covers every dpkg package plus `rustc`, `cargo` and
`nats-server` — the tools that arrive outside dpkg. It did not, at first, and
adding an entire Rust toolchain left it byte-identical; a fingerprint blind to
the parts the Dockerfile installs by hand is blind to exactly what it is
supposed to watch.

To change what is in the image:

```sh
# edit bootstrap.toml (or Dockerfile.ci), then:
make ci-image           # build both bases locally
make ci-image-check     # will FAIL until the pin is updated -- that is the point
git push                # ci-image.yml rebuilds and prints the pin block
# commit the printed block into .github/ci-images.env
```

`ci-image-check` runs inside `make lint`. It is offline and instant, and asks
two questions: does the tree's input hash still match what the pin recorded,
and does every image a workflow can run in trace back to the pin file?

The second half is `scripts/ci_image_refs_check.py`. It **resolves**
`${{ … }}` rather than skipping it — a `needs.<job>.outputs.<name>` ref is
accepted only when that job is in the consumer's `needs`, declares the output,
and genuinely reads `.github/ci-images.env`; a `matrix.<key>` ref is followed
into the include entries and each result re-checked. A literal is still legal
and still must be a pinned digest, because a tag is mutable by definition and
the image a PR passed on would not have to be the one it merges with. Anything
else fails: the gate refuses to guess at an interpolation.

The scan it replaced *skipped* expressions, on the reasoning that a matrix
`image:` line elsewhere carried the literal they resolved to. Removing the
literals voided exactly that reasoning — it would have walked six expressions,
checked none, and printed OK. Finding **zero** references is therefore a
failure too: a scan that matches nothing has not passed, it has not run.

**"Inputs" is narrower than "those two files", and the difference matters.**
The hash was `cat bootstrap.toml Dockerfile.ci | sha256sum` — the *files*, not
the *image*. `bootstrap.toml` also carries doppler's `[project] version`, which
no layer reads, so keeping that version in step with a release would have
demanded an image rebuild on every release; a reformatted comment did the same.
`scripts/ci_image_source_hash.py` now parses `bootstrap.toml`, drops
`[project]`, and hashes the rest by value alongside the Dockerfile — so the
`[dev.*]`/`[docs.*]` package lists and `[tools.install-deps] groups`, which
decide what `jbx install-deps` installs, still fire a rebuild, while identity
and formatting no longer do.

Excluding one reasoned table rather than listing the ones to include is the
whole design: an include-list stops covering a table added later, which is a
gate that keeps passing because it stopped looking. Forgetting, here, costs an
extra rebuild rather than a missed one. `ci-image.yml` calls
`make -s ci-image-source-hash` rather than spelling the hash a second time, so
the gate and the pin cannot compute different numbers.

______________________________________________________________________

## The compiler cache

The C core is built several times per run with the same compiler and the same
flags — once per Python job (only the extension differs per ABI) plus the
ubuntu-24.04 leg. `ccache` is in every dev group and reaches every configure
step, including the coverage tree's.

Measured in the image: a cold build is 434 misses, and a rebuild after
deleting the tree is 434 hits — the whole second build served from cache. In
CI the `Build` step went from 79 s to 12 s once a cache existed to restore.

What is *not* duplicated stays that way, by construction rather than by our
being careful: ccache hashes the compiler binary and the full flag set, so the
22.04 leg's gcc, coverage's instrumented clang and the Debian 10 floor
toolchain land in separate entries.

`make ccache-stats` runs after each build so the hit rate is in the log. A
cache that quietly stops hitting has no other symptom than builds slowly
getting longer.

______________________________________________________________________

## Gates that watch CI itself

These exist because each one failed to hold at least once:

| gate                | what it refuses                                                                                                                                                                                                                                                                         |
| ------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `gates-check`       | CI running a `make` target that `make gates` cannot reach. It reads one file, so moving steps out of `ci.yml` into a composite action shrinks what it checks *while still reporting OK* — it dropped from 29 targets to 21 that way once, which is why the build/test steps are inline. |
| `ci-image-check`    | The tree describing an image CI is not running, or a `container:` pinned to a mutable tag.                                                                                                                                                                                              |
| `deps-budget-check` | `DEPS_DEADLINE × DEPS_TRIES + backoff` exceeding the smallest step ceiling in any workflow, so a retry cannot be killed mid-download.                                                                                                                                                   |
| `lint-ci-pipefail`  | A workflow step whose shell pipeline discards an exit code. The default Actions shell is `bash -e`, where a pipeline reports the *last* command's status — `make coverage \| tee` was green over a recipe that had failed, and the missing report only surfaced a step later.           |

______________________________________________________________________

## What runs where

| job                                                              | environment                           | notes                                                                                             |
| ---------------------------------------------------------------- | ------------------------------------- | ------------------------------------------------------------------------------------------------- |
| `build-and-test-linux`                                           | pinned image, one per glibc           | split from macOS because `container:` is Linux-only and cannot be switched off for one matrix leg |
| `build-and-test-macos`                                           | hosted runner, brew                   | no macOS container to bake                                                                        |
| `python` (3.9–3.14)                                              | pinned image                          | uv supplies the interpreters; only the extension build differs per ABI                            |
| `coverage`                                                       | pinned image                          | clang source-based, C ∪ Python ∪ Rust — see [Coverage](coverage.md)                               |
| `glibc-228`                                                      | Debian 10 image via `make glibc-gate` | the floor gate; its own toolchain by necessity                                                    |
| `doxygen`, `docs`, `pre-commit`, `manifest-drift`, `specan-demo` | pinned image or plain runner          | no system deps beyond the image                                                                   |
| `docker`                                                         | hosted runner                         | builds the shipped images, so it needs a daemon                                                   |

The check names are load-bearing: branch protection requires them by string,
so a refactor that renames one silently stops requiring it.
