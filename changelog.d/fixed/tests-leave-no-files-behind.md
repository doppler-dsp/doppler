- **A passing test suite now leaves no file behind, and `make test` fails if
    one does.** Three C tests left 67 captures in their build directories on
    every green run (and in the repo root when run from there), and every
    pytest run created `.benchmarks/` in the root. Every test target now runs
    under `scripts/check_test_leaks.py`.
