- **`issue-link-check` and `changelog-check` no longer pass when they cannot
    see the work.** Both compare against `origin/main`, so before the first
    commit they have an empty diff, report `inert`, and exit 0 — which reads
    exactly like a verdict. Running `make lint` before committing is the most
    natural order and it is precisely when these answer about nothing; #1012
    reached CI red on a check that had "passed" locally minutes earlier.
    Inert plus a dirty tree is now a failure naming the files, inert plus a
    clean tree is still a pass, and with commits ahead a dirty tree warns
    that the verdict read the commits rather than the working tree.
