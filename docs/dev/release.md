# Release Checklist

Step-by-step process for cutting a doppler release.
Run these steps in order — each one is a gate for the next.

---

## 1. Verify the tree is clean

```sh
git status          # nothing uncommitted
make test-all       # C (CTest) + Python (pytest) + Rust (cargo test)
make docs-build     # docs build clean --strict
```

All suites must pass. Fix failures before continuing.

---

## 2. Update CHANGELOG.md

In `CHANGELOG.md`:

1. Rename `## [Unreleased]` → `## [X.Y.Z] — YYYY-MM-DD`
2. Add a fresh empty `## [Unreleased]` section above it
3. Update the comparison links at the bottom of the file:

```markdown
[Unreleased]: https://github.com/doppler-dsp/doppler/compare/vX.Y.Z...HEAD
[X.Y.Z]: https://github.com/doppler-dsp/doppler/compare/vPREV...vX.Y.Z
```

Commit the CHANGELOG separately so the diff is easy to review:

```sh
git add CHANGELOG.md
git commit -m "docs: update CHANGELOG for vX.Y.Z"
```

---

## 3. Bump the version

`make bump-version` updates **five files** atomically:

| File | Field |
|------|-------|
| `pyproject.toml` | `version` |
| `python/specan/pyproject.toml` | `version` |
| `python/cli/pyproject.toml` | `version` |
| `ffi/rust/Cargo.toml` | `version` |
| `CMakeLists.txt` | `project(doppler VERSION …)` |

```sh
make bump-version VERSION=X.Y.Z
```

Review the diff, then commit:

```sh
git diff
git add pyproject.toml python/specan/pyproject.toml python/cli/pyproject.toml \
        ffi/rust/Cargo.toml CMakeLists.txt
git commit -m "chore: release vX.Y.Z"
```

---

## 4. Tag and push

`make tag-release` creates an annotated tag and pushes it, which
**triggers the release workflow automatically**:

```sh
make tag-release VERSION=X.Y.Z
```

!!! warning "This push is irreversible"
    Once the tag is pushed, the release workflow starts and PyPI
    uploads begin. Double-check the version number before running.

---

## 5. Release workflow (automatic)

The [`release.yml`](https://github.com/doppler-dsp/doppler/blob/main/.github/workflows/release.yml)
workflow runs these jobs in order:

```
verify-version
    │
    ▼
build-python ── matrix ──┬── ubuntu-latest  (manylinux_2_28 x86_64 wheels via cibuildwheel)
                         ├── macos-14       (arm64 wheels via cibuildwheel)
                         └── macos-13       (x86_64 wheels via cibuildwheel)
    │
    ▼
publish-python  ──  PyPI (OIDC trusted publishing, no token needed)
    │
    ▼
github-release  ──  GitHub Release + auto-generated notes + wheel attachments
```

**What cibuildwheel does per Python version (cp312, cp313):**

1. `before-all` — install system deps (`zeromq-devel`, `fftw-devel`), build C library
2. `before-build` — clean stale `.so` files, build and copy extensions for this interpreter
3. `uv_build` — package the wheel
4. `repair-wheel-command` (`scripts/retag_wheel.sh`) — retag `py3-none-any` → `cpXYZ-cpXYZ`, then `auditwheel repair` (Linux) / `delocate-wheel` (macOS) to bundle shared-lib deps

**`verify-version` checks** — the workflow fails immediately if any of these disagree with the tag:

- `pyproject.toml`
- `python/specan/pyproject.toml`
- `python/cli/pyproject.toml`
- `ffi/rust/Cargo.toml`
- `CMakeLists.txt`

If it fails, bump the missed file manually, push a fixup commit on main, then re-tag.

---

## 6. Verify the release

Once the workflow goes green:

```sh
# Fresh venv — confirm all three packages install and import
python -m venv /tmp/doppler-verify && source /tmp/doppler-verify/bin/activate
pip install doppler-dsp==X.Y.Z doppler-specan==X.Y.Z doppler-cli==X.Y.Z

python -c "import doppler; print(doppler.__version__)"
doppler --help
doppler-specan --help
```

Check the [GitHub Release page](https://github.com/doppler-dsp/doppler/releases)
to confirm wheels for all three platforms are attached.

---

## 7. Bump to the next development version

Immediately after tagging, advance to the next alpha so `main` is
never at a released version:

```sh
make bump-version VERSION=X.Y.(Z+1)a0
git add pyproject.toml python/specan/pyproject.toml python/cli/pyproject.toml \
        ffi/rust/Cargo.toml CMakeLists.txt
git commit -m "chore: begin vX.Y.(Z+1) development"
git push origin main
```

---

## Version conventions

| Version | When to use |
|---------|-------------|
| `X.Y.Z` | Stable release |
| `X.Y.Za0`, `a1`, … | Alpha — API may change |
| `X.Y.Zb0`, `b1`, … | Beta — feature-complete, stabilising |
| `X.Y.Zrc0`, `rc1`, … | Release candidate — no new features |

Doppler follows [Semantic Versioning](https://semver.org):

- **Patch** (`Z`) — bug fixes only, no API changes
- **Minor** (`Y`) — new features, backwards-compatible
- **Major** (`X`) — breaking API changes (rare before 1.0)

!!! note "Pre-1.0"
    Minor releases **may** contain breaking changes before `1.0.0`.
    The [CHANGELOG](../../CHANGELOG.md) is the authoritative record.
