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

Four properties are demonstrated, each with an assertion rather than a
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

**4. Every read-back is checked against the capture that produced it.**
`events()`'s fields are the object's whole diagnostic surface, and each is
compared here with what the scene says it must be -- the bin width
with `fs / (sf*spc)`, the C/N0 estimate with the Es/N0 the segment was
generated at, the coarse Doppler with its own bin, the refined residual
with a hundredth of it. A read-back nobody checks is a number, not a
measurement.

Four panels
-----------
Top left
    The capture, with every true burst start marked and every decoded start
    overlaid. They coincide, which is the point.

Top right
    Decoded-burst count against push block size, over four decades. A flat
    line is the claim; anything else is doppler#1008 coming back.

Bottom left
    Frequency: what the search grid can promise (half a bin) against what
    refine and demod actually resolve, per burst, on a log axis. The gap is
    three orders of magnitude and it is the reason `est_freq_hz` exists
    alongside `doppler_hz_est`.

Bottom right
    Quality: the C/N0 estimate against the analytic C/N0 of the scene, the
    estimator's own confidence, and the refine margin against the 1.0 it
    must stay under. Every bar is a probe a caller can read at run time.
"""

import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from doppler.dsss import DsssBurstReceiver
from doppler.wfm import PN, Composer, FrameDesc, Segment

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
PAYLOAD_OFF = len(SYNC)  # push() returns the FRAME; the payload is a slice
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
        frame_syms=FRAME_SYMS,
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
        f"{bits.size // FRAME_SYMS} frame(s), dropped={rx.dropped}"
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
assert ref_bits.size == N_BURSTS * FRAME_SYMS
for k in range(N_BURSTS):
    assert np.array_equal(
        ref_bits[k * FRAME_SYMS + PAYLOAD_OFF :][:PAYLOAD], payload
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
    f"  fed {cut} samples (mid-burst): {first.size // FRAME_SYMS} frame(s), "
    f"pending={held}"
)
print(
    f"  fed the rest:                  {second.size // FRAME_SYMS} frame(s), "
    f"pending={rx.pending}"
)
assert first.size == 0, "a half-arrived burst must not be emitted"
assert held == 1, "pending must report the burst being held"
assert rx.pending == 0, "pending must clear once the burst is emitted"
assert np.array_equal(second[PAYLOAD_OFF:][:PAYLOAD], payload), (
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
n_tight = tight_bits.size // FRAME_SYMS
print(
    f"\npacked at {TIGHT_SPACING} (inside refine_span {REFINE_SPAN}): "
    f"{n_tight} of {N_BURSTS} bursts decoded"
)
assert n_tight < N_BURSTS, (
    "bursts packed inside refine_span were all decoded — either the "
    "coalescing rule changed or this demo no longer demonstrates it"
)
print("  -> coalesced, as documented. refine_span IS the minimum spacing.")

# ── 4. every read-back the object offers, for EVERY burst ──────────────────
# --8<-- [start:probes]
# The scalar properties describe the LAST burst only — one push() can
# complete several and one set of scalars cannot speak for all of them.
# `events()` hands back the same fields per burst, and between them they
# are the whole diagnostic surface: where the burst was, what the
# search grid thought its Doppler was and how wide that grid is, what C/N0
# the hit implies, what the estimator refined the residual frequency and
# chirp rate to and how confident it was, how far the winning preamble beat
# its runner-up, and whether the CRC checked out.
_, _, rx_all = results[capture.size]
events = np.asarray(rx_all.events(rx_all.events_max_out()))

# What the scene says each read-back must be. Derived from the geometry
# above, not from a previous run of this script: `acq` transforms sf*spc
# verbatim, so the bin width is fs over that, and a segment generated at
# Es/N0 with a DATA_SF-chip symbol carries C/N0 = Es/N0 + 10log10(Rs).
BIN_HZ = FS / (ACQ_SF * SPC)
CN0_DBHZ_TRUE = ESN0_DB + 10.0 * np.log10(CHIP_RATE / DATA_SF)

print("\nevery read-back, per burst (events(), one row per decoded burst):")
print(
    f"  {'#':>2} {'start':>7} {'dopp_hz':>8} {'res_hz':>8} {'cn0_dBHz':>9} "
    f"{'freq_hz':>8} {'rate_hz':>8} {'conf_dB':>8} {'margin':>7}"
)
for i, e in enumerate(events):
    print(
        f"  {i:>2} {int(e['preamble_start']):>7} "
        f"{e['doppler_hz_est']:>8.1f} {e['doppler_res_hz']:>8.1f} "
        f"{e['cn0_dbhz_est']:>9.2f} {e['est_freq_hz']:>8.2f} "
        f"{e['est_rate_hz']:>8.2f} {e['est_snr_db']:>8.2f} "
        f"{e['refine_margin']:>7.3f}"
    )
print(
    f"  scene: bin {BIN_HZ:.1f} Hz, C/N0 {CN0_DBHZ_TRUE:.2f} dB-Hz "
    f"(Es/N0 {ESN0_DB:.0f} dB at {CHIP_RATE / DATA_SF / 1e3:.1f} ksym/s), "
    "true offset 0 Hz, no chirp"
)

assert events.size == N_BURSTS, "one event per decoded burst, always"
for i, e in enumerate(events):
    where = f"burst {i}"
    # The record must stand on its own: its start is the burst's start.
    assert int(e["preamble_start"]) == truth[i], (
        f"{where}: event says {int(e['preamble_start'])}, burst starts at "
        f"{truth[i]}"
    )
    # The search grid: its width is derived, and its estimate must sit in
    # the bin that contains the truth (0 Hz).
    assert e["doppler_res_hz"] == BIN_HZ, (
        f"{where}: bin width {e['doppler_res_hz']} != fs/(sf*spc) {BIN_HZ}"
    )
    assert abs(e["doppler_hz_est"]) <= BIN_HZ / 2, (
        f"{where}: coarse Doppler {e['doppler_hz_est']:.1f} Hz is outside "
        "the bin containing the true 0 Hz"
    )
    # ...and the whole reason the chain does not stop at acquisition: the
    # refined residual is finer than the grid by orders of magnitude.
    assert abs(e["est_freq_hz"]) < BIN_HZ / 100, (
        f"{where}: refined residual {e['est_freq_hz']:.2f} Hz is no better "
        f"than a hundredth of the {BIN_HZ:.0f} Hz search bin — refine and "
        "demod are not adding anything over the grid"
    )
    # A zero here is a CONFIGURATION fact, not a measurement: max_rate=0
    # switches the chirp axis off. Pass a non-zero max_rate to ask for one.
    assert e["est_rate_hz"] == 0.0, (
        f"{where}: chirp rate {e['est_rate_hz']} Hz/s from a receiver built "
        "with max_rate=0, which does not search that axis"
    )
    # The C/N0 estimate is a LOWER BOUND, so it may not run hot; the scene
    # says what it is bounding.
    assert e["cn0_dbhz_est"] <= CN0_DBHZ_TRUE + 1.5, (
        f"{where}: C/N0 estimate {e['cn0_dbhz_est']:.2f} dB-Hz runs hot "
        f"against the scene's {CN0_DBHZ_TRUE:.2f} dB-Hz — it is documented "
        "as a lower bound, and per-burst estimator spread here is a few "
        "tenths of a dB"
    )
    assert e["cn0_dbhz_est"] >= CN0_DBHZ_TRUE - 3.0, (
        f"{where}: C/N0 estimate {e['cn0_dbhz_est']:.2f} dB-Hz is more than "
        f"3 dB under the scene's {CN0_DBHZ_TRUE:.2f} dB-Hz"
    )
    # `est_snr_db` is the estimator's own peak-to-mean confidence, NOT a
    # link SNR — do not compare it with Es/N0.
    assert e["est_snr_db"] > 10.0, (
        f"{where}: estimator confidence {e['est_snr_db']:.1f} dB — the "
        "winning row barely stood out from its own mean"
    )
    assert 0.0 < e["refine_margin"] < 1.0, (
        f"{where}: refine margin {e['refine_margin']:.3f} — the runner-up "
        "period was not beaten by the winner"
    )

# The scalars are not a second source: they ARE the last row.
last = events[-1]
for name in events.dtype.names:
    assert getattr(rx_all, name) == last[name], (
        f"scalar {name} disagrees with the last event row — the scalars "
        "describe the most recent burst and nothing else"
    )
print(
    f"  -> all {N_BURSTS} rows check out against the scene; the scalar "
    "read-backs equal row -1, which is all they ever claim to be"
)
# --8<-- [end:probes]

# ── 5. the frame is undone one layer up ────────────────────────────────────
# --8<-- [start:deframe]
# The receiver stopped at decisions: `push()` handed back FRAME BITS and no
# opinion about them (doppler#1022). Turning those into a payload — and into
# a verdict — needs the frame's description, which is what `FrameDesc` is.
# One object, built once, describing exactly what the generator built.
empty = np.empty(0, np.uint8)
deframer = FrameDesc(empty, empty, empty)
deframer.add_field(SYNC)  # found, not decoded
deframer.add_field(np.zeros(PAYLOAD, np.uint8))  # the geometry
deframer.add_field(empty, derived_by=1, derived_bits=16)  # the CRC, by stage 0
deframer.add_stage(0, first_field=1, n_fields=2)  # CRC-16 over both
deframer.build()

deframed_ok, payloads = [], []
for k in range(N_BURSTS):
    frame = ref_bits[k * FRAME_SYMS : (k + 1) * FRAME_SYMS]
    got = np.asarray(deframer.deframe(frame))
    deframed_ok.append(deframer.rx_ok == deframer.rx_units == 1)
    payloads.append(got[PAYLOAD_OFF : PAYLOAD_OFF + PAYLOAD])

print("\ndeframed by wfm.FrameDesc (the receiver has no opinion):")
print(
    f"  {sum(deframed_ok)}/{N_BURSTS} frames check out, "
    f"{sum(int(np.array_equal(p, payload)) for p in payloads)}/{N_BURSTS} "
    "payloads bit-exact"
)
assert all(deframed_ok), "every frame's CRC must check out"
for k, got in enumerate(payloads):
    assert np.array_equal(got, payload), f"burst {k} payload is not exact"
print("  -> decide, then deframe. Two objects, one frame, no shared secret.")
# --8<-- [end:deframe]

# ── figure ──────────────────────────────────────────────────────────────────
fig, ((ax0, ax1), (ax2, ax3)) = plt.subplots(2, 2, figsize=(12, 8.4))

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

# ── the read-backs, plotted ────────────────────────────────────────────────
# Frequency, on a log axis because that is the only way the two live on one
# plot: the search grid can promise half a bin and the estimator resolves
# three orders finer. Absolute values, since the sign carries no information
# once the truth is 0 Hz.
idx = np.arange(N_BURSTS)
resid = np.abs(events["est_freq_hz"])
ax2.axhline(
    BIN_HZ / 2,
    color="0.45",
    lw=1.2,
    ls="--",
    label=f"search grid: half a bin ({BIN_HZ / 2:.0f} Hz)",
)
ax2.bar(
    idx,
    resid,
    width=0.45,
    color="tab:purple",
    label="|est_freq_hz| after refine + demod",
)
for i, v in enumerate(resid):
    ax2.annotate(
        f"{v:.2f} Hz",
        (i, v),
        textcoords="offset points",
        xytext=(0, 4),
        ha="center",
        fontsize=7,
    )
ax2.set_yscale("log")
ax2.set_xticks(idx)
ax2.set_xlabel("burst")
ax2.set_ylabel("|frequency error| (Hz)")
ax2.set_title(
    "What the grid promises vs what the chain resolves\n"
    f"(median {np.median(resid):.2f} Hz — "
    f"{BIN_HZ / 2 / max(np.median(resid), 1e-9):.0f}x inside one bin)",
    fontsize=9,
)
ax2.set_ylim(min(resid.min(), 0.1) / 3, BIN_HZ * 4)
ax2.legend(fontsize=8, loc="upper right")
ax2.grid(alpha=0.3, which="both", axis="y")

# Quality: two dB quantities on the left axis, the dimensionless refine
# margin on the right, and the CRC flag as the marker that says the row is
# worth reading at all.
w = 0.35
ax3.bar(
    idx - w / 2,
    events["cn0_dbhz_est"],
    width=w,
    color="tab:blue",
    label="cn0_dbhz_est (lower bound)",
)
ax3.bar(
    idx + w / 2,
    events["est_snr_db"],
    width=w,
    color="tab:orange",
    label="est_snr_db (estimator confidence)",
)
ax3.axhline(
    CN0_DBHZ_TRUE,
    color="tab:blue",
    lw=1.0,
    ls="--",
    alpha=0.8,
    label=f"scene C/N0 = {CN0_DBHZ_TRUE:.1f} dB-Hz",
)
ax3.set_xticks(idx)
ax3.set_xlabel("burst")
ax3.set_ylabel("dB / dB-Hz")
ax3.set_ylim(0, CN0_DBHZ_TRUE * 1.45)
ax3.grid(alpha=0.3, axis="y")

ax3m = ax3.twinx()
ax3m.plot(
    idx,
    events["refine_margin"],
    "D-",
    color="tab:green",
    ms=5,
    label="refine_margin (runner-up / winner)",
)
ax3m.axhline(1.0, color="tab:green", lw=0.8, ls=":", alpha=0.7)
ax3m.set_ylim(0, 1.45 / 1.25)
ax3m.set_ylabel("margin (must stay < 1)")

for i in range(len(events)):
    ax3.annotate(
        "CRC ok" if deframed_ok[i] else "CRC BAD",
        (i, 2.0),
        ha="center",
        fontsize=7,
        color="0.25",
    )
h0, l0 = ax3.get_legend_handles_labels()
h1, l1 = ax3m.get_legend_handles_labels()
ax3.legend(h0 + h1, l0 + l1, fontsize=7, loc="upper center", ncol=2)
ax3.set_title(
    "Every quality read-back, checked against the scene\n"
    "(C/N0 is a lower bound; confidence is peak-to-mean, not a link SNR)",
    fontsize=9,
)

fig.tight_layout()
fig.savefig("dsss_burst_receiver_demo.png", dpi=110)
print("\nwrote dsss_burst_receiver_demo.png")
