# Ending a capture — spooling an endless stream to disk while reading it back

[Ending a Wait](io-termination.md) is one contract across three
transports, and two thirds of it is built. This is the third: the disk
half, which §1 of that page ranks the **subtlest of the three, because it
does not hang — it lies**.

**Phase 1–3 built; see §9 for what is proven.** Every number below is a
measurement, not an estimate.

______________________________________________________________________

## 1. The case, and its boundary

```text
   a stream we do not control            we own both of these
   ─────────────────────────      ┌──────────────────────────────────┐
     NATS subject / SDR / socket ─┼─► writer ──► capture file        │
                                  │                  │               │
                                  │                  ▼               │
                                  │            follow reader ──► DSP │
                                  └──────────────────────────────────┘
```

The stream never ends on its own. We take whatever it gives us and spool
it to disk, and concurrently we read the same file back and process it —
faster than it arrives, so the reader spends most of its life waiting.
Nobody will ever tell us the stream is over. Then someone presses Ctrl+C.

**The scope line is the box, and it is what makes this tractable.** What
we do not control is the *stream*. The **writer is ours**, the **reader is
ours**, and the file between them is a private channel with two doppler
processes on it. Following a capture written by something else — a
third-party recorder, a `dd`, another machine — is a different and weaker
problem, and this design deliberately does not solve it. Everything that
follows assumes the box.

That assumption buys three things, and they are the reason to state it
first rather than discover it later:

- **An end-of-capture marker is always available**, because we write it.
    `DP_ERR_EOF` on this transport is *guaranteed*, not best-effort — the
    same standing the ring's `close()` has, and unlike PUB/SUB where the
    marker can be lost.
- **The producer can be asked to flush**, so the reader's view of the
    file is ours to make sample-aligned rather than something to defend
    against.
- **The shutdown is a sequence we author end to end** (§3), rather than
    a reader guessing at a writer that will not answer.

Four requirements:

1. **No hang.** Neither half waits on something that will never happen.
1. **No crash.** Including the quiet kind — a stream that desynchronises
    and hands corrupted samples to the DSP.
1. **No loss.** Data already on disk when the stop arrives is processed.
    A stop is not a discard.
1. **An attributable ending.** The reader exits because something *told*
    it to, never because the file went quiet.

______________________________________________________________________

## 2. Why the naive loop does not work

The obvious implementation is to call the existing `read()` in a loop
until it returns something. It fails twice, both silently — and both
failures are properties of `read()`, not of the file.

**A reader that once reaches EOF never reads again.** C stdio latches the
end-of-file indicator: after `fread` has hit it, further calls return 0
without looking at the file until `clearerr()` or a reposition. Measured:

```text
file has 2 samples; reader drains it; 2 more samples are appended
   same Reader after growth : 0 samples
   fresh Reader on same file: 4 samples
```

In §1's topology the reader is faster than the stream, so it catches up
within milliseconds — and then reports a clean end-of-capture forever.

**A partial sample is consumed and discarded.** `read()` asks for `max`
and lets `fread` return what it can, then does `n = got / stride` — the
remainder bytes are already out of the stream and are dropped. The next
read resumes one `int16` out of phase and every sample after it is wrong.

### Both dissolve rather than needing defences

This is the part worth carrying, because the first draft of this page got
it wrong and built three mechanisms for it. **`follow_available()` seeks
and divides**: it reports whole samples, so the read is never asked for a
partial one, and the seek clears the latched EOF flag as a side effect of
asking. Neither hazard reaches the follow path.

So §2 is not "two defects that defeat any design"; it is why the
availability check is seek-based and expressed in samples. The tell that
this was over-engineered: of four sabotage runs against the follow path,
only the marker went red — three mechanisms were covering one fault and
none of them was pinned.

**The partial-sample fix is kept, on `read()`, where it is real.**
Sabotage-verified there. Its actor is not our writer — measured, 3601
samples of a growing capture, **zero** non-sample-aligned sizes, because
stdio buffers in powers of two and every doppler sample size divides them
— but a capture truncated mid-sample by a killed recorder, which is what
`wfm_reader_get_trailing_bytes` exists to report.

