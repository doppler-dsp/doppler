- **The release's C-library job ran half its script on the runner, and
    v0.43.0 shipped without tarballs, containers or a release page.** The step
    passed its script to the manylinux container as a double-quoted argument,
    and a COMMENT inside it carried an unbalanced `"`:

    ```
    # than assumed: without the line `git archive` prints "detected
    # dubious ownership" and writes nothing, and with it the export
    ```

    That closed the argument early, so every line after it was parsed by the
    **runner** instead of the container — and `make package-starter-tarball`
    ran as the runner user against a `build/` that cmake had just created as
    **root** inside the container. It surfaced as

    ```
    mkdir: cannot create directory 'build/starter-pkg': Permission denied
    ```

    a permissions error with no permissions bug behind it. The backticks in
    those comments were command-substituted on the runner too, which is where
    the log's `safe.directory: command not found` came from — the tell that
    the wrong shell was reading the script. Only two of the four commands ever
    reached the container.

    It surfaced now because `package-starter-tarball` is new since v0.42.0, so
    v0.43.0 was its first release. `publish-python` had already succeeded, so
    the wheels are on PyPI and 0.43.0 is installable; `github-release` needs
    the C jobs and was skipped.

    The script now reaches the container on **stdin through a quoted
    heredoc**, so the runner performs no expansion on it at all. Verified by
    delivering it to a stand-in for `docker`: four commands arrive where two
    did.

- **Every workflow `run:` block is parsed, by `make workflow-syntax-check`.**
    GitHub does not parse them, so a shell syntax error does not announce
    itself as one — the shell runs what it managed to parse and the step fails
    somewhere else entirely. `bash -n` reports the defect above in under a
    millisecond, naming the job and the step. `${{ }}` is substituted first,
    since GitHub does that before any shell sees it, and a `shell:` naming
    python/pwsh is skipped rather than guessed at.

- **The container publish no longer fails on PyPI's index lag.** It waited 30
    x 10s for `pip index versions` to show the new release and gave up at
    exactly five minutes; v0.43.0 took longer, so a job whose dependency had
    genuinely succeeded failed anyway and took the images with it. The budget
    is the whole gate — there is nothing to retry against a dependency that is
    simply still propagating — so it is now 15 minutes, inside the job's
    45-minute cap, and the success line reports how long it actually took.
