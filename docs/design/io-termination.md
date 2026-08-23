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

|             | the wait                                        | how it fails                                                                                                                                                                                                                     | producer-side question                                                                                             |
| ----------- | ----------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| **network** | blocking `recv` inside the NATS client          | keeps the caller out of the loop where the flag is read                                                                                                                                                                          | `dp_pub_flush` / `dp_stream_drain` — the client's own close is best-effort with a 500 ms cap and no failure report |
| **memory**  | `dp_<t>_wait()` in `native/inc/buffer/buffer.h` | **was an unbounded busy-spin** — no timeout, no flag, no producer-done, so a stopped producer left the consumer at 100% CPU with no signal able to rescue it. Fixed: it checks `closed` and the interrupt flag, and returns NULL | the `dropped` counter — an overrun is a field someone might read, not an answer anyone gets. **Still true**        |
| **disk**    | `wfm_reader_read()` returning 0                 | `0` means end-of-file *or* not-written-yet. A live capture cannot tell them apart                                                                                                                                                | the writer's close returns `int` and raises `OSError`                                                              |

Ranked by severity, which is not the order they were built in:

- **Memory is the worst and has no escape at all.** The other two waits
    can at least be abandoned by a signal; a `DP_SPIN_HINT()` loop checks
    nothing, so no handler can rescue it and it burns a core while
    failing.
- **Disk is the subtlest, because it does not hang — it lies.** A
    tail-following reader treats a slow writer as end-of-file, gets a
    short read, and reports a clean finish on a truncated capture. A
    hang is at least visible.
- **Network was the one already fixed**, and it was the model: §8b of
    [Streaming](streaming.md) slices the wait and checks a flag inside
    it. Memory now follows it; **disk is the one still open**, which
    means the failure ranked second-worst here is the one that survives.

______________________________________________________________________

## 2. The use cases

