"""Gardner or DTTL: which holds rectangular BPSK at Es/N0 = 3 dB, gapped?

`RateSync`'s header prices its two timing-error detectors on two claims.
The DTTL's *"lower self-noise near lock"* is measured (the RateSync
validator's finding `header_lower_self_noise_near`); its *"degrades faster
at low SNR"* is not, because nothing in that report adds noise (finding
`dttl_low_snr_claim_unmeasurable`, gh-751). This subject measures the second
claim at the one operating point a caller of this library has to meet: the
specification's floor, Es/N0 = 3 dB, on the stream the specification sends.

**The stream.** Rectangular (integrate-and-dump) BPSK at 8 samples a
symbol, framed as the specification frames it:

    zeros(G) | dotted 128 | EB90 | payload (1..1017 octets) | CRC-16

`G` is the idle run between frames, 367 symbols at the 3000 sym/s point and
357 at 7000 sym/s. This subject is stated in symbols, so the two rates
differ in `G` alone; both rows are run because `G` is the one thing that
changes. The idle run is the hard part for a timing loop: it has no
transitions, so a detector that needs them has nothing to steer on for `G`
symbols, and then the dotted preamble hands it a transition on every
symbol.

The frame is described with `wfm.FrameDesc` (the gap, the dots and the
sync word as fields, the payload as a data field, the CRC as a derived field
a `STAGE_CRC16` fills) and materialised by `wfm.Composer`, so the frame
layout, the CRC and the bit-to-symbol mapping are the library's. The
samples come from the shared `_mpsk_rx_harness.make_signal`, driven with
those symbols through its `idx=` argument: the same generator, level
convention and noise every other M-PSK measurement in `track` uses.

**The receiver.** `RateSync` alone, `pulse="iandd"`, at two loop
bandwidths: 0.005, which is what `AsyncDsssReceiver` builds its demodulator
with (`ASYNC_DSSS_RX_DEMOD_BN_TIMING`), and `RateSync`'s own default for
contrast. Four terminal outputs a symbol (`m=4`), which its docstring
requires of the rectangle. The data clock is 1 ppm off nominal, applied
the harness's way, by telling the receiver an `sps` that far from the
stimulus's, and stated against the loop's own bound
(`clock_offset_inside_bw`): 2e-4 of `bn` at 0.005. There is no carrier, so
this is not the trap of moving a clock apart from its carrier.

**The level.** `RateSync` carries no AGC: its contract is unit SYMBOL
amplitude, and its detectors' gains go as the amplitude (DTTL, `A^1`) and
its square (Gardner, `A^2`) -- the validator's finding
`amplitude_laws_differ`. So 1x here is that
contract, and the sweep runs 0.5x, 1x and 2x of it with the noise scaled
alongside, so Es/N0 is 3 dB at every level. That is NOT the harness's own
level: `make_signal` sets unit average power of signal plus noise, which at
3 dB and 8 samples a symbol leaves the symbols at 0.45 of the contract
(`contract_scale`).

**What is measured, per (rate, detector, bandwidth, level):**

- *slips* -- whole-symbol slips, counted as changes in the alignment
  `BerMeter.align` detects on each frame's sync word and payload head: the
  library's instrument, gated on a false-alarm probability, never a lag
  search. The number in brackets is how many of those symbols were
  slipped inside an idle run, read off the loop's own timing: the whole
  symbols its `mu` probe moved across that gap. The rest slipped in data,
  which costs the frame; a slip in the idle run costs nothing a frame sync
  does not recover.
- *SER* and its exact interval, scored frame by frame against the
  alignment detected for that frame, only while the strobe holds the eye it
  held at the marker (`score_frame`), and stopped on an error count. *loss*
  is `ESN0_DB - ber_esn0_db_for_ser(2, ser.lo)`: the Es/N0 the coherent
  bound would need to make the measured rate's lower limit, against the
  Es/N0 sent. A loss of 0 is a receiver on the bound. So SER is the
  decision quality between slips, and the slip column is the rest.
- *walk* -- how far the strobe drifted across each idle run short of a
  slip, RMS over the gaps in symbols, from the loop's `mu` probe; *data* is
  the same over the `G` data symbols before each gap, so the two are one
  loop over two kinds of symbol and their ratio is what the gap costs.
- *at sync* and *settle* -- how much of a gap's walk is still there when
  the sync word arrives (128 dotted symbols after the gap), and how many
  symbols after the gap it takes to fall to 10%. Both come from one curve,
  estimated across every gap that did not slip by regressing the post-gap
  displacement on the gap's own walk (`recovery`).
- *lock* -- the share of gap symbols during which `RateSync` still declares
  lock, and its `lock_stat` inside the gaps against inside the data.

**What this does not measure.** One Es/N0. The header's claim is about a
trend, and a crossover between the detectors, if there is one, needs an
Es/N0 axis this subject does not sweep; it answers the claim only where
the specification puts the floor.

Run:  make characterize      (this subject, plus every other)
      python -m doppler.track.tests.characterization.ted_low_snr.characterize
"""

