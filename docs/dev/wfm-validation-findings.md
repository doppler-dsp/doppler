# wfm / wfmgen validation findings

**Status:** all three bugs found resolved (in #196, individually linked
below); their `xfail` markers are gone from the suite. The **behaviour
notes** are not bugs and remain open documentation gaps by design. This
page is kept as the historical record of the validation pass.

This document records what the exhaustive validation pass
(`src/doppler/wfm/tests/test_dsp_correctness.py`,
`src/doppler/wfm/tests/test_api_surface.py`) uncovered. Each **bug** maps 1:1 to
an `@pytest.mark.xfail(reason="…#anchor", strict=True)` in the suite, so the
tests stay green while the gap is tracked; `strict=True` means a later fix that
makes the check pass will fail CI until the marker is removed (forcing the
follow-up PR to clean up). **Behaviour notes** are not bugs — they document
surprising-but-intended behaviour that the validation clarified, and feed the
documentation gap-closure (Part C).

Per the agreed policy for this pass: **document & file only** — no C/binding
fixes here. Fixes land in a follow-up PR.

______________________________________________________________________

## Bugs

### pn-default-poly — `PN()` default polynomial is degenerate

> **✅ Resolved in #196** (closes #191) — `PN()` with `poly` omitted/0 now
> auto-selects the MLS primitive polynomial, matching `Synth(pn_poly=0)`. The
> xfail is removed; `TestPN::test_default_poly_is_maximal_length` is now a
> positive regression test.

**Symbol:** `doppler.wfm.PN`
**Severity:** medium (silent wrong output — a non-maximal sequence)
**xfail:** `TestPN::test_default_poly_is_maximal_length`

**Expected.** `wfm.pyi` documents `poly: int = 96` as the constructor default,
and `PN`'s own docstring/doctest shows a maximal-length sequence for `length=7`
(`chips[:8] == [1,0,0,0,0,0,1,1]`, 64 ones per period 127).

**Observed.** `PN(seed=1, length=7).generate(127)` (poly omitted) returns a
**degenerate** sequence: a single `1` followed by all zeros (sum = 1), i.e. the
LFSR runs with no feedback (effective `poly = 0`).

<!-- docs-snippet: skip=pre-#196 bug output; PN() now auto-selects MLS -->

```python
>>> import doppler.wfm as w
>>> int(w.PN(seed=1, length=7).generate(127).sum())        # omitted poly
1
>>> int(w.PN(poly=96, seed=1, length=7).generate(127).sum())          # works
64
>>> int(w.PN(poly=w.mls_poly(7), seed=1, length=7).generate(127).sum())  # works
64
>>> int(w.PN(poly=0, seed=1, length=7).generate(127).sum())  # confirms poly=0
1
```

**Suspected root cause.** The binding default for `poly` (when the kwarg is
absent) is `0`, not `96`, and the standalone `PN` core treats `poly = 0`
literally (no feedback) rather than auto-selecting a primitive polynomial. By
contrast `Synth(pn_poly=0)` *does* auto-select an MLS polynomial — so the two PN
entry points disagree on what `poly = 0` means. Reference:
`native/src/wfm/wfm_*` PN core + the `pn` object binding in
`native/src/wfm/wfm_ext_pn.c`.

**Workaround (today).** Always pass an explicit primitive polynomial:
`PN(poly=w.mls_poly(length), …)`.

**Fix options (follow-up PR).** Either (a) make the binding default `poly`
actually `96` (or `mls_poly(length)`), or (b) make the `PN` core treat
`poly = 0` as "auto-select MLS for `length`", matching `Synth(pn_poly=0)`. Option
(b) is the more consistent contract. Either fix should also align the `.pyi`
docstring.

### cli-output-dash — `wfmgen --output -` writes a file named `-`, not stdout

> **✅ Resolved in #196** (closes #192) — `wfmgen --output -` now writes to
> stdout. The xfail is removed; the check is now a positive regression test.

**Symbol:** `wfmgen` CLI (`--output`/`-o`)
**Severity:** low (documented stdout idiom silently writes a stray file)
**xfail:** `TestCLI::test_output_dash_is_stdout`

**Expected.** `docs/guide/wfmgen.md:396` states "(or `-`) it prints to stdout".

**Observed.** `wfmgen --type tone --count 256 --output -` creates a file
**literally named `-`** in the cwd (2048 bytes) and writes nothing to stdout.

<!-- docs-snippet: no-exec=historical record of the pre-#196 bug; the fix makes this output obsolete -->

```console
$ wfmgen --type tone --count 256 --sample-type cf32 --output - >/dev/null
$ ls -l ./-          # a stray 2048-byte file appears
```

The **omitted**-`--output` form *does* go to stdout correctly:

```console
$ wfmgen --type tone --count 256 --sample-type cf32 > out.iq   # 2048 bytes, OK
```

