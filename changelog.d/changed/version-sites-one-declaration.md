- **The version sites have one declaration, one reader and one writer.** There
    were two lists — `VERSION_PROBES` (how to READ each site) and
    `BUMP_VERSION_CMD` (how to WRITE it) — and they differed by more than a
    filename: five bespoke `grep | sed` reads against five bespoke `sed -i`
    writes, so the two spellings of one site could disagree about which line
    they meant, and a site in one list and not the other was invisible.

    They did disagree, and the consequence was not hypothetical. The writer
    stripped a pre-release suffix on the way into `CMakeLists.txt` and
    `Cargo.toml` (CMake and Cargo reject `1.2.3rc1`) while the reader read back
    whatever was there and `version-check` demanded every site match:

    ```console
    $ make bump-version VERSION=0.44.0rc1
    $ make version-check
    ERROR: CMakeLists.txt has 0.44.0, but pyproject.toml has 0.44.0rc1
    ```

    A bump that cannot pass its own check — in the exact step `release.yml`
    runs against the tag, so doppler could not have cut a pre-release at all.
    Each rule was locally reasonable and jointly impossible, which is what two
    lists buy you.

    `scripts/version_sites.py` now holds one table of (path, pattern, table
    scope) with one reader and one writer over it, and the Makefile carries
    only the labels. Because the writer replaces **capture group 1** rather
    than rewriting a line, quoting and surrounding syntax survive by
    construction instead of by each site's sed getting it right; because it
    requires **exactly one** match per site, a pattern that stops matching is
    an error rather than a silent no-op — sabotage-checked three ways
    (reformatted key, duplicate key, renamed table). After writing it reads
    every declared site back, so a site the Makefile forgets to probe is still
    verified.

    Pre-releases are now **refused with that reason** rather than
    half-supported. `standard.mk` is vendored verbatim and its `version-check`
    requires every probe to return the same string, which CMake and Cargo
    cannot do; supporting them needs a change there, not a local workaround
    that produces an untaggable tree. doppler has cut zero pre-releases.