from __future__ import annotations

import sys
from dataclasses import dataclass

import numpy as np

from doppler.ber import BerMeter, ber_esn0_db_for_ser, ber_settle_syms
from doppler.telemetry import Telemetry
from doppler.track import RateSync
from doppler.track.tests._mpsk_rx_harness import (
    clock_offset_inside_bw,
    make_signal,
)
from doppler.wfm import STAGE_CRC16, Composer, FrameDesc, Segment, field_bits

#: BPSK: the specification's modulation, and the only order both detectors
#: accept that this question is about.
M = 2

#: Samples per symbol. An integer, because `make_signal` holds each symbol
#: for exactly `sps` samples -- which is what makes the pulse rectangular.
SPS = 8

#: `RateSync`'s terminal outputs per symbol. Its docstring requires at
#: least four with `pulse="iandd"`: at two the rectangle's matched filter is
#: a two-tap sum and the lock detector never clears its declare threshold.
M_OUT = 4

#: The specification's floor, and the only Es/N0 this subject sweeps. The
#: header's claim is about low SNR; this is the lowest one a caller must
#: work at, so it is where the claim decides anything.
ESN0_DB = 3.0

#: Data-clock offset, parts per million. Small against both loops' bounds
#: on purpose: the question is the detector at the floor, not pull-in.
CLOCK_PPM = 1.0

#: `ASYNC_DSSS_RX_DEMOD_BN_TIMING`, the timing bandwidth `AsyncDsssReceiver`
#: builds its demodulator with. The fast twin reads the header and fails if
#: the two part.
BN_RECEIVER = 0.005

#: `RateSync`'s own default bandwidth, read from the object rather than
#: restated, for contrast with the receiver's.
BN_DEFAULT = RateSync().bn

BANDWIDTHS = (BN_RECEIVER, BN_DEFAULT)
TEDS = ("gardner", "dttl")

#: Input level as a multiple of the harness's nominal (unit average power).
#: The noise scales with it: Es/N0 is `ESN0_DB` at every level.
LEVELS = (0.5, 1.0, 2.0)

#: Idle run between frames, in symbols, at each specification symbol rate.
GAP_SYMBOLS = {3000: 367, 7000: 357}

#: The frame's fixed fields, as `wfm.field_bits` spells them.
DOTS = "dotted:128"
SYNC = "0xEB90"

#: Payload length bounds in octets, drawn uniformly per frame.
MIN_OCTETS, MAX_OCTETS = 1, 1017

#: Frames per row, so this many gaps are crossed. The slip count is a
#: count of rare events, so it needs gaps rather than errors.
FRAMES = 1000

#: Symbol errors the SER stops on: a relative standard error of 1/sqrt(r),
#: about 3%.
TARGET_ERRORS = 1000

#: Alignment marker: the sync word and what follows it, in symbols. The
#: dotted preamble is useless as a marker -- it is periodic in two symbols,
#: so the lags either side of the true one score almost as high and the
#: detector rightly refuses them as ambiguous. 256 symbols clear the
#: false-alarm gate with several dB to spare at 3 dB; 56 did not.
N_MARKER = 256

#: Half-width of each frame's alignment search, around the previous frame's
#: lag, so a slow accumulation of slips never walks off the search's edge.
LAG_SPAN = 16

