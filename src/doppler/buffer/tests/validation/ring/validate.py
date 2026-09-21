"""The ring buffer — certification evidence, through the binding.

Run directly to regenerate `results.md` and the CSVs:

    uv run python src/doppler/buffer/tests/validation/ring/validate.py

`--check` re-renders in memory and diffs against the committed bytes;
`make validate` writes, `make validate-check` checks. Every limit this
records is asserted by `src/doppler/buffer/tests/test_validation_limits.py`,
which runs this same `build(write=False)`.

The order is the campaign's, not this file's: `native/inc/buffer/buffer.h`
was read first and every claim in its prose mapped onto
`native/tests/test_buffer_core.c`; the uncovered ones got C tests, each
proven by sabotage; only then was this written. That matters more for the
ring than for most objects, because its hardest claims cannot be reached
from Python at all. A ring is a contract between TWO threads about two
indices, and the GIL serialises exactly the interleavings that contract is
about; the file-backed form and `wait_status()` have no Python face. Those
are reported C-ONLY with the C section that carries them, rather than this
report quietly certifying the half of the ring that is easiest to reach.

One object, three widths. `F32Buffer`, `F64Buffer` and `I16Buffer` are one
macro over three element types, so every measurement here runs on all
three and the report says where they differ (nowhere it should).
"""

from __future__ import annotations

import os
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from doppler.buffer import F32Buffer, F64Buffer, I16Buffer
from doppler.interrupt import Interrupt
from doppler.tests._validation_common import Report, cli

HERE = Path(__file__).resolve().parent
DATA = HERE / "data"

R = Report()

IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
SEED = 20260921

#: Random-walk length per width. Long enough for ~150 wraps of a 1024
#: ring, short enough to run on every push.
WALK_OPS = 20_000
#: Samples moved across two threads per width.
THREAD_TOTAL = 800 * 512  # whole frames, or the count cannot match
THREAD_FRAME = 512  # THREAD_TOTAL above is 800 of these
#: How long a stop may take to reach a blocked wait(), and how many times
#: it is asked. The ceiling is generous on purpose: the limit is "it
#: arrives", not "it is fast" -- a loaded CI runner must not fail it.
STOP_AFTER_S = 0.05
STOP_TRIALS = 5
STOP_CEILING_S = 5.0


def _ramp_c(dtype):
    def make(start: int, n: int) -> np.ndarray:
        return np.arange(start, start + n).astype(dtype)

    return make


def _ramp_iq(start: int, n: int) -> np.ndarray:
    x = np.zeros(n, dtype=IQ16)
    # int16 wraps; the walk compares modulo 2**15 on every width.
    x["i"] = np.arange(start, start + n) % 32768
    return x


def _key(v: np.ndarray) -> np.ndarray:
    """A sample's stream position, modulo what an int16 can carry."""
    raw = v["i"] if v.dtype.names else v.real
    return np.asarray(raw).astype(np.int64) % 32768


@dataclass(frozen=True)
class Width:
    name: str
    cls: type
    dtype: np.dtype
    nbytes: int
    ramp: object


WIDTHS = (
    Width("f32", F32Buffer, np.dtype(np.complex64), 8, _ramp_c(np.complex64)),
    Width(
        "f64", F64Buffer, np.dtype(np.complex128), 16, _ramp_c(np.complex128)
    ),
    Width("i16", I16Buffer, IQ16, 4, _ramp_iq),
)


@dataclass
class Data:
    page: int = 0
    cap_rows: list = field(default_factory=list)
    cap_ok: dict = field(default_factory=dict)
    bad_sizes_refused: dict = field(default_factory=dict)
    wrap_cases: dict = field(default_factory=dict)
    wrap_ok: dict = field(default_factory=dict)
    view_ok: dict = field(default_factory=dict)
    walk: dict = field(default_factory=dict)
    refusal_clean: dict = field(default_factory=dict)
    eos_ok: dict = field(default_factory=dict)
    consume_ok: dict = field(default_factory=dict)
    strict_ok: dict = field(default_factory=dict)
    threads: dict = field(default_factory=dict)
    stop_latency: list = field(default_factory=list)
    stop_all: bool = False
    stop_usable: bool = False


# ── 1 ──────────────────────────────────────────────────────────────────────


