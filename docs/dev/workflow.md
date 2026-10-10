# How we work: change, merge, release

The policy for getting a change from a branch to a release. [CI](ci.md)
says what the checks are, and [Release](release.md) gives the release
steps. This page says how we work so those checks stay fast.

**The one idea: the fast path comes from how we work, not from CI skipping
things.** A pull request runs the full matrix: there has been no merge
queue since 2026-10-01. A full run takes **16–31 min** (the last eight
green `push` runs on `main`, 2026-10-01:
`gh run list --workflow CI --branch main --event push --status success`).
The rules below make that run the only thing a change waits for, and they
make sure **each tree that lands on `main` is tested once**.

CI runs less in exactly three cases, and each has a reason it can prove
rather than assume:

| lane                      | what it skips               | why it is safe                                                |
| ------------------------- | --------------------------- | ------------------------------------------------------------- |
| a docs-only PR (§1)       | every job docs cannot break | nothing compiled or imported changed (`make ci-docs`)         |
| `main` after a merge (§4) | the matrix                  | the PR already passed this exact tree (`make ci-tree-tested`) |
| a release PR (§6)         | the matrix                  | only the version string moved (`make ci-changes`)             |

Each is earned by how a change is shaped and merged, which is what the
rest of this page is about.

______________________________________________________________________

## 1. Shape the change

- **One logical change per PR, about 400 lines or fewer.** Several commits
    are fine; each one tells a step of the story. Count the lines a
    reviewer reads: generated files (goldens, `.pyi` stubs, `docs/c-api`,
    `uv.lock`) do not count. A refactor, a behaviour change and a
    regeneration are three PRs. A small PR reviews in minutes, fails for
    one reason, reverts cleanly, and rebases cheaply when another lands
    first (§4).

- **Branch from `origin/main`, independently, by default.** Stack only on a
    real dependency, and **no deeper than two**. When a base branch is
    deleted, GitHub closes the PR stacked on it, and that PR cannot be
    reopened. Retarget the child (`gh pr edit <n> --base main`) *before*
    the base merges.

- **Keep docs changes in their own PR.** A PR that touches only the docs
    (`CI_DOCS_RE` in the Makefile: `docs/`, `mkdocs.yml`, the changelog,
    `README.md`, `CONTRIBUTING.md`) skips the C builds on every platform,
    Doxygen, sanitizers, coverage, packages and Docker. It still runs lint,
    the site build, and the full Python suite on one interpreter, because
    docs can break those (doc fences, tests that read the live tree). Mix
    one code line in and the PR runs everything.

- **A PR carries its own paperwork.** A `changelog.d/<kind>/<slug>.md`
    fragment, and a `Closes #N` or `No-issue: <why>` line in a commit
    message. These are what keep the release PR a bump alone (§6). A PR
    without them is refused by `changelog-check` and `issue-link-check`.

- **A carve-out gets an issue before the PR merges.** Something you
    deliberately leave undone is filed, then linked from the PR body, and
    never only explained in a comment.

## 2. Prove it locally, then push

**Lint and formatting run locally; testing is CI's job.** The pre-commit
hook runs on every commit and is never skipped (no `--no-verify`). Every
hook is a `make -s lint-*` target, and they are strictly lint and
formatting: no build, no test. That keeps a commit cheap enough that nobody
is tempted to skip it. A hook that runs a test belongs in CI instead.

Beyond the hook, run only what proves *this* change:

1. its test, written first and seen red, then green;
1. a sabotage, in a copy, that turns that test red again;
1. the tests the change can reach.

Run those through the target, narrowed with `TEST_PATHS`, a space-separated
list of files or directories. It is the same mem-guard, leak check and flags
as the full suite, and `-k` still narrows by name within it:

```sh
make test-python TEST_PATHS=src/doppler/spectral/tests/test_psd.py
make test-python TEST_PATHS="src/doppler/agc/tests src/doppler/track/tests"
make test-python TEST_PATHS=src/doppler/track/tests PYTEST_ARGS="-k costas"
```

