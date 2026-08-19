- **The coverage job's exit code stopped being thrown away a second time**,
    and a gate now holds the rule for every workflow step. Removing the
    leading `-` from the recipe's pytest fixed one discard; the step around
    it was doing the same thing one layer out:

    ```yaml
    run: make coverage | tee coverage.txt
    ```

    Actions runs a `run:` block under `bash -e`, where a pipeline reports the
    **last** command's status — so this step was green whenever `tee` was,
    which is always. `make coverage` had in fact failed: the install of the
    instrumented `wfmgen` died on a directory that does not exist in a clean
    checkout, no report was written, and the only symptom was the patch gate
    one step later opening a `coverage.lcov` nothing had produced. A missing
    number is harder to notice than a wrong one.

    Both halves are fixed. The step declares `shell: bash` (GitHub's alias
    for `bash -eo pipefail`), and the recipe creates the destination
    directory instead of relying on one. That directory is the whole reason
    the failure was CI-only: the build bundles `wfmgen` into the copied
    package, the purge that keeps deleted tests from lingering removes it —
    it is not a `*.so` — and takes the emptied `wfm/_bin/` with it, after
    which the directory came back only from `src/doppler/wfm/_bin/`, which is
    `.gitignore`d and therefore present on a developer's tree and never on a
    runner.

    The gate is `make lint-ci-pipefail`
    (`scripts/check_workflow_pipelines.py`): every step whose script contains
    a shell pipeline must have pipefail in effect, via `shell: bash`, a shell
    string that names it, or `set -o pipefail`. Registration-free — it walks
    every workflow and composite action, so a new file is covered the moment
    it exists. It reads shell quoting rather than searching for `|`, because
    a `jq` filter carries one as data and a pipeline inside `$( )` discards
    its status just the same; that distinction is not academic, since it
    found two more real discards in `release.yml`, both now declared.
    Sabotage-proven: restoring the bare `make coverage | tee` line turns the
    gate red, and `src/doppler/tests/test_ci_pipefail_gate.py` drives it over
    seeded YAML so it is exercised against a step that must fail, not only
    against a tree that passes.