**Suspected root cause.** `native/src/app/wfmgen.c` treats the `-` argument as an
ordinary output path (`fopen("-")`) instead of special-casing it to `stdout`.

**Workaround (today).** Omit `--output` entirely to stream to stdout.

**Fix options (follow-up PR).** Either special-case `--output -` to `stdout` in
`wfmgen.c` (matching the docs), or drop the `-` claim from the docs and rely on
the omitted-`--output` stdout default. The former matches common CLI convention.

### streamsink-stream-dtype-gap — `doppler.stream` can't decode `ZmqSink` cf32/ci16/ci8

> **✅ Resolved in #196** (closes #193) — `doppler.stream` now decodes all six
> `dp_sample_type_t` wire types (cf32/ci16/ci8 added). The xfail is removed and
> `test_streamsink_cf32_decodes_in_stream` is now a positive round-trip test.
> `ZmqSink` (named at the time this bug was observed) was later renamed to
> `StreamSink` when the ZMQ transport backend was removed in favor of NATS;
> the finding below is preserved as originally written.

**Symbols:** `doppler.wfm.ZmqSink` ↔ `doppler.stream.Subscriber`/`Pull`
**Severity:** high (the **default** `ZmqSink` sample type is undeliverable to the
Python receiver — a C transmitter cannot talk to a Python subscriber for the
common case, contra the "shared wire formats" architecture rule)
**xfail:** `TestStreamSinkAndClock::test_streamsink_cf32_decodes_in_stream`

**Expected.** `ZmqSink` and `doppler.stream` share one wire enum
(`dp_sample_type_t`, `native/inc/stream/stream.h:84`):

```
CI32 = 0, CF64 = 1, CF128 = 2, CI8 = 3, CI16 = 4, CF32 = 5
```

The C sink emits the correct code for every type (`wfm_sink.c:91-112`,
`WT_CF32 → CF32` etc.), so a `Subscriber` should decode any of them.

**Observed.** The Python `doppler.stream` binding only implements **three** of
the six members — it exposes `CI32`/`CF64`/`CF128` and `recv` raises
`ValueError("Unknown sample_type: N")` for the other three. Measured over a live
`ipc://` PUB→SUB round-trip:

| `ZmqSink` sample_type | wire code | `Subscriber.recv`                    |
| --------------------- | --------- | ------------------------------------ |
| `cf32` (**default**)  | 5         | `ValueError: Unknown sample_type: 5` |
| `cf64`                | 1         | OK (`complex128`)                    |
| `ci32`                | 0         | OK (`int32` interleaved)             |
| `ci16`                | 4         | `ValueError: Unknown sample_type: 4` |
| `ci8`                 | 3         | `ValueError: Unknown sample_type: 3` |

So the most common path — `wfmgen --output zmq://…` (cf32 by default) consumed by
a `doppler.stream` subscriber — silently fails on the receive side.

**Suspected root cause.** The receive/decode table in the `stream` CPython
binding (`native/src/stream/*` — the `recv` sample-type switch) and its exported
module constants cover only `CI32`/`CF64`/`CF128`; `CF32`/`CI16`/`CI8` were never
added on the Python side even though the C enum and the C senders
(`dp_pub_send_cf32`/`_ci16`/`_ci8`) support them.

**Workaround (today).** Publish from `ZmqSink` with `sample_type="cf64"` (or
`"ci32"`) when the consumer is `doppler.stream`; reserve cf32/ci16/ci8 for file
containers (`Writer`/`read_iq` handle all five).

**Fix options (follow-up PR).** Teach the `stream` receive binding the full
`dp_sample_type_t` enum (add the `CF32`/`CI16`/`CI8` decode arms + export the
constants) so every wire type round-trips. This is a `stream`-module fix, not a
`wfm` one. Removing the strict xfail is the signal that it landed.

______________________________________________________________________

## Behaviour notes (not bugs — documentation gaps)

### compose-repeat-unbounded — `Composer.compose()` never returns on a repeat/continuous spec

**Symbols:** `doppler.wfm.Composer(repeat=True | continuous=True)`
**Covered by:** `TestComposerGraph::test_to_dict_and_json_roundtrip` (asserts the
flag round-trips through JSON, but composes the *finite* spec).

`compose()` materialises the **entire** stream into one array. A `repeat=True`
(loop the sequence) or `continuous=True` (never stop) timeline has no bounded
length, so `compose()` on such a spec loops forever / grows without bound. This
is **intended** — `repeat`/`continuous` exist for the streaming faces
(`stream()`, `execute(n)`, the CLI `--continuous`), which pull bounded blocks —
but it is an easy trap and is undocumented on the `compose()` method.

