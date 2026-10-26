- **A second Ctrl+C means "stop now".** `dp_interrupt()` counts requests,
    saturating at 2, and `dp_interrupted()` / `Interrupt.interrupted()` return
    that count. A follow read that is waiting for a writer's end marker (its
    grace, unbounded by default) gives up when the count reaches 2, with
    `ending = "interrupted"`, so a writer that died before the first Ctrl+C can
    no longer hang the reader. Samples already on disk are still returned
    first. The POSIX signal handler now counts too: it used to store 1 directly,
    so a second signal could never escalate (#2601).
