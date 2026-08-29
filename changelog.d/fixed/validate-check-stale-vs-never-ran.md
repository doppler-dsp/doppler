- **`make validate-check` tells a stale report apart from a validator that
    never ran.** Any non-zero exit read as STALE and was answered with
    `run 'make validate'` — but the common case is a fresh worktree where the
    Python extensions are not built, and re-rendering a report cannot fix an
    import. Staleness now has its own exit status (an uncaught exception also
    exits 1, which is why the two were indistinguishable), a crash is reported
    verbatim and names `make pyext`, and the classification moved into a
    script so it can be sabotaged.
    See [#1074](https://github.com/doppler-dsp/doppler/issues/1074).
