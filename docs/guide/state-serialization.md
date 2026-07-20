# Checkpoint & Resume

Every stateful doppler object — a filter's delay line, a tracking loop's
phase, a receiver's whole composed state — can hand its running state to a
fresh instance and resume **bit-for-bit**: `blob = a.get_state()`,
`b.set_state(blob)`, and `b`'s next sample is exactly what `a`'s would have
been. This guide is the practitioner's path through it: the basic pattern,
what happens when you get it wrong, how a composed receiver resumes as one
blob, and the payoff it exists for — shipping a running pipeline to another
process or pod mid-stream.

For the mechanics underneath (the envelope, the cursor helpers, the three
serializer shapes, the CI gate that keeps every object honest) see the
[State Serialization design note](../design/state-serialization.md). This
page is about *using* it.

## The pattern

Two things ride in a blob: the object's **running** state, and nothing else.
Construction-time config (tap coefficients, a spreading code, a sample rate)
is **not** in the blob — it comes back from building the fresh instance
identically, the same way `reset()` doesn't forget the taps either.

```python
import numpy as np
from doppler.filter import FIR

taps = np.array([0.1, -0.2, 0.3, 0.6, 0.3, -0.2, 0.1], dtype=np.complex64)
rng = np.random.default_rng(0)
stream = (rng.standard_normal(2048) + 1j * rng.standard_normal(2048)).astype(
    np.complex64
)

# Uninterrupted reference.
ref = FIR(taps)
_ = ref.execute(stream[:1000])
ref_tail = ref.execute(stream[1000:])

# Checkpoint mid-stream; hand the blob to a fresh, identically-built filter.
a = FIR(taps)
_ = a.execute(stream[:1000])
blob = a.get_state()
assert len(blob) == a.state_bytes()

b = FIR(taps)  # same taps — the descriptor, not the blob, carries config
b.set_state(blob)
b_tail = b.execute(stream[1000:])

assert np.array_equal(b_tail, ref_tail)
```

`FIR`'s running state is just its delay line — `taps` never appears in the
blob. That split (descriptor rebuilds config, blob restores only what
changed since) is universal: it's why `set_state` always needs an instance
built with the *same* constructor arguments as the one that produced the
blob, never a blank default.

## It rejects a mismatch, never guesses

`set_state` opens with a validation pass over a 16-byte header (type tag,
format version, size) before touching a single field. A blob for the wrong
object, the wrong size, or with a clobbered header is rejected outright — it
is never silently reinterpreted into garbage state.

<!-- docs-snippet: raises=ValueError -->

```python
c = FIR(taps)
c.set_state(blob[:-1])  # truncated — size no longer matches
```

<!-- docs-snippet: raises=TypeError -->

```python
c = FIR(taps)
c.set_state("not bytes")
```

## Compositions resume as one blob

An object built from other serializable objects (a receiver's carrier loop,
code loop, and matched filter) nests each child as its own self-validating
sub-blob. Calling `get_state`/`set_state` on the parent threads through to
every child in one call — you never assemble a composed receiver's state by
hand.

```python
from doppler.dsss import Despreader

code = (np.arange(31, dtype=np.uint8) & 1).astype(np.uint8)
kw = dict(code=code, sps=2, periods_per_bit=1)

ref = Despreader(**kw)
ref.steps(stream[:1200])
ref.steps(stream[1200:])  # continues past the checkpoint below

d1 = Despreader(**kw)
d1.steps(stream[:1200])
d_blob = d1.get_state()

d2 = Despreader(**kw)  # same descriptor: code, sps, periods_per_bit
d2.set_state(d_blob)
d2.steps(stream[1200:])

# The carrier loop, code loop, and matched filter all resumed together —
# the two receivers' full internal state is byte-identical after the split.
assert d2.get_state() == ref.get_state()
```

