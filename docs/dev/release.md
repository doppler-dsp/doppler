# Release Checklist

Step-by-step process for cutting a doppler release.
Run these steps in order — each one is a gate for the next.

______________________________________________________________________

## 1. Confirm `main` is green and the tree is clean

`main`'s required CI already ran the full suite (`test-all`'s equivalent)
plus the docs build on every commit that landed there — step 5's PR merge
is the real correctness gate (see the callout below), and step 7's
`verify-ci` explicitly does **not** re-test, only confirms. Re-running
`make test-all` / `make docs` locally here just repeats work CI
already did on the exact same tree; skip it unless you have a specific
reason to distrust CI (a suspected environment-specific flake, or extra
confidence before an unusually disruptive release).

```sh
git status                    # nothing uncommitted
git checkout main && git pull  # sync to the latest green main
gh pr checks <last-merged-pr>  # spot-check CI was actually green, if in doubt
just-makeit bench --python-only --tag vX.Y.Z   # local fallback; CI commits automatically on tag push
```

!!! tip "`make gates` is the pre-push command — use it on the *work*, not here"

    Every gate CI requires, fail-fast, in one command. `make gates` runs the
    full set — `lint`, the changelog / drift / doxygen / docs checks,
    `test-all`, the stub / api-docs / snippet doc-test gates, `test-rust`, the
    ABI / link / consumer-faces / glibc / specan portability checks, `coverage`
    and its gate, and `docker-examples`. Run it before pushing a feature
    branch, not at release time — by release time it is too late for the cheap
    ones to help.

    You do not curate that list by hand. `make gates-check` (part of `make lint`, from the vendored `standard.mk`) scans `ci.yml` and fails if `gates`
    omits any target CI actually runs — so the pre-push command is
    correct-by-construction and cannot silently drift behind CI.

    The failure mode it exists for is not a missing check; every one of those
    targets already existed. It is running all but one of them and not noticing
    which one you skipped. A long branch once reached release day with **88
    doxygen warnings** and an **empty `[Unreleased]`**, both introduced ~25
    commits earlier, because the doxygen gate lived inline in `ci.yml` with no
    local equivalent and nothing watched the changelog at all.

______________________________________________________________________

## 2. Check examples

If any gallery example scripts in `src/doppler/examples/` changed since the
last release, regenerate the plots:

```sh
make gallery
git add docs/assets/
git commit -m "docs: update gallery plots for vX.Y.Z"
```

If you added a new plot-generating script, add it to `GALLERY_SCRIPTS`
in the Makefile before running `make gallery`.

______________________________________________________________________

## 2a. When freshness asks for work that is not there

`make release-freshness-check` asks whether a gallery script or a
perf-relevant path moved since the last tag. It cannot see *what* moved
inside one, so a change confined to a platform this release does not
measure — Windows, for doppler's Linux-measured benchmarks and art — still
reads as stale.

Answer it in writing rather than with a re-render that changes nothing:
add `release-waivers/v<ver>.md` (repo root) with one line per item,

```text
- gallery: path/to/script.py -- why this needs no re-render
- benchmarks: native/inc/header.h -- why the numbers still describe the tree
```

Each line waives exactly that item, must carry a reason, and is **printed**
by the gate while it runs, so a skipped check lands in the release log. A
line that matches nothing stale fails the gate, so a waiver cannot outlive
its reason. `v0.55.0.md` is the worked example: four Windows-only items.

______________________________________________________________________

## 2b. Refresh the benchmarks page