- **A continuous producer that must stop cleanly.** `wfmgen --continuous --realtime` to a subject, a file, or a ring. Ctrl+C must
    end it without losing the tail already handed over
    ([#969](https://github.com/doppler-dsp/doppler/issues/969)).
- **A consumer at rate that must stay responsive.** A dashboard or
    analyzer reading as fast as the transport delivers, still answering
    Ctrl+C.
- **A pipeline stage whose upstream finishes.** The case that motivated
    the page: a consumer must learn that no more data is coming and exit
    0, rather than block, spin, or silently truncate. Network and memory
    serve it now; a disk reader still cannot, and silently truncates.

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

**The second half of that has not happened**, and saying so here is the
point of §0. `dp_interrupt.c` exists and the old names forward to it
verbatim, but nothing has been removed and doppler's own Python binding
still calls the deprecated spellings throughout — so the tree currently
has the two-spellings state this paragraph argues against, reached by
default rather than by decision. Tracked as
[#974](https://github.com/doppler-dsp/doppler/issues/974).

#### The primitive is an OBJECT, and the guard moves into C

Renaming it was half the problem. The other half is that it was never
*declared* — it has no manifest fragment at all, so its CMake object
library is hand-written in the root `CMakeLists.txt`, three modules splice
it into their link lines by hand, its binding lives inside `stream`'s
`no_generate` extension, and it has no `.pyi` of its own. Every symptom
in [#974](https://github.com/doppler-dsp/doppler/issues/974) follows from
that one omission, which is why the fix is phase 2 rather than a delete.

**It cannot be declared as free functions.** A module function's C symbol
*is* its Python name — jm has no `fn` override there, and says so
(`unknown function key 'fn' — it is a method key`). So a `dp_`-prefixed C
API could only present as `doppler.interrupt.dp_interrupt()`, or by
renaming the C to bare `interrupt()` / `resume()`, which pollutes the
global namespace of every C consumer. That is what the prefix exists to
prevent.

**A class escapes both**, and by derivation rather than by override.
Name the component after the C prefix — `dp_interrupt_guard` — and every
symbol jm derives is already the name doppler would have chosen:
`dp_interrupt_guard_create`, `_destroy`, `_interrupt`, `_interrupted`,
`_resume`. `class_name = "Interrupt"` places the Python face. That is the
`dp_tlm` pattern exactly, and it needs no `fn` overrides at all.

It does need three one-line C wrappers, and the reason is worth stating so
it is not mistaken for the aliasing mess above: **jm passes the handle as
a method's first argument** (`dp_tlm_emit_checked(self->handle, …)`),
while the process-wide functions take `void`. So the object face is
`dp_interrupt_guard_interrupt(g)` delegating to `dp_interrupt()`. These
are not a second spelling of the same function the way `dp_stream_*` is —
they have a different signature and a different job: they are the object's
face onto a facility it does not own. The shape:

```python
it = Interrupt()                  # a handle to the process-wide facility
it.interrupt(); it.interrupted(); it.resume()

with Interrupt(sigint=True) as it:   # construction ARMS; exit restores
    while not it.interrupted():
        ...
```

Two things this decides, both deliberate:

- **The flag stays process-wide**, so two instances observe one flag. The
    object is a handle to a facility, not an instance of one, and §6 still
    holds — per-wait interruption is a different feature. What the object
    scopes is the *arming*: which signals this guard installed, and the
    latency it overrode.
- **The guard's logic moves into C.** Today `InterruptGuardObject` lives
    in `stream_ext.c` and does real work there — saving and restoring the
    latency, tracking which handlers it installed, unwinding a partial
    install. That is orchestration in a binding, which the repo forbids
    everywhere else, and it is why the guard has no C test: there is no C
    to test. `dp_interrupt_guard_t` with a create/destroy pair puts it
    where every other lifecycle lives, and `__enter__`/`__exit__` are then
    generated rather than hand-written.

**It is a breaking change, and pre-1.0 is when to take it.**
`doppler.stream.interrupt()` becomes `doppler.interrupt.Interrupt().interrupt()`.
The free functions go: they are the spelling that cannot be declared, and
keeping them would mean keeping the hand binding that this exists to
retire. The C face is untouched — every `dp_*` symbol keeps its name — so
no C caller moves.

The cost worth naming: a declared component owes a benchmark, and `make lint` will require one (jm warns about it on scaffold). The primitive has
a C test and no benchmark today, which nothing notices, because an
undeclared component is invisible to that gate too.

______________________________________________________________________

### End-of-stream — the piece that does not exist

No transport could say "I am done" when this was written. This is the
genuinely new mechanism, and it is what kills the race in all three
cases — two of them now, the third still owed:

- **memory** *(built)* — a producer marks the ring closed; `wait()`
    returns end-of-stream instead of spinning when it is closed and
    drained.
- **disk** *(not built —
    [#972](https://github.com/doppler-dsp/doppler/issues/972))* — a reader
    distinguishes "short read, writer still open" from "end of capture",
    which is what makes tail-following honest.
- **network** *(built)* — an explicit end-of-stream frame, so a subscriber
    learns the sender finished rather than inferring it from silence.

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
    than once; EOS must therefore be idempotent). It reaches **exactly one
    consumer**, which is the caveat below.
- **PUB/SUB** — best-effort. A subscriber that must not hang on a lost
    marker still needs a timeout; EOS turns the common case from "wait
    forever" into "finish promptly", not from "unreliable" into
    "guaranteed".
- **ring and file** — reliable, because the marker is a flag in shared
    memory or a fact about the file, not a message that can be lost.

That asymmetry is a property of the transports, not of this design, and
the vocabulary is uniform anyway: a caller handles `DP_ERR_EOF` the same
way everywhere and is told which tiers can fail to deliver it.

#### On the work queue an ending is a MESSAGE, with both consequences

"At-least-once" was written above as unqualified good news, and building
the tier found two ways it is not. Both follow from one fact — on
PUSH/PULL the marker is an ordinary work-queue message — and neither has
an analogue on the other transports, where the marker is a flag or a
fact.

**It goes to one consumer, not to the pool.** A work queue load-balances,
and an EOS frame load-balances with everything else. So a single
`send_eos()` ends *one* worker and leaves the rest waiting on a stream
that is over. Measured with five consumers racing: twenty frames spread
across all five, one of them told, four silent — and *which* one varies
per run, because the choice is the broker's.

This is the opposite shape from PUB/SUB, and the pairing is worth stating
plainly, because the tier with the *reliable* marker is the one that
cannot broadcast it:

|               | reaches              | can be lost |
| ------------- | -------------------- | ----------- |
| **PUB/SUB**   | every subscriber     | yes         |
| **PUSH/PULL** | exactly one consumer | no          |

Ending a pool of N therefore needs N markers, or a second PUB/SUB subject
carrying the shutdown alongside the work queue carrying the data.
`work_queue_shutdown_demo.py` asserts the single-consumer property rather
than describing it.

**And it must be acked, by the receive path, because nobody else can.**
`dp_sub_recv` reports an ending as a *state* and hands back no
`dp_msg_t` — so there is nothing for a caller to `dp_msg_ack()`. PULL is
an explicit-ack consumer on a WorkQueue stream, where a message is
removed only once acked, so an unacked marker redelivers every `AckWait`
forever *and* stays in the stream: the next run against that subject
opens onto the previous run's ending, a stream told it finished before it
began. Measured, then fixed — the receive path acks it itself.

That the reporting convention (a state, not a frame) is what removes the
caller's ability to ack is the part worth carrying forward. Any future
"report it as a state" decision on an acked transport inherits the same
obligation.

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

Tracked as [#975](https://github.com/doppler-dsp/doppler/issues/975); see
§7. A list of gaps inside a design page is read once, by whoever is
already working on that page.

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

______________________________________________________________________

## 7. The record — resolved and open

This page was written as a proposal and two thirds of it is built. Rather
than rewrite each section into the past tense and lose the reasoning for
why it was decided that way, each decision is listed here with what
became of it.

- **The interrupt primitive is general, and moves down a layer** —
    *resolved, shipped.* `dp_interrupt.c` has no NATS dependency and never
    did; the ring's `wait()` is its second caller and reads the same flag
    the NATS wait does.

- **The `dp_stream_*` spellings are removed, not aliased** — *half done,
    and the half that is missing is the one this page argued for*
    ([#974](https://github.com/doppler-dsp/doppler/issues/974)). The
    rename happened and the old names forward verbatim; nothing has been
    deleted, and doppler's own Python binding still calls the deprecated
    spellings throughout. "Two spellings forever" is the state §3 rejected,
    now reached by default rather than by decision.

- **End of stream is a KIND, not a flag bit** — *resolved, shipped.*
    `DP_KIND_EOS`, with the qualification §3 records: a receiver built
    before it existed *refuses* the frame rather than ignoring it, because
    the validator rejects a zero element size. Additive in the sense that
    matters — no existing frame changes meaning — and not in the sense
    first assumed.

- **On the work queue the marker is an ordinary MESSAGE** — *resolved,
    and it corrected this page.* §3 had recorded PUSH/PULL's at-least-once
    as unqualified good news. Building it found the two consequences that
    follow from the marker being a message: it reaches exactly one
    consumer, so the tier with the *reliable* ending is the one that
    cannot broadcast it; and it must be acked by the receive path, because
    reporting an ending as a *state* is precisely what leaves the caller
    nothing to ack. Both are in §3 now.

- **The ring's wait ends instead of spinning** — *resolved, shipped.*
    `close()` / `closed`, and the wait checks the interrupt flag. One
    branch inside it — the re-load after observing the flag — is reached
    only when the consumer sees `closed` before the write, a window one
    spin iteration wide. It was documented as untestable and is not:
    a single race catches its removal about 5% of the time, so the C test
    runs the race repeatedly instead, which is a gate rather than a flake.

- **A disk reader distinguishes "short read" from "end of capture"** —
    *open* ([#972](https://github.com/doppler-dsp/doppler/issues/972)).
    The third transport, and §1 ranks its failure the subtlest of the
    three because it does not hang — it reports a clean finish on a
    truncated capture.

- **Memory gets a durable-completion verb** — *open.* Network and disk
    each answer "did my data land"; the ring still offers only the
    `dropped` counter, which is a field someone might read rather than an
    answer anyone gets. §3 names the gap and nothing has closed it.

- **The quantitative claims get measured** — *open*
    ([#975](https://github.com/doppler-dsp/doppler/issues/975)). §5's
    list. The sharpest is the 100 ms interrupt bound, which is not prose
    here but `DP_INTERRUPT_LATENCY_DEFAULT_MS` in a public header, with
    "on an idle waiter" doing the work in the sentence — the one case
    nobody needs an interrupt budget for.

- **None of it is certified** — *open.* No transport in this contract has
    a validation report: the header claims below are enumerated nowhere,
    and [Object Validation](../dev/contributing/validation.md) is the
    process that would. The findings above are the raw material for one.