def section_object() -> None:
    R.md("## 1. The object")
    R.md()
    R.md(
        "A lock-free single-producer / single-consumer ring of complex "
        "samples over a double-mapped region, in three widths. What it "
        "promises and why is "
        "[The Ring Buffer](../../../../../../docs/design/ring-buffer.md); "
        "what was measured on the way, and the guesses that were wrong, is "
        "its [measurements page]"
        "(../../../../../../docs/design/ring-buffer-measurements.md). The "
        "contract itself is the prose in `native/inc/buffer/buffer.h`, "
        "pinned by `native/tests/test_buffer_core.c` and, for the "
        "element-typed face, `native/tests/test_<w>_buffer_core.c`."
    )
    R.md()
    R.md(
        "This report measures the **Python face** of that contract on all "
        "three widths, and names — rather than skips — what that face "
        "cannot reach."
    )
    R.md()


# ── 2 ──────────────────────────────────────────────────────────────────────


def _capacity(d: Data) -> None:
    R.md("### 2.1 Capacity is what you asked for, on every machine (C §sizes)")
    R.md()
    d.page = os.sysconf("SC_PAGESIZE")
    asks = (1, 3, 96, 511, 1000, 1024, 1025, 65537)
    rows = []
    for w in WIDTHS:
        ok = True
        for ask in asks:
            with w.cls(ask) as b:
                cap, room = b.capacity, b.space
                # The slack in the mapping is never room: fill it, and one
                # more sample is refused.
                took = b.write_some(w.ramp(0, ask + 7))
                full = b.write(w.ramp(0, 1)) is False
            ok = ok and cap == ask and room == ask and took == ask and full
            d.cap_rows.append((w.name, ask, cap))
        d.cap_ok[w.name] = ok
        rows.append(
            [w.name, ", ".join(f"{a:,}" for a in asks), "yes" if ok else "NO"]
        )
        refused = 0
        for bad in (0,):
            try:
                w.cls(bad)
            except ValueError:
                refused += 1
        d.bad_sizes_refused[w.name] = refused == 1
    R.table(
        ["width", "capacities asked", "each is exactly what it holds"], rows
    )
    R.md()
    R.md(
        f"Page size on this machine: {d.page:,} bytes — and it does not "
        "appear in the table, which is the point. Any size from 1 up is a "
        "ring of exactly that many samples; a power of two is not "
        "required. What is rounded is the **mapping** behind it, up to a "
        "power of two (indexing is a mask) and to whole pages (the mirror "
        "is built from them), and that slack is address space, never "
        "room: a ring of 1,000 takes 1,000 and refuses the next sample. "
        "The mapping itself is not visible from this face; its size, its "
        "minimality and the unmap are pinned in C (§sizes, §destroy)."
    )
    R.md()


def _wrap(d: Data) -> None:
    R.md("### 2.2 A frame across the wrap is one contiguous array (C §wrap)")
    R.md()
    R.md(
        "The reason the pages are mapped twice. For each width the ring is "
        "advanced to every one of 64 evenly spaced positions, a frame that "
        "straddles the physical end is written, and the lent view is "
        "checked for contiguity, for content, and for being the ring's own "
        "memory rather than a copy."
    )
    R.md()
    rows = []
    for w in WIDTHS:
        with w.cls(1024) as b:
            cap = b.capacity
            frame = cap // 2
            straddled = 0
            ok = view_ok = True
            pos = 0
            for k in range(64):
                target = (k * cap) // 64 + cap // 128
                step = (target - pos) % cap
                if step:
                    b.write(w.ramp(0, step))
                    b.wait(step)
                    b.consume()
                    pos = (pos + step) % cap
                straddled += pos + frame > cap
                b.write(w.ramp(1000 + k, frame))
                v = b.wait(frame)
                ok = (
                    ok
                    and v.flags["C_CONTIGUOUS"]
                    and v.shape == (frame,)
                    and v.dtype == w.dtype
                    and np.array_equal(_key(v), _key(w.ramp(1000 + k, frame)))
                )
                view_ok = (
                    view_ok
                    and v.base is b
                    and not v.flags.owndata
                    and not v.flags.writeable
                )
                b.consume()
                pos = (pos + frame) % cap
        d.wrap_cases[w.name] = straddled
        d.wrap_ok[w.name] = ok
        d.view_ok[w.name] = view_ok
        rows.append([w.name, "64", str(straddled), "yes" if ok else "NO"])
    R.table(["width", "positions", "straddling the end", "all intact"], rows)
    R.md()


