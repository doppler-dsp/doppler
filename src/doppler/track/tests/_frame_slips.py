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
    """

    frame: int
    gap: int
    data: int
    other: int
    markers: tuple[Marker, ...]
    lags: tuple[int | None, ...]


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
        off its edge. More than this many slips between two markers is a
        miss, not a count.

    Returns
    -------
    list of FrameSlips
        One per frame, in order. The first frame's `gap` is 0: nothing
        precedes it to compare with.

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
    meter = BerMeter(m=m, target_errors=1)
    meter.set_truth(np.ascontiguousarray(truth, np.uint8))
    pad = 4 * lag_span + 64
    rpad = np.concatenate([np.zeros(pad, np.complex64), rx])
    markers = frame_markers(sync_at, ends, n_marker)

    per: dict[int, list] = {}
    guess = 0
    prev: tuple[Marker, int] | None = None
    for mk in markers:
        # view[j] = rpad[j + pad - guess] = rx[j - guess]: a symbol at lag
        # L in `rx` sits at L - guess here, inside the search span.
        meter.align(
            rpad[pad - guess :], t0=mk.t, n_marker=mk.n, lag_span=lag_span
        )
        rec = per.setdefault(mk.frame, [0, 0, 0, [], []])
        rec[3].append(mk)
        if not meter.align_ok:
            rec[4].append(None)
            continue
        lag = meter.lag + guess
        rec[4].append(lag)
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
    return [
        FrameSlips(k, g, d, o, tuple(ms), tuple(ls))
        for k, (g, d, o, ms, ls) in sorted(per.items())
    ]