`Despreader` doesn't expose its carrier phase or code-loop NCO as separate
properties — the equal-`get_state()` check above *is* the observable, and
it's exactly what
[`src/doppler/tests/test_state_serialization.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/tests/test_state_serialization.py)
asserts for every composed type in the library.

## The payoff: elastic hand-off across pods

The reason this interface exists: a running pipeline is shippable as a
**`(descriptor, state, next_block)`** triple. Rebuild an identical object
from the descriptor anywhere — a different thread, a different process, a
different pod — restore the state blob, and it continues as if it had never
moved.

[`doppler.dsss.orchestrator`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/orchestrator.py)
is this cashed in for real: `CoarseChannel` composes a `DDC` mixer/decimator
with an `Acquisition` search, and `Acquirer` composes a whole bank of
channels for wide-Doppler-uncertainty search. Both checkpoint and resume at
their own level, so a bank mid-search can be handed to a fresh pod without
losing a single in-progress detection:

```python
from doppler.dsss.orchestrator import CoarseChannel

ch_kw = dict(
    source_rate=8.0e6,
    code=np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8),
    reps=4,
    spc=2,
    chip_rate=1.0e6,
    cn0_dbhz=40.0,
    pfa=1e-3,
    pd=0.9,
)
burst_rng = np.random.default_rng(1)
first = (
    burst_rng.standard_normal(4096) + 1j * burst_rng.standard_normal(4096)
).astype(np.complex64) * 0.2
second = (
    burst_rng.standard_normal(4096) + 1j * burst_rng.standard_normal(4096)
).astype(np.complex64) * 0.2

# Uninterrupted reference: one channel runs straight through both blocks.
ref_ch = CoarseChannel(0.0, **ch_kw)
ref_ch.process(first, 0)
ref_hits = ref_ch.process(second, 0)

# Checkpoint after the first block; hand the blob to a channel built fresh
# elsewhere — the "other pod" — from the same descriptor.
live = CoarseChannel(0.0, **ch_kw)
live.process(first, 0)
handoff_blob = live.get_state()

resumed = CoarseChannel(0.0, **ch_kw)  # rebuilt from the descriptor
resumed.set_state(handoff_blob)
resumed_hits = resumed.process(second, 0)

# Detection-for-detection identical to the uninterrupted run.
assert resumed_hits == ref_hits
```

`Acquirer.get_state`/`set_state` do the same thing one level up — a
length-prefixed concatenation of every channel's blob — so a whole
coarse-Doppler bank checkpoints and resumes as one call. See
`test_bank_pod_handoff_resumes_bit_exact` in
[`test_orchestrator.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/dsss/tests/test_orchestrator.py)
for the full-bank version of the same proof.

## What's covered

Every object that carries running state between calls implements this
interface — filters, resamplers, generators, tracking loops, detectors,
correlators, the spectrum analyzer, quantizers, accumulators, and every
composition built from them. A stateless object (a pure format converter, an
FFT plan, a by-value measurement analyzer) is exempt by design — there is
nothing to resume.

This isn't a convention that can quietly rot: `scripts/check_serializable.py`
runs in CI and fails if any `objects/*.toml` entry declares neither
`serializable = "true"` nor a reviewed stateless opt-out. To see the current
roster yourself rather than trust a number that will drift out of date here:

```sh
python scripts/check_serializable.py --list   # anything undeclared (should be empty)
grep -l 'serializable = "true"' objects/*.toml # everything that resumes
```

## See also

- [State Serialization design note](../design/state-serialization.md) — the
    envelope, the cursor helpers, the three serializer shapes, and how to add
    the triplet to a new object.
- [Pure-functional acquisition kernel](../design/acq-fn.md) — why the
    acquisition engine is shaped as `f(config, state_in, input) -> (state_out,   output)`, and how that shape is what makes `orchestrator.py`'s fan-out
    elastic.
- [DSSS Burst Acquisition](dsss-acquisition.md) — the acquisition guide this
    page's orchestrator example builds on.
