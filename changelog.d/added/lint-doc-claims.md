- **`make lint-doc-claims`: a docs fence or example must check what it
    shows.** A bare `np.array_equal(a, b)` or `a == b` statement runs green and
    checks nothing, and an example with no `assert`/`raise`/`sys.exit(<expr>)`
    can only exit 0. Three such claims on `main` are now asserts (#1682).
