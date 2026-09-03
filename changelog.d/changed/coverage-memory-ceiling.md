- **`make coverage` runs under a memory ceiling where the host can hold
    one.** `scripts/mem-guard.sh` puts the instrumented build and all three
    suites in a systemd user scope with `MemoryMax` at 3/4 of RAM, proved
    enforced by a probe before it is trusted, so a runaway test is killed on
    its own instead of taking the machine down (three WSL kills this year).
    Elsewhere it runs unguarded and says so.