______________________________________________________________________

## 3. The shutdown sequence — the interrupt is not the reader's exit

The intuitive answer is wrong. Ctrl+C sets the flag, both halves observe
it, both stop — that satisfies requirement 1 and **violates requirement
3**: the writer holds buffered samples and the file holds samples the
reader has not reached, so a reader that returns the moment the flag is
set discards a tail already safely on disk, by an amount that varies per
run because it is a race between two loops.

The ending propagates *through* the file instead:

```text
  SIGINT
    ├─► WRITER: upstream wait returns DP_ERR_INTERRUPTED
    │           stop pulling
    │           flush            ← makes the tail visible, sample-aligned
    │           close            ← patches data_size: the MARKER
    │
    └─► READER: keeps draining while whole samples remain
                sees the marker, drains to it
                returns 0 with ending = "eof"     ← "reader stopped"
```

Three rules, each a decision:

**Drain always wins.** A follow read with whole samples available returns
them *even when a stop has been requested*. This is the ring's rule —
`close()` "does not discard what was already written" — extended from
close to interrupt, because on disk the two arrive in that order.

**The reader's normal exit is the marker, not the flag.** The interrupt
makes the *writer* close; the marker tells the *reader*. A reader exiting
on the flag is racing the shutdown, not participating in it.

**Both waits are unbounded by default; the escape is the stop, not a
budget.** The stream has no rhythm we control, so any finite budget fires
during an ordinary quiet patch and reports an ending that has not
happened. A caller may bound either; most will not. This also puts the
disk reader alongside the ring, whose `wait()` has no timeout either —
io-termination fixed its unbounded spin by giving it a *flag to check*.

| `ending`        | meaning                                     | tail          |
| --------------- | ------------------------------------------- | ------------- |
| `"eof"`         | the writer closed and said so               | complete      |
| `"interrupted"` | stop requested, a **bounded** grace expired | may be short  |
| `"timeout"`     | a **bounded** wait expired                  | more may come |

The last two are opt-in. With the defaults only `"none"` and `"eof"` are
reachable, which is why the loop needs no check.

