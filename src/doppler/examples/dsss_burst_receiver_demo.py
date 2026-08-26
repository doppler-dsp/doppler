"""dsss_burst_receiver_demo.py — the burst chain as ONE object.

`dsss_burst_pipeline_demo.py` shows `BurstAcquisition`, `BurstDespreader`
and `BurstDemod` driven separately, each on its own, and it is worth reading
first: it is where you see what each stage does. This file is the other
half — the same job through :class:`~doppler.dsss.DsssBurstReceiver`, which
composes **search → refine → demod** behind a single ``push()``.

What the composition buys is not fewer lines. It is that the *hand-off*
stops being the caller's problem:

- acquisition reports an **end** anchor and a code phase modulo one period,
  and neither of those is a burst start. Refine turns them into one;
- one preamble raises several detections, and coalescing them is a rule
  about `refine_span`, not a loop the caller writes;
- a bin→frequency fold that four call sites once restated three mutually
  inconsistent ways is now inside.

Three properties are demonstrated, each with an assertion rather than a
claim — exit 0 means measured, not merely run.

**1. Block size does not change the answer.** The same capture is pushed as
one giant call, as 64k blocks, and as blocks far smaller than a burst. All
three decode the same bursts at the same samples. This is not free: it is
what `dropped == 0` and an internally sliced history ring are for
(doppler#1008 was three separate discard sites that broke exactly this).

**2. A burst split across two calls is held, not lost.** ``pending`` reads
non-zero while the receiver is waiting for the rest of a burst and drops
back to zero when it emits. Read it at the END of a stream: closing a file
while it is set discards a burst that would have decoded, and no other
read-back distinguishes that from an empty capture.

**3. Bursts closer than `refine_span` are coalesced.** That is the minimum
spacing, it is a property rather than a constant, and packing tighter
silently costs bursts rather than erroring.

Two panels
----------
Left
    The capture, with every true burst start marked and every decoded start
    overlaid. They coincide, which is the point.

Right
    Decoded-burst count against push block size, over four decades. A flat
    line is the claim; anything else is doppler#1008 coming back.
"""

import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from doppler.dsss import DsssBurstReceiver
from doppler.wfm import PN, Composer, Segment

# ── geometry ────────────────────────────────────────────────────────────────
# 255 chips at 2 samples/chip is 510 = 2*3*5*17 correlation bins. The length
# is chosen on the TRANSFORM as well as the code: `acq` transforms sf*spc
# verbatim (the code axis is a circular correlation, so it cannot be padded),
# and a 127-chip m-sequence at spc=4 gives 508 = 2^2*127, where pocketfft
# falls back to Bluestein and costs 12x. 255 is smooth AND keeps the ideal
# m-sequence autocorrelation. spc >= 2 always.
ACQ_BITS, DATA_BITS = 8, 5  # 255- and 31-chip m-sequences
REPS, SPC, PAYLOAD = 5, 2, 96
CHIP_RATE = 1.0e6
FS = CHIP_RATE * SPC
ESN0_DB = 12.0
N_BURSTS = 4
SYNC = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], dtype=np.uint8)


def mls(n_stages, seed):
    """An m-sequence from doppler's own PN generator, not a random array."""
    n = 2**n_stages - 1
    return (
        np.asarray(PN(poly=0, seed=seed, length=n_stages).generate(n)) & 1
    ).astype(np.uint8)


acq_code = mls(ACQ_BITS, seed=1)
data_code = mls(DATA_BITS, seed=3)
payload = np.random.default_rng(0).integers(0, 2, PAYLOAD).astype(np.uint8)

ACQ_SF, DATA_SF = len(acq_code), len(data_code)
FRAME_SYMS = len(SYNC) + PAYLOAD + 16  # sync | payload | CRC-16
BURST_LEN = (REPS * ACQ_SF + FRAME_SYMS * DATA_SF) * SPC


# --8<-- [start:receiver]
def receiver():
    return DsssBurstReceiver(
        acq_code=acq_code,
        data_code=data_code,
        sync=SYNC,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        payload_len=PAYLOAD,
        cn0_dbhz=60.0,
        doppler_uncertainty=0.0,
        pfa=1e-3,
        pd=0.9,
        carrier_hz=0.0,
        max_rate=0.0,
        est_segments=10,
    )


# --8<-- [end:receiver]

