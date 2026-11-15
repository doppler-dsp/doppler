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

- *slips gap / data / other* -- whole symbols slipped, read off the STREAM by
  the shared counter `_frame_slips.frame_slips`: every frame is cut into
  256-symbol markers from its sync word to its end, and `BerMeter.align`
  detects each marker's lag -- the library's instrument, gated on a
  false-alarm probability, never a lag search. A change of lag between two
  markers of one frame is a slip in data, which costs the frame. A change
  between one frame's last marker and the next frame's first is a slip in the
  idle run or the dotted preamble, which a frame sync recovers. *other* is a
  change spanning a marker that was not detected. The count is net per
  bracket: a slip that goes and comes back inside one 256-symbol block, or
  inside one idle run, is not seen. *unaligned* counts markers that missed
  the false-alarm gate even after a wide search. A row whose counter never
  re-acquires the stream reads "lost from frame N" with the counts before
  it, and no SER or loss: past that frame there is nothing to count.
- *SER* and its exact interval, scored block by block under the alignment
  the block before it gave, only where the two agree (`measure`), and
  stopped on an error count. *loss* is
  `ESN0_DB - ber_esn0_db_for_ser(2, ser.lo)`: the Es/N0 the coherent bound
  would need to make the measured rate's lower limit, against the Es/N0
  sent. So SER is the decision quality between slips, and no loss is
  quoted without the slip count beside it.
- *lock* -- the share of gap symbols during which `RateSync` still declares
  lock, and its `lock_stat` inside the gaps against inside the data.

**What this does not measure.** The loop's timing drift. `RateSync`'s
`mu` probe is the strobe's fractional position, and unwrapping it assumes
less than half a sample of motion a symbol; on the hot rows that fails
hundreds of thousands of times, so no column here is derived from it.
And one Es/N0. The header's claim is about a
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
from doppler.track.tests._frame_slips import (
    Marker,
    frame_slips,
    lost_from,
)
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


def sync_at(stream: Stream) -> np.ndarray:
    """Each frame's sync-word index in truth: start + gap + dots."""
    return stream.starts + stream.gap + len(field_bits(DOTS))


@dataclass
class Row:
    """One (rate, detector, bandwidth, level) result."""

    rate: int
    ted: str
    bn: float
    level: float
    frames: int
    unaligned: int
    slips_gap: int
    slips_data: int
    slips_other: int
    lost_from: int | None
    ser: object
    loss_db: float
    lock_in_gap: float
    stat_gap: float
    stat_data: float
    clipped: bool

    @property
    def slips(self) -> int:
        """Every slipped symbol the markers saw."""
        return self.slips_gap + self.slips_data + self.slips_other