**The cost of the unbounded grace**, stated rather than glossed: a writer
that died *before* the stop leaves a wait nothing ends, and the Ctrl+C
that would have escaped has been spent on a sticky flag with no second
meaning. The fix belongs to the interrupt primitive — a second request
while one is pending means "stop now" — and is tracked with
[#977](https://github.com/doppler-dsp/doppler/issues/977) rather than
worked around here.

______________________________________________________________________

## 4. The three verbs

### 4a. The marker — `data_size`, and why the transition rather than the value

[#972](https://github.com/doppler-dsp/doppler/issues/972) rules out "read
until the declared count" because the count is a *promise* while the file
is open. That is right about the **value** and leaves the **event** on the
table: a BLUE writer patches `data_size` at `close()`, so a reader
re-reading the 512-byte HCB each poll observes `0 → N`, and a change is
not a promise — it is the writer having finished, observed.

Because the writer is ours (§1), this is a marker we are guaranteed to
get. It costs nothing to write: `close()` already patches the field.

The alternative — a reserved `DP_EOS` extended-header keyword — carries
something the transition cannot: *why* the capture ended. Measured, our
three endings are byte-identical:

```text
size known       : 4096 samples, keywords: {}
app stop         : 4096 samples, keywords: {}
interrupted+with : 4096 samples, keywords: {}
```

A capture cut short by an operator is a complete record of a truncated
stream; one cut short by a failed writer has a hole in it. The transition
says *finished*; only a marker says *which*. **Not built** — §10.

**Validate, never trust the field.** The HCB patch is eight bytes and not
atomic against a concurrent reader, so a torn `data_size` reads as "not
yet" and the next poll succeeds — a 100 ms delay rather than a misparse.

**Raw and CSV never end, and the reason is discardability rather than a
lack of room.** A followed capture must be one we wrote as BLUE. That is a
constraint on the caller, not a hazard to defend against, because the
writer is ours.

The tempting fix is to give them a marker anyway, and it is worth saying
why it is refused rather than merely absent. Raw and CSV *do* have
somewhere to put one: both already get a `<path>.sigmf-meta` sidecar
carrying the `fs`/`fc`/`t0` their containers cannot hold. A marker could
go there — and it would not be a guarantee, because a sidecar is a
**second file**. It is opt-out (`sidecar=false`, for a downstream whose
glob an extra file would break) and it separates from its data under an
ordinary move, copy or glob. Writing the marker *inline* is worse: raw has
no framing, so those bytes are indistinguishable from samples and a reader
would return them as data.

So the ending stays unavailable rather than becoming best-effort. That is
the same reasoning §1's scope rests on: `DP_ERR_EOF` is a guarantee here
because the marker is in the artifact the reader is **already reading**,
patched in place by the writer that owns it. A marker that usually arrives
is worse than none, because a reader learns to trust it and is then wrong
exactly when the sidecar went missing — which is the "clean finish on a
truncated capture" failure this whole design exists to remove, reintroduced
one layer out.

### 4b. `wfm_writer_flush()` — alignment, not just durability

The writer had no flush verb at all: the only `fflush` was inside
`close()`. Measured consequence — a reader opened against a writer
holding 4096 samples sees **3968**, the shortfall being 15 872 flushed
bytes ÷ 4.

The obvious reason to want a flush is durability. The sharper one is
**alignment**: `write()` emits whole samples, so a flush *between* write
calls leaves the file ending on a sample boundary. That is the producer
side of §2, and it is available precisely because the writer is ours.

`close()`'s error discipline without the finalisation — `ferror()` first,
because a rejected write leaves nothing buffered and `fflush` alone
reports success afterwards.

### 4c. `read_follow()` — the wait

<!-- docs-snippet: skip=API signature summary, not a standalone program -->

```c
size_t wfm_reader_read_follow (wfm_reader_state_t *r, size_t n,
                               float complex *out, size_t max_out);
int    wfm_reader_get_ending  (const wfm_reader_state_t *r);
void   wfm_reader_set_stop_fn (wfm_reader_state_t *r, int (*fn) (void));
```

**Zero means the capture ended.** With an unbounded wait the call does not
come back for "not yet", so an empty result is unambiguous and the loop is
the one every reader doctest already uses:

<!-- docs-snippet: skip=the follow loop's shape; needs a live writer on another thread -->

```python
while len(block := r.read_follow(4096)):
    consume(block)
```

Three shapes here were forced rather than chosen, and phase 2/3 found each
of them:

- **The return is a count, not a status code.** A `variable_output` method
    has no way to raise — `error` needs `status_return` or `error_negative`,
    and the C return is already the count the binding sizes the result with
    (doppler#938). So `ending` is a property.
- **It takes `count`, sized by the caller's ask.** jm's binding calls
    `<m>_max_out` to size the output array *before* the kernel runs, so a
    hook answering "how many are available now" hands a blocking read a
    zero-length buffer at exactly the moment it should wait.
- **The stop predicate is injected, not linked.** A capture reader has no
    business depending on the process interrupt primitive; hard-wiring it
    would put `dp_interrupt.c` on the link line of every consumer of
    `wfm_reader_core`. doppler passes `dp_interrupted`, a test passes its
    own, `NULL` never stops. The slice is the `DP_INTERRUPT_LATENCY_*`
    **macro**, so it costs no link dependency either.

______________________________________________________________________

## 5. The threading contract

With both waits unbounded, threading stops being advice: an infinite wait
on the wrong thread is a permanent deadlock.

|            | thread  | waits on                | owns                             |
| ---------- | ------- | ----------------------- | -------------------------------- |
| **writer** | its own | the uncontrolled stream | `wfm_writer_state_t`, the `FILE` |
| **reader** | its own | the file                | `wfm_reader_state_t`             |
| **stop**   | any     | —                       | the interrupt state              |

- **The writer and the follow reader must not share a thread.** A blocking
    follow read starves the writer that would produce the data it waits
    for. The API cannot prevent it; the shipped example is threaded.
- **Neither object is shared.** The only things crossing are the interrupt
    state and the file. No locks are introduced.
- **The file is the queue**, and the only synchronisation is the
    filesystem — which is why §4a validates the header field rather than
    trusting it.
- **Signal delivery is not ours to steer.** POSIX picks an arbitrary
    unblocked thread, which is safe only because the handler assigns to a
    `volatile sig_atomic_t`. Nothing may depend on *which* thread noticed.
- **Install the handler before either thread starts** (io-termination §4
    measured the window at ~5 ms).
- **`nogil` is correctness here, not throughput.** A follow read holding
    the GIL across an unbounded wait stops the Python writer thread from
    producing what it waits for. Declared.

______________________________________________________________________

## 6. What this does not do

- **No policy about when to stop.** The application owns that.
- **No inference of an ending from quiet.** A slow, a stalled and a
    finished writer are one observation; only the marker separates them.
- **No support for a writer that is not ours.** §1. A foreign recorder
    has no marker we can read, so `DP_ERR_EOF` would become best-effort
    and the shutdown sequence would have no writer to interrupt. Out of
    scope, deliberately, and the scope line is what keeps `DP_ERR_EOF` a
    guarantee.
- **No unified source type.** io-termination §6 already refuses one.
- **No recovery.** An unmarked capture is *reported*, not repaired.

______________________________________________________________________

## 7. What was unknown, and what it measures at

`native/validation/wfm_follow_keepup.c`, run in full by `make validate-c`
and as a fast subset by `ctest`. This is also the disk bullet of
[#975](https://github.com/doppler-dsp/doppler/issues/975), which
io-termination §5 has carried as prose across all three transports.

**Does the reader keep up?** Backlog — whole samples still readable the
instant a read returns — is that question as a number, and the harness
provisions the reader at, above and *below* the producer's rate so the
metric is shown to detect the bad case:

```text
  w/rnd  r/rnd  verdict     written  backlog_max  backlog_end    wait_ms
  1      2      keeps up      65536            0            0       40.6
  1      1      keeps up      65536            0            0        0.0
  2      1      behind       131072        65536        65536        0.0
  4      1      behind       262144       196608       196608        0.0
```

A reader at or above the producer's rate ends at backlog 0. Under-
provisioned, backlog grows without bound and **the file absorbs it
silently** — the disk transport's one hidden failure mode, and the reason
this number is worth having: unlike a ring there is no `dropped` counter,
no overrun, and nothing that ever says so.

The first draft of this harness drained to empty every round, so backlog
was identically zero in all four rows. A metric that cannot show the bad
case measures nothing; the rate ratio is what makes it real.

**What does the poll slice cost?** `wait_ms` tracks the budget exactly and
is paid *only when starved* — a reader with work never sleeps. So the
slice bounds the stop, and costs throughput only on an idle stream. Left
at `DP_INTERRUPT_LATENCY_DEFAULT_MS`; §10 records the knob.

Still unmeasured: **what a bounded grace would have to be**. It is a
policy nobody has needed yet, and the default is unbounded.

______________________________________________________________________

## 7b. Not instrumented, and the constraint that is now gone

Phase 6 would normally add telemetry taps for exactly the two quantities
above. It was tried and reverted, and the reason turned out not to be about
this object *or* about telemetry.

`dp_tlm_core.h` includes `buffer/buffer.h`, which defined `_GNU_SOURCE`
**at its own line 51** — after any libc header the translation unit had
already reached. So a TU that included `<stdio.h>` first got `syscall`
undeclared, and putting telemetry in `wfm_reader_core.h` exported that
include-ORDER constraint to every consumer of the reader;
`bench_wfm_reader_core.c` failed to compile on the spot. A forward
declaration avoided the include, but declaring the `dp_tlm` dependency makes
`jm apply` inject the header anyway — the same problem through the front
door.

That was read at the time as "telemetry doesn't work here". It was
[#986](https://github.com/doppler-dsp/doppler/issues/986): the macro was a
no-op in the common case, and `-std=gnu99` was quietly supplying the two
functions via `_DEFAULT_SOURCE`, so the line looked like what made
`buffer.h` work while contributing nothing. Feature-test macros are on the
compile line now, and on the exported target, so **the constraint no longer
exists** and phase 6 is a free choice rather than a refusal.

It is still not taken here. A harness that owns both ends computes backlog
and `wait_ms` directly, which is what §7 does, and the taps would add a
`dp_tlm` dependency to the reader for numbers nothing is currently reading
at runtime. That is a decision about value, and it is reversible; the
sentence it replaces was a decision about a bug, and it was not.

The reader carries no serializable state triplet either, and that is the
existing decision for this object rather than one this design makes: its
running state is a file offset, and resuming means reopening a file.

______________________________________________________________________

## 8. The Python stop path, and how it was closed

The interrupt flag used to be per-extension-module
([#976](https://github.com/doppler-dsp/doppler/issues/976)): static linkage
duplicates a file-static and CPython imports extensions `RTLD_LOCAL`, so
`Interrupt().interrupt()` set a flag in one `.so` while a wait in another
read a different one.

**It never blocked the C face**, which is one archive and one copy — §9's
tests exercise the stop path and always did. It blocked the *Python* face
exactly as described: a stop raised in `doppler.interrupt` did not reach a
follow read in `doppler.wfm`.

`process_global = true` (just-buildit/just-makeit#1117, jm 0.67.0) is the
generated fix and names doppler#976 as its motivating case. It is adopted,
and **this section's own prediction about the cost was wrong**, which is
worth recording because the wrong version is the intuitive one.

It said `dp_interrupt` "is not a jm component … making it one is a file move
plus every includer", and treated that migration as the prerequisite. No file
moved. `process_global` attaches to a **component**, and
`dp_interrupt_guard` — the object face declared in phase 2 — already is one;
its core carries `dp_interrupt.c`'s objects. Declaring the flag on the guard
was a manifest key plus two accessors.

What *was* load-bearing is a different thing entirely, and neither #976 nor
#977 predicted it: **the link declaration**. jm generates the rendezvous only
into modules whose link line it wrote, so every consumer had to move from a
raw `$<TARGET_OBJECTS:dp_interrupt_obj>` splice to
`depends_on { link = true }`. A module jm cannot see keeps its own flag
silently — the original defect, one level up. `wfm_reader` is one of them,
declared for exactly this reason.

[#977](https://github.com/doppler-dsp/doppler/issues/977) proposed going
further — an owned heap `dp_interrupt_t` handed to each wait. It is closed:
the declarative route removes the duplication structurally, with no API
change and nothing to migrate, and a per-wait `set_interrupt` would give
every caller a new silent failure mode of the same family (an object nobody
attached an interrupt to is a wait nobody can stop).

**What the binding had to do.** `read_follow()` takes an injected predicate
(§4c) rather than calling `dp_interrupted` itself, so that the core does not
drag `dp_interrupt.c` onto the link line of every C consumer. Nothing was
injecting one on the Python side, so a follow read had no escape at all. The
binding now installs `dp_interrupted` at construction — the binding is the
layer that *can* link it, and it is what `wfm_reader_core.h`'s own example
says doppler passes.

**A stop still does not, by itself, end the read**, and that is the design
rather than a gap. `follow_grace_ms` defaults to 0 = forever, so a stopped
reader waits for the writer's marker indefinitely: the shutdown propagates
*through* the file (§4a), and the reader joins it rather than racing it. A
caller that wants a bounded stop sets a grace; `test_wfm_reader_follow.py`
pins both halves.

______________________________________________________________________

## 9. What is built, and what is proven

Phase 3 complete. `make build` and `ctest` green.

```text
2a  first=4 after-growth=6  ending='none'          PASS
2b  [(1,2), (3,4), (5,6), (7,8)]                   PASS
EOF read=8 ending='eof' in 0.20s                   PASS
TMO len=0 ending='timeout' in 0.25s                PASS
```

The phase-1 prototype (throwaway, scratch) ran §3's sequence under real
concurrency with both waits unbounded — ~3.1 M samples, bit-exact, three
runs — and proved each rule load-bearing by removing it: no whole-sample
reads desynchronises, no drain-wins loses ~800 samples of tail, no marker
hangs until the test's own clock stops it.

**The C tests are the coverage, not a stand-in for it.** doppler is
C-first and the whole mechanism lives in C: the poll loop, the marker, the
two clocks, drain-wins. The Python face is thin glue over it and is gated
as glue (phase 5), not as a second copy of this.

The §3 shutdown rules are testable here with no interrupt primitive
anywhere, which is what `wfm_reader_set_stop_fn` is for: the test supplies
its own predicate. Nothing in phase 4 waits on
[#976](https://github.com/doppler-dsp/doppler/issues/976).

**Sabotage is also what made this page shorter.** Of four sabotage runs
against the follow path only the marker went red; the rest were mechanisms
covering for each other, and two were deleted. Every surviving test fails
when its fix is removed:

| test                                                 | sabotage                                  |     |
| ---------------------------------------------------- | ----------------------------------------- | --- |
| `test_read_never_consumes_a_partial_sample`          | drop the `fseek`-back in `read_block`     | RED |
| `test_follow_ends_on_the_marker_not_on_silence`      | never observe the `data_size` transition  | RED |
| `test_follow_drains_before_honouring_a_stop`         | check the stop before draining            | RED |
| `test_follow_distinguishes_timeout_from_interrupted` | report one ending for both budgets        | RED |
| `test_flush_makes_samples_observable`                | drop the `fflush`                         | RED |
| `test_follow_resumes_after_catching_up`              | — pins behaviour; no independent sabotage |     |

______________________________________________________________________

## 10. Open, with leans

- **The `DP_EOS` reason keyword** (§4a). *Lean: build it* — the three
    endings are otherwise the same file, and a consumer acts differently
    on "operator stopped" than on "writer failed".
- **What a plain `read()` reports on an unclosed capture.** A `SIGKILL`'d
    BLUE capture with 524 160 samples on disk reads back **0**, because
    `data_size` is still the placeholder and `apply_hcb` enforces it as a
    bound. *Lean: yield the samples and expose the ending as a property.*
    The one place this design would touch existing behaviour.
- **A second interrupt escalates** (§3). Belongs to #977.
- **`follow_slice_ms`** — currently the interrupt macro; §7's first
    unknown decides whether it needs to be settable.

______________________________________________________________________

## 11. Where this sits in the lifecycle

Per [Adding an algorithm](../dev/contributing/adding-algorithms.md):

| #   | phase      | state                                                                                                                                     |
| --- | ---------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | Why        | this page; prototype run (§9)                                                                                                             |
| 2   | Declare    | **done** — `read_follow`, `ending`, the budgets, `flush`; `make drift-check` green                                                        |
| 3   | Implement  | **done** — §9                                                                                                                             |
| 4   | Pin        | **done** — six C tests, five sabotage-proven; this is the coverage (C-first)                                                              |
| 5   | Bind       | **done** — the binding installs `dp_interrupted`; `test_wfm_reader_follow.py` covers the stop, the drain, the marker and a bounded budget |
| 6   | Instrument | **declined, and now freely** — §7b: the `_GNU_SOURCE` constraint that forced it is fixed (#986)                                           |
| 7   | Explore    | **done** — `wfm_follow_keepup`, `make validate-c` + a `ctest` subset; closes #975's disk bullet                                           |
| 8   | Certify    | no transport in this contract has a validation report yet                                                                                 |
| 9   | Document   | **partial** — `native/examples/spool_follow_demo.c` runs the topology and self-validates                                                  |
| 10  | Land       | CHANGELOG; §10's deferrals filed                                                                                                          |