The published [Benchmarks](../benchmarks.md) page is rendered from committed
snapshots under `benchmarks/published/v<ver>/` — **representative numbers
measured by hand on a real machine**, deliberately *not* from CI (shared
runners aren't hardware-representative). Each release is measured in **two
builds** so the page shows the from-source upside: **portable** (the wheel) and
**native** (`-DDOPPLER_NATIVE=ON`).

On the **same representative machine** each release (so the history stays
comparable), measure both builds **interleaved** and publish. First put the CPU
in a peak, repeatable state — the published `doppler_meta` records the governor
either way, but `powersave` understates the numbers:

```sh
sudo cpupower frequency-set -g performance   # peak, repeatable; quiesce other load

make bench-interleaved VERSION=X.Y.Z   # builds portable + native, runs them
                                       # alternately, keeps the per-bench best
make bench-docs                        # render docs/benchmarks.md (two columns)
git add benchmarks/published docs/benchmarks.md
git commit -m "docs: publish benchmarks for vX.Y.Z (<cpu>)"
```

`bench-interleaved` builds both flavours in throwaway git worktrees, runs the
suite alternately K times (default 5; `K=N` to override), and keeps each
benchmark's lowest-mean run — so the *from src* column reflects the real build
difference, not cross-run system drift. The measurement (not the build) is
pinned to the CPU's **fastest core class**, read from `cpuinfo_max_freq`: on a
heterogeneous part an unpinned benchmark is bimodal — the same binary measured
3.5 µs on a Zen 5 core and 5.6 µs on a Zen 5c one — and min-of-K only *usually*
finds the fast mode. The snapshot records the pinned CPUs and the page prints
them. Each snapshot is stamped with the
compiler + flags it read from `compile_commands.json`, so the page is
self-describing. Skip only if no perf-relevant code changed since the last
release.

______________________________________________________________________

!!! danger "`main` is protected — everything goes through a PR"

    All changes to `main`, **including the release bump**, land via a pull
    request whose required `CI passed` check must be green. Never push
    to `main` directly, and never tag a commit that is not already on a green
    `main`. The release workflow (step 7) runs independently of CI and is *not*
    gated on it — so **the PR merge in step 5 is the real gate**. The tag only
    ever points at a commit those checks already passed.

## 3. Cut the release branch + bump the version

**One command does steps 3, 4 and 5's mechanics:**

```sh
make release-pr VERSION=X.Y.Z
```

It first asks the tag-time questions that only the tree can answer: whether
benchmarks and the gallery are fresh for this version, and whether the notes
fit (sizes and entry lengths read from the fragments, BEFORE assembly empties
them). Then it branches off `origin/main`, bumps the five version sites,
promotes the changelog fragments and cuts the section, regenerates the
comparison links, checks the versions and that nothing is left unassembled,
sizes the body `release-notes.sh` will actually publish, commits, pushes and
opens the PR — so a merged, green release commit passes `make ship`'s own
checks without a surprise at the tag.
Read the sections below for what each part is doing and why — then review the
changelog prose it assembled, which is the one part no command can judge.

To drive it by hand instead (or to stop after the bump):

```sh
make release-branch VERSION=X.Y.Z   # branches chore/release-X.Y.Z, then bumps
```

`bump-version` updates **five files** atomically, from one declaration in
`scripts/version_sites.py` — there is no second list, and `version-check`
reads the same table:

| File                  | Field                        |
| --------------------- | ---------------------------- |
| `pyproject.toml`      | `version`                    |
| `ffi/rust/Cargo.toml` | `version`                    |
| `CMakeLists.txt`      | `project(doppler VERSION …)` |
| `bootstrap.toml`      | `[project] version`          |
| `just-makeit.toml`    | `[project] version`          |

(`uv.lock` is re-synced by the `uv lock` in the same command, and `CHANGELOG.md`
is prose you write in step 4 — neither is a probe.)

The last two joined on 2026-08-24, having never been bumped at all:
`bootstrap.toml` was frozen at `0.3.7` and `just-makeit.toml` at `0.1.0`, the
value it was given in the **initial commit**. Nothing read either, which is
why nothing noticed — `doppler_version()` returns `DOPPLER_VERSION`, stamped
by CMake from `PROJECT_VERSION`. `make version-check` now probes all five, so
a missed one is a red gate rather than a number nobody reads.

!!! warning "Pre-releases are refused"

    `make bump-version VERSION=1.2.3rc1` errors. CMake and Cargo reject the
    suffix, and `standard.mk`'s `version-check` requires every site to return
    the same string — so a pre-release bump used to produce a tree that could
    not pass `release.yml`'s own `verify-version` step. Supporting them needs a
    change in the vendored `standard.mk`
    ([just-buildit#30](https://github.com/just-buildit/just-buildit.github.io/issues/30));
    doppler has cut zero pre-releases.

!!! note "Why `bootstrap.toml` could not be bumped before"

    Its bytes fed the CI image fingerprint, so moving the version demanded an
    image rebuild and a repin — every release, for a string no image layer
    reads. `make ci-image-source-hash` now hashes the tables that decide the
    image (everything but `[project]`) instead of the whole file, so the
    version travels freely and a rebuild still fires on any real change. See
    [Build Internals](build-internals.md).

    `just-makeit.toml`'s copy has a second reason to be right:
    [just-makeit#1141](https://github.com/just-buildit/just-makeit/issues/1141)
    is that `[project] version` reaches none of its four generated copies. When
    jm closes that, this value starts propagating outward — and `0.1.0` would
    have propagated into the three files that were correct.

## 4. Assemble the changelog

Entries are written as **fragments** under `changelog.d/` — one file per
change, which is what stopped `CHANGELOG.md` conflicting on every rebase — so
`## [Unreleased]` stays near-empty between releases and the release is where
they get promoted. This step did
not exist in this runbook until 2026-08-28, and its absence was not
theoretical: cutting from the old step 4 alone would have published the 5
entries already sitting under `[Unreleased]` and left 62 fragments on the
floor.

On the release branch:

```sh
make changelog-assemble VERSION=X.Y.Z   # promote, then cut the section
make docs-relink                        # comparison links from the headings
```

`changelog-assemble` promotes `changelog.d/` into `[Unreleased]` and **STAGES
the promotion** (the fragments are deleted but still tracked, so an unstaged
assemble makes the next `make lint` fail on paths that are gone). With
`VERSION=` it then renames `## [Unreleased]` → `## [X.Y.Z] — YYYY-MM-DD` and
opens a fresh empty `## [Unreleased]` above it.

That rename was a hand step until 2026-09-16, sitting immediately after a
command that had already edited the same file — and a hand step beside an
automated one is the half that rots. It refuses to cut a version that already
has a section, so a second run cannot open a duplicate.

**Writing the fragments is prose and stays prose.** Renaming a heading was
never prose; reviewing what the fragments *say* still is, and that review is
the reason the release is a PR.

!!! tip "You no longer write the comparison links"

    `## [X.Y.Z]` is a markdown reference link, and without a matching
    `[X.Y.Z]: …` definition it renders as the literal text `[X.Y.Z]`. This
    step used to ask for that line by hand and nothing checked, so 0.43.0,
    0.43.1 and 0.43.2 all shipped without one and there was no
    `[unreleased]:` definition at all ([#996][i996]).

    `scripts/gen_changelog_links.py` derives the whole block from the
    headings — `make docs-relink` writes it, and `make lint` fails on drift.
    On its first run it found seven more missing definitions nobody had
    noticed, and one link that chained `v0.4.1...v0.5.0` straight past 0.4.6.

## 5. Open the PR and merge it green (the gate)

```sh
git commit -am "chore: release vX.Y.Z"
git push -u origin HEAD
gh pr create --fill
gh pr merge --rebase   # (or --squash) ONLY once `CI passed` is green — never bypass it
```

The release tag will point at this merged commit, so CI passing here is what
makes the release safe.

## 6. Tag merged main

```sh
git checkout main && git pull
make tag-release VERSION=X.Y.Z   # verifies on-main + in-sync, tags, pushes the tag
```

`tag-release` pushes **only the tag** (never `main`), which triggers the release
workflow. It refuses to run while any `changelog.d/` fragment is still
unassembled — step 4 is not optional, and the tag is the irreversible step.

!!! warning "The tag push is irreversible"

    Pushing the tag starts the release workflow and PyPI uploads begin. Because
    PyPI is independent of CI, the safety comes entirely from step 5 — only ever
    tag a commit whose `CI passed` check is already green on `main`.

______________________________________________________________________

## 7. Release workflow (automatic)

The [`release.yml`](https://github.com/doppler-dsp/doppler/blob/main/.github/workflows/release.yml)
workflow runs these jobs in order:

```
verify-version  ──  tag == pyproject == Cargo == CMakeLists
verify-ci       ──  poll the "CI passed" aggregator on the tagged SHA
    │                (the merge to main already ran the full suite — we do
    │                 NOT re-test here, we confirm it was green)
    ▼
build-python         ──  manylinux_2_28 x86_64 + aarch64 wheels, one docker run per cp3x/arch
build-macos          ──  macOS arm64 wheels, `make wheel` per Python version
build-sdist          ──  source distribution (`make sdist`)
build-c-linux        ──  C library tarballs (linux-x86_64 + linux-aarch64, matrixed)
build-c-macos        ──  C library tarball (macos-arm64)
    │  (all five build-* jobs run in parallel, gated only on verify-version)
    │  every C tarball is `make package-c-tarball VERSION=x.y.z`, which a
    │  developer can run before cutting a tag
    ▼
smoke-wheel      ──  pip-install the cp312 wheel (x86_64 + aarch64, matrixed)
    │                 + run deploy/validation/wfm_e2e.py
    │                 (smoke-tests the ARTIFACT, not the source tree)
    ▼
publish-python   ──  PyPI (OIDC trusted publishing, no token needed)
    │
    ▼
publish-container ──  ghcr.io/doppler-dsp/doppler:X.Y.Z (+ :latest) —
    │                  pip-installs the just-published wheel, no rebuild
    ▼
github-release   ──  GitHub Release + auto-generated notes + wheel/tarball attachments
    │
    ▼
smoke-c          ──  download the published C tarball; find_package + pkg-config
```

The release pipeline **does not re-run the test suite** — the bump PR's merge
to `main` is the gate, so `release.yml` only *confirms* CI was green on the
tagged commit (`verify-ci` polls the `ci-passed` aggregator job in `ci.yml`),
then builds, smoke-tests the built wheel, and publishes. This matches the
canonical `release-process` skill.

**How each wheel is actually built** (no `cibuildwheel` — `just-buildit` is a
minimal PEP 517 backend driving the whole thing):

1. **Linux (`build-python`)** — one `docker run` per `cp3x` × arch inside
    `quay.io/pypa/manylinux_2_28_x86_64` or `_aarch64` (arch picked by the job
    matrix), installing `cmake`/`pkg-config` (`fftw`/`zeromq` were dropped once
    the FFT was fully vendored and ZMQ was removed — see CHANGELOG) plus
    `numpy`/`uv`/`build`/`just-buildit`, then `python -m build --no-isolation  --wheel`. The aarch64 leg runs **natively** on a `ubuntu-24.04-arm` runner
    (free for public repos) — no QEMU or cross-toolchain anywhere.
1. **macOS (`build-macos`, a separate job)** — natively on a macOS arm64
    runner. `astral-sh/setup-uv` selects the Python, **`make install-deps`**
    installs the system deps (reads `bootstrap.toml`, the single source of truth for
    doppler's system deps), and **`make wheel`** (= `uv build --wheel`) drives
    the same `just-buildit` backend as the Linux leg — no hand-rolled `uv  venv` / `python -m build` bootstrap to drift from CI. The job pins
    `MACOSX_DEPLOYMENT_TARGET=11.0` and
    `_PYTHON_HOST_PLATFORM=macosx-11.0-arm64` so `delocate` tags the wheel
    `arm64`: cmake emits an arm64-only `.so` on this runner, and without the
    override `delocate` fails demanding the absent `x86_64` slice.
1. `just-buildit`'s backend (`make just-build` → cmake + pyext) assembles the
    wheel from the build's output dir, detects the platform tag from the `.so`
    suffix, and runs `auditwheel repair` (Linux, via `uvx`) / `delocate-wheel`
    (macOS) to bundle shared-lib deps into the wheel — this is why the
    published wheel needs zero system packages to `pip install`. Each Linux
    wheel is then scanned for both a leaked `-march=native` (AVX/AVX-512 on
    x86_64) and a leaked `-mcpu=native` (SVE/SVE2 on aarch64) — see
    [Build Internals](build-internals.md#portability-gate) for both scans.

**`verify-version` checks** — it runs `make version-check VERSION=<tag>`, which
fails immediately if any probed file disagrees with the tag or with the others:

- `pyproject.toml`
- `ffi/rust/Cargo.toml`
- `CMakeLists.txt`
- `bootstrap.toml` (`[project] version`)
- `just-makeit.toml` (`[project] version`)

If it fails, bump the missed file manually, push a fixup commit on main, then re-tag.

______________________________________________________________________

## 8. Verify the release

**`make ship` already did this** — there is nothing to type here, which is the
point. This step used to be a hand-run `pip install` in a scratch venv, and a
runbook step reports nothing when it is skipped.

Two gates cover it now:

- **`smoke-pypi`** (`release.yml`, after `publish-python`) installs
    `doppler-dsp==X.Y.Z` **from PyPI** on x86_64 and aarch64 and runs the full
    `wfm_e2e.py` against it. `smoke-wheel` installs a *local file* before the
    upload, so this is the only thing in the release that exercises the index —
    the C library had `smoke-c` doing exactly this and the wheel did not
    ([#1347](https://github.com/doppler-dsp/doppler/issues/1347)).
- **`release-watch.sh`** (the watch half of `make ship`) confirms PyPI serves
    the version and that `latest` has caught up, and that the GitHub Release is
    published, non-draft, with notes and its assets attached.

Neither is a substitute for a look at the
[GitHub Release page](https://github.com/doppler-dsp/doppler/releases): the
asset check counts tarballs, so confirming that **every** platform wheel (Linux
x86_64, Linux aarch64, macOS arm64) plus the three C library tarballs are
present is still worth ten seconds of your eyes.

To re-run the published-wheel check by hand — against any released version,
which is what makes it rehearsable before touching the script:

```sh
make release-smoke-pypi VERSION=X.Y.Z
```

______________________________________________________________________

## Version conventions

Doppler uses plain [Semantic Versioning](https://semver.org) —
`MAJOR.MINOR.PATCH`, which read positionally is **`BREAKING.FEATURE.PATCH`**:

| Position  | Bumps on                                              |
| --------- | ----------------------------------------------------- |
| **MAJOR** | a backward-**incompatible** (breaking) API change     |
| **MINOR** | a new, backward-**compatible** feature / module / API |
| **PATCH** | a backward-compatible bug fix or small additive tweak |

Stable `X.Y.Z` releases only — no alpha/beta/rc suffixes. `main` stays at the
last released version between releases (no post-release dev bump).

### Pre-1.0 (where we are)

The **MAJOR** digit stays `0` until we commit to a stable public API and cut
`1.0.0`, and that has a consequence sharper than it first looks.
[SemVer §4](https://semver.org/#spec-item-4) says of `0.y.z`: *"Anything MAY
change at any time. The public API SHOULD NOT be considered stable."* So the
words "backward compatible" in the table above **do not apply pre-1.0** — the
qualifier presupposes a stable public API, and there isn't one yet. There is no
such thing as a backward-compatible-or-not `0.y.z` release.

Which leaves exactly two questions:

| Increment       | Pre-1.0 meaning                |
| --------------- | ------------------------------ |
| **MINOR** (`Y`) | the release adds functionality |
| **PATCH** (`Z`) | the release only fixes bugs    |
| **MAJOR** (`X`) | unused before `1.0.0`          |

**Breaking changes do not affect the version.** They are not a tiebreaker, not
an escalation, and not a reason to hesitate over the bump: `0.y.z` has already
told users the API is unstable. A release that breaks every signature in the
library and adds one feature is a MINOR; one that breaks the same signatures
and only fixes bugs is a PATCH. The whole obligation is to **say so in a
`### Breaking` section of `CHANGELOG.md`**, which `github-release` publishes
verbatim as the release notes.

Worked examples: 0.6.0 (waveform generator), 0.7.0 (`read_iq`), 0.8.0 (the
Python composer subsystem), and 0.9.0 (the `timing` pacing/timestamping
subsystem) are all **feature** bumps. A bug-fix-only release off 0.8.0 would
have been 0.8.1.

A release can be a feature bump *and* carry breakage: the one that added
`wfm.Reader.header` and `wfm_kw_check_standard()` also changed 38 C kernel
signatures. It took the FEATURE digit because it added functionality — the
breakage went under `### Breaking` in the changelog and had no bearing on
which digit moved.

!!! warning "Don't hand-type the version you are about to release"

    `scripts/check_version_strings.py` greps hand-owned docs for the literal
    version in `pyproject.toml` and fires at *introduction* time. Naming the
    next version here would pass today and then fail **the release bump PR
    itself**, once `pyproject.toml` catches up. Describe releases by what they
    contained, not by their number — historical numbers (0.8.0 above) are
    fine, because they never match the current one.

!!! note "Authoritative record"

    `CHANGELOG.md` in the repository root is the source of truth for what each
    version changed.

[i996]: https://github.com/doppler-dsp/doppler/issues/996