def _walk(d: Data) -> None:
    R.md("### 2.3 The accounting, against a model (C §space, §write_some)")
    R.md()
    R.md(
        f"A seeded random walk of {WALK_OPS:,} operations per width — "
        "`write`, `write_some`, `peek` + `consume` of random sizes — run "
        "beside a model that is nothing but two integers. After every "
        "operation the ring must agree with it: `available + space == "
        "capacity`; `write` refuses exactly when the block exceeds "
        "`space`, and `dropped` grows by exactly the refused length; "
        "`write_some` takes exactly `min(n, space)`; and every sample "
        "read back is the next one of the stream. An external truth, "
        "deliberately: comparing one entry point against another is "
        "blind to a defect they share."
    )
    R.md()
    rows = []
    rng = np.random.default_rng(SEED)
    for w in WIDTHS:
        with w.cls(1024) as b:
            cap = b.capacity
            wrote = read = dropped = 0
            refusals = partials = bad = 0
            for _ in range(WALK_OPS):
                op = rng.integers(0, 3)
                n = int(rng.integers(1, cap + cap // 4))
                room = cap - (wrote - read)
                if op == 0:
                    got = b.write(w.ramp(wrote, n))
                    if got != (n <= room):
                        bad += 1
                    if got:
                        wrote += n
                    else:
                        dropped += n
                        refusals += 1
                elif op == 1:
                    k = b.write_some(w.ramp(wrote, n))
                    if k != min(n, room):
                        bad += 1
                    partials += k < n
                    wrote += k
                else:
                    have = wrote - read
                    n = min(n, cap)
                    v = b.peek(n)
                    if (v is None) != (n > have):
                        bad += 1
                    if v is not None:
                        if not np.array_equal(_key(v), _key(w.ramp(read, n))):
                            bad += 1
                        take = int(rng.integers(1, n + 1))
                        b.consume(take)
                        read += take
                if (
                    b.available != wrote - read
                    or b.available + b.space != cap
                    or b.dropped != dropped
                ):
                    bad += 1
            d.walk[w.name] = (bad, wrote, refusals, partials, wrote // cap)
        rows.append(
            [
                w.name,
                f"{wrote:,}",
                f"{wrote // cap:,}",
                f"{refusals:,}",
                f"{partials:,}",
                str(bad),
            ]
        )
    R.table(
        [
            "width",
            "samples through",
            "wraps",
            "refused writes",
            "partial write_some",
            "disagreements",
        ],
        rows,
    )
    R.md()


def _refusal(d: Data) -> None:
    R.md("### 2.4 A refused write takes nothing and disturbs nothing (C §A)")
    R.md()
    for w in WIDTHS:
        with w.cls(1024) as b:
            cap = b.capacity
            b.write(w.ramp(0, cap - 10))
            before = _key(b.peek(cap - 10)).copy()
            refused = b.write(w.ramp(5000, 11)) is False
            same = np.array_equal(_key(b.peek(cap - 10)), before)
            counts = (b.available, b.space) == (cap - 10, 10)
            fits = b.write(w.ramp(cap - 10, 10)) is True
            whole = np.array_equal(_key(b.peek(cap)), _key(w.ramp(0, cap)))
            d.refusal_clean[w.name] = (
                refused and same and counts and fits and whole
            )
    R.md(
        "With 10 samples of room, an 11-sample block is refused on every "
        "width; the readable samples, both counts and the 10 free slots "
        "are as they were, and a block that does fit then completes the "
        "ring exactly. The free slots themselves are inspected in C, "
        "before they are refilled — this face cannot see memory it has "
        "not been lent."
    )
    R.md()


def _eos(d: Data) -> None:
    R.md("### 2.5 Not yet, never, and the end (C §end of stream)")
    R.md()
    rows = []
    for w in WIDTHS:
        with w.cls(1024) as b:
            cap = b.capacity
            ok = b.peek(8) is None  # not yet
            for call in (b.wait, b.peek):  # never: names both numbers
                try:
                    call(cap + 1)
                    ok = False
                except ValueError as e:
                    ok = ok and str(cap + 1) in str(e) and str(cap) in str(e)
            b.write(w.ramp(0, 300))
            b.close()
            ok = ok and b.closed
            ok = ok and len(b.wait(256)) == 256  # a closed ring still drains
            b.consume()
            for call in (b.wait, b.peek):  # 44 left, 256 asked: the end
                try:
                    call(256)
                    ok = False
                except EOFError:
                    pass
            ok = ok and np.array_equal(_key(b.peek(44)), _key(w.ramp(256, 44)))
            b.consume()
            b.write(w.ramp(0, cap))
            b.write(w.ramp(0, 1))  # refused: counted
            kept = b.dropped
            b.reset()
            ok = ok and (b.closed, b.available, b.space) == (False, 0, cap)
            ok = ok and b.dropped == kept == 1
            ok = ok and b.write(w.ramp(0, 4))  # usable again
        d.eos_ok[w.name] = ok
        rows.append([w.name, "yes" if ok else "NO"])
    R.table(["width", "every answer as documented"], rows)
    R.md()
    R.md(
        "`None` is *not yet* and nothing else. A request larger than the "
        "ring is a `ValueError` carrying both numbers, from `wait()` and "
        "`peek()` alike; a closed ring hands back every whole frame it "
        "still holds and only then raises `EOFError`, after which the tail "
        "is still readable; `reset()` empties **and reopens**, keeping "
        "`dropped`."
    )
    R.md()


def _release(d: Data) -> None:
    R.md("### 2.6 The release is bounded, and defaults to what was lent")
    R.md()
    for w in WIDTHS:
        with w.cls(1024) as b:
            cap = b.capacity
            b.write(w.ramp(0, 10))
            ok = True
            for too_many in (11, cap + 1, 1_000_000):
                try:
                    b.consume(too_many)
                    ok = False
                except ValueError:
                    pass
            ok = ok and (b.available, b.space) == (10, cap - 10)
            # The write that used to leave the mapping is an ordinary refusal.
            ok = ok and b.write(w.ramp(0, 4 * cap)) is False
            b.wait(10)
            b.consume()  # what was lent
            ok = ok and b.available == 0
            try:
                b.consume()  # nothing on loan
                ok = False
            except RuntimeError:
                pass
            b.write(w.ramp(0, 8))
            starts = []
            while (v := b.peek(4)) is not None:
                starts.append(int(_key(v)[0]))
                b.consume(1)  # a hop under the frame
            ok = ok and starts == [0, 1, 2, 3, 4]
        d.consume_ok[w.name] = ok
    R.md(
        "`consume(n)` with `n` past `available` raises `ValueError` and "
        "releases nothing, so the two counts go on describing a ring. "
        "With no argument it releases what `wait()` or `peek()` lent; with "
        "nothing on loan that is a `RuntimeError`. A hop of 1 under a "
        "frame of 4 reads frames starting at 0, 1, 2, 3, 4 — overlapped "
        "frames are a release smaller than the loan, nothing more."
    )
    R.md()


def _strict(d: Data) -> None:
    R.md("### 2.7 Inputs are refused, never coerced")
    R.md()
    rows = []
    for w in WIDTHS:
        with w.cls(1024) as b:
            good = w.ramp(0, 16)
            wrongs = {
                "wrong dtype": (np.zeros(16, dtype=np.float32), TypeError),
                "2-D": (np.zeros((8, 2), dtype=w.dtype), ValueError),
                "strided": (np.zeros(32, dtype=w.dtype)[::2], ValueError),
            }
            if w.name == "i16":
                wrongs["bare int16"] = (np.zeros(16, np.int16), TypeError)
                wrongs["int16 (n, 2)"] = (
                    np.zeros((8, 2), np.int16),
                    TypeError,
                )
            ok = True
            for bad, exc in wrongs.values():
                for call in (b.write, b.write_some):
                    try:
                        call(bad)
                        ok = False
                    except exc:
                        pass
            ok = ok and (b.available, b.dropped) == (0, 0)
            ok = ok and b.write(good) is True
        d.strict_ok[w.name] = ok
        rows.append([w.name, str(len(wrongs) * 2), "yes" if ok else "NO"])
    R.table(["width", "refusals tried", "all refused, nothing taken"], rows)
    R.md()
    R.md(
        "A ring exists to avoid copies, so an input that would need "
        "casting, flattening or compacting is an error rather than a "
        "silent allocation — and for `I16Buffer` a bare `int16` array, "
        "flat or `(n, 2)`, is not an array of samples."
    )
    R.md()


def _threads(d: Data) -> None:
    R.md("### 2.8 Two threads, and an end")
    R.md()
    rows = []
    for w in WIDTHS:
        b = w.cls(8192)
        block = w.ramp(0, THREAD_FRAME)
        errors: list[BaseException] = []

        def producer(b=b, block=block, errors=errors):
            try:
                sent = 0
                while sent < THREAD_TOTAL:
                    while b.space < THREAD_FRAME:
                        pass
                    b.write(block)
                    sent += THREAD_FRAME
            except BaseException as e:
                errors.append(e)
            finally:
                b.close()  # so the consumer ends instead of spinning

        t = threading.Thread(target=producer)
        got = wrong = 0
        t0 = time.perf_counter()
        t.start()
        try:
            while True:
                v = b.wait(THREAD_FRAME)
                wrong += int(
                    _key(v)[0] != 0 or _key(v)[-1] != THREAD_FRAME - 1
                )
                got += THREAD_FRAME
                b.consume()
        except EOFError:
            pass
        dt = time.perf_counter() - t0
        t.join()
        tail = b.available
        d.threads[w.name] = (got + tail, wrong, b.dropped, len(errors))
        rows.append(
            [
                w.name,
                f"{got + tail:,}",
                str(wrong),
                str(b.dropped),
                f"{(got + tail) / dt / 1e6:.1f}",
            ]
        )
        b.destroy()
    R.table(
        ["width", "samples across", "frames wrong", "refused", "MSa/s"],
        rows,
    )
    R.md()
    R.md(
        "The producer waits for `space` and closes in `finally`; the "
        "consumer blocks in `wait()` and ends on `EOFError`. The rate "
        "column is this machine's and the GIL's, not the ring's — C moves "
        "two orders of magnitude more (measurements §6) — and it is "
        "reported, not gated."
    )
    R.md()


def _stop(d: Data) -> None:
    R.md("### 2.9 A stop reaches a wait that nothing will ever satisfy")
    R.md()
    b = F32Buffer(1024)
    raised = 0
    with Interrupt([]) as stop:
        for _ in range(STOP_TRIALS):
            # The fallback is what keeps a REGRESSION from hanging this
            # report: if the stop never arrives, close() ends the wait in
            # EOFError instead, and that trial simply does not count.
            rescue = threading.Timer(STOP_CEILING_S, b.close)
            rescue.start()
            threading.Timer(STOP_AFTER_S, stop.interrupt).start()
            t0 = time.perf_counter()
            try:
                b.wait(512)
            except KeyboardInterrupt:
                raised += 1
                d.stop_latency.append(time.perf_counter() - t0 - STOP_AFTER_S)
            except EOFError:
                pass
            rescue.cancel()
            stop.resume()
            b.reset()
        b.write(np.zeros(4, dtype=np.complex64))
        d.stop_usable = len(b.wait(4)) == 4
    b.destroy()
    d.stop_all = raised == STOP_TRIALS
    worst = max(d.stop_latency) if d.stop_latency else float("nan")
    R.table(
        [
            "trials",
            "ended in KeyboardInterrupt",
            "worst latency after the ask",
        ],
        [[str(STOP_TRIALS), str(raised), f"{1e3 * worst:.1f} ms"]],
    )
    R.md()
    R.md(
        "`wait()` spins in C with the GIL released, so no Python flag can "
        "end it; the process-wide interrupt can, from a guard constructed "
        "in a *different* module. The ring is usable afterwards."
    )
    R.md()


def characterise() -> Data:
    R.md("## 2. Characterisation")
    R.md()
    d = Data()
    _capacity(d)
    _wrap(d)
    _walk(d)
    _refusal(d)
    _eos(d)
    _release(d)
    _strict(d)
    _threads(d)
    _stop(d)
    if R.write:
        DATA.mkdir(exist_ok=True)
        with (DATA / "capacity.csv").open("w", encoding="utf-8") as fh:
            fh.write("width,asked,capacity\n")
            for name, ask, cap in d.cap_rows:
                fh.write(f"{name},{ask},{cap}\n")
    return d


# ── 3 ──────────────────────────────────────────────────────────────────────


def review(d: Data) -> None:
    R.md("## 3. Findings, with verdicts")
    R.md()
    R.find(
        "F1",
        "FIXED",
        "`consume()` was unbounded, and from this face that was a crash: "
        "`buf.consume(1_000_000)` followed by a large `write()` killed the "
        "interpreter with SIGSEGV. Past `available` the read position "
        "overtook the write position, `space` came out larger than "
        "`capacity`, `write()` believed it, and the copy left the mapping. "
        "The release now refuses (`DP_ERR_INVALID`; here `ValueError`) and "
        "releases nothing, and `write()` / `write_some()` never copy more "
        "than `capacity` whatever the indices say. Bounding it was "
        "measured free — the `wait()` before every release has already "
        "loaded the producer's index (measurements §6; §2.6 here; C "
        "§consume).",
    )
    R.find(
        "F2",
        "FIXED",
        "A file-backed ring recreated at a different size was **not** "
        "zeroed, though `create_backed()` says it is: on POSIX the resize "
        "was a single `ftruncate()`, which keeps the old bytes, so "
        "`existed = 0` was reported over the previous ring's samples. "
        "Found by the first test the file-backed family ever had at the "
        "ring's own level; now cut to zero and regrown, as the Windows "
        "path always did (C §B).",
    )
    R.find(
        "F3",
        "FIXED",
        "Two header claims were stale: `wait()` told callers to size a "
        "block from `dp_<t>_capacity`, a function that does not exist "
        "(`->capacity` is a field), and `create_backed()` said it returns "
        "NULL on Windows, where it is implemented. And two claims were "
        "pinned only at literals: `reset()`'s \"both positions return to "
        'zero" was asserted as counts, which any `head == tail` satisfies, '
        'and a refused `write`\'s "nothing is copied" was asserted as '
        "counts too, with the ring's contents never inspected (C §A, §C).",
    )
    R.find(
        "F4",
        "C-ONLY",
        '`space()` and `available()` "never go stale in the unsafe '
        'direction" — the guarantee that lets a caller size a block from '
        "them with the other side running. The GIL serialises exactly the "
        "interleavings this is about, so a Python stress would pass "
        "whatever the C did. Certified in C: two threads each sizing every "
        "call from its own count, across several hundred wraps, with +1 on "
        "either count taking it red (C §D).",
    )
    R.find(
        "F5",
        "C-ONLY",
        "The file-backed ring (`create_backed`, `existed`, `sync`), "
        "`wait_status()` and its precedence, and the element-typed `_view` "
        "face have no Python face of their own — the binding is generated "
        "*over* the last of these. Certified in `test_buffer_core.c` §B and "
        "§wait_status, and `test_<w>_buffer_core.c`.",
    )
    R.find(
        "F6",
        "BY DESIGN",
        "`dropped` counts samples in **refused** writes, not samples lost: "
        "a refusal copies nothing and the caller still holds the block "
        "(§2.3, §2.4). A producer that retries inflates it while losing "
        "nothing. The name invites the other reading, and both faces' "
        "docstrings say so.",
    )
    R.find(
        "F7",
        "BY DESIGN",
        "`wait()` has no timeout and spins. It is for a consumer with a "
        "core to spend, ended by `close()` or an interrupt (§2.8, §2.9); a "
        "caller that is its own producer uses `peek()`, which never "
        "blocks.",
    )
    R.find(
        "F8",
        "GAP",
        "A re-attached file-backed ring starts empty over its old samples: "
        "the read and write positions live in the struct, not in the file, "
        "so resuming a history is the caller's bookkeeping and there is no "
        "API for it. Whether position-addressed access and "
        "`restore(head, tail)` belong in the ring is open — "
        "[#1425](https://github.com/doppler-dsp/doppler/issues/1425).",
    )


# ── 4 ──────────────────────────────────────────────────────────────────────


def limits(d: Data) -> None:
    R.md("## 4. Limits")
    R.md()
    R.md(
        "Claims a caller may rely on. A failure here is a regression, "
        "not a new finding."
    )
    R.md()
    for w in WIDTHS:
        n = w.name
        R.limit(
            d.cap_ok[n],
            f"{n}: capacity is exactly what was asked, for sizes from 1 to "
            f"65,537 including ones that are not a power of two, and the "
            f"ring holds that many samples and not one more",
        )
        R.limit(
            d.bad_sizes_refused[n],
            f"{n}: a ring of nothing (capacity 0) is a ValueError",
        )
    for w in WIDTHS:
        n = w.name
        R.limit(
            d.wrap_ok[n] and d.wrap_cases[n] >= 16,
            f"{n}: a lent frame is one contiguous 1-D array of the width's "
            f"element at every position, including the "
            f"{d.wrap_cases[n]} that straddle the end of the ring",
        )
        R.limit(
            d.view_ok[n],
            f"{n}: a lent view is the ring's own memory, read-only, and "
            f"keeps the ring alive",
        )
    for w in WIDTHS:
        n = w.name
        bad, _wrote, refusals, partials, wraps = d.walk[n]
        R.limit(
            bad == 0,
            f"{n}: over {WALK_OPS:,} random operations and {wraps:,} wraps "
            f"the ring never disagrees with a two-integer model — counts, "
            f"refusals, `dropped`, partial writes and sample order",
        )
        R.limit(
            refusals >= 100 and partials >= 100 and wraps >= 50,
            f"{n}: that walk was not vacuous — {refusals:,} refused writes, "
            f"{partials:,} partial ones, {wraps:,} wraps",
        )
    for w in WIDTHS:
        n = w.name
        R.limit(
            d.refusal_clean[n],
            f"{n}: a refused write changes neither the readable samples "
            f"nor either count, and the ring still completes exactly",
        )
        R.limit(
            d.eos_ok[n],
            f"{n}: None is only *not yet*; n > capacity is a ValueError "
            f"naming both numbers; a closed ring drains whole frames then "
            f"raises EOFError; reset() reopens and keeps `dropped`",
        )
        R.limit(
            d.consume_ok[n],
            f"{n}: consume() refuses more than is readable and releases "
            f"nothing, defaults to what was lent, and a smaller release "
            f"overlaps frames",
        )
        R.limit(
            d.strict_ok[n],
            f"{n}: a wrong dtype, rank or stride is refused by write() and "
            f"write_some() alike, and takes nothing",
        )
    for w in WIDTHS:
        n = w.name
        moved, wrong, dropped, errors = d.threads[n]
        R.limit(
            moved == THREAD_TOTAL
            and wrong == 0
            and dropped == 0
            and errors == 0,
            f"{n}: {THREAD_TOTAL:,} samples cross two threads whole and in "
            f"order, none refused, ended by close()",
        )
    R.limit(
        d.stop_all and d.stop_usable,
        f"a process-wide interrupt ends a wait() nothing will ever satisfy, "
        f"{STOP_TRIALS} times out of {STOP_TRIALS}, and the ring is usable "
        f"afterwards",
    )


def build(write: bool = True) -> Report:
    global R
    R = Report(write=write)
    R.md("# The ring buffer — validation report")
    R.md()
    section_object()
    d = characterise()
    review(d)
    limits(d)
    R.executive(
        "The ring buffer",
        [
            "**`capacity` is the number you passed, on every machine.** Any "
            "size from 1 up, a power of two or not; the rounding a mask and "
            "a page mirror need happens in the mapping, where it costs "
            "address space and nothing per call. A request past it is a "
            "`ValueError` naming both numbers rather than a wait that never "
            "ends (§2.1, §2.5).",
            "**The contract holds against a model, not just against "
            "itself**: 20,000 random operations per width, hundreds of "
            "wraps, zero disagreements in counts, refusals, `dropped` or "
            "sample order (§2.3).",
            "**Certifying it found a crash.** An unbounded `consume()` let "
            "two lines of Python take the interpreter down; it is bounded "
            "now, at no measured cost, and the write path no longer trusts "
            "the indices with a `memcpy` (F1, §2.6).",
            "**A refusal is not a loss.** `write()` refuses whole and "
            "touches nothing, `dropped` counts what was refused, and the "
            "caller still holds it; `write_some()` is the one that takes a "
            "part (§2.3, §2.4, F6).",
            "**The hardest claims are C-only, and this report says so.** "
            "The two-thread index guarantee cannot be tested through the "
            "GIL, and the file-backed ring has no Python face; both are "
            "certified in C, where one of them turned out to be wrong "
            "(F2, F4, F5).",
        ],
    )
    R.summary("\n- Raw capacities: `data/capacity.csv`")
    R.emit(HERE / "results.md")
    return R


if __name__ == "__main__":
    sys.exit(cli(build, HERE))