# --8<-- [start:spacing]
# The spacing is READ from the receiver, not computed here: detections closer
# than `refine_span` are coalesced as one preamble, so a capture that packs
# them tighter loses bursts rather than erroring. A margin over the minimum
# because sitting on an inequality is how a geometry change breaks a demo.
probe = receiver()
REFINE_SPAN, RETAIN_SPAN = probe.refine_span, probe.retain_span
SPACING = REFINE_SPAN + REFINE_SPAN // 5
GAP = SPACING - BURST_LEN
assert GAP > 0, "the geometry cannot fit a gap at this spacing"
# --8<-- [end:spacing]

# ── the capture ─────────────────────────────────────────────────────────────
# One declarative dsss segment per burst: the engine tiles the preamble,
# spreads sync|payload|CRC-16 by the data code, and sizes the on-time itself.
segments = [
    Segment(
        type="dsss",
        fs=FS,
        freq=0.0,
        snr=ESN0_DB,
        snr_mode="esno",  # Es/N0 of the DATA_SF-chip data symbol
        seed=k + 1,
        sps=SPC,  # samples per CHIP
        acq_code=acq_code.tobytes(),
        acq_reps=REPS,
        data_code=data_code.tobytes(),
        sync=SYNC.tobytes(),
        payload=payload.tobytes(),  # CRC-16 appended by the engine
        gap_noise="auto",  # the floor runs through the gaps, as a real capture
        off_samples=GAP if k < N_BURSTS - 1 else RETAIN_SPAN * 2,
    )
    for k in range(N_BURSTS)
]
capture = Composer(segments).compose()
truth = [k * SPACING for k in range(N_BURSTS)]

print(f"capture: {capture.size} samples at {FS / 1e6:.1f} MHz")
print(f"  burst_len   {BURST_LEN:>7}   spacing {SPACING}")
print(
    f"  refine_span {REFINE_SPAN:>7}   (minimum spacing, read from the object)"
)
print(f"  retain_span {RETAIN_SPAN:>7}   (history kept per anchor)")


# --8<-- [start:decode]
def decode(block_size):
    """Push the whole capture in blocks of `block_size`; return starts+bits."""
    rx = receiver()
    starts, bits = [], []
    for off in range(0, capture.size, block_size):
        out = np.asarray(rx.push(capture[off : off + block_size]))
        if out.size:
            bits.append(out)
            starts.extend(int(e[0]) for e in rx.events())
    return (
        starts,
        (np.concatenate(bits) if bits else np.empty(0, np.uint8)),
        rx,
    )


# --8<-- [end:decode]

# ── 1. block size does not change the answer ────────────────────────────────
sizes = [capture.size, 65_536, 8_192, 1_000, 333]
results = {n: decode(n) for n in sizes}

print("\nblock size independence:")
for n, (starts, bits, rx) in results.items():
    label = "whole capture" if n == capture.size else f"{n} samples"
    print(
        f"  {label:>14}: {len(starts)} burst(s), "
        f"{bits.size // PAYLOAD} payload(s), dropped={rx.dropped}"
    )

ref_starts, ref_bits, _ = results[capture.size]
for n, (starts, bits, rx) in results.items():
    assert starts == ref_starts, (
        f"block size {n} found {starts}, the whole-capture push found "
        f"{ref_starts} — push() is not block-size independent (doppler#1008)"
    )
    assert np.array_equal(bits, ref_bits), (
        f"block size {n} decoded different bits"
    )
    assert rx.dropped == 0, f"block size {n} dropped {rx.dropped} samples"

# every burst found, at its exact sample, bit-exact, CRC valid
assert len(ref_starts) == N_BURSTS, (
    f"found {len(ref_starts)} of {N_BURSTS} bursts at {ref_starts}, "
    f"expected {truth}"
)
for got, want in zip(ref_starts, truth):
    assert got == want, f"burst start {got} != {want}"
assert ref_bits.size == N_BURSTS * PAYLOAD
for k in range(N_BURSTS):
    assert np.array_equal(
        ref_bits[k * PAYLOAD : (k + 1) * PAYLOAD], payload
    ), f"burst {k} payload is not bit-exact"
print(f"  -> {N_BURSTS}/{N_BURSTS} bursts, exact samples, payloads bit-exact")