```python
>>> import doppler.wfm as w
>>> c = w.Composer([w.Segment("tone", freq=1e5, num_samples=128)], repeat=True)
>>> c.to_dict()["repeat"]      # the flag is set and round-trips through JSON
True
>>> # c.compose()             # DON'T: unbounded, never returns
>>> blocks = []                # DO: pull bounded blocks instead
>>> for i, b in enumerate(c.stream(block=128)):
...     blocks.append(b)
...     if i == 3: break
>>> sum(len(b) for b in blocks)
512
```

**Action (Part C):** document that `compose()` requires a finite (non-`repeat`,
non-`continuous`) spec, and point repeating/streaming users to `stream()` /
`execute(n)`, in `docs/design/wfmgen-composition.md` and the `Composer` API docs.

### csv-reader-count — `Reader.num_samples` is 0 for CSV captures

**Symbols:** `doppler.wfm.Reader` (CSV)
**Covered by:** `TestReaderWriter` CSV round-trip (asserts `read()` works).

A `Reader` opened on a CSV capture reports `num_samples == 0` even though
`read(n)` returns the correct samples and they round-trip bit-faithfully. CSV has
no header to give a cheap count, so the reader cannot pre-report the length
without a full scan. This is a **limitation**, not a bug — but the `num_samples`
property silently returning 0 (rather than, say, counting lines) is worth
documenting. The raw/BLUE/SigMF readers report the true count.

### noise-scaling — `level`/`snr` are composition-time gains, not `Synth` gains

**Symbols:** `doppler.wfm.Synth(type="noise")`, `Composer`
**Covered by:** `TestNoise::test_bare_synth_is_unit_power`,
`test_bare_synth_ignores_snr`, `test_composer_applies_level_gain`

A standalone `Synth` is the **raw kernel**. `Synth(type="noise").steps(n)` always
emits **unit complex power** (σ² = 0.5 per quadrature) regardless of `snr`,
`snr_mode`, or `level`:

```python
>>> import numpy as np, doppler.wfm as w
>>> for lvl in (0.0, -20.0):
...     x = w.Synth(type="noise", level=lvl, seed=7).steps(100_000)
...     round(float(np.mean(np.abs(x)**2)), 3)
0.999
0.999
```

The per-segment `level` (dBFS) gain — and, for multi-source segments, the
`snr`-driven noise-floor placement (`native/src/wfm/wfm_resolve.c`) — are applied
by the **`Composer`**, which post-multiplies each source by `10**(level/20)`
(`native/src/wfm/wfm_compose.c`). So scaling only appears once samples flow
through a `Composer`/`Segment`:

```python
>>> c = w.Composer([w.Segment("noise", level=-20.0, num_samples=100_000, seed=7)])
>>> round(float(np.mean(np.abs(c.compose())**2)), 3)
0.01
```

This is **intended architecture** (the synth kernel stays scale-free; the
composer owns amplitude), but it is easy to trip over and is currently
undocumented. **Action (Part C):** document the raw-kernel-vs-composer amplitude
split in `docs/design/wfmgen-composition.md` and the `Synth`/`Composer` API docs,
with the worked numerical example.

### pn-generate-aliasing — `PN.generate()` returns a reused zero-copy buffer

**Symbol:** `doppler.wfm.PN.generate`
**Covered by:** tests `.copy()` every `generate()` result.

`PN.generate(n)` returns a **zero-copy NumPy view over a single reused buffer**;
a second `generate()` call overwrites the first result in place
(`np.shares_memory(a, b)` is `True`). This *is* documented in the method
docstring ("copy the result before calling generate again"), so it is a note, not
a bug — but it is a sharp edge worth surfacing in the gallery/PN docs.

### version-skew-0.17.0 — HEAD carries unreleased public API under 0.17.0

**Symbols:** `doppler.wfm.Synth(bits=…)`, `doppler.wfm.Composer.to_sigmf`
**Surfaced by:** the Python 3.9 e2e container
(`deploy/validation/wfm_e2e.py`) run against the **published**
`doppler-dsp==0.17.0` wheel.

The repo's working tree (version `0.17.0` in `pyproject.toml`) exposes public
API that the **published** 0.17.0 wheel does not: the `Synth(bits=…)`
constructor kwarg and `Composer.to_sigmf`. Running the e2e against the PyPI
wheel fails those two paths (`TypeError: unexpected keyword argument 'bits'`;
`AttributeError: 'Composer' object has no attribute 'to_sigmf'`) — they were
added after the 0.17.0 tag without a version bump, so installing the wheel and
building from a `0.17.0` checkout give **different** public surfaces.

This is a **release-hygiene** note, not a code bug: the next release must bump
the version so the new API ships under a number that advertises it. The e2e
script **feature-detects** both (exercises them when present, degrades to the
`wfmgen` CLI for SigMF otherwise), so it validates the current wheel and the
next release unchanged. **Action:** bump `version` before the next publish.
