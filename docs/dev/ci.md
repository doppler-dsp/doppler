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

`ci-gates` reproduces CI locally, for when a CI red needs more than its job
log. It composes `gates` rather than listing gates again: `gates` is already
*every gate CI runs* — `gates-check` enforces that against `ci.yml` — so the
only thing the container adds is the environment. It is not a pre-push
ritual: before pushing, run what proves your change and let CI be the gate
([Release](release.md), step 1's tip).

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

The image is the org standard's (`HAS_CI_IMAGE` in the Makefile): canonical's
vendored `docker/ci.Dockerfile` builds it, `.github/workflows/ci-image.yml`
publishes it, `scripts/ci-image.py` decides when it owes a repin, and
`.github/ci-images.env` pins it. doppler supplies configuration — the
`CI_IMAGE_*` settings at the top of the Makefile — and one extension,
`docker/ci-extra.sh`. Why each pin exists is `scripts/ci-image.py`'s
docstring; change any of the vendored files in canonical, never here.

Every Linux job used to open with `make install-deps-ci`, which apt-installs
the dev group — about 112 MB per job, ten jobs a run. Most of it was already
on the runner under a different owner: cmake and cargo live in `/usr/local`,
outside dpkg, so apt did not know they were there and fetched the distro
copies anyway. That download was the entire exposure to mirror weather, and
on one bad day it stalled five runs of a single PR, one job trickling for
21 minutes against a 25-minute ceiling.

**The image has no package list of its own.** It copies `bootstrap.toml` and
installs its `CI_IMAGE_GROUPS` (`dev docs`) with a pinned just-bashit
release's `install-deps.sh` — the same file `make install-deps` reads. A
second list is exactly what `bootstrap.toml` exists to prevent, and it would
rot in the way hardest to notice: the image would keep working while no
longer being what a developer gets.

Two things are installed *outside* that list, by `docker/ci-extra.sh`, each
for a reason one cross-distro package list cannot express:

| what                  | why it cannot come from `bootstrap.toml`                                                                                                                                                                                                                                                                                  |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `libclang-rt-<n>-dev` | Whether clang bundles its profile runtime is a property of the distro *release*. 22.04 bundles it and has no such package at all — naming one fails apt outright — while 24.04 splits it out and clang does not depend on it. The script asks apt, tolerates a miss, and then *compiles the probe* as the real assertion. |
| `nats-server`         | `make nats-up` shells out to `docker run`, and there is no docker daemon inside a container job. Pinned by version **and** by the release's SHA-256.                                                                                                                                                                      |

That last one matters more than it looks. The `nats://` stream tests
**self-skip** when 127.0.0.1:4222 is unreachable, so without a broker the
suite would stay green while silently dropping the whole NATS path — and the
coverage number with it. `scripts/start-nats.sh` prefers the binary and falls
back to docker, so a dev box without docker *gains* those tests rather than
skipping them.

`ci-extra.sh --fingerprint` prints `rustup`, `cargo` and `nats-server`
version lines, which the image hashes together with every dpkg package. A
dpkg-only fingerprint once left an entire Rust toolchain invisible: anything
installed outside dpkg has to be in it, or the weekly comparison is blind to
exactly the parts installed by hand.

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
environments while testing one, so `CI_IMAGE_BASES` names both and each is
built natively on amd64 and arm64.

!!! note "The glibc 2.28 floor is a separate question"

    The floor is not what this image answers. `make glibc-gate` builds the
    tree in Debian 10 and `glibc-check` reads `libdoppler.so` plus every
    example binary, failing closed if it reads nothing; release wheels are
    built in `manylinux_2_28`. A modern base cannot answer a floor question,
    and the CI image does not try to.

### Pinning, and the weekly re-pin

Jobs pin the image **by digest**, so an image rebuild cannot change what an
in-flight PR was tested against.

**The digest is written in exactly one place: `.github/ci-images.env`.** No
workflow names one. A `pin` job reads that file and publishes the two refs as
job outputs, and every containerised job consumes
`${{ needs.pin.outputs.image_2404 }}`.

It was not always so, and the reason is worth keeping. The digest used to live
in the pin file *and* in six literal `container:` refs in `ci.yml`, while
nothing could move both: `ci-image.yml` writes the pin file, and the platform
refuses any `GITHUB_TOKEN` push touching `.github/workflows/**`. So the
repin branch was **born failing lint**
([#1215](https://github.com/doppler-dsp/doppler/issues/1215)). A whole job for
two `echo`s is the price of `container:` being resolved *before* any of its
job's steps run: nothing a step sets can reach it.

Every input to the image is pinned beside its digest — the apt snapshot every
source is rewritten to, each base by digest, and the just-bashit release — so
a rebuild reproduces the pinned package set
([#1748](https://github.com/doppler-dsp/doppler/issues/1748),
[#1751](https://github.com/doppler-dsp/doppler/issues/1751)). Only the Monday
`ci-image.yml` run (or a dispatch with `refresh`) picks new inputs, and it
repins only when a package fingerprint or the image's sources moved — a new
snapshot alone is not a change.

**A weekly repin lands on a branch, not a PR** (`CI_IMAGE_LANDING = branch`):
the `doppler-dsp` org forbids Actions from opening pull requests, so the run
pushes `ci/repin-image` and stops. On `main`, every `ci-image.yml` run then
**ends red** while that branch carries a pin `main` does not, until a human
lands it:

```sh
gh pr create --head ci/repin-image --fill
```

A push to any other branch that touches the image's sources builds from the
pinned inputs and commits the new pin onto that branch, so the PR carries its
own pin. To change what is in the image:

```sh
# edit bootstrap.toml or docker/ci-extra.sh, then:
make ci-image-build     # [BASE=ubuntu:22.04] build one base locally
make ci-image-check     # FAILS until the pin is updated -- that is the point
git push                # ci-image.yml builds, smokes and commits the pin
```

`ci-image-check` runs inside `make lint`. It is offline and instant: the pin
must be complete and well formed, and its `CI_IMAGE_SOURCE_HASH` must be this
tree's hash of `docker/ci.Dockerfile`, `docker/ci-extra.sh` and
`bootstrap.toml`. It leaves out `bootstrap.toml`'s `[project]` table, which
no layer reads, so a release's version bump owes no repin
([#1765](https://github.com/doppler-dsp/doppler/issues/1765)).

`ci-image-refs-check`, also in `make lint`, asks doppler's half: does every
image a workflow can run in trace back to the pin file?
`scripts/ci_image_refs_check.py` **resolves** `${{ … }}` rather than skipping
it — a `needs.<job>.outputs.<name>` ref is accepted only when that job is in
the consumer's `needs`, declares the output, and genuinely reads
`.github/ci-images.env`; a `matrix.<key>` ref is followed into the include
entries and each result re-checked. A literal is still legal and still must be
a pinned digest, because a tag is mutable by definition. Finding **zero**
references is a failure too: a scan that matches nothing has not run.

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
| `ci-image-check`    | The tree describing an image CI is not running (canonical's); `ci-image-refs-check` adds a `container:` that does not trace to the pin, such as a mutable tag.                                                                                                                          |
| `deps-budget-check` | `DEPS_DEADLINE × DEPS_TRIES + backoff` exceeding the smallest step ceiling in any workflow, so a retry cannot be killed mid-download.                                                                                                                                                   |
| `lint-ci-pipefail`  | A workflow step whose shell pipeline discards an exit code. The default Actions shell is `bash -e`, where a pipeline reports the *last* command's status — `make coverage \| tee` was green over a recipe that had failed, and the missing report only surfaced a step later.           |

______________________________________________________________________

## Pull requests

`ci.yml` runs on `pull_request` and on `push` to `main` / `develop`, and it
does the same work on both: **every job, and `python` on every version
3.9–3.14**. The PR run is the one that gates the merge (`CI passed`, the
one required check), so it is the full matrix.

There is **no merge queue**. It was retired on 2026-10-01. While it ran, a
PR got only the fast gates and the queue ran the heavy jobs on main plus
the PR. That split meant a PR's own checks could be green while the run
that decided the merge had not started. It also meant a pending CI-image
repin ejected every queued PR. Instead, `protect-main` requires a PR to be
up to date with `main` before it merges, so the tree a PR's run tested is
the tree that lands. A behind PR rebases and runs again. See
[How We Work](workflow.md#4-merge).

**The primary leg.** Some steps of the `python` job are worth running once,
not on every interpreter. They run on the *primary* leg, which is named by
role, never by version: `make print-python-primary` prints it (through
`scripts/python_versions.py --primary`), and the rule is that **the primary
is the floor**. These steps used to select
`'3.12'`, the first leg of the original matrix. When 3.9 was added below it
the literal stayed, and while a PR ran the floor alone, no PR ran them
(#1714).

The `pythons` job emits a `primary` output, and a step's `if:` compares
`matrix.python-version` against it one of two ways. Which one is the step's
*lane*:

- `== primary`: **primary**, the primary leg alone. For the
    version-independent gates and the coverage-producing run.
- `!= primary`: **rest**, every other leg.

<!-- python-legs:start -->

| step                                 | target                    | lane    | cost (run 36820085885, 3.12) |
| ------------------------------------ | ------------------------- | ------- | ---------------------------- |
| Doc fence gates (python + C + shell) | `make test-snippets`      | primary | 74 s                         |
| Test                                 | `make test-python`        | rest    | 264 s (3.9 leg)              |
| Test with coverage                   | `make test-python`        | primary | 288 s                        |
| Validation reports are not stale     | `make validate-check`     | primary | 233 s                        |
| Upload coverage report               | `actions/upload-artifact` | primary | 1 s                          |

<!-- python-legs:end -->

`make ci-aggregator-check` holds this table to `ci.yml`: same steps, same
targets, same lanes. It refuses any other leg selector, a version literal
above all, and a `pythons` job that does not derive `primary` from the
classifiers. To move a step between lanes, flip its comparison and this
table's row.

The `pythons` job also emits the Python matrix itself (its `pythons`
output: every version, or the primary alone when `code=false`). It is
doppler's own, so it sits beside `changes` rather than in it. The matrix is
read from `pyproject.toml`'s
`Programming Language :: Python :: 3.N` classifiers by
`scripts/python_versions.py`, so `ci.yml` names no version.
`make python-versions-check` holds the lowest classifier equal to the
`requires-python` floor and every classifier inside its range.

**`changes` is canonical's.** It is the vendored
`.github/workflows/changes.yml` (`standard-check` holds it byte-identical),
the same job every just-buildit repo calls, so the rules below are the org's.
It writes `src`, `docs` and `code` on every path; `src=false` implies
`code=false`. `make ci-changes-wiring-check` refuses a `ci.yml` job that
ignores it, except the three this repo runs on every tree by policy, declared
in the Makefile's `CI_ALWAYS_RUN_JOBS`: `pin` (a repin PR must resolve its
image), `pre-commit` and `manifest-drift` (lint and drift check exactly what
a version bump changes).

**`src=false`: nothing here is untested.** `changes` answers it for two
reasons. A diff that is a version bump alone (`make ci-changes`). Or, on a
push to `main`, a tree that already passed as a PR whose head contained the
previous tip (`make ci-tree-tested`). `protect-main` requires a PR to be up
to date, so that is every merge: `main`'s push run checks the cheap gates
and skips the matrix the PR just ran.

**`code=false`: only docs changed.** `make ci-docs` reads `CI_DOCS_RE` in
the Makefile, the one declaration of what the docs are (`docs/`,
`mkdocs.yml`, the changelog, `README.md`, `CONTRIBUTING.md`). When every
changed path matches, and nothing outside `docs/` or `changelog.d/` was
deleted, it answers `code=false`. Then every job docs cannot break skips:
the C builds on every platform, Doxygen (it reads `native/` only),
sanitizers, coverage, glibc, packages, Docker and the sweep. Each is in
`CI passed`'s `CODE_ONLY` list. What docs *can* break still runs: lint,
the site build, and the `python` job, on the primary leg alone. Docs can
fail the suite (the doc fences, and the gate tests that read the live tree),
but not on one interpreter more than another. `make ci-aggregator-check`
holds `CODE_ONLY` to exactly the jobs gated on `code`, as it holds
`SKIPPABLE` to the jobs gated on `src`.

**Nightly, `main` gets the full run anyway.** A `schedule` run at 03:43 UTC
never takes a skip. Its job is the environment, not the tree: hosted runner
images and unpinned tools move under an unchanged tree, and the nightly is
where that goes red, rather than on whichever PR meets it first.
Every heavy job is gated on `if: needs.changes.outputs.src == 'true'`, and
`scripts/ci_passed.py` treats a skip as green only for a job in the
aggregator's `SKIPPABLE` list and only on `src=false`.
`make ci-aggregator-check` holds `SKIPPABLE` to exactly the gated jobs. It
also refuses any surviving trace of the queue's split: a `full`, `heavy` or
`primary_full` output, a job gated on one, or a `HEAVY` list. Each of those
would be a skip permission that nothing takes.

**Per-PR bases.** `CHANGELOG_BASE` (for `changelog-check` and
`issue-link-check`) and `coverage`'s `COV_BASE` both read
`pull_request.base.sha`, falling back to `origin/main` on a push.

**Merge method: rebase.** `issue-link-check` reads the `Closes #N` /
`No-issue:` declaration from commit messages. A squash commit carries
whatever message the squash template produces, which can drop that
declaration.

`windows.yml` is advisory.

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

**One check name is load-bearing: `CI passed`.** It is the only status check
the `protect-main` ruleset requires, and it is green only when every job in
its `needs` succeeded. So the other jobs can be renamed freely, and a job
left out of that list **gates nothing**, however red it goes.
`make ci-aggregator-check` fails when a job in `ci.yml` is missing from
`ci-passed`'s `needs`.