A path in `PYTEST_ARGS` does **not** narrow: it is collected in addition to
`src/`, and the whole suite runs ([#1998](https://github.com/doppler-dsp/doppler/issues/1998)).
Never run `pytest` directly: the make-SSOT hook stops it, and the bypass
skips the guard and the leak check.

For one file, add `PYTEST_ARGS="-n 0"`: xdist otherwise starts a worker per
core to run a handful of tests. Doc-fence and example tests are not
`test-python`'s: it deselects their markers, so pointing `TEST_PATHS` at
one exits 5 (nothing collected). They run through `make test-snippets`
(`PAGE=<path>` to narrow) and `make test-examples-python`
(`PYTEST_ARGS="-k <name>"`).

Then push, and let CI run the rest.

**`make gates` exists only to debug a CI failure. Never run it before CI.**
It runs serially on one machine what CI runs in parallel, and CI then runs
all of it again anyway. When CI goes red, read the failing job's log first
(minutes). Reach for `make gates`, or `make ci-gates` in the pinned image,
only when the log does not explain the failure.

Keep locally only what CI cannot see: whether two open PRs compose
(`git merge-tree`), and the benchmark and gallery renders a release owes.

## 3. While CI runs

Do not wait on it. Start the next **independent** change, or review
someone else's.

Bound the work in flight, though. A lane is finished when its PR has
merged *and* `main`'s `push` run after it is green. Prefer two or three
finished lanes over nine open ones: past that, mistakes start coming from
the pace rather than the code. And do not cancel someone else's CI run to
speed up your own.

## 4. Merge

- **On green, by rebase.** `CI passed` is the one required check, and
    the person who opened the PR merges it once that check is green. No
    approving review is required, but every review thread must be
    resolved. Rebase keeps each commit and its message (and its
    `Closes #N`); squash is allowed but can drop that line. Merge commits
    are refused, because history is linear.

- **Up to date with `main`, always.** `protect-main` requires it: a PR
    whose branch does not contain the current tip cannot merge. So the tree
    that lands is always the tree the PR's own run tested, and `main`'s
    `push` run sees that (`make ci-tree-tested`) and skips the matrix, the
    same way it skips a version bump. Only the cheap gates run. Each tree
    on `main` is tested exactly once, before it lands.

- **Behind: rebase, let it run, merge.** When another PR lands first,
    yours is behind. Rebase it, and its run tests the combination:

    ```sh
    git fetch origin
    git rebase origin/main && git push --force-with-lease
    ```

    Never use GitHub's *Update branch* button: it makes a merge commit,
    which linear history refuses.

- **Keep few PRs in flight, and this stays cheap.** Every merge puts every
    other open PR behind, and each rebase costs one full run (§3). With two
    or three in flight, that is one extra run now and then. With nine, it
    becomes a queue you run by hand.

## 5. When `main` goes red

**Stop the line.** A red `main` blocks everyone, because every PR is
tested against it. Whoever sees it first:

1. Merge nothing else except the fix.
1. Read the failing job's log and find the commit that broke it.
1. If the fix is not obvious within minutes, **revert** that commit in a PR
    of its own, then fix forward from green.

Every tree on `main` was tested green before it landed (§4), so a red
`main` is the *environment* moving: a hosted runner image, an unpinned
tool, a service. Find what moved before calling anything flaky. **The
nightly full run on `main`** (03:43 UTC, never skipped) is where that drift
shows up first, on `main` and not on someone's PR.

**A run that fails at startup cannot be re-run** ("This workflow run cannot
be retried"). Every workflow that triggers itself also takes
`workflow_dispatch`, so start it again by hand: Actions → the workflow →
*Run workflow*, or `gh workflow run <file> --ref main`. A release names its
existing tag (`gh workflow run release.yml -f tag=vX.Y.Z`).

## 6. Release: the bump-only fast path

A release is designed to be a change CI can skip. `changes` classifies a
diff as `src=false` when everything outside `CI_INERT_RE` (the changelog,
gallery PNGs, published benchmarks) differs from its base only by the
version string. Then `CI passed` runs the cheap gates (lint, manifest
drift, the image pin) and skips the matrix the parent commit already
passed.

That only holds if everything else has already landed:

- **Release from a green `main` that has not moved.** Every change since
    the last tag is merged, each through its own full-matrix PR, and
    `main`'s `push` run after the last one is green.
- **The release PR holds the bump, the assembled changelog, and inert
    renders. Nothing else.** A fix found while releasing is its own PR,
    merged first. Folding it in would make the release PR full-matrix and
    would ship code no PR tested on its own.
- **Nothing merges between the release commit and the tag.** If something
    must, fold its changelog entry into the unpublished section.
- **Release when it is solid, not by a date.** Cut a release when a
    coherent set has landed and been verified. Steps:
    [Release](release.md), mostly `make release-pr` then `make ship`.

## 7. The weekly image repin

`ci-image.yml` runs every Monday at 04:17 UTC. It picks a new apt
snapshot and new base digests, and when the package set has moved it
pushes a one-file repin to `ci/repin-image`. Until that lands,
`CI image repin pending` is red on **every** PR, so the repin is the
first thing merged on Monday:

```sh
gh pr create --head ci/repin-image --fill
```

Every other rebuild reproduces the pinned image, so a repin is owed at most
once a week. See [CI](ci.md).

## 8. Changing the shared `standard.mk`

`standard.mk` and its vendored scripts come from
[just-buildit.github.io](https://github.com/just-buildit/just-buildit.github.io),
and every repo holds them byte for byte (`standard-check`). So a canonical
merge turns every adopter's lint red until it re-vendors. The canonical
repo's own bot opens that re-vendor PR in each adopter once Pages serves the
merge.

- **Land a canonical change as one merge.** Each merge opens one bot PR per
    adopter, so a batch merged one PR at a time produces several per repo.
- **Fix the adopters first.** A new gate needs each adopter to already pass
    it (a declared exception, a timeout, a trigger). Land those fixes, then
    the canonical change, so the bot's PR is a pure sync that goes green.
- **Never re-vendor by hand straight after a merge.** Until Pages deploys,
    `make standard-update` fetches the old file and reports a match. Merge
    the bot's PR.

______________________________________________________________________

## What enforces this

| rule                                             | enforced by                                                                                     |
| ------------------------------------------------ | ----------------------------------------------------------------------------------------------- |
| every change goes through a PR                   | `protect-main` (no direct push, force-push or delete)                                           |
| full matrix on every PR                          | `CI passed` + `make ci-aggregator-check` (refuses a lighter PR lane)                            |
| linear history                                   | `protect-main` (rebase or squash only)                                                          |
| changelog fragment, issue link                   | `changelog-check`, `issue-link-check` (in `make lint`)                                          |
| pre-commit is `make` lint targets only           | `hook-dispatch-check` (any other hook is a declared `HOOK_DISPATCH_EXEMPT`), `hook-stage-check` |
| pre-commit is never skipped                      | CI's `pre-commit` job runs the same hooks, so `--no-verify` goes red there                      |
| the release PR skips the matrix                  | `make ci-changes` (`src=false` only for a bump plus `CI_INERT_RE` files)                        |
| `main` does not re-test a tested tree            | `make ci-tree-tested` (`src=false` when a green PR head had this tree)                          |
| a docs-only PR skips only what docs cannot break | `make ci-docs` + `CI passed`'s `CODE_ONLY`, held to the gated jobs by `ci-aggregator-check`     |
| `main` gets a full run anyway, nightly           | `ci.yml`'s `schedule` (never skipped)                                                           |
| every job has a ceiling                          | `workflow-timeout-check` (in `make lint`)                                                       |
| every self-triggered workflow can be re-run      | `workflow-dispatch-check` (in `make lint`)                                                      |
| a pending repin is loud                          | `ci-image.yml` (every `main` run red while `ci/repin-image` differs)                            |
| PR up to date before merging                     | `protect-main` (strict required status checks)                                                  |
| review threads resolved                          | `protect-main`                                                                                  |
| one change per PR, ≤ ~400 lines, stack depth ≤ 2 | judgement (a target, not a limit)                                                               |
| stop the line on a red `main`                    | judgement                                                                                       |
| work in flight bounded                           | judgement                                                                                       |
