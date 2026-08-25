# detection/detection.pyi — type stubs for the detection C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class SyncHit(tuple[int, int, int, int]):
    """Where a marker was found, and in which polarity. `found` is the verdict:
    the other three fields mean nothing without it, which is why the record
    carries it rather than spelling a miss as a sentinel offset.

    Attributes
    ----------
    found : int
        A marker was found: 1 yes, 0 no.
    offset : int
        Bit index where the marker starts.
    inverted : int
        The stream is complemented — a BPSK carrier recovered through a 180-degree ambiguity delivers every bit inverted, and a marker no randomiser covers is the only thing in a frame that can report it.
    errors : int
        Hamming distance to the marker at that offset, in the polarity reported.
    """

    @property
    def found(self) -> int:
        """A marker was found: 1 yes, 0 no."""

    @property
    def offset(self) -> int:
        """Bit index where the marker starts."""

    @property
    def inverted(self) -> int:
        """The stream is complemented — a BPSK carrier recovered through a
        180-degree ambiguity delivers every bit inverted, and a marker no
        randomiser covers is the only thing in a frame that can report it.
        """

    @property
    def errors(self) -> int:
        """Hamming distance to the marker at that offset, in the polarity
        reported.
        """

@final
class LockDet:
    """LockDet component.

    Parameters
    ----------
    up_thresh : float, default 1.0
        up_thresh constructor parameter.
    down_thresh : float, default 1.0
        down_thresh constructor parameter.
    n_up : int, default 1
        n_up constructor parameter.
    n_down : int, default 1
        n_down constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.detection import LockDet
    >>> obj = LockDet(up_thresh=1.0, down_thresh=1.0, n_up=1, n_down=1)

    """
    def __init__(
        self,
        up_thresh: float = ...,
        down_thresh: float = ...,
        n_up: int = ...,
        n_down: int = ...,
    ) -> None: ...

    def step(self, x: float) -> int:
        """Feed one look of the lock metric; return the current decision.

        Unlocked: a hit (`x > up_thresh`) advances the verify run and the
        n_up-th consecutive hit declares lock; any miss resets the run. Locked:
        a miss (`x < down_thresh`) advances the run and the n_down-th
        consecutive miss drops the lock; any hit (`x >= down_thresh`) resets
        it. A metric inside the `[down_thresh, up_thresh]` band is sticky — it
        neither advances a declare nor a drop.

        A **non-finite look is a miss in both states**: it never advances a
        declare, and while locked it advances the drop run like any other miss.
        An unknown lock is not a lock, which is the rule util_core.h states for
        lock statistics generally. So a metric that goes NaN drops the lock
        after n_down looks rather than holding it lit indefinitely.

        Parameters
        ----------
        x : float
            Lock metric for this look. Non-finite counts as a miss.

        Returns
        -------
        int
            Decision after this look (1 = locked, 0 = not).

        Examples
        --------
        >>> from doppler.detection import LockDet
        >>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=3)
        >>> [d.step(2.0), d.step(2.0)]     # declared on the 2nd straight hit
        [0, 1]
        >>> d.step(1.3)                    # in the hysteresis band: stays up
        1
        >>> [d.step(1.0), d.step(1.0), d.step(1.0)]  # 3rd straight miss drops
        [1, 1, 0]

        """

    def steps(
        self,
        x: NDArray[np.float64],
        out: NDArray[np.int32] | None = None,
    ) -> NDArray[np.int32]:
        """Run a block of lock-metric looks through the detector. Applies
        lockdet_step() to each look in turn, so the decision flag and the
        in-flight verify run carry across the block exactly as they would look
        by look — a signal can be processed in frames of any size with no seam.

        Parameters
        ----------
        x : NDArray[np.float64]
            Lock-metric looks, one scalar per look (length >= n).

        Returns
        -------
        NDArray[np.int32]
            Output.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.detection import LockDet
        >>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
        >>> x = np.array([2.0, 2.0, 1.0, 2.0])   # declares on the 2nd hit
        >>> d.steps(x).tolist()
        [0, 1, 1, 1]

        """

    def configure(
        self,
        up_thresh: float,
        down_thresh: float,
        n_up: int,
        n_down: int,
    ) -> None:
        """Re-tune thresholds and verify counts; a live lock survives, the
        in-flight verify run restarts under the new config.

        The current locked flag survives (a live lock is not dropped by a
        re-tune); the in-flight verify counter is cleared so the next run is
        counted entirely under the new config.

        Parameters
        ----------
        up_thresh : float
            Declare threshold (hit when metric > up_thresh).
        down_thresh : float
            Drop threshold (miss when metric < down_thresh).
        n_up : int
            Consecutive hits to declare; clamped to >= 1.
        n_down : int
            Consecutive misses to drop; clamped to >= 1.

        Examples
        --------
        >>> from doppler.detection import LockDet
        >>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
        >>> d.configure(up_thresh=3.0, down_thresh=2.5, n_up=1, n_down=1)
        >>> d.up_thresh          # thresholds re-tuned in place
        3.0
        >>> d.step(4.0)          # a single hit now declares (n_up=1)
        1

        """

    def reset(self) -> None:
        """Drop the lock and clear the verify counter; keep the config.

        Examples
        --------
        >>> from doppler.detection import LockDet
        >>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=1, n_down=1)
        >>> d.step(2.0)          # one hit declares lock (n_up=1)
        1
        >>> d.reset()            # drop it and clear the verify run
        >>> d.locked
        False

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the LockDet has already been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the LockDet has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the LockDet has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def up_thresh(self) -> float:
        """declare side: hit when metric > up_thresh."""
    @up_thresh.setter
    def up_thresh(self, value: float) -> None: ...

    @property
    def down_thresh(self) -> float:
        """drop side: miss when metric < down_thresh."""
    @down_thresh.setter
    def down_thresh(self, value: float) -> None: ...

    @property
    def n_up(self) -> int:
        """consecutive hits required to declare (>= 1)."""

    @property
    def n_down(self) -> int:
        """consecutive misses required to drop (>= 1)."""

    @property
    def cnt(self) -> int:
        """Running consecutive-look verify counter: hits toward a declare while
        unlocked, misses toward a drop while locked.
        """

    @property
    def locked(self) -> bool:
        """Current decision (True = locked)."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "LockDet":
        """Enter a context manager, returning this object.

        Lets a LockDet be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        LockDet
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the LockDet.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

@final
class SyncFinder:
    """Create a searcher for marker.

    Parameters
    ----------
    marker : NDArray[np.uint8]
        Unpacked bits, one per byte; only the LSB is used.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``SyncFinder: the
        marker must be a non-empty array of 0/1 bits, one per element``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.detection import SyncFinder
    >>> from doppler.wfm import ccsds_asm_bits
    >>> asm = ccsds_asm_bits()   # 0x1ACFFC1D, no transcription
    >>> f = SyncFinder(asm)
    >>> f.nbits
    32
    >>> rx = np.concatenate([np.zeros(96, np.uint8), asm])
    >>> hit = f.find(rx, max_errors=f.max_errors_for(96, pfa=1e-3))
    >>> hit.found, hit.offset, hit.inverted
    (1, 96, 0)

    """
    def __init__(self, marker: NDArray[np.uint8]) -> None: ...

    def find(self, bits: NDArray[np.uint8], max_errors: int = 0) -> SyncHit:
        """Find the first marker in bits, either polarity.

        The FIRST offset whose Hamming distance to the marker, or to its
        complement, is at most max_errors. First rather than best, because a
        best-match search has to see the whole stream before it can answer and
        a synchroniser reading a live capture cannot wait for that.

        Choose max_errors with `max_errors_for`, against the window this caller
        actually searches — the marker length is the wrong thing to halve.

        Parameters
        ----------
        bits : NDArray[np.uint8]
            Unpacked bits, one per byte.
        max_errors : int
            Largest tolerated Hamming distance, in bits.

        Returns
        -------
        SyncHit
            A record whose found says whether the rest of it means anything; a
            miss returns it zeroed.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.detection import SyncFinder
        >>> m = np.array([1, 0, 1, 1, 0, 0, 1, 0], dtype=np.uint8)
        >>> rx = np.concatenate([np.zeros(20, np.uint8), 1 - m])
        >>> hit = SyncFinder(m).find(rx, max_errors=1)
        >>> hit.found, hit.offset, hit.inverted
        (1, 20, 1)

        """

    def pfa(self, max_errors: int) -> float:
        """Probability that ONE random offset false-hits this marker at a
        tolerance of max_errors.

        `2 * sum_{i <= max_errors} C(n, i) / 2^n`, the factor of two because
        `find` searches the complement too. Measured against the 32-bit CCSDS
        marker, this tracks the observed false-alarm rate to within 20 % at
        every threshold where the count supports a rate
        (`src/doppler/tests/validation/ccsds_tm/results.md` §2.2).

        This is the PER-OFFSET number. What a synchroniser cares about is its
        whole window; `max_errors_for` is this inverted through it.

        Parameters
        ----------
        max_errors : int
            Tolerance in bits.

        Returns
        -------
        float
            Probability in &#91;0, 1&#93;.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.detection import SyncFinder
        >>> from doppler.wfm import ccsds_asm_bits
        >>> f = SyncFinder(ccsds_asm_bits())
        >>> # the marker and its complement, out of 2**32 windows
        >>> round(f.pfa(0) * 2**32)
        2
        >>> # ...plus each one's 32 one-bit neighbours
        >>> round(f.pfa(1) * 2**32)
        66

        """

    def max_errors_for(self, window_bits: int, pfa: float) -> int:
        """The largest tolerance whose false-frame rate over a search window
        still meets pfa.

        The question `find`'s signature cannot ask. Every offset ahead of the
        true marker is an independent chance to win the race, so the
        probability the window produces a false frame is `1 - (1 -
        pfa(t))^window_bits`, which rises with `t`. The largest `t` that still
        holds is the most tolerant threshold a caller can afford — and it falls
        as they search further, which is the whole of doppler#897.

        Parameters
        ----------
        window_bits : int
            Offsets tried AHEAD of the marker: the length of stream searched,
            not the length of the frame.
        pfa : float
            Tolerated probability of a false frame over that window.

        Returns
        -------
        int
            Tolerance in bits, or -1 when even an exact match exceeds pfa over
            that window.

        Examples
        --------
        >>> from doppler.detection import SyncFinder
        >>> from doppler.wfm import ccsds_asm_bits
        >>> f = SyncFinder(ccsds_asm_bits())
        >>> f.max_errors_for(window_bits=96, pfa=1e-3)
        3
        >>> f.max_errors_for(window_bits=100000, pfa=1e-3)   # search further
        0

        """

    @property
    def nbits(self) -> int:
        """Marker length in bits."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "SyncFinder":
        """Enter a context manager, returning this object.

        Lets a SyncFinder be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        SyncFinder
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the SyncFinder.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

def marcum_q(m: int, a: float, b: float) -> float:
    """Marcum Q function Q_M(a, b) for integer M >= 1.

    Probability that a Rice(a, sigma=1) random variable exceeds b. For M=1:
    Q_1(a, b) = P(Rice(a,1) > b). General integer M relates to the
    noncentral chi-squared CDF with 2M degrees of freedom.

    Computed via the Poisson-weighted chi-squared series (exact for M=1):

    Q_M(a, b) = sum_{k=0}^inf w_k * Q_{M+k}(0, b)

    where: w_k = exp(-u) * u^k/k! (u = a^2/2) Q_n(0,b) = exp(-v) *
    sum_{j=0}^{n-1} v^j/j! (v = b^2/2)

    Each iteration advances both the Poisson weight and the chi-sum in O(1)
    using the recurrences w_{k+1} = w_k * u/(k+1) and Q_{n+1}(0,b) =
    Q_n(0,b) + exp(-v)*v^n/n!.

    The window is CENTRED on the Poisson mode k ~ u = a^2/2 and its
    half-width scales as 12*sqrt(u+1) + 60 terms, so the term count grows
    with `a` rather than being the fixed ~60 this comment used to claim:
    about 60 terms at a = 0, but ~187 at a = 15. That scaling is the whole
    point -- a Poisson(u) distribution's mass sits at k ~ u with spread
    ~sqrt(u), so a fixed window anchored at k = 0 misses it entirely once
    `a` is large, which is a real bug this code already carries a comment
    about (see marcum_q.c). Total cost: O(sqrt(u) + M).

    Special cases:

    - a = 0:   Q_M(0, b) = exp(-b^2/2) * sum_{j=0}^{M-1} (b^2/2)^j/j!
    - b <= 0:  Q_M(a, b) = 1.0

    Parameters
    ----------
    m : int
        Integration order; must be >= 1.
    a : float
        Non-centrality parameter (signal strength). a = 0 for H0.
    b : float
        Threshold (same units as test_stat).

    Returns
    -------
    float
        Q_M(a, b) in &#91;0, 1&#93;.

    Examples
    --------
    >>> from doppler.detection import marcum_q
    >>> round(marcum_q(m=1, a=0.0, b=1.0), 5)  # P(Rayleigh>1) = exp(-.5)
    0.60653
    >>> round(marcum_q(m=1, a=0.0, b=2.0), 5)   # exp(-2)
    0.13534
    >>> round(marcum_q(m=2, a=0.0, b=2.0), 5)   # 3*exp(-2)
    0.40601
    >>> round(marcum_q(m=1, a=2.0, b=1.0), 5)   # signal present (a=2)
    0.91811

    """

def det_threshold(pfa: float) -> float:
    """Threshold eta for a given false-alarm probability.

    Exact closed-form inversion of Pfa = exp(-eta^2/2):

    eta = sqrt(-2 * ln(pfa))

    The threshold is independent of dwell and SNR; it depends only on the
    desired Pfa.

    Parameters
    ----------
    pfa : float
        Desired false-alarm probability; must be in (0, 1).

    Returns
    -------
    float
        Threshold eta > 0.

    Examples
    --------
    >>> from doppler.detection import det_threshold
    >>> round(det_threshold(pfa=1e-6), 4)
    5.2565

    """

def det_pd(snr: float, dwell: int, threshold: float) -> float:
    """Detection probability for given per-sample amplitude SNR and dwell.

    Computes Pd = Q_1(a, eta) where a = sqrt(2 * dwell) * snr.

    At snr = 0, det_pd returns Pfa (the false-alarm rate, as expected for a
    noise-only input). As snr or dwell increase, Pd approaches 1.

    Parameters
    ----------
    snr : float
        Per-sample amplitude SNR (signal / noise amplitude, linear). snr =
        0 gives Pd = Pfa.
    dwell : int
        Coherent integration depth; must be >= 1.
    threshold : float
        Test-stat threshold eta, e.g. from det_threshold().

    Returns
    -------
    float
        Detection probability in &#91;0, 1&#93;.

    Examples
    --------
    >>> from doppler.detection import det_pd, det_threshold
    >>> thr = det_threshold(pfa=1e-6)
    >>> round(det_pd(snr=1.613, dwell=8, threshold=thr), 2)  # Pd 0.9
    0.9
    >>> round(det_pd(snr=0.0, dwell=8, threshold=thr), 6)    # Pd = Pfa
    1e-06

    """

def det_dwell(snr: float, pd_min: float, pfa: float, max_dwell: int) -> int:
    """Minimum dwell such that Pd >= pd_min for the given SNR and Pfa.

    Iterates dwell = 1, 2, ..., max_dwell, computing det_pd() at each step.
    Returns the first dwell that satisfies the Pd requirement, or -1 if
    none is found within max_dwell iterations.

    Parameters
    ----------
    snr : float
        Per-sample amplitude SNR (linear).
    pd_min : float
        Required detection probability, e.g. 0.9.
    pfa : float
        False-alarm probability; used to derive eta.
    max_dwell : int
        Search upper bound; prevents infinite loops for low SNR.

    Returns
    -------
    int
        Minimum dwell >= 1, or -1 if not achievable.

    Examples
    --------
    >>> from doppler.detection import det_dwell
    >>> det_dwell(snr=0.5, pd_min=0.9, pfa=1e-6, max_dwell=256)
    84

    """

def det_snr(dwell: int, pd_min: float, pfa: float) -> float:
    """Minimum per-sample amplitude SNR achieving Pd >= pd_min.

    Binary search over SNR in &#91;0, hi&#93; where hi is doubled from 1.0
    until det_pd(hi, dwell, threshold) >= pd_min. 64 bisection iterations
    yield ~1e-19 relative precision on the final interval.

    Parameters
    ----------
    dwell : int
        Coherent integration depth; must be >= 1.
    pd_min : float
        Required detection probability.
    pfa : float
        False-alarm probability; used to derive eta.

    Returns
    -------
    float
        Minimum amplitude SNR >= 0.

    Examples
    --------
    >>> from doppler.detection import det_snr, det_pd, det_threshold
    >>> snr = det_snr(dwell=8, pd_min=0.9, pfa=1e-6)
    >>> round(snr, 3)
    1.613
    >>> pd = det_pd(snr=snr, dwell=8, threshold=det_threshold(pfa=1e-6))
    >>> abs(pd - 0.9) < 1e-9   # det_snr inverts det_pd, to tolerance
    True

    """

def det_threshold_noncoherent(pfa: float, n_noncoh: int) -> float:
    """CFAR threshold eta_nc for a non-coherent detector of n_noncoh looks.

    Solves marcum_q(n_noncoh, 0, eta_nc) = pfa (the order-M central tail,
    monotone decreasing in eta_nc) by bisection. For n_noncoh = 1 this is
    the exact closed form sqrt(-2 ln pfa) (== det_threshold).

    Parameters
    ----------
    pfa : float
        Per-test false-alarm probability in (0, 1).
    n_noncoh : int
        Number of non-coherent looks; must be >= 1.

    Returns
    -------
    float
        Threshold eta_nc on the normalized statistic R.

    Examples
    --------
    >>> from doppler.detection import det_threshold_noncoherent
    >>> from doppler.detection import det_threshold
    >>> round(det_threshold_noncoherent(pfa=1e-3, n_noncoh=4), 3)
    5.111
    >>> det_threshold_noncoherent(pfa=1e-6, n_noncoh=1) == det_threshold(
    ...     pfa=1e-6)
    True

    """

def det_q_inv(p: float) -> float:
    """Upper-tail quantile of the standard normal: the eta with Q(eta) = p.

    `Q(eta) = 0.5*erfc(eta/sqrt(2))`, so this is `sqrt(2)*erfcinv(2p)`.
    Everything below is expressed in it, and a caller thresholding its own
    zero-mean Gaussian statistic wants `det_q_inv(pfa) * sd_H0`.

    **Signed, and that matters.** Above the median the quantile is
    negative, which is exactly why det_dwell_gauss()'s `Q_inv(pfa) -
    Q_inv(pd)` is a sum of two tails rather than a difference: every
    caller's `pd` is above 0.5. Clamping it to zero there halves the dwell
    without failing anything.

    Parameters
    ----------
    p : float
        Tail probability in (0, 1).

    Returns
    -------
    float
        Quantile in H0 sigmas -- positive below the median, exactly 0 at
        it, negative above. Fails closed (0.0) for p outside (0, 1).

    Examples
    --------
    >>> from doppler.detection import det_q_inv, det_threshold
    >>> round(det_q_inv(p=5e-6), 4)     # the carrier lock metric's 4.42 sigma
    4.4172
    >>> round(det_q_inv(p=0.5), 4)      # the median
    0.0
    >>> round(det_q_inv(p=0.99), 4)     # above it: NEGATIVE, by design
    -2.3263
    >>> round(det_threshold(pfa=5e-6), 4)   # the OTHER law -- not this one
    4.9409

    """

def det_dwell_gauss(mean: float, var: float, pd: float, pfa: float) -> int:
    """Looks a Gaussian statistic must average to separate H1 from H0.

    The classic sizing: with a per-look H0 variance var and an H1 mean mean
    (H0 mean zero), block-averaging `n` looks shrinks the H0 spread as
    `1/n`, and the smallest `n` whose H0 and H1 tails clear both budgets is

    `n = var * ((Q_inv(pfa) - Q_inv(pd)) / mean)^2`

    `Q_inv(pd)` is negative for `pd > 0.5`, so the difference is the total
    separation both tails must fit inside.

    Parameters
    ----------
    mean : float
        H1 mean of one look, > 0 (H0 mean is taken as zero).
    var : float
        H0 variance of one look, > 0.
    pd : float
        Required detection probability, in (0, 1).
    pfa : float
        Allowed false-alarm probability, in (0, 1) and below pd.

    Returns
    -------
    int
        Looks needed, rounded up and clamped to >= 1; -1 on invalid input.

    Examples
    --------
    >>> from doppler.detection import det_dwell_gauss
    >>> det_dwell_gauss(mean=0.4, var=0.5, pd=0.99, pfa=1e-5)
    136
    >>> det_dwell_gauss(mean=0.8, var=0.5, pd=0.99, pfa=1e-5)   # 2x mean
    34
    >>> det_dwell_gauss(mean=0.0, var=0.5, pd=0.99, pfa=1e-5)   # no signal
    -1

    """

def det_threshold_gauss(mean: float, pd: float, pfa: float) -> float:
    """Declare threshold for a Gaussian statistic sized by det_dwell_gauss.

    The crossover point that meets both budgets at once, in the statistic's
    own units:

    `thresh = Q_inv(pfa) * mean / (Q_inv(pfa) - Q_inv(pd))`

    Independent of the variance and of the look count -- those set how many
    looks are needed to reach this point, not where it is.

    Parameters
    ----------
    mean : float
        H1 mean of one look, > 0.
    pd : float
        Required detection probability, in (0, 1).
    pfa : float
        Allowed false-alarm probability, in (0, 1) and below pd.

    Returns
    -------
    float
        Threshold in the statistic's units; 0.0 on invalid input.

    Examples
    --------
    >>> from doppler.detection import det_threshold_gauss
    >>> round(det_threshold_gauss(mean=0.4, pd=0.99, pfa=1e-5), 4)
    0.2588
    >>> round(det_threshold_gauss(mean=0.8, pd=0.99, pfa=1e-5), 4)  # scales
    0.5176

    """

def det_ema_alpha(snr_in_db: float, snr_out_db: float) -> float:
    """EMA coefficient for a target estimator SNR (DC level in noise).

    Sizes a first-order EMA `y = (1-alpha)*y + alpha*x` that estimates a DC
    level from noisy i.i.d. measurements x. Per sample the estimator SNR
    (mean^2 / variance) is `snr_in`; the EMA improves it by its variance
    reduction `(2-alpha)/alpha`, so the output SNR is `snr_out = snr_in *
    (2-alpha)/alpha`. Solving for the coefficient:

    alpha = 2 * snr_in / (snr_in + snr_out) (SNRs linear)

    Returns 1.0 (no averaging) when snr_out_db <= snr_in_db. Typical
    inputs: a signal-free power reference |n|^2 is exponential (0 dB per
    sample); a lock signal at known C/N0 has per-look SNR from its coherent
    integration (minus squaring loss), and this picks the smoothing
    bandwidth that makes the lock decision variable meet a chosen decision
    SNR.

    Parameters
    ----------
    snr_in_db : float
        Per-sample estimator SNR, dB (mean^2 / variance).
    snr_out_db : float
        Desired EMA-output estimator SNR, dB.

    Returns
    -------
    float
        EMA coefficient alpha in (0, 1].

    Examples
    --------
    >>> from doppler.detection import det_ema_alpha
    >>> det_ema_alpha(0.0, 0.0)      # no gain requested -> no averaging
    1.0
    >>> round(1 / det_ema_alpha(0.0, 20.0), 1)   # 20 dB gain ~ 50 looks
    50.5
    >>> round(1 / det_ema_alpha(10.0, 30.0), 1)  # same 20 dB gain, shifted
    50.5

    """

def det_verify_count(p_look: float, p_target: float) -> int:
    """Verify count: consecutive looks needed to compound to a budget.

    n consecutive independent looks at per-look probability p compound to
    ~p^n, so the smallest n with `p_look^n <= p_target` is `ceil(ln
    p_target / ln p_look)` (clamped to >= 1).

    That `~` is a BUDGET, and deliberately the conservative side of one: a
    consecutive-run detector's exact declare rate is `p^n (1-p)/(1-p^n)`
    (lockdet_core.h), which is lower, so sizing on p^n over-provisions n
    rather than under. The gap is ~p -- negligible where a detector is
    really sized, 10% at p = 0.1 -- so pick n here and predict what a
    caller will observe with det_verify_delay().

    One function serves both sides of a lock detector (lockdet_core.h): the
    declare count from (per-look pfa, false-declare budget) and the drop
    count from (per-look miss rate 1 - pd, false-drop budget). Degenerate
    inputs resolve naturally: a target already met by one look returns 1;
    p_look >= 1 can never compound below a smaller target and returns
    INT_MAX.

    Parameters
    ----------
    p_look : float
        Per-look probability (pfa or 1 - pd), in (0, 1).
    p_target : float
        Compound probability budget, in (0, 1).

    Returns
    -------
    int
        Smallest verify count n with p_look^n <= p_target.

    Examples
    --------
    >>> from doppler.detection import det_verify_count
    >>> det_verify_count(1e-3, 1e-6)   # two 1e-3 looks reach 1e-6
    2
    >>> det_verify_count(1e-3, 1e-9)
    3
    >>> det_verify_count(0.5, 1e-3)    # drop side: pd = 0.5 per look
    10
    >>> det_verify_count(1e-3, 0.5)    # budget already met -> 1
    1

    """

def det_verify_delay(p_look: float, n: int) -> float:
    """Expected looks until a run of n consecutive successes completes.

    The mean waiting time of the consecutive-run process a lockdet verify
    counter implements: at per-look success probability p, the first run of
    n straight successes takes on average

    `E[T]` = (1 - p^n) / (p^n * (1 - p)) looks,

    which is the declare latency bought by a verify count of n (multiply by
    the look period for time). Limits are handled exactly: p = 1 gives n
    (the run completes immediately), p = 0 gives infinity.

    Parameters
    ----------
    p_look : float
        Per-look success probability (e.g. pd), in &#91;0, 1&#93;.
    n : int
        Run length (the verify count); clamped to >= 1.

    Returns
    -------
    float
        Expected number of looks to the first length-n run.

    Examples
    --------
    >>> from doppler.detection import det_verify_delay
    >>> det_verify_delay(1.0, 8)             # certain hits: exactly n
    8.0
    >>> round(det_verify_delay(0.5, 2), 6)   # 2 straight coin heads: 6
    6.0
    >>> round(det_verify_delay(0.9, 8), 1)
    13.2

    """

def det_threshold_f(pfa: float, n: int) -> float:
    """Upper quantile of F(n, n) — the exact H0 law for a ratio test whose
    noise reference is estimated from as many samples as the signal sum.

    A chi-square threshold (det_threshold_noncoherent) prices a statistic
    normalised by a KNOWN noise power. When the noise power is instead
    estimated from n same-burst samples (the BurstDespreader lock test: sum
    Re^2 against sum Im^2), the ratio's tail fattens to F(n, n) and the
    chi-square gate realizes tens of times the priced pfa (41x at n = 16,
    pfa = 1e-3). This helper returns the exact gate: P(chi2_n / chi2_n > g)
    = I_{1/(1+g)}(n/2, n/2) = pfa, solved on the regularized incomplete
    beta — valid for every n >= 1, odd included. As n grows the estimate
    hardens and g approaches the known-noise value. Threshold a
    BurstDespreader as `lock_stat > sqrt(stat_n * det_threshold_f(pfa,
    stat_n))`.

    Parameters
    ----------
    pfa : float
        Tail probability budget, in (0, 1).
    n : int
        Degrees of freedom on each side (>= 1).

    Returns
    -------
    float
        The F(n, n) upper-pfa quantile; 0 on invalid input.

    Examples
    --------
    >>> from doppler.detection import det_threshold_f
    >>> round(det_threshold_f(1e-3, 2), 6)  # exact: (1 - pfa)/pfa
    999.0
    >>> round(det_threshold_f(1e-3, 4), 4)
    53.4358
    >>> round(det_threshold_f(1e-3, 64), 4)  # hardens toward known-noise
    2.1931

    """

def det_pd_noncoherent(
    snr: float,
    n_coh: int,
    n_noncoh: int,
    threshold: float,
) -> float:
    r"""Detection probability for n_noncoh non-coherent looks.

    Computes Pd = Q_{n_noncoh}(a, threshold) with the non-centrality a =
    sqrt(2 * n_coh * n_noncoh) * snr. At n_noncoh = 1 this is exactly
    det_pd(snr, n_coh, threshold); at snr = 0 it returns the per-test Pfa.

    Parameters
    ----------
    snr : float
        Per-sample amplitude SNR (signal / noise amplitude).
    n_coh : int
        Coherent integration length in samples (dwell * N).
    n_noncoh : int
        Number of non-coherent looks; must be >= 1.
    threshold : float
        Threshold eta_nc, e.g. from det_threshold_noncoherent().

    Returns
    -------
    float
        Detection probability in &#91;0, 1&#93;.

    Examples
    --------
    >>> from doppler.detection import det_pd_noncoherent, det_pd
    >>> from doppler.detection import det_threshold_noncoherent
    >>> from doppler.detection import det_threshold
    >>> eta = det_threshold(pfa=1e-6)
    >>> det_pd_noncoherent(snr=0.5, n_coh=8, n_noncoh=1, threshold=eta) \
    ...     == det_pd(snr=0.5, dwell=8, threshold=eta)  # -> coherent
    True
    >>> eta4 = det_threshold_noncoherent(pfa=1e-3, n_noncoh=4)
    >>> round(det_pd_noncoherent(
    ...     snr=0.3, n_coh=16, n_noncoh=4, threshold=eta4), 2)
    0.19

    """

def det_n_noncoh(
    snr: float,
    n_coh: int,
    pd_min: float,
    pfa: float,
    max_n_noncoh: int,
) -> int:
    """Minimum non-coherent looks achieving Pd >= pd_min at fixed n_coh.

    Iterates n_noncoh = 1, 2, ..., max_n_noncoh, recomputing the threshold
    (det_threshold_noncoherent, which grows with the look count) at each
    step. Returns the first look count that meets the Pd requirement, or -1
    if none does within max_n_noncoh. Used by the acquisition engine's (M,
    N_nc) split.

    Parameters
    ----------
    snr : float
        Per-sample amplitude SNR (linear).
    n_coh : int
        Coherent integration length in samples (dwell * N).
    pd_min : float
        Required detection probability, e.g. 0.9.
    pfa : float
        Per-test false-alarm probability.
    max_n_noncoh : int
        Search upper bound on the look count.

    Returns
    -------
    int
        Minimum n_noncoh >= 1, or -1 if not achievable.

    Examples
    --------
    >>> from doppler.detection import det_n_noncoh
    >>> det_n_noncoh(
    ...     snr=2.0, n_coh=16, pd_min=0.9, pfa=1e-3, max_n_noncoh=64)
    1

    """

def det_threshold_power(pfa: float) -> float:
    """Power threshold p from Pfa for the power detector.

    Exact closed-form: P(Exponential(1) > p) = exp(-p) = Pfa, so

    p = -ln(Pfa)

    Parameters
    ----------
    pfa : float
        Desired false-alarm probability; must be in (0, 1).

    Returns
    -------
    float
        Threshold p > 0.

    Examples
    --------
    >>> from doppler.detection import det_threshold_power
    >>> round(det_threshold_power(pfa=1e-6), 3)   # -ln(1e-6) = 6*ln(10)
    13.816

    """

def det_pd_power(
    snr_power: float,
    dwell: int,
    power_threshold: float,
) -> float:
    """Detection probability for the power detector.

    Pd = Q_1(sqrt(2·dwell·snr_power), sqrt(2·power_threshold))

    The result equals det_pd() at the equivalent amplitude SNR: power SNR
    `s` corresponds to amplitude SNR `sqrt(s)`, and the Q_1 arguments
    match.

    Parameters
    ----------
    snr_power : float
        Per-sample power SNR (signal power / noise power at the correlator
        output, linear). 0 gives Pd = Pfa.
    dwell : int
        Coherent integration depth; must be >= 1.
    power_threshold : float
        Threshold p, e.g. from det_threshold_power().

    Returns
    -------
    float
        Detection probability in &#91;0, 1&#93;.

    Examples
    --------
    >>> from doppler.detection import det_pd_power, det_threshold_power
    >>> thr = det_threshold_power(pfa=1e-6)
    >>> round(det_pd_power(
    ...     snr_power=2.6017, dwell=8, power_threshold=thr), 2)
    0.9

    """

def det_dwell_power(
    snr_power: float,
    pd_min: float,
    pfa: float,
    max_dwell: int,
) -> int:
    """Minimum dwell such that Pd >= pd_min for the power detector.

    Parameters
    ----------
    snr_power : float
        Per-sample power SNR (linear).
    pd_min : float
        Required detection probability.
    pfa : float
        False-alarm probability; used to derive p.
    max_dwell : int
        Search upper bound.

    Returns
    -------
    int
        Minimum dwell >= 1, or -1 if not achievable.

    Examples
    --------
    >>> from doppler.detection import det_dwell_power
    >>> det_dwell_power(
    ...     snr_power=0.25, pd_min=0.9, pfa=1e-6, max_dwell=256)
    84

    """

def det_snr_power(dwell: int, pd_min: float, pfa: float) -> float:
    """Minimum per-sample power SNR achieving Pd >= pd_min.

    Parameters
    ----------
    dwell : int
        Coherent integration depth; must be >= 1.
    pd_min : float
        Required detection probability.
    pfa : float
        False-alarm probability.

    Returns
    -------
    float
        Minimum power SNR >= 0.

    Examples
    --------
    >>> from doppler.detection import (det_snr_power, det_pd_power,
    ...                                det_threshold_power)
    >>> sp = det_snr_power(dwell=8, pd_min=0.9, pfa=1e-6)
    >>> round(sp, 4)
    2.6017
    >>> pd = det_pd_power(snr_power=sp, dwell=8,
    ...                   power_threshold=det_threshold_power(pfa=1e-6))
    >>> abs(pd - 0.9) < 1e-9   # det_snr_power inverts det_pd_power
    True

    """