#: Samples pushed per `steps` call: a block's telemetry, six probes a
#: symbol, must fit the ring before it is drained.
BLOCK = 1 << 18

#: Symbols after a gap over which its displacement is followed, and the
#: fraction of it that counts as settled.
FOLLOW = 1500
SETTLED_FRAC = 0.1

#: One seed per rate for the payload, one for the noise. Every detector,
#: bandwidth and level sees the same symbols AND the same noise, so the
#: comparisons between them are paired rather than across realisations.
SEED = 751


@dataclass(frozen=True)
class Stream:
    """The transmitted symbol indices and where each frame sits in them."""

    truth: np.ndarray  #: symbol index per symbol, 0 or 1 (uint8)
    starts: np.ndarray  #: first symbol of each frame's idle run
    ends: np.ndarray  #: one past each frame's CRC
    gap: int  #: idle-run length, symbols


def frame_segment(gap: int, octets: np.ndarray) -> Segment:
    """One frame of the specification's layout, as a composer segment.

    Each frame is its own description because the payload length varies
    from frame to frame. The data field takes its bits from the segment's
    `data=`, and the CRC stage covers exactly that field.
    """
    d = FrameDesc()
    d.add_field("gap", np.zeros(gap, np.uint8))
    d.add_field("dots", field_bits(DOTS))
    d.add_field("sync", field_bits(SYNC))
    d.add_data("payload", 8 * octets.size)
    d.add_derived("fecf", 16)
    d.add_stage_over(STAGE_CRC16, "payload", "fecf")
    return Segment(
        type="bits",
        sps=1,
        modulation="bpsk",
        frame=d,
        data=np.unpackbits(octets),
    )


def make_stream(gap: int, frames: int = FRAMES, seed: int = SEED) -> Stream:
    """`frames` frames back to back, composed by `wfm.Composer`.

    The composer is run at one sample a symbol with no noise, so each of
    its samples IS a BPSK symbol: +1 for a 0 bit, -1 for a 1 bit. A symbol
    index is therefore the bit itself (index k is phase pi*k), and the
    frame's CRC is the library's, computed over the payload it drew.
    """
    rng = np.random.default_rng(seed)
    nfix = len(field_bits(DOTS)) + len(field_bits(SYNC)) + 16
    segs, starts, t = [], [], 0
    for _ in range(frames):
        n = int(rng.integers(MIN_OCTETS, MAX_OCTETS + 1))
        segs.append(frame_segment(gap, rng.integers(0, 256, n, np.uint8)))
        starts.append(t)
        t += gap + nfix + 8 * n
    z = np.asarray(Composer(segs).compose())
    truth = (z.real < 0).astype(np.uint8)
    if truth.size != t:
        raise AssertionError(f"composer gave {truth.size} symbols, not {t}")
    starts_a = np.asarray(starts)
    return Stream(truth, starts_a, np.append(starts_a[1:], t), gap)


def contract_scale(esn0_db: float = ESN0_DB, sps: int = SPS) -> float:
    """Gain from `make_signal`'s level to `RateSync`'s unit symbol amplitude.

    `make_signal` normalises signal plus noise to unit average power. Its
    Es/N0 is `Es = A^2*sps` against `N0` the noise variance per complex
    sample, so the signal-to-noise ratio PER SAMPLE is `r = Es/N0 / sps`
    and the symbols come out at amplitude `sqrt(r / (1 + r))`: 0.447 at
    3 dB and 8 samples a symbol. Dividing by that restores unit-amplitude
    symbols with the same Es/N0. The fast twin measures the delivered
    amplitude against the truth rather than trusting this algebra.
    """
    r = 10.0 ** (esn0_db / 10.0) / sps
    return float(np.sqrt((1.0 + r) / r))


def stimulus(stream: Stream, seed: int = SEED) -> np.ndarray:
    """The stream as rectangular BPSK at `ESN0_DB`, unit symbol amplitude.

    Baseband (`fc=0`): `RateSync` takes complex baseband and there is no
    carrier in this question. A row's level multiplies this.
    """
    x, _ = make_signal(
        SPS,
        stream.truth.size,
        real=False,
        m=M,
        fc=0.0,
        esn0_db=ESN0_DB,
        seed=seed,
        idx=stream.truth,
    )
    return (x * np.float32(contract_scale())).astype(np.complex64)


