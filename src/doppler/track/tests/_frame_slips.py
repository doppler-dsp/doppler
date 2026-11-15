"""Symbol slips per frame, read off the decided stream against its truth.

One counter, shared by every measurement that has to say WHERE a timing
loop slipped on a framed stream: `RateSync` alone at the specification's
floor (`characterization/ted_low_snr`) and frame delivery through the
whole `AsyncDsssReceiver`. Two private copies of this would drift the way
the first one did -- its gap/data split was once derived from the loop's
`mu` probe, which aliases on a hot loop and booked gap slips to data
(doppler#3087's review).

**The method.** Every frame is cut into `n_marker`-symbol markers that
tile it from its sync word to its end; when the tiling stops short of the
end, one more marker is laid on the frame's last `n_marker` symbols. Each
marker's lag is DETECTED by `BerMeter.align` -- a correlation against
truth gated on a false-alarm probability, never a search over lags for the
fewest errors. Then, between consecutive detected markers:

- both in one frame: a change of lag is a slip in **data**, which costs
  the frame;
- the last marker of frame k-1 and the first of frame k: a change is a
  slip in the **gap**, meaning the idle run AND the dotted preamble, which
  a frame sync recovers;
- anything else (a marker in between missed its gate): **other**.

**Why the gap and the dots are one region.** Neither can carry a marker:
a transition-free run, and a two-periodic one, score as well one lag
either side, and `align` rightly refuses them as ambiguous -- so no marker
can sit between them to split the change.

**A miss is searched again, wide.** Each marker is searched within
`lag_span` of the last detected lag; on a miss it is searched again within
`reacquire_span`, and while that also misses the counter searches wide at
each frame's head until it re-finds the stream. A change of lag across the
missed markers is counted, as *other*. If it never re-finds the stream,
every frame from the first miss on is marked `lost` (`lost_from`): a lost
stream reads as lost, never as a small count. A miss with no later frame
head left to search -- in the last frame, or at the capture's end -- is
not a loss; it stays an unresolved miss.

**The count is net per bracket.** A slip that goes and comes back between
two markers -- inside one `n_marker` block of data, or inside one gap --
nets to zero and is not seen. Shorter markers shrink the bracket at the
cost of missing their gate at low Es/N0; 256 clears it with several dB to
spare at Es/N0 = 3 dB on BPSK.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from doppler.ber import BerMeter

#: The widest search `BerMeter.align` makes in one call: it holds at most
#: `BER_MAX_LAGS` = 2048 lags (`native/src/ber_meter/ber_meter_core.c`)
#: and silently narrows a wider `lag_span` to 1023. A wider search is laid
#: as tiles of this half-width.
TILE_SPAN = 1023

#: Lags each tile shares with the next, so a peak on one tile's edge, which
#: that tile refuses as saturated, lies inside its neighbour.
TILE_OVERLAP = 64


@dataclass(frozen=True)
class Marker:
    """One marker: `n` truth symbols from truth index `t`, in `frame`."""

    frame: int
    t: int
    n: int
    head: bool  #: the frame's first marker, starting at its sync word
    last: bool  #: the marker that reaches the frame's end
    tiles: bool  #: on the tiling, so it overlaps no other marker


@dataclass(frozen=True)
class FrameSlips:
    """What the markers saw in one frame.

    `gap` is the slips between the previous frame's last marker and this
    frame's head (the idle run and the dots before this frame), `data` the
    slips between this frame's own markers, and `other` a change spanning
    a marker that missed its gate. All are whole symbols, net per bracket.
    `lags` is each marker's detected lag, `rx[i]` carrying `truth[i + lag]`,
    or None where it missed; `markers` the markers themselves, in order.
    `lost` is True from the first marker the counter missed and never
    re-acquired after: from there on its counts are not counts, and
    `lost_from` names the frame.
    """

    frame: int
    gap: int
    data: int
    other: int
    markers: tuple[Marker, ...]
    lags: tuple[int | None, ...]
    lost: bool = False


def lost_from(fs: list[FrameSlips]) -> int | None:
    """The frame from which the counter lost the stream for good, or None.

    >>> lost_from([])
    """
    return next((f.frame for f in fs if f.lost), None)


def frame_markers(sync_at, ends, n_marker: int = 256) -> list[Marker]:
    """Every frame's markers, sync word to end, in stream order.

    Parameters
    ----------
    sync_at : sequence of int
        Each frame's sync-word index in truth: where its first marker
        starts. For the specification's frame that is
        `start + gap + len(dots)`, so the gap and dots before it are
        implied by where the previous frame ends.
    ends : sequence of int
        One past each frame's last truth symbol (its CRC's end).
    n_marker : int
        Marker length in symbols. A frame with no more than this after its
        sync word gets one marker, its head, covering the rest of it.

    Examples
    --------
    >>> ms = frame_markers([400], [1000], n_marker=256)
    >>> [(m.t, m.n, m.head, m.last, m.tiles) for m in ms]
    [(400, 256, True, False, True), (656, 256, False, False, True), \
(744, 256, False, True, False)]
    """
    out: list[Marker] = []
    for k, (t0, e) in enumerate(zip(sync_at, ends)):
        e, t0 = int(e), int(t0)
        if e - t0 <= n_marker:
            out.append(Marker(k, t0, e - t0, True, True, False))
            continue
        tiles = list(range(t0, e - n_marker + 1, n_marker))
        for i, t in enumerate(tiles):
            out.append(Marker(k, t, n_marker, i == 0, t + n_marker == e, True))
        if tiles[-1] + n_marker < e:
            out.append(Marker(k, e - n_marker, n_marker, False, True, False))
    return out


def frame_slips(
    rx,
    truth,
    sync_at,
    ends,
    *,
    m: int = 2,
    n_marker: int = 256,
    lag_span: int = 16,
    reacquire_span: int = 4096,
) -> list[FrameSlips]:
    """Slips per frame, each in its region: gap (idle run + dots) or data.

    Parameters
    ----------
    rx : ndarray of complex64
        The decided symbols, one per symbol period, in stream order: a
        receiver's output, at whatever lag it has.
    truth : ndarray of uint8
        The transmitted symbol indices, `0..m-1`, the whole stream.
    sync_at, ends : sequence of int
        Per frame, in truth indices: its sync word and one past its end
        (see `frame_markers`). That is the whole layout the counter needs:
        the payload runs from the sync word to the end, and the gap and
        dots are what lies between one frame's end and the next sync.
    m : int
        Constellation order, for the meter.
    n_marker : int
        Marker length in symbols.
    lag_span : int
        Half-width of each marker's search, around the lag the previous
        marker found, so an accumulation of slips never walks the search
        off its edge.
    reacquire_span : int
        Half-width of the search after a miss. A marker that misses at
        `lag_span` is searched again at this width, so a lag that jumped
        more than `lag_span` between two markers is re-found and counted
        rather than lost. Once a wide search also misses, the counter skips
        to the next frame's head and searches wide there, once a frame,
        until it re-acquires; if it never does, every frame from the first
        miss on is marked `lost`. A lag that moved more than this since the
        last detection cannot be re-found, and reads as lost, never as a
        smaller count. Searched in tiles of `TILE_SPAN`, the meter's
        own limit, so any width is the width asked for.

    Returns
    -------
    list of FrameSlips
        One per frame, in order. The first frame's `gap` is 0: nothing
        precedes it to compare with. `lost_from` reads where, if anywhere,
        the stream was lost for good.

    Examples
    --------
    A noiseless BPSK stream of two frames, the receiver dropping one
    symbol in the second frame's idle run and repeating one in its data:

    >>> import numpy as np
    >>> rng = np.random.default_rng(0)
    >>> gap, dots, body = 300, 128, 1200
    >>> frame = lambda: np.r_[
    ...     np.zeros(gap), np.arange(dots) % 2, rng.integers(0, 2, body)
    ... ].astype(np.uint8)
    >>> truth = np.r_[frame(), frame()]
    >>> n = gap + dots + body
    >>> sym = (1 - 2.0 * truth).astype(np.complex64)
    >>> rx = np.r_[
    ...     sym[: n + 100], sym[n + 101 : 2 * n - 400], sym[2 * n - 401 :]
    ... ].astype(np.complex64)
    >>> fs = frame_slips(rx, truth, [gap + dots, n + gap + dots], [n, 2 * n])
    >>> [(f.gap, f.data, f.other) for f in fs]
    [(0, 0, 0), (1, 1, 0)]
    """
    rx = np.ascontiguousarray(rx, np.complex64)
    truth = np.ascontiguousarray(truth, np.uint8)
    meter = BerMeter(m=m, target_errors=1)
    meter.set_truth(truth)
    # Room for the largest lag a search can reach: the whole length
    # difference, plus the widest span.
    pad = abs(truth.size - rx.size) + reacquire_span + 64
    rpad = np.concatenate([np.zeros(pad, np.complex64), rx])
    markers = frame_markers(sync_at, ends, n_marker)

    def search_one(mk: Marker, centre: int, span: int) -> int | None:
        # view[j] = rpad[j + pad - centre] = rx[j - centre]: a symbol at
        # lag L in `rx` sits at L - centre here, inside the search span.
        start = pad - centre
        if not 0 <= start < rpad.size:
            return None
        meter.align(rpad[start:], t0=mk.t, n_marker=mk.n, lag_span=span)
        return meter.lag + centre if meter.align_ok else None

    def search(mk: Marker, centre: int, span: int) -> int | None:
        # The meter searches at most TILE_SPAN either side and quietly
        # narrows a wider request, so a wide search is laid as tiles,
        # nearest first, overlapping by TILE_OVERLAP so a peak on one
        # tile's edge (refused there as saturated) is inside the next.
        if span <= TILE_SPAN:
            return search_one(mk, centre, span)
        step = 2 * TILE_SPAN + 1 - TILE_OVERLAP
        k_max = -(-(span - TILE_SPAN) // step)
        for k in sorted(range(-k_max, k_max + 1), key=abs):
            lag = search_one(mk, centre + k * step, TILE_SPAN)
            if lag is not None and abs(lag - centre) <= span:
                return lag
        return None

    per: dict[int, list] = {}
    guess = 0
    prev: tuple[Marker, int] | None = None
    searching = False  # a wide search missed: wait for the next head
    first_miss: int | None = None  # frame of the first miss since a find
    confirmed = False  # a LATER frame's head was searched wide, and missed
    for mk in markers:
        rec = per.setdefault(mk.frame, [0, 0, 0, [], []])
        rec[3].append(mk)
        if mk.t + mk.n - guess > rx.size:
            # Past the end of the capture at the current lag: nothing was
            # received there to miss, so it is neither a miss nor a loss.
            rec[4].append(None)
            continue
        lag = None
        if not searching:
            lag = search(mk, guess, lag_span)
            if lag is None:
                lag = search(mk, guess, reacquire_span)
                searching = lag is None
        elif mk.head:
            lag = search(mk, guess, reacquire_span)
            searching = lag is None
            if searching and first_miss is not None:
                confirmed = confirmed or mk.frame > first_miss
        rec[4].append(lag)
        if lag is None:
            # A miss is evidence of a loss only where every lag the wide
            # search could reach still lands inside the capture; one near
            # its end may simply have run off it.
            inside = mk.t + mk.n - guess + reacquire_span <= rx.size
            if first_miss is None and inside:
                first_miss = mk.frame
            continue
        first_miss, confirmed = None, False
        if prev is not None:
            pm, plag = prev
            d = abs(lag - plag)
            if pm.frame == mk.frame:
                rec[1] += d
            elif pm.last and mk.head and mk.frame == pm.frame + 1:
                rec[0] += d
            else:
                rec[2] += d
        prev, guess = (mk, lag), lag
    # Lost only when a later frame's head was searched wide and missed too:
    # a miss in the last frame, or near the capture's end, had no later
    # head to be re-found at, so it stays an unresolved miss (lag None).
    gone = first_miss if searching and confirmed else None
    return [
        FrameSlips(
            k, g, d, o, tuple(ms), tuple(ls), gone is not None and k >= gone
        )
        for k, (g, d, o, ms, ls) in sorted(per.items())
    ]
