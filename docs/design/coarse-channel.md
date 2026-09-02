# CoarseChannel — is a channel an object, or a slice of the bank?

*Question 1 of [`burst-bank.md`](burst-bank.md), opened 2026-09-01. Context,
the trades, and the work that answers it. Nothing here is decided; §5 says
what has to be calculated or measured before it is.*

______________________________________________________________________

## 1. The question

A coarse channel is `DDC(mix −f_k, decimate) → BurstCapture` plus the one
line that turns the capture's folded Doppler into an absolute one. The bank
is `K` of them and a rule for when two of them saw the same target.

Two shapes can produce that:

- **A. The channel is a slice of the bank.** `burst_bank_state_t` holds
    `ddc[K]` and `cap[K]` as internal arrays. A single channel is reachable
    through a factory, `burst_bank_channel_create(descriptor, k)`, which
    builds channel `k`'s DDC and capture from the bank's descriptor and
    hands back an internal channel struct.
- **B. The channel is its own object.** `coarse_channel` is a jm object in
    `[module.dsss]` — header claims, C pins, a state triplet, a report, both
    examples — and `burst_bank` composes `K` of them the way
    `DsssBurstReceiver` composes one `BurstCapture`. `CoarseChannel` in
    Python becomes that object's binding; `Acquirer` becomes `burst_bank`'s.

The Python prototype is shape B by accident: `CoarseChannel` is a class with
its own `get_state`/`set_state`, and `Acquirer` composes it. That is not an
argument for B; it is what makes B cheap to compare against.

## 2. Two primary use cases

Everything below is judged against these two, and only these two. *(These
are the maintainer's two; the reading of them here is the author's and may
need correcting.)*

### 2.1 One process runs the whole bank

A wide-Doppler receiver on one node: `±U` of uncertainty, `K` channels, a
thread per channel or a bounded parallel-for, one blob for the whole bank
when it checkpoints. The caller wants **one object** — create it, push
blocks, read detections and windows, get one blob back. It does not want to
know that a channel exists.

What this use case asks of the shape: that the bank is a single object with
a single `push`, that the per-channel faces (`windows(k)`, `events(k)`,
`release(k, i)`) are reachable without holding `K` handles, and that
checkpoint/restore is one call each.

### 2.2 One pod runs one channel

The autoscaled fleet: the bank's descriptor is shared, and each pod is
handed a channel index. A pod builds **only its channel**, with its ring
file named by the channel's centre on a shared volume, pushes the same
source stream every other pod gets, and reports its detections and windows
tagged with `k`. The cross-channel dedup happens **downstream**, wherever
the fleet's detections meet — or does not happen at all if the consumer
tolerates a boundary target twice.

What this use case asks of the shape: that a channel can be constructed
alone from `(descriptor, k)`; that a channel's blob is restorable into a
channel built alone *and* is the same bytes the bank's blob carries for
that channel (a pod can be spun up from a slice of a bank checkpoint, and a
bank can be rebuilt from `K` pod checkpoints); and that the dedup rule is
callable on detections that did not come out of one `push` — because in
this use case they never do.

That last point is the one that reshapes the bank: **the dedup is a
function over detections, not a step inside `push`.** `burst_bank_push`
calls it; a fleet aggregator calls the same function on what `K` pods sent.
One implementation, two callers — which is the reason the rule moves to C
at all (`burst-bank.md` §1).

## 3. What each shape gives, and what it costs

|                                      | A. slice of the bank                                                                                                                                               | B. its own object                                                                                                                                         |
| ------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| whole-bank caller (§2.1)             | one object, one push, one blob — native                                                                                                                            | one object, one push, one blob — the bank composes; the caller never sees a channel                                                                       |
| single-channel pod (§2.2)            | a factory returning an internal struct; its header claims, tests and report are the bank's, so the pod runs code certified only as part of a bank                  | a certified unit with its own report; `(descriptor, k)` is its constructor                                                                                |
| blob composability                   | the bank's blob carries `K` internal sections; a slice restores into the factory's struct only if the section is the same bytes — a private convention             | the bank's blob is `[hdr][K][child blob]…` with each child self-validating; a slice IS a channel blob, by construction (`dp_state.h`'s composition rule)  |
| the dedup rule                       | a bank method over its own last push                                                                                                                               | a free function over `detection_t[]`; `push` and the aggregator both call it                                                                              |
| lifecycle cost                       | one object: one toml, header, core, test, report, two examples, two benches                                                                                        | two objects — but the bank's own claims shrink to layout + dedup + composition, exactly what `burst_acq`'s "thin forwarder" report certifies in 17 limits |
| Python                               | `Acquirer` binds the bank; `CoarseChannel` becomes a view over an internal struct, or disappears                                                                   | `CoarseChannel` binds `coarse_channel` unchanged in name; `Acquirer` binds the bank; the 24 orchestrator tests split by object                            |
| detector-only mode (`burst_len = 0`) | the bank would carry both a capture array and an acquisition array, or drop the mode                                                                               | the channel carries the choice; the bank never sees it                                                                                                    |
| precedent                            | `DsssBurstReceiver` composes `BurstCapture` (an object), not a slice — and the doctrine that came out of that split is *certify each object, then the composition* | the same precedent, applied once more                                                                                                                     |

