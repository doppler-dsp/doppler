- **`make build BUILD_TARGET=<t>` and `make test-snippets PAGE=<path>`** — both
    were all-or-nothing, so iterating on one binary or one docs page meant
    reaching past `make` for a raw command (seven `MAKE_SSOT_OK=1` prefixes in
    one session, every one only to scope something). A cold `wfmgen_cli` build
    is 5s against 34s for the tree. A `PAGE` that matches nothing FAILS, so a
    typo cannot read as green.