def run_sync(x: np.ndarray, ted: str, bn: float):
    """`RateSync` over `x`, told a clock `CLOCK_PPM` off; symbols + probes.

    The probes are the object's own, one value per recovered symbol:
    `mu` (the strobe's fractional position), `rate`, `ctrl`, `lock`,
    `locked` and `e`.
    """
    eps = clock_offset_inside_bw(bn, CLOCK_PPM * 1e-6 / bn)
    rs = RateSync(
        sps=SPS * (1.0 + eps),
        pulse="iandd",
        m=M_OUT,
        bn=bn,
        ted=ted,
    )
    # Blocks, so a 1000-frame run never outgrows the ring: each block's
    # records are drained before the next is pushed. State carries across
    # `steps` calls, so the blocks are the same loop as one call.
    tlm = Telemetry(1 << 20)
    rs.set_telemetry(tlm, "s")
    ys, recs = [], []
    for b in range(0, x.size, BLOCK):
        ys.append(np.asarray(rs.steps(x[b : b + BLOCK])))
        recs.append(tlm.read_dict())
        if tlm.dropped:
            raise AssertionError("telemetry ring overran; probes truncated")
    y = np.concatenate(ys)
    probes = {
        k.removeprefix("s."): np.concatenate([r[k] for r in recs])
        for k in recs[0]
    }
    return y, probes, bool(rs.clipped)


def strobe_symbols(mu: np.ndarray) -> np.ndarray:
    """The strobe's position in symbols, unwrapped, from the `mu` probe.

    `mu` is the fractional part of the strobe's position in input samples,
    so its change per symbol is the tracked rate's departure from the true
    one and it wraps once per sample the strobe walks. Unwrapped and
    divided by `SPS`, it is the strobe's displacement in symbols: a whole
    symbol of it is a slip. The fast twin checks that calibration against
    a known clock offset rather than trusting this paragraph.
    """
    return np.unwrap(mu.astype(np.float64), period=1.0) / SPS


def recovery(walks: np.ndarray, after: np.ndarray):
    """Share of a gap's walk left at the sync word, and symbols to 10%.

    `after[k, n]` is gap k's strobe displacement `n` symbols after the gap
    ends, and `walks[k]` the displacement at its end, both against the
    strobe's average over the data before the gap. Regressing the one on
    the other across the gaps, `h(n) = sum(w*d(n)) / sum(w*w)`, gives the
    share of a gap's walk still present `n` symbols later; noise after the
    gap is independent of the walk and averages out.

    Gaps that slipped are left out by the caller: a slip lands the strobe
    on the neighbouring eye, where it is settled with a whole symbol of
    "walk" left, and the slip column already counts it.

    Returns `(h at the sync word, first n with h <= SETTLED_FRAC)`, the
    second None when it never gets there inside `FOLLOW` -- an absence,
    not a large number -- and both None with no gap to regress.
    """
    den = float(np.sum(walks * walks)) if walks.size else 0.0
    if den <= 0.0:
        return None, None
    h = (walks[:, None] * after).sum(axis=0) / den
    below = np.flatnonzero(h <= SETTLED_FRAC)
    at_sync = float(h[len(field_bits(DOTS))])
    return at_sync, (int(below[0]) if below.size else None)


@dataclass
class Row:
    """One (rate, detector, bandwidth, level) result."""

    rate: int
    ted: str
    bn: float
    level: float
    gaps: int
    unaligned: int
    slips: int
    slips_in_gap: int
    ser: object
    loss_db: float
    walk_gap: float
    walk_data: float
    at_sync: float | None
    settle: int | None
    lock_in_gap: float
    stat_gap: float
    stat_data: float
    clipped: bool


