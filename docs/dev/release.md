# Release Checklist

Step-by-step process for cutting a doppler release.
Run these steps in order — each one is a gate for the next.

______________________________________________________________________

## 1. Verify the tree is clean

```sh
git status          # nothing uncommitted
make test-all       # C (CTest) + Python (pytest) + Rust (cargo test)
make docs-build     # docs build clean --strict
just-makeit bench --python-only --tag vX.Y.Z   # local fallback; CI commits automatically on tag push
```

All suites must pass. Fix failures before continuing.

______________________________________________________________________

## 2. Check examples

If any gallery example scripts in `examples/python/` changed since the
last release, regenerate the plots:

```sh
make gallery
git add docs/assets/
git commit -m "docs: update gallery plots for vX.Y.Z"
```

If you added a new plot-generating script, add it to `GALLERY_SCRIPTS`
in the Makefile before running `make gallery`.

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
difference, not cross-run system drift. Each snapshot is stamped with the
compiler + flags it read from `compile_commands.json`, so the page is
self-describing. Skip only if no perf-relevant code changed since the last
release.

______________________________________________________________________

!!! danger "`main` is protected — everything goes through a PR"

    All changes to `main`, **including the release bump**, land via a pull
    request that the required status checks must pass before merge. Never push
    to `main` directly, and never tag a commit that is not already on a green
    `main`. The release workflow (step 7) runs independently of CI and is *not*
    gated on it — so **the PR merge in step 5 is the real gate**. The tag only
    ever points at a commit those checks already passed.

## 3. Cut the release branch + bump the version

```sh
make release-branch VERSION=X.Y.Z   # branches chore/release-X.Y.Z, then bumps
```

`bump-version` updates **three files** atomically:

| File                  | Field                        |
| --------------------- | ---------------------------- |
| `pyproject.toml`      | `version`                    |
| `ffi/rust/Cargo.toml` | `version`                    |
| `CMakeLists.txt`      | `project(doppler VERSION …)` |

## 4. Update CHANGELOG.md

On the release branch:

1. Rename `## [Unreleased]` → `## [X.Y.Z] — YYYY-MM-DD`
1. Add a fresh empty `## [Unreleased]` section above it
1. Update the comparison links at the bottom of the file:

```markdown
[X.Y.Z]: https://github.com/doppler-dsp/doppler/compare/vPREV...vX.Y.Z
[unreleased]: https://github.com/doppler-dsp/doppler/compare/vX.Y.Z...HEAD
```

## 5. Open the PR and merge it green (the gate)

```sh
git commit -am "chore: release vX.Y.Z"
git push -u origin HEAD
gh pr create --fill
# merge ONLY once every required check is green — do not bypass them
```

The release tag will point at this merged commit, so CI passing here is what
makes the release safe.

## 6. Tag merged main

```sh
git checkout main && git pull
make tag-release VERSION=X.Y.Z   # verifies on-main + in-sync, tags, pushes the tag
```

`tag-release` pushes **only the tag** (never `main`), which triggers the release
workflow.

!!! warning "The tag push is irreversible"

    Pushing the tag starts the release workflow and PyPI uploads begin. Because
    PyPI is independent of CI, the safety comes entirely from step 5 — only ever
    tag a commit that already passed the required checks on `main`.

______________________________________________________________________

## 7. Release workflow (automatic)

The [`release.yml`](https://github.com/doppler-dsp/doppler/blob/main/.github/workflows/release.yml)
workflow runs these jobs in order:

```
verify-version
    │
    ▼
build-python ── matrix ──┬── ubuntu-latest  (manylinux_2_28 x86_64 wheels via cibuildwheel)
                         └── macos-14       (arm64 wheels via cibuildwheel)
    │
    ▼
publish-python  ──  PyPI (OIDC trusted publishing, no token needed)
    │
    ▼
github-release  ──  GitHub Release + auto-generated notes + wheel attachments
```

**What cibuildwheel does per Python version (cp312, cp313):**

1. `before-all` — install system deps (`zeromq-devel`, `fftw-devel`), build C library
1. `before-build` — clean stale `.so` files, build and copy extensions for this interpreter
1. `uv_build` — package the wheel
1. `repair-wheel-command` (`scripts/retag_wheel.sh`) — retag `py3-none-any` → `cpXYZ-cpXYZ`, then `auditwheel repair` (Linux) / `delocate-wheel` (macOS) to bundle shared-lib deps

**`verify-version` checks** — the workflow fails immediately if any of these disagree with the tag:

- `pyproject.toml`
- `ffi/rust/Cargo.toml`
- `CMakeLists.txt`

If it fails, bump the missed file manually, push a fixup commit on main, then re-tag.

______________________________________________________________________

## 8. Verify the release

Once the workflow goes green:

```sh
# Fresh venv — confirm the package installs and imports
python -m venv /tmp/doppler-verify && source /tmp/doppler-verify/bin/activate
pip install doppler-dsp==X.Y.Z

python -c "import doppler; print(doppler.__version__)"
```

Check the [GitHub Release page](https://github.com/doppler-dsp/doppler/releases)
to confirm wheels for both platforms (Linux x86_64, macOS arm64) are attached.

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
`1.0.0`. So every current version is **`0.FEATURE.PATCH`**, and — per
[SemVer §4](https://semver.org/#spec-item-4) — while pre-1.0 a *breaking* change
also bumps the FEATURE digit (there is nowhere else for it to go yet):

| Increment       | Pre-1.0 meaning                                               |
| --------------- | ------------------------------------------------------------- |
| **MINOR** (`Y`) | a new feature / module / public API, **or** a breaking change |
| **PATCH** (`Z`) | a backward-compatible bug fix or small additive tweak         |
| **MAJOR** (`X`) | unused before `1.0.0`                                         |

Worked examples: 0.6.0 (waveform generator), 0.7.0 (`read_iq`), 0.8.0 (the
Python composer subsystem), and 0.9.0 (the `timing` pacing/timestamping
subsystem) are all **feature** bumps. A bug-fix-only release off 0.8.0 would
have been 0.8.1.

!!! note "Authoritative record"

    `CHANGELOG.md` in the repository root is the source of truth for what each
    version changed.
