# Ending a wait — one contract for network, memory and disk

doppler moves samples over three transports. A NATS subject, a
double-mapped ring in shared memory, and a capture file on disk. They
look unrelated, and they have the same defect three times.

**"No data right now" is indistinguishable from "no data ever."** Every
consumer-side wait in this library has a termination condition it cannot
determine from the data path alone, and every producer has a question it
cannot answer: did the bytes I handed over actually land?

This page is the single answer. It exists because the alternative is
three mechanisms that drift — the transports already disagree about what
"empty" means, and each one that grows its own escape hatch makes the
next one harder to unify.

Not to be confused with [Streaming](streaming.md), which is the NATS wire
envelope. This is the *wait contract*, and NATS is one of its three
callers.

______________________________________________________________________

## 1. The same bug, three costumes

|             | the wait                                        | how it fails                                                                                                                  | producer-side question                                                                                             |
| ----------- | ----------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| **network** | blocking `recv` inside the NATS client          | keeps the caller out of the loop where the flag is read                                                                       | `dp_pub_flush` / `dp_stream_drain` — the client's own close is best-effort with a 500 ms cap and no failure report |
| **memory**  | `dp_<t>_wait()` in `native/inc/buffer/buffer.h` | **unbounded busy-spin.** No timeout, no flag, no producer-done. If the producer stops, the consumer spins forever at 100% CPU | the `dropped` counter — an overrun is a field someone might read, not an answer anyone gets                        |
| **disk**    | `wfm_reader_read()` returning 0                 | `0` means end-of-file *or* not-written-yet. A live capture cannot tell them apart                                             | the writer's close returns `int` and raises `OSError`                                                              |

Ranked by severity, which is not the order they were built in:

- **Memory is the worst and has no escape at all.** The other two waits
    can at least be abandoned by a signal; a `DP_SPIN_HINT()` loop checks
    nothing, so no handler can rescue it and it burns a core while
    failing.
- **Disk is the subtlest, because it does not hang — it lies.** A
    tail-following reader treats a slow writer as end-of-file, gets a
    short read, and reports a clean finish on a truncated capture. A
    hang is at least visible.
- **Network is the one already fixed**, and it is the model: §8b of
    [Streaming](streaming.md) slices the wait and checks a flag inside
    it.

______________________________________________________________________

## 2. The use cases