# ── 2. a burst split across two calls is HELD, and pending says so ──────────
print("\nsplit across two push() calls:")
# --8<-- [start:split]
cut = truth[0] + BURST_LEN // 2
rx = receiver()
first = np.asarray(rx.push(capture[:cut]))
held = rx.pending
second = np.asarray(rx.push(capture[cut:]))
print(
    f"  fed {cut} samples (mid-burst): {first.size // PAYLOAD} payload(s), "
    f"pending={held}"
)
print(
    f"  fed the rest:                  {second.size // PAYLOAD} payload(s), "
    f"pending={rx.pending}"
)
assert first.size == 0, "a half-arrived burst must not be emitted"
assert held == 1, "pending must report the burst being held"
assert rx.pending == 0, "pending must clear once the burst is emitted"
assert np.array_equal(second[:PAYLOAD], payload), (
    "the split burst is not exact"
)
print("  -> held, then returned whole. Read pending before you stop feeding.")
# --8<-- [end:split]

# ── 3. closer than refine_span, and they coalesce ──────────────────────────
# Between one burst and one refine_span: the bursts do not overlap, they are
# simply nearer than the window that decides "same preamble". Halving
# refine_span would give a NEGATIVE gap at this geometry, which is a
# different thing entirely (overlapping bursts) and not what this shows.
TIGHT_SPACING = (BURST_LEN + REFINE_SPAN) // 2
assert BURST_LEN < TIGHT_SPACING < REFINE_SPAN
tight = Composer(
    [
        Segment(
            type="dsss",
            fs=FS,
            freq=0.0,
            snr=ESN0_DB,
            snr_mode="esno",
            seed=k + 1,
            sps=SPC,
            acq_code=acq_code.tobytes(),
            acq_reps=REPS,
            data_code=data_code.tobytes(),
            sync=SYNC.tobytes(),
            payload=payload.tobytes(),
            gap_noise="auto",
            off_samples=(TIGHT_SPACING - BURST_LEN)
            if k < N_BURSTS - 1
            else RETAIN_SPAN * 2,
        )
        for k in range(N_BURSTS)
    ]
).compose()
rx_tight = receiver()
tight_bits = np.asarray(rx_tight.push(tight))
n_tight = tight_bits.size // PAYLOAD
print(
    f"\npacked at {TIGHT_SPACING} (inside refine_span {REFINE_SPAN}): "
    f"{n_tight} of {N_BURSTS} bursts decoded"
)
assert n_tight < N_BURSTS, (
    "bursts packed inside refine_span were all decoded — either the "
    "coalescing rule changed or this demo no longer demonstrates it"
)
print("  -> coalesced, as documented. refine_span IS the minimum spacing.")

# ── figure ──────────────────────────────────────────────────────────────────
fig, (ax0, ax1) = plt.subplots(1, 2, figsize=(12, 4.2))

t_ms = np.arange(capture.size) / FS * 1e3
ax0.plot(t_ms, np.abs(capture), lw=0.4, color="0.6")
for i, s in enumerate(truth):
    ax0.axvline(
        s / FS * 1e3,
        color="tab:blue",
        lw=1.1,
        ls="--",
        label="true start" if i == 0 else None,
    )
for i, s in enumerate(ref_starts):
    ax0.axvline(
        s / FS * 1e3,
        color="tab:red",
        lw=1.1,
        ls=":",
        label="decoded start" if i == 0 else None,
    )
ax0.set_xlabel("time (ms)")
ax0.set_ylabel("|x|")
ax0.set_title(
    f"{N_BURSTS} bursts, one push() — every start exact, every CRC valid",
    fontsize=9,
)
ax0.legend(fontsize=8)
ax0.grid(alpha=0.3)

counts = [len(results[n][0]) for n in sizes]
ax1.semilogx(sizes, counts, "o-", color="tab:green")
ax1.axhline(N_BURSTS, color="k", lw=0.8, ls="--", alpha=0.6)
ax1.set_ylim(0, N_BURSTS + 1)
ax1.set_xlabel("push() block size (samples)")
ax1.set_ylabel("bursts decoded")
ax1.set_title(
    "Block size does not change the answer\n"
    f"(smallest block {min(sizes)} is {BURST_LEN / min(sizes):.0f}x shorter "
    "than one burst)",
    fontsize=9,
)
ax1.grid(alpha=0.3, which="both")

fig.tight_layout()
fig.savefig("dsss_burst_receiver_demo.png", dpi=110)
print("\nwrote dsss_burst_receiver_demo.png")