Two things the table does not settle and §5 measures:

- **whether the detector-only mode survives.** A capture reports detections
    anyway (`detections()`, built for the bank); what a detector-only channel
    saves is the ring — `retain_span · 8` bytes per channel, ~40 kB at the
    test geometry and ~2.5 MB at an 8029-symbol frame. For a pure detector
    bank of 21 channels at the long frame that is 50 MB of rings nobody
    reads. Keeping the mode costs a second child type in the channel;
    dropping it costs that memory to callers who only wanted Doppler and
    phase.
- **what a channel's own claims are.** If the header ends up saying only
    "forwards to the DDC and the capture", shape B's report is a delivery
    report (`burst_acq`'s F1) and the cost of B is mostly ceremony. If it says
    things the bank cannot — the absolute-Doppler translation, the fold at
    the band edge, the detector/capture choice, the ring's name — B is
    certifying real behaviour.

## 4. The author's expectation

B, for the second use case alone: a pod holding one channel should be
running a certified object whose blob is the slice of the bank's, and the
dedup has to be a function that a fleet aggregator can call. A is the
right answer only if the single-channel pod turns out not to be a primary
use case after all — and §2 says it is.

Held loosely until §5 is done.

## 5. The work that answers it

1. **Enumerate the channel's claims from the Python prototype.** Walk
    `CoarseChannel` in `orchestrator.py` and list every promise it makes
    that is not the DDC's or the capture's: the `f_hz + bin_to_signed(...) ·  res` translation, the `code_phase = epoch mod code_bins` reduction on the
    capture path, the ring path, the `burst_len = 0` choice, the `n_noncoh =  1` pin. Each becomes a header claim of `coarse_channel` (B) or of the
    bank (A). If the list is under five items, A is cheaper; if it is the
    list above, B is certifying something.
1. **Count the lifecycle cost from the tree, not from the table.** Measure
    `burst_acq` — the existing thin-forwarder object — in lines and files:
    header, core, C test, toml, fragment, report, characterization, two
    examples, two benchmarks. That is B's marginal cost for the channel,
    within a factor of two.
1. **Prototype the blob slice.** With the Python bank as it stands: take
    `Acquirer.get_state()`, cut channel `k`'s section out of it by the
    length prefix, `CoarseChannel.set_state()` it into a channel built alone
    from the descriptor, push the rest of a scene, and compare windows
    bit-for-bit against the uninterrupted bank's channel `k`. If that
    already works in Python, the composability B promises is a property of
    the *envelope*, and A can have it too by adopting the same envelope —
    which weakens B's strongest row. If it does not, find out why.
1. **Prototype the fleet dedup.** Run two `CoarseChannel`s at adjacent
    centres over a scene with a target midway, collect both detection lists,
    and apply `Acquirer._dedup` to the concatenation. Then move the target
    across the boundary in `res/4` steps. This is the same measurement
    `burst-bank.md` §9 needs for the sign fold, and it decides whether the
    dedup can be a pure function of `(doppler_hz, code_phase, test_stat)`
    tuples or needs the channel index — a pure function is what §2.2 needs.
1. **Size the rings for the detector-only question.** `retain_span` at the
    two geometries above, times the channel counts a real link needs
    (`ceil(U / 2·span)` at 50 sym/s and at 1 Msym/s — the native span is
    tens of Hz at one end and tens of kHz at the other, so `K` ranges from a
    few to hundreds). If the pure-detector bank at the low-rate end needs
    hundreds of channels, the ring memory decides the mode's fate.

Steps 1, 3 and 4 are an afternoon in Python against the shipped bank. Step
2 is a `wc`. Step 5 is arithmetic on numbers the objects already publish.
