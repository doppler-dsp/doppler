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

## 4. Update CHANGELOG.md

In `CHANGELOG.md`:

1. Rename `## [Unreleased]` → `## [X.Y.Z] — YYYY-MM-DD`
1. Add a fresh empty `## [Unreleased]` section above it
1. Update the comparison links at the bottom of the file:

```markdown
[Unreleased]: https://github.com/doppler-dsp/doppler/compare/vX.Y.Z...HEAD
[X.Y.Z]: https://github.com/doppler-dsp/doppler/compare/vPREV...vX.Y.Z
```

Open a release branch, commit the CHANGELOG, get CI green, then merge:

```sh
git checkout -b chore/release-X.Y.Z
git add CHANGELOG.md
git commit -m "docs: update CHANGELOG for vX.Y.Z"
git push -u origin chore/release-X.Y.Z
gh pr create --fill
# merge once CI is green
```

______________________________________________________________________

## 5. Bump the version

`make bump-version` updates **three files** atomically:

| File                  | Field                        |
| --------------------- | ---------------------------- |
| `pyproject.toml`      | `version`                    |
| `ffi/rust/Cargo.toml` | `version`                    |
| `CMakeLists.txt`      | `project(doppler VERSION …)` |

```sh
make bump-version VERSION=X.Y.Z
```

Review the diff. Do **not** commit here — `make tag-release` (next
step) creates the `chore: release vX.Y.Z` commit itself.

______________________________________________________________________

## 6. Tag and push

`make tag-release` creates the release commit, an annotated tag, and
pushes both, which **triggers the release workflow automatically**:

```sh
make tag-release VERSION=X.Y.Z
```

!!! warning "This push is irreversible"

    Once the tag is pushed, the release workflow starts and PyPI
    uploads begin. Double-check the version number before running.

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

Doppler uses [Semantic Versioning](https://semver.org) with stable
`X.Y.Z` releases only — no alpha/beta/rc suffixes. `main` stays at the
last released version between releases (no post-release dev bump).

While pre-1.0 the digits shift down one place: the minor digit stands
in for major, and the patch digit absorbs both features and fixes.

| Increment       | When to use                               |
| --------------- | ----------------------------------------- |
| **Patch** (`Z`) | New features and bug fixes (non-breaking) |
| **Minor** (`Y`) | Breaking API changes                      |
| **Major** (`X`) | Unused before `1.0.0`                     |

!!! note "Pre-1.0"

    `CHANGELOG.md` in the repository root is the authoritative record.