def measure(stream: Stream, y, probes, *, rate, ted, bn, level, clipped):
    """Every column of a row, from one run's symbols and probes.

    **Slips are read off the stream, not the loop**, by the shared counter
    `_frame_slips.frame_slips`: 256-symbol markers tile each frame from its
    sync word to its end, each one's lag is detected against truth, and a
    change between two markers of one frame is a data slip, one across the
    idle run and the dots a gap slip. Its docstring states the bound: the
    count is net per bracket. Frames before the loop's settling budget are
    not counted.

    **SER is scored between slips.** A marker's block is scored with the
    alignment detected on the marker before it in the same frame, and only
    when the two agree: a slip inside it would score half of it against
    its neighbour's truth, and one slip would then outweigh every honest
    error in the row. Which blocks are scored is decided by the markers,
    never by the errors. The head is a marker only, so a frame's first
    `N_MARKER` symbols are never scored.
    """
    starts, ends = stream.starts, stream.ends
    fs = frame_slips(
        y,
        stream.truth,
        sync_at(stream),
        ends,
        m=M,
        n_marker=N_MARKER,
        lag_span=LAG_SPAN,
    )
    scorer = BerMeter(m=M, target_errors=TARGET_ERRORS)
    scorer.set_truth(stream.truth)
    pad = 4 * LAG_SPAN + 64
    ypad = np.concatenate([np.zeros(pad, np.complex64), y])
    locked, stat = probes["locked"], probes["lock"]
    g = stream.gap
    settle = ber_settle_syms(bn, 0.0)

    unaligned = gap_sl = data_sl = other_sl = 0
    lock_gap, st_gap, st_data = [], [], []
    for f in fs:
        a, e = int(starts[f.frame]), int(ends[f.frame])
        if a < settle:
            continue
        gap_sl += f.gap
        data_sl += f.data
        other_sl += f.other
        unaligned += sum(lag is None for lag in f.lags)
        found = [
            (mk, lag) for mk, lag in zip(f.markers, f.lags) if lag is not None
        ]
        for (pm, plag), (mk, lag) in zip(found, found[1:]):
            if lag == plag and mk.tiles and not scorer.enough:
                _score_block(scorer, ypad[pad - plag :], pm, mk)
        head = f.lags[0]
        if f.frame == 0 or head is None or e - head > locked.size:
            continue
        i0, i1 = a - head, a + g - head
        lock_gap.append(locked[i0:i1].mean())
        st_gap.append(stat[i0:i1].mean())
        st_data.append(stat[i1 + len(field_bits(DOTS)) : e - head].mean())

    ser = scorer.ser()
    return Row(
        rate=rate,
        ted=ted,
        bn=bn,
        level=level,
        frames=len(lock_gap),
        unaligned=unaligned,
        slips_gap=gap_sl,
        slips_data=data_sl,
        slips_other=other_sl,
        lost_from=lost_from(fs),
        ser=ser,
        loss_db=ESN0_DB - ber_esn0_db_for_ser(M, ser.lo),
        lock_in_gap=float(np.mean(lock_gap)),
        stat_gap=float(np.mean(st_gap)),
        stat_data=float(np.mean(st_data)),
        clipped=clipped,
    )


def _score_block(scorer, rx, marker: Marker, blk: Marker) -> None:
    """Score `blk` under the alignment `marker` gives, in steps of 16.

    `rx` is the decided stream shifted so `marker`'s lag sits at zero.
    Detecting the marker on the scorer leaves the marker's own symbols
    excluded and `blk`'s in, and the steps of 16 stop the count on the
    error target rather than a block past it.
    """
    scorer.align(rx, t0=marker.t, n_marker=marker.n, lag_span=LAG_SPAN)
    if not scorer.align_ok:
        return
    lo = blk.t - scorer.lag
    for b in range(lo, lo + blk.n, 16):
        if scorer.enough:
            return
        scorer.score(rx, lo=b, hi=min(b + 16, lo + blk.n))


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
    "| sym/s | G | TED | bn | level | slips gap / data / other "
    "| SER [99% CI] | loss dB | lock in gap | lock_stat gap / data |"
)


def table_row(r: Row) -> str:
    """One markdown table row."""
    s = r.ser
    clip = " clipped" if r.clipped else ""
    unal = f", {r.unaligned} unaligned" if r.unaligned else ""
    if r.lost_from is not None:
        # Counts after the loss are not counts, and a SER over the frames
        # before it is not the row's: print neither as though it were.
        return (
            f"| {r.rate} | {GAP_SYMBOLS[r.rate]} | {r.ted} | {r.bn:g} "
            f"| {r.level:g}x{clip} "
            f"| lost from frame {r.lost_from}: {r.slips_gap} / "
            f"{r.slips_data} / {r.slips_other} before it{unal} "
            f"| — | — | {100 * r.lock_in_gap:.0f}% "
            f"| {r.stat_gap:+.2f} / {r.stat_data:+.2f} |"
        )
    return (
        f"| {r.rate} | {GAP_SYMBOLS[r.rate]} | {r.ted} | {r.bn:g} "
        f"| {r.level:g}x{clip} "
        f"| {r.slips_gap} / {r.slips_data} / {r.slips_other}{unal} "
        f"| {s.p_hat:.4f} [{s.lo:.4f}, {s.hi:.4f}] | {r.loss_db:+.2f} "
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