- **A continuous producer that must stop cleanly.** `wfmgen --continuous --realtime` to a subject, a file, or a ring. Ctrl+C must
    end it without losing the tail already handed over
    ([#969](https://github.com/doppler-dsp/doppler/issues/969)).
- **A consumer at rate that must stay responsive.** A dashboard or
    analyzer reading as fast as the transport delivers, still answering
    Ctrl+C.
- **A pipeline stage whose upstream finishes.** The case none of the
    three handles today: a consumer must learn that no more data is
    coming and exit 0, rather than block, spin, or silently truncate.

______________________________________________________________________

## 3. Three verbs, one vocabulary

### Interruptible — the primitive already exists

`dp_stream_interrupt()` is a `volatile sig_atomic_t` and four trivial
accessors in `native/src/stream/stream_core.c`, a file that includes
nothing but libc and its own header. **It has no NATS dependency.** It is
already the general primitive; only its name and location say otherwise.

So nothing new gets written here. It moves down a layer and gains two
callers: the ring's wait and the disk tail-wait check the same flag the
NATS wait already checks.

**It is renamed.** `dp_interrupt()`, `dp_interrupted()`, `dp_resume()`,
`dp_interrupt_on_signal()`, and the latency accessors lose the `stream_`
infix. The `dp_stream_*` spellings are deprecated and then removed rather
than aliased indefinitely: this is a breaking change to an API that
shipped in v2 days ago, its callers are in-tree plus the examples, and a
general primitive carrying the name of one of its three users is exactly
the naming rot this repo keeps rediscovering. Two spellings forever is
the more expensive option, just later.

### End-of-stream — the piece that does not exist

No transport can say "I am done" today. This is the genuinely new
mechanism, and it is what kills the race in all three cases:

- **memory** — a producer marks the ring closed; `wait()` returns
    end-of-stream instead of spinning when it is closed and drained.
- **disk** — a reader distinguishes "short read, writer still open" from
    "end of capture", which is what makes tail-following honest.
- **network** — an explicit end-of-stream frame, so a subscriber learns
    the sender finished rather than inferring it from silence.

Explicit, not inferred, and the alternatives were checked rather than
assumed away. NATS does offer one adjacent mechanism — a JetStream stream
can be **sealed** — and it is the wrong instrument: an admin operation on
a whole stream, irreversible, blocking *every* future writer rather than
saying one sender finished, and absent from core PUB/SUB entirely. On a
multi-sender subject it asserts something false. The remaining candidates
are inference wearing a better name: a pull consumer's "no messages"
means the stream is empty *now*, which is the exact ambiguity being
removed, and connection-drop detection cannot distinguish a clean finish
from a crash.

### On the wire it is a KIND, not a flag

doppler's own validation rules decide this. §3 of
[Streaming](streaming.md) lists what a receiver checks: any `flags` bit
outside `DP_FLAG_KNOWN` is **refused**, because an unknown block moves
where the payload starts. The `kind` field is not among the five checks.

So a new flag bit would be breaking — every existing receiver rejects the
frame — while a new kind is the precedent `DP_KIND_TLM` already set when
telemetry stopped pretending to be a sample format.

"Additive" needs one qualification, found by implementing it rather than
by reading: doppler's own validator refuses any frame whose element size
is zero, and an end-of-stream frame has no element size because it has no
elements. So a receiver built before this existed does not ignore the
marker — it rejects the frame as `DP_ERR_INVALID`. That is the safe
failure (a refusal, not a misparse) and it is still additive in the sense
that matters, since no *existing* frame changes meaning. But "an old
receiver quietly skips it" would have been wrong, and the validator had to
be taught that a frame carrying nothing is checked differently rather than
not at all: an EOS frame that claims a payload is still refused.

### The one thing it cannot promise

**PUB/SUB is at-most-once (§9), so an end-of-stream frame can be
dropped.** Explicit beats inferred, and it does not beat physics: on that
tier a subscriber may simply never see the marker, and a design that
quietly assumed otherwise would have replaced a visible ambiguity with an
invisible one.

What that buys per tier, stated rather than blurred:

- **PUSH/PULL** — at-least-once, so the marker arrives (possibly more
    than once; EOS must therefore be idempotent).
- **PUB/SUB** — best-effort. A subscriber that must not hang on a lost
    marker still needs a timeout; EOS turns the common case from "wait
    forever" into "finish promptly", not from "unreliable" into
    "guaranteed".
- **ring and file** — reliable, because the marker is a flag in shared
    memory or a fact about the file, not a message that can be lost.

That asymmetry is a property of the transports, not of this design, and
the vocabulary is uniform anyway: a caller handles `DP_ERR_EOF` the same
way everywhere and is told which tiers can fail to deliver it.

### Durable completion — one bounded, failure-reporting verb

Each transport answers "did my data land", with the same contract: a
budget the caller chooses, and a result the caller can act on.

Network and disk each have one. Memory does not: `dropped` is a counter,
and a silent overrun is the memory-transport twin of the 500 ms cap that
drops a backlog without telling anyone.

### The return codes mean one thing each

`DP_ERR_EOF`, `DP_ERR_TIMEOUT` and `DP_ERR_INTERRUPTED` mean the same
three things on every transport. That is the whole point of doing this
once: a pipeline stage handles three cases regardless of what it is
reading from, and the characterization measures one mechanism three ways
instead of three mechanisms.

______________________________________________________________________

## 4. A floor that is not ours to move

Time-to-shutdown has a lower bound no library code can lower: **a process
cannot answer a signal before it is running.**

Measured while fixing the C pair test — SIGINT delivered ~0 ms after
`Popen` returns kills the transmitter outright (exit `-2`, three of
three), because it is still in the dynamic linker and has not reached its
`signal()` call. By 5 ms it exits 0, three of three.

Consequences, both load-bearing:

- A shutdown measurement must start its clock **after** the handler is
    installed, and prove that rather than assume it. A harness timing
    from process launch measures startup and labels it shutdown latency.
- An application installs its handler early — before opening transports,
    not after.

______________________________________________________________________

## 5. What is not measured

The quantitative claims across all three transports are prose today. See
[Streaming §11](streaming.md) for the network figures; the same treatment
is owed to the other two:

- the ring's spin cost and hand-off latency under a producer at rate;
- the disk reader's short-read behaviour against a writer still
    appending;
- whether the 100 ms interrupt bound holds **at full rate** on each
    transport, which is the claim a caller reads as a guarantee and the
    one measured only at idle.

______________________________________________________________________

## 6. What this does not do

- **No unified transport type.** There is no `dp_source_t` that a stage
    reads from regardless of backing. Three transports keep three APIs;
    they share a *contract*, not a vtable. A common abstraction is a
    second design, and this one has to work first.
- **No thread-safety change.** The interrupt flag is process-wide, and
    stays so — per-wait interruption is a different feature with a
    different cost.
- **No ordering or delivery guarantees.** Those remain each transport's
    own; see [Streaming §9](streaming.md).
