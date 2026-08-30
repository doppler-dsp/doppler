- **just-makeit pin 0.71.0 → 0.71.1.** It carries
    [just-makeit#1191](https://github.com/just-buildit/just-makeit/issues/1191),
    filed from here: a manifest `doc` edit never reached a sacred fragment's
    runtime slot once that slot held content, so `help()` and a type checker
    could tell a reader different things. Verified with the sentinel that
    diagnosed it — under 0.71.0 it reached only the `.pyi`, under 0.71.1 it
    reaches both. `jm apply` then corrected 26 fragments, one of which had
    been advertising `steps(x)` for a method that takes `steps(x, out)`.