def score_frame(meter, rx, tau, *, a, e, t0, lag) -> None:
    """Score one frame's symbols where the strobe holds the marker's eye.

    The alignment is the one `align` detected at this frame's marker, so
    it is right only while the strobe stays on the eye it held there. A
    slip in the data moves it half a symbol and then a whole one, after
    which every symbol is scored against its neighbour's truth -- half of
    them wrong, and a single slip early in a frame would outweigh every
    honest error in the run. So a symbol is scored only while the loop's
    own timing (`tau`, from its `mu` probe) is within half a symbol of
    where it stood at the marker. The cut is made on the timing, never on
    the errors, and what it removes is counted where it belongs: in the
    slip column.

    Scored in blocks of 16 so the count stops on the error target rather
    than overshooting it by a frame.
    """
    i_m = int(t0 - lag)
    if not 0 <= i_m < tau.size:
        return
    lo, hi = max(int(a - lag), 0), min(int(e - lag), tau.size)
    held = np.abs(tau[lo:hi] - tau[i_m]) < 0.5
    # rx index of output i is i + lag - meter.lag (see `measure`).
    shift = lag - meter.lag
    edges = np.flatnonzero(np.diff(np.r_[0, held.astype(np.int8), 0]))
    for r0, r1 in zip(edges[::2], edges[1::2]):
        for b in range(lo + r0, lo + r1, 16):
            if meter.enough:
                return
            meter.score(rx, lo=b + shift, hi=min(b + 16, lo + r1) + shift)


def _sub_slip(d) -> np.ndarray:
    """Displacements with their whole-symbol part removed, in [-0.5, 0.5)."""
    d = np.asarray(d, np.float64)
    return d - np.round(d)


def measure(stream: Stream, y, probes, *, rate, ted, bn, level, clipped):
    """Every column of a row, from one run's symbols and probes."""
    meter = BerMeter(m=M, target_errors=TARGET_ERRORS)
    meter.set_truth(stream.truth)
    pad = 4 * LAG_SPAN + 64
    ypad = np.concatenate([np.zeros(pad, np.complex64), y])
    tau = strobe_symbols(probes["mu"])
    locked = probes["locked"]
    stat = probes["lock"]
    nfix = len(field_bits(DOTS))
    g = stream.gap
    settle = ber_settle_syms(bn, 0.0)

    guess, prev = 0, None
    unaligned = slips = in_gap = 0
    walks, datas, lock_gap, st_gap, st_data = [], [], [], [], []
    follow_w, follow_d = [], []
    for k, (a, e) in enumerate(zip(stream.starts, stream.ends)):
        t0 = int(a + g + nfix)  # the sync word
        # The marker stops at the frame's end: running on into the next
        # idle run would put a transition-free stretch in it, which scores
        # as well one lag either side and is refused as ambiguous.
        n_marker = int(min(N_MARKER, e - t0))
        # rx[j] = ypad[j + pad - guess] = y[j - guess], so a symbol that
        # sits at lag L in `y` sits at L - guess here, inside the span.
        rx = ypad[pad - guess :]
        meter.align(rx, t0=t0, n_marker=n_marker, lag_span=LAG_SPAN)
        if not meter.align_ok:
            # Counted, and `prev` kept: a slip across an unaligned frame
            # still shows as the next aligned frame's change of lag.
            unaligned += a >= settle
            continue
        lag = meter.lag + guess
        # The loop's own timing across this frame's idle run. Symbol t of
        # the stream is output t - lag.
        i0, i1 = int(a - lag), int(a + g - lag)
        if k > 0 and a >= settle and i0 - g >= 0 and i1 < tau.size:
            w = tau[i1] - tau[i0]
            slipped = prev is not None and lag != prev
            if slipped:
                slips += abs(lag - prev)
                # Whole symbols the strobe moved across this idle run: the
                # part of the change of lag that happened in the gap.
                in_gap += min(abs(lag - prev), round(abs(w)))
            walks.append(w)
            lock_gap.append(locked[i0:i1].mean())
            st_gap.append(stat[i0:i1].mean())
            # The data reference only where the previous frame holds G
            # data symbols, so it never reaches back into the last gap.
            if a - stream.starts[k - 1] - g >= g:
                datas.append(tau[i0] - tau[i0 - g])
                st_data.append(stat[i0 - g : i0].mean())
            j1 = i1 + FOLLOW
            if not slipped and e - (a + g) >= FOLLOW and j1 <= tau.size:
                ref = tau[i0 - 128 : i0].mean()
                follow_w.append(tau[i1] - ref)
                follow_d.append(tau[i1:j1] - ref)
        if a >= settle and not meter.enough:
            score_frame(meter, rx, tau, a=a, e=e, t0=t0, lag=lag)
        prev, guess = lag, lag

    ser = meter.ser()
    at_sync, settle_n = recovery(
        np.asarray(follow_w), np.asarray(follow_d).reshape(-1, FOLLOW)
    )
    # The walk columns are the drift SHORT of a slip: the whole-symbol part
    # is the slip column's, and left in, one slip outweighs a thousand
    # gaps of honest drift in an RMS.
    walks_a, datas_a = _sub_slip(walks), _sub_slip(datas)
    return Row(
        rate=rate,
        ted=ted,
        bn=bn,
        level=level,
        gaps=len(walks),
        unaligned=unaligned,
        slips=slips,
        slips_in_gap=int(in_gap),
        ser=ser,
        loss_db=ESN0_DB - ber_esn0_db_for_ser(M, ser.lo),
        walk_gap=float(np.sqrt(np.mean(walks_a**2))),
        walk_data=float(np.sqrt(np.mean(datas_a**2))),
        at_sync=at_sync,
        settle=settle_n,
        lock_in_gap=float(np.mean(lock_gap)),
        stat_gap=float(np.mean(st_gap)),
        stat_data=float(np.mean(st_data)),
        clipped=clipped,
    )


