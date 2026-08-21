- **`make test-examples-c` discovers the C examples instead of listing
    them, and runs each under a deadline.** It iterated a hand-written list
    of nine binary names, so the other four compiled, shipped, and were
    executed by nothing — with no reason recorded, nothing failing if a
    fifth joined them, and nothing noticing if one was deleted. A C example
    is documentation that claims to be executable, so one that runs nowhere
    is the shape this repo already calls indistinguishable from a gate
    passing.

    Discovery is over `examples/c/*.c`, so a new example is gated the moment
    it exists. Opting one out costs an entry in `examples/c/.examples-skip`
    with a **mandatory reason** — the same contract
    `src/doppler/examples/.examples-skip` already holds the Python side to,
    and the mechanism this mirrors.

    Writing the four exclusions down turned out to separate them.
    `pipeline_demo` is PUSH/PULL between two **in-process** threads: it needs
    a broker but no peer, and it exits on its own. So it takes the
    conditional `broker:` idiom — it now RUNS wherever `127.0.0.1:4222`
    answers, which CI arranges. Ten examples run where nine did. The other
    three print *"Press Ctrl+C to stop"* and have no exit condition, so no
    broker makes them terminate; their round-trips stay covered by the
    stream suite and `make docker-stream`.

    Four properties, each failing rather than warning: a stale waiver, a
    waiver with no reason, a source that produced no binary (the same
    fail-open bug one layer down, in `examples/c/CMakeLists.txt`'s own hand
    lists), and running nothing at all. Every run is under a deadline, which
    is load-bearing rather than defensive — the cheapest way to reintroduce
    "an example nothing runs" is an example that runs forever, and without a
    deadline the gate hangs instead of failing, reading as *still working*
    until CI's own ceiling kills the job and names the wrong thing.

    The deadline runs through `scripts/with-deadline.sh` rather than
    `timeout(1)` directly. `timeout` is coreutils and is absent from
    GitHub's macOS runner — `gtimeout` too — which that script already
    measured and already solves behind one contract, POSIX-watchdog fallback
    and the 124 exit code included. A bare `timeout` there does not time
    anything out; it fails with 127 and reports the example as broken. The
    discovery loop is likewise a `while read` rather than `mapfile`, which
    is bash 4 against the runner's bash 3.2.
