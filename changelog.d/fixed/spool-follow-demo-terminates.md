- **`spool_follow_demo` waited for a Ctrl+C nobody could send, and hung the
    example gate.** It took its run length from `argv[1]` and defaulted to
    `0.0`, which means *wait for a real SIGINT*. `make test-examples-c` runs
    every example with no arguments and no terminal, so the demo blocked until
    the gate's 120 s timeout and reported `FAIL (timed out after 120s)` — on
    every machine and in the glibc 2.28 (Debian 10) CI job. This was the
    unexplained half of `make gates` being red.

    An example must terminate on its own: that is what makes it a gate rather
    than a demonstration nobody runs. The default is now a bounded 0.3 s run
    (~61k samples at the writer's pace), matching what
    `graceful_shutdown_demo` already did with its `RUN_MS`. Passing an
    explicit `0` still asks for the interactive form, so the Ctrl+C
    demonstration stays one keystroke away.

    It also stopped leaving `spool_follow_demo.blue` behind: the `remove()` at
    the end of `main` never ran when the process was killed on timeout.