def run_row(stream: Stream, x_unit, *, rate, ted, bn, level) -> Row:
    """One row: scale the unit-amplitude stimulus, run the loop, measure."""
    x = (x_unit * np.float32(level)).astype(np.complex64)
    y, probes, clipped = run_sync(x, ted, bn)
    return measure(
        stream,
        y,
        probes,
        rate=rate,
        ted=ted,
        bn=bn,
        level=level,
        clipped=clipped,
    )


def sweep(frames: int = FRAMES, levels=LEVELS, bandwidths=BANDWIDTHS):
    """Every row, rates outermost so each stream is composed once."""
    rows = []
    for rate, gap in GAP_SYMBOLS.items():
        stream = make_stream(gap, frames)
        x = stimulus(stream)
        for ted in TEDS:
            for bn in bandwidths:
                for level in levels:
                    rows.append(
                        run_row(
                            stream, x, rate=rate, ted=ted, bn=bn, level=level
                        )
                    )
    return rows


HEADER = (
    "| sym/s | G | TED | bn | level | slips (gap) | SER [99% CI] | loss dB "
    "| walk gap / data | at sync | settle | lock in gap "
    "| lock_stat gap / data |"
)


def table_row(r: Row) -> str:
    """One markdown table row."""
    s = r.ser
    settle = "never" if r.settle is None else f"{r.settle}"
    at_sync = "n/a" if r.at_sync is None else f"{100 * r.at_sync:.0f}%"
    clip = " clipped" if r.clipped else ""
    unal = f", {r.unaligned} unaligned" if r.unaligned else ""
    return (
        f"| {r.rate} | {GAP_SYMBOLS[r.rate]} | {r.ted} | {r.bn:g} "
        f"| {r.level:g}x{clip} | {r.slips} ({r.slips_in_gap}){unal} "
        f"| {s.p_hat:.4f} [{s.lo:.4f}, {s.hi:.4f}] | {r.loss_db:+.2f} "
        f"| {r.walk_gap:.3f} / {r.walk_data:.3f} | {at_sync} | {settle} "
        f"| {100 * r.lock_in_gap:.0f}% "
        f"| {r.stat_gap:+.2f} / {r.stat_data:+.2f} |"
    )


def main(frames: int = FRAMES) -> list[Row]:
    """Run the sweep and print it as one markdown table."""
    print(
        f"ted_low_snr: rectangular BPSK, sps {SPS}, Es/N0 {ESN0_DB:g} dB, "
        f"clock {CLOCK_PPM:g} ppm, RateSync(pulse='iandd', m={M_OUT}), "
        f"{frames} frames a row, SER stops at {TARGET_ERRORS} errors\n"
    )
    rows = sweep(frames)
    print(HEADER)
    print("|" + "---|" * (HEADER.count("|") - 1))
    for r in rows:
        print(table_row(r))
    return rows


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else FRAMES)
