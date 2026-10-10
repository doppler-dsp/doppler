- **`check_doc_targets` holds a bare target name, on every tracked page.**
    It saw only `make <target>` on a hand list of pages that left out
    CLAUDE.md, so "`bench-check` remains" there, `test-example-tarball` in
    the downstream-jm README and a `python-tests` CI job in
    docs-conventions.md all outlived their renames. A backticked name in a
    target's family must now name a target, CI job or hook, and the page
    set is every tracked `.md` file but the changelog. The three are
    fixed (#2116).
