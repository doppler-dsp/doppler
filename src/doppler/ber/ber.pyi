# ber/ber.pyi — type stubs for the ber C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class BerInterval(tuple[float, float, float, float, float, int, int]):
    """Error-rate point estimate with a Gamma/chi-square confidence interval.
    Assert on `lo`, never `p_hat`.

    Attributes
    ----------
    p_hat : float
        Unbiased point estimate `(r-1)/(N-1)`.
    lo : float
        Lower confidence limit.
    hi : float
        Upper confidence limit.
    rel : float
        Relative standard error `1/sqrt(r)`.
    conf : float
        config: confidence level for the interval
    errors : int
        running: frames not delivered
    symbols : int
        `N` (or bits, for a BER).
    """

    @property
    def p_hat(self) -> float:
        """Unbiased point estimate `(r-1)/(N-1)`."""

    @property
    def lo(self) -> float:
        """Lower confidence limit."""

    @property
    def hi(self) -> float:
        """Upper confidence limit."""

    @property
    def rel(self) -> float:
        """Relative standard error `1/sqrt(r)`."""

    @property
    def conf(self) -> float:
        """config: confidence level for the interval"""

    @property
    def errors(self) -> int:
        """running: frames not delivered"""

    @property
    def symbols(self) -> int:
        """`N` (or bits, for a BER)."""

@final
class BerMeter:
    """Create a meter for constellation m stopping at target_errors.

    Parameters
    ----------
    m : int, default 4
        Constellation order (2, 4, 8).
    target_errors : int, default 200
        Inverse-binomial stop condition; 0 selects 200.
    conf : float, default 0.99
        Two-sided confidence level; 0 selects 0.99.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``m must be 2, 4 or 8
        and conf must lie in (0, 1)``.

    Examples
    --------
    Create with defaults:

    >>> from doppler.ber import BerMeter
    >>> obj = BerMeter(m=4, target_errors=200, conf=0.99)

    """
    def __init__(
        self,
        m: int = ...,
        target_errors: int = ...,
        conf: float = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Zero the running counters; keep the configuration and the truth.

        Returns the meter to a fresh count while preserving m, the error
        target, the confidence level and the installed truth sequence, so one
        meter can measure independent captures back to back without
        reinstalling truth. The last detected alignment is left untouched; call
        align() again for the next capture before scoring it.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ber import BerMeter
        >>> rng = np.random.default_rng(0)
        >>> truth = rng.integers(0, 4, size=400).astype(np.uint8)
        >>> ang = 2 * np.pi * truth / 4 + np.pi / 4
        >>> rx = np.exp(1j * ang).astype(np.complex64)
        >>> met = BerMeter(m=4)
        >>> met.set_truth(truth)
        >>> met.align(rx, n_marker=64)
        1
        >>> met.score(rx, hi=truth.size)
        336
        >>> met.symbols
        336
        >>> met.reset()               # reuse the meter for the next capture
        >>> (met.errors, met.symbols)
        (0, 0)

        """

    def set_truth(self, truth: NDArray[np.uint8]) -> None:
        """Install the transmitted symbol INDICES (0..m-1, not Gray labels)
        this meter scores against. Copied, so the caller's buffer need not
        outlive the call, and reused across every burst. Raises ValueError if
        any index is outside 0..m-1.

        Copied, so the caller's buffer need not outlive the call, and reused
        across every burst. Values are symbol INDICES in `0..m-1` (not Gray
        labels): the meter Gray-encodes each side itself when it counts bit
        errors, so handing it Gray labels would double-encode and inflate the
        rate.

        Parameters
        ----------
        truth : NDArray[np.uint8]
            Transmitted symbol indices, each in `0..m-1`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_truth failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ber import BerMeter
        >>> met = BerMeter(m=4)
        >>> truth = np.array(
        ...     [0, 3, 1, 2, 2, 0], dtype=np.uint8)  # indices, 0..3
        >>> met.set_truth(truth)
        >>> met.set_truth(np.array([9], dtype=np.uint8))  # 9 is not in 0..3
        Traceback (most recent call last):
        ValueError: set_truth failed (rc=-4)

        """

    def align(
        self,
        rx: NDArray[np.complex64],
        t0: int = 0,
        n_marker: int = 0,
        period: int = 0,
        lag_span: int = 0,
        pfa: float = 0.0,
    ) -> int:
        """Detect where the recovered symbols sit against truth, returning a
        BerAlign. The alignment is DETECTED by correlating a known marker --
        truth[t0 : t0+n_marker], optionally repeating every `period` symbols --
        and gated by a false-alarm probability, NOT searched by minimising the
        error count. That distinction is the whole point: a min-over-(lag,
        rotation) search is an optimisation over the answer, and it both
        false-passes on a lucky alignment and false-floors when the true lag
        falls outside the span. A marker too short to identify an alignment
        returns ok=False rather than a plausible wrong lag. Repeats are
        combined non-coherently, which raises the processing gain and exposes
        cycle slips.

        Correlates the known marker `truth[t0 .. t0 + n_marker)` against rx
        over a span of lags, gates the peak with a false-alarm probability, and
        stores the winning lag, absolute carrier phase and marker geometry on
        the meter so score() later uses exactly this detection — never a lag
        searched to minimise the error count. The peak's phase is the ABSOLUTE
        constellation rotation, so no M-fold ambiguity is left to resolve; a
        marker too short to clear the gate reports failure rather than a
        plausible wrong lag. Read the outcome through align_ok, lag, phase and
        align_margin_db.

        Parameters
        ----------
        rx : NDArray[np.complex64]
            Recovered symbols to align against the truth.
        t0 : int
            Truth index of the marker's first occurrence.
        n_marker : int
            Marker length in symbols; 0 selects BER_SYNC_SYMS.
        period : int
            Repeat period in symbols; 0 for a single occurrence.
        lag_span : int
            Search half-width in symbols; 0 selects BER_LAG_SPAN.
        pfa : float
            Whole-search false-alarm probability; 0 selects 1e-6.

        Returns
        -------
        int
            1 when the detection passed its false-alarm gate, else 0.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ber import BerMeter
        >>> rng = np.random.default_rng(0)
        >>> truth = rng.integers(0, 4, size=600).astype(np.uint8)
        >>> ang = 2 * np.pi * truth / 4 + np.pi / 4
        >>> rx = np.exp(1j * ang).astype(np.complex64)
        >>> met = BerMeter(m=4)
        >>> met.set_truth(truth)
        >>> met.align(rx, n_marker=64)     # correlate a 64-symbol marker
        1
        >>> met.lag, met.align_ok          # detected, so score() is valid
        (0, 1)

        """

    def score(
        self,
        rx: NDArray[np.complex64],
        lo: int = 0,
        hi: int = 0,
    ) -> int:
        """Score rx[lo:hi] against the truth and accumulate; returns the
        symbols scored. Uses the supplied alignment VERBATIM -- no lag search,
        no rotation search, no minimisation of any kind over the answer. Uses
        the alignment align() last detected, together with the marker geometry
        that found it, so a measurement cannot be handed an alignment belonging
        to a different burst. Symbols covered by a marker occurrence are
        excluded, as are any whose truth index falls outside the installed
        sequence; both land in `skipped`.

        Demodulates each symbol in the window under the alignment the last
        align() detected — its lag and absolute phase — and tallies symbol and
        Gray-coded bit errors against the installed truth. The alignment is
        used VERBATIM: no lag search, no rotation search, no minimisation of
        any kind over the answer. Symbols covered by a marker occurrence are
        excluded, as are any whose truth index falls outside the installed
        sequence; both land in skipped.

        Parameters
        ----------
        rx : NDArray[np.complex64]
            Recovered symbols to score.
        lo : int
            First symbol index to score (inclusive).
        hi : int
            One past the last symbol index to score; clamped to rx_len. `hi =
            0` scores nothing, so pass the window's true end.

        Returns
        -------
        int
            Symbols actually scored (window length minus skipped symbols).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ber import BerMeter
        >>> rng = np.random.default_rng(1)
        >>> truth = rng.integers(0, 4, size=800).astype(np.uint8)
        >>> ang = 2 * np.pi * truth / 4 + np.pi / 4
        >>> rx = np.exp(1j * ang).astype(np.complex64)
        >>> met = BerMeter(m=4)
        >>> met.set_truth(truth)
        >>> met.align(rx, n_marker=64)
        1
        >>> met.score(rx, hi=truth.size)   # the 64 marker symbols are excluded
        736
        >>> met.errors, met.skipped
        (0, 64)

        """

    def ser(self) -> BerInterval:
        """Symbol error rate with its EXACT confidence interval, as a
        BerInterval. Assert on `lo`, never on `p_hat`: comparing the lower
        limit against a spec is the form that cannot flake on counting noise.
        The interval is the Gamma/chi-square one for inverse binomial sampling
        -- no normal approximation anywhere, so it stays honest at the small
        error counts where a Wald interval is worst -- and its quantiles come
        from doppler's own detection primitives rather than a second copy of an
        incomplete-gamma kernel.

        Divides the accumulated symbol-error count by the symbols scored and
        wraps it in the exact Gamma/chi-square interval for inverse binomial
        sampling — no normal approximation anywhere. Assert on lo, never on
        p_hat: comparing the lower limit against a spec is the form that cannot
        flake on counting noise.

        Returns
        -------
        BerInterval
            A BerInterval record — `(p_hat, lo, hi, rel, conf, errors,
            symbols)` — the symbol error rate with its exact two-sided limits.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ber import BerMeter
        >>> rng = np.random.default_rng(1)
        >>> truth = rng.integers(0, 4, size=800).astype(np.uint8)
        >>> ang = 2 * np.pi * truth / 4 + np.pi / 4
        >>> rx = np.exp(1j * ang).astype(np.complex64)
        >>> rx[200:260:5] *= -1            # corrupt 12 symbols (pi rotation)
        >>> met = BerMeter(m=4)
        >>> met.set_truth(truth)
        >>> met.align(rx, n_marker=64)
        1
        >>> _ = met.score(rx, hi=truth.size)
        >>> r = met.ser()
        >>> r.errors, r.symbols
        (12, 736)
        >>> round(r.lo, 4)                 # assert on lo, never on p_hat
        0.0067

        """

    def ber(self) -> BerInterval:
        """Gray-coded bit error rate with its EXACT confidence interval, as a
        BerInterval. Same statistics as ser(), counted over bits.

        The same exact statistics as ser(), counted over Gray-coded bits rather
        than symbols, so a QPSK/8PSK symbol error contributes as many bit
        errors as its Gray labels differ by. Assert on lo, never on p_hat.

        Returns
        -------
        BerInterval
            A BerInterval record — `(p_hat, lo, hi, rel, conf, errors,
            symbols)` — the bit error rate with its exact two-sided limits
            (`errors` and `symbols` are bit counts here).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ber import BerMeter
        >>> rng = np.random.default_rng(1)
        >>> truth = rng.integers(0, 4, size=800).astype(np.uint8)
        >>> ang = 2 * np.pi * truth / 4 + np.pi / 4
        >>> rx = np.exp(1j * ang).astype(np.complex64)
        >>> rx[200:260:5] *= -1            # corrupt 12 symbols
        >>> met = BerMeter(m=4)
        >>> met.set_truth(truth)
        >>> met.align(rx, n_marker=64)
        1
        >>> _ = met.score(rx, hi=truth.size)
        >>> r = met.ber()                  # as ser(), but over bits
        >>> r.errors, r.symbols
        (24, 1472)
        >>> round(r.lo, 4)
        0.009

        """

    def interval(self, errors: int, symbols: int) -> BerInterval:
        """The exact confidence interval for error/trial counts gathered
        elsewhere, at this meter's confidence level. Same statistics as ser():
        the Gamma/chi-square interval for inverse binomial sampling, with
        quantiles from doppler's own inverse regularized incomplete gamma
        rather than a normal approximation, so it stays honest at the small
        error counts where a Wald interval is worst. Assert on `lo`, never on
        `p_hat`.

        The pure-function face of the meter's statistics, at this meter's own
        confidence level: hand it any error and trial counts and it returns the
        same exact Gamma/chi-square interval ser() would, with quantiles from
        doppler's own inverse regularized incomplete gamma rather than a normal
        approximation, so it stays honest at the small error counts where a
        Wald interval is worst. Assert on lo, never on p_hat.

        Parameters
        ----------
        errors : int
            Errors counted, `r`.
        symbols : int
            Trials counted, `N` (symbols, or bits for a BER).

        Returns
        -------
        BerInterval
            A BerInterval record — `(p_hat, lo, hi, rel, conf, errors,
            symbols)` — the unbiased rate with its exact two-sided limits.

        Examples
        --------
        >>> from doppler.ber import BerMeter
        >>> met = BerMeter(m=4, conf=0.99)
        >>> ci = met.interval(errors=8, symbols=20000)   # external counts
        >>> round(ci.p_hat, 6), round(ci.lo, 6), round(ci.hi, 6)
        (0.00035, 0.000129, 0.000857)

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the BerMeter has already been destroyed.

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

        Raises ``RuntimeError`` if the BerMeter has already been destroyed.

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
        ``RuntimeError`` if the BerMeter has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def errors(self) -> int:
        """Symbol errors counted so far."""

    @property
    def symbols(self) -> int:
        """Symbols scored so far."""

    @property
    def bit_errors(self) -> int:
        """Gray-coded bit errors counted so far."""

    @property
    def bits(self) -> int:
        """Bits scored so far."""

    @property
    def skipped(self) -> int:
        """Symbols skipped: covered by a marker occurrence, or with a truth
        index outside the installed sequence. Marker symbols are excluded on
        purpose -- they are known, so scoring them would flatter the rate with
        symbols that had no chance of being wrong.
        """

    @property
    def m(self) -> int:
        """Constellation order."""

    @property
    def target_errors(self) -> int:
        """The inverse-binomial stop condition."""

    @property
    def conf(self) -> float:
        """Two-sided confidence level used by ser() and ber()."""

    @property
    def enough(self) -> int:
        """True once target_errors have been counted. THE stop condition for a
        measurement loop: fix the ERROR count and let the symbol count fall out
        (inverse binomial sampling), so the relative standard error is
        1/sqrt(r) -- a function of the error count alone. Stopping on a fixed
        symbol count instead makes the precision depend on the very rate being
        measured: 20000 symbols at SER 1e-3 gives ~20 errors and ~22% relative
        error, which reads as real seed-to-seed variation in the receiver and
        is not.
        """

    @property
    def lag(self) -> int:
        """Alignment from the last align(): rx[i] carries truth[i + lag]."""

    @property
    def phase(self) -> float:
        """Absolute residual constellation rotation from the last align(),
        radians. The marker resolves this outright, so there is no M-fold
        ambiguity left to search over.
        """

    @property
    def align_stat(self) -> float:
        """Detection statistic at the correlation peak, in threshold units."""

    @property
    def align_margin_db(self) -> float:
        """20*log10(stat/threshold) -- headroom over the false-alarm gate.
        Negative means the alignment was NOT detected.
        """

    @property
    def align_runner_db(self) -> float:
        """Peak over the best runner-up lag, dB. Under ~3 dB the peak is
        ambiguous and the alignment is rejected.
        """

    @property
    def align_occurrences(self) -> int:
        """Marker occurrences combined non-coherently."""

    @property
    def align_slips(self) -> int:
        """Occurrences whose phase disagreed with the peak by more than half a
        decision sector -- cycle slips.
        """

    @property
    def align_saturated(self) -> int:
        """True when the peak sat on an edge of the lag search: lag_span is too
        small and the result is not trustworthy.
        """

    @property
    def align_ok(self) -> int:
        """True when the last align() was detected, unambiguous and
        unsaturated. score() is meaningless unless this is True.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "BerMeter":
        """Enter a context manager, returning this object.

        Lets a BerMeter be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        BerMeter
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the BerMeter.

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
class FrameMeter:
    """Create an accumulator.

    Parameters
    ----------
    target_errors : int, default 200
        frame errors to accumulate before `enough`; 0 is taken as
        BER_TARGET_ERRORS.
    conf : float, default 0.99
        confidence level in (0, 1); 0 is taken as BER_CONF.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``conf must lie in (0,
        1)``.

    Examples
    --------
    Create with defaults:

    >>> from doppler.ber import FrameMeter
    >>> obj = FrameMeter(target_errors=200, conf=0.99)

    """
    def __init__(
        self,
        target_errors: int = ...,
        conf: float = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Clear every counter; the configuration is untouched.

        The target and the confidence level are what the caller asked for, so
        resetting the accumulation must not silently re-negotiate them. Use it
        between records, or to discard a run that turned out to be measuring
        the wrong thing.

        Examples
        --------
        >>> from doppler.ber import FrameMeter
        >>> met = FrameMeter(target_errors=10)
        >>> met.add(1, 0)
        >>> met.frames, met.errors
        (1, 1)
        >>> met.reset()
        >>> met.frames, met.errors
        (0, 0)

        """

    def add(self, sync_ok: int, crc: int) -> None:
        """Record one frame's outcome. `sync_ok` is the DETECTOR's own decision
        (ber_align_t.ok, or burst_demod's frame offset validity) -- never a
        threshold applied afterwards to a statistic. `crc` is
        wfm_frame_crc_ok()'s return passed straight through: 1 pass, 0 fail, -1
        the frame carries no CRC. A frame is an error when its sync was not
        detected, or when it was and the CRC failed; with no CRC carried, a
        detected frame counts as delivered, because counting it as an error
        would measure the frame format rather than the receiver.

        Parameters
        ----------
        sync_ok : int
            non-zero when the frame's sync word was detected. Pass the
            detector's own decision — `ber_align_t::ok`, or `burst_demod`'s
            frame_offset validity — never a threshold applied afterwards to a
            statistic.
        crc : int
            `wfm_frame_crc_ok()`'s return, passed straight through: 1 pass, 0
            fail, -1 the frame carries no CRC. A frame counts as an error when
            its sync was not detected, or when it was and the CRC failed. With
            `crc = -1` a detected frame counts as delivered, because nothing
            about it can be checked.

        Examples
        --------
        >>> from doppler.ber import FrameMeter
        >>> met = FrameMeter(target_errors=10)
        >>> met.add(1, 1)    # found, and it checked
        >>> met.add(1, 0)    # found, and the CRC failed
        >>> met.add(0, 0)    # never found: still a frame you did not deliver
        >>> met.add(1, -1)   # found, no CRC: delivered but not CHECKED
        >>> met.frames, met.sync_detected, met.crc_passed, met.errors
        (4, 3, 1, 2)

        """

    def fer(self) -> BerInterval:
        """Frame error rate with its exact interval, as a BerInterval. Assert
        on `lo`, never on `p_hat`: comparing the lower limit against a spec is
        the form that cannot flake on counting noise. A frame that was never
        detected counts as an error -- a frame you did not detect is a frame
        you did not deliver.

        `ber_confidence(errors, frames, conf)` — the same interval `ber_meter`
        reports, which is generic over trials and therefore applies to frames
        unchanged. Assert on `lo`, never on `p_hat`.

        Returns
        -------
        BerInterval
            the rate with its exact interval.

        Examples
        --------
        >>> from doppler.ber import FrameMeter
        >>> met = FrameMeter(target_errors=4)
        >>> for i in range(20):
        ...     met.add(1, 0 if i % 5 == 0 else 1)
        >>> met.enough
        1
        >>> fer = met.fer()
        >>> round(fer.p_hat, 3), fer.lo < fer.p_hat < fer.hi
        (0.158, True)

        """

    def sync_miss(self) -> BerInterval:
        """Sync MISS rate with its exact interval, as a BerInterval. Reported
        as a miss rather than a detection rate so it is an ERROR rate like
        every other number in this module and the same interval applies
        unchanged. This is what turns 'is this sync word long enough at this
        Es/N0' into a measurement rather than a judgement.

        Reported as a miss rate rather than a detection rate so it is an ERROR
        rate like every other number here, and so the same interval applies
        without reinterpretation. **This is what turns "is this sync word long
        enough at this Es/N0" into a measurement** — `ber_align_detect()`
        already returns `margin_db` and `runner_db` per attempt, and
        accumulating the decisions is what answers the question with a number.

        Returns
        -------
        BerInterval
            the miss rate with its exact interval.

        Examples
        --------
        >>> from doppler.ber import FrameMeter
        >>> met = FrameMeter()
        >>> for i in range(50):
        ...     met.add(0 if i % 10 == 0 else 1, 1)
        >>> met.frames, met.sync_detected
        (50, 45)
        >>> miss = met.sync_miss()
        >>> round(miss.p_hat, 3), miss.hi > miss.p_hat
        (0.082, True)

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the FrameMeter has already been destroyed.

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

        Raises ``RuntimeError`` if the FrameMeter has already been destroyed.

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
        ``RuntimeError`` if the FrameMeter has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def frames(self) -> int:
        """Frames attempted."""

    @property
    def sync_detected(self) -> int:
        """Frames whose sync word was detected."""

    @property
    def crc_passed(self) -> int:
        """Frames whose CRC checked."""

    @property
    def errors(self) -> int:
        """Frames not delivered: no sync detected, or a failed CRC."""

    @property
    def enough(self) -> int:
        """True once `target_errors` frame errors have accumulated -- the
        stopping condition, so a caller loops records until the measurement has
        the precision it asked for rather than until a frame count someone
        guessed.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "FrameMeter":
        """Enter a context manager, returning this object.

        Lets a FrameMeter be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        FrameMeter
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the FrameMeter.

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

def ber_theory_ser(m: int, esn0: float) -> float:
    """Coherent M-PSK symbol error rate at matched-filter Es/N0 (LINEAR,
    not dB). BPSK Q(sqrt(2 Es/N0)); QPSK 2Q(sqrt(Es/N0)); 8PSK 2Q(sqrt(2
    Es/N0) sin(pi/8)). This is a COHERENT bound: a differentially-decoded
    rate is ~2x it, so pairing a differential measurement with this curve
    invents a factor of two of implementation loss.

    `BPSK: Q(sqrt(2 Es/N0))`, `QPSK: 2 Q(sqrt(Es/N0))`, `8PSK: 2 Q(sqrt(2
    Es/N0) sin(pi/8))` — the nearest-neighbour union bound, tight to well
    under a percent at any Es/N0 worth testing at.

    **This is a COHERENT bound.** A differentially-decoded rate is ~2x it,
    because a differential decision fails when either of its two symbols is
    wrong (measured 1.88-2.11 across M and both receiver paths). Pairing a
    differential measurement with this curve invents a factor of two of
    "implementation loss".

    Parameters
    ----------
    m : int
        Input.
    esn0 : float
        Input.

    Returns
    -------
    float
        Output.
    """

def ber_theory_ber(m: int, esn0: float) -> float:
    """Coherent GRAY-coded M-PSK bit error rate at Es/N0 (LINEAR). BPSK and
    Gray QPSK are exactly Q(sqrt(2 Eb/N0)); 8PSK uses SER/log2(M).

    Parameters
    ----------
    m : int
        Input.
    esn0 : float
        Input.

    Returns
    -------
    float
        Output.
    """

def ber_esn0_db_for_ser(m: int, ser: float) -> float:
    """Es/N0 (dB) at which the coherent bound equals `ser`. How an
    implementation loss is quoted honestly: convert the MEASURED rate to
    the Es/N0 theory would need to produce it, and subtract. A loss in dB
    is comparable across M and across operating points; a ratio of rates is
    not.

    How an implementation loss is quoted honestly: convert the MEASURED
    rate to the Es/N0 theory would need to produce it, and subtract. A loss
    in dB is comparable across M and across operating points; a ratio of
    rates is not.

    Parameters
    ----------
    m : int
        Input.
    ser : float
        Input.

    Returns
    -------
    float
        Output.
    """

def ber_evm_scatter_floor_db(m: int) -> float:
    """EVM (dB) of an M-PSK constellation at a UNIFORMLY RANDOM rotation --
    the FLOOR of a self-referenced EVM, i.e. what a completely destroyed
    constellation reads: -1.4 dB at BPSK, -7.0 at QPSK, -12.9 at 8PSK. ANY
    fixed EVM threshold must be stated against this, never against 0 dB:
    'scattered reads ~0 dB' is the BPSK limit only, and at 8PSK a stream
    with no carrier recovery reads the same -12.9 dB a healthy 13 dB link
    does, so a `< -12.0` assertion is satisfied by pure noise. The room
    between 'on the bound at the SER=1e-3 anchor' and 'completely broken'
    collapses as M grows (5.4 / 3.3 / 2.8 dB), so at high M the EVM cannot
    carry a verdict alone. Not to be confused with the NOISE floor
    -(Es/N0).

    The FLOOR of a self-referenced EVM: what a completely destroyed
    constant-modulus constellation reads. Slicing a unit-modulus point at a
    uniformly random phase to its nearest of M neighbours leaves `E|e|^2 =
    2 - 2 sin(pi/M)/(pi/M)`: **-1.4 dB at BPSK, -7.0 at QPSK, -12.9 at
    8PSK**.

    **Any fixed EVM threshold must be stated against this, never against 0
    dB.** "Scattered reads ~0 dB" is the BPSK limit only. At 8PSK a stream
    with no carrier recovery at all reads -12.9 dB — which is also what a
    perfectly healthy 13 dB link reads — so a `< -12.0` assertion is
    satisfied by pure noise. That was live in this repo's own receiver
    tests until 2026-07-27. The room between "on the bound at the SER=1e-3
    anchor" and "completely broken" collapses as M grows: 5.4 dB at BPSK,
    3.3 at QPSK, 2.8 at 8PSK, so at high M the EVM cannot carry a verdict
    by itself.

    Parameters
    ----------
    m : int
        Input.

    Returns
    -------
    float
        Output.
    """

def ber_settle_syms(bn_timing: float, bn_carrier: float) -> int:
    """Symbols to discard before a steady-state measurement means anything:
    2*(5/bn_timing + 5/bn_carrier). Three factors, and skipping any
    produces a confident wrong number -- 5/Bn per loop is the standard
    second-order settling time (in SYMBOLS, since both bn are symbol-rate
    normalised); the two budgets ADD because the loops are cascaded (the
    carrier discriminator reads the on-time strobe, so it cannot converge
    until timing has); and the sum DOUBLES for joint tracking. This is a
    FLOOR, not the answer: take the max of it and every lock indicator the
    receiver publishes. Pass a loop's bn as 0 if it is not running.

    `2 * (5/bn_timing + 5/bn_carrier)`. Three factors, and skipping any of
    them produces a confident wrong number: 5/Bn per loop is the standard
    second-order settling time (in symbols, because both `bn` are
    normalised to the SYMBOL rate); the two budgets ADD because the loops
    are CASCADED (the carrier discriminator reads the on-time strobe, so it
    cannot converge until timing has); and the sum DOUBLES for joint
    tracking, where each loop sees the other's transient as a disturbance.

    This is a floor, not the answer — take the max of it and every lock
    indicator the receiver publishes, plus the handover instant again if
    one is enabled. Pass a loop's `bn` as 0 if it is not running.

    Parameters
    ----------
    bn_timing : float
        Input.
    bn_carrier : float
        Input.

    Returns
    -------
    int
        Output.
    """

def ber_lock_symbol(
    flags: NDArray[np.uint8],
    sustain: int = 200,
    min_frac: float = 0.9,
) -> int:
    """First symbol from which a verify-counted lock flag is SUSTAINED, or
    -1 for 'never locked'. Sustained means `sustain` consecutive symbols
    high AND at least `min_frac` of everything after that point high too:
    the run rejects a single lucky decision, the fraction rejects a
    detector that declares early then flaps. Dating the lock by the FINAL
    contiguous run instead is right with no noise and badly wrong with it
    -- one late dip once moved a reported lock from 415 to 2286 and left no
    measurement window at all. The -1 is deliberate: it forces the caller
    to say 'never locked' rather than quietly measure a transient.

    "Sustained" is sustain consecutive symbols high AND at least min_frac
    of everything after that point high too. Both halves carry weight: the
    run rejects a single lucky decision, the fraction rejects a detector
    that declares early then flaps. Dating the lock by the FINAL contiguous
    run instead is right with no noise and badly wrong with it — one late
    dip once moved a reported lock from 415 to 2286 and left no measurement
    window.

    Parameters
    ----------
    flags : NDArray[np.uint8]
        Input.
    sustain : int
        Input.
    min_frac : float
        Input.

    Returns
    -------
    int
        The symbol index, or -1 for "never locked" — the honest answer,
        which forces the caller to say so rather than measure a transient.
    """

def ber_evm_db(
    rx: NDArray[np.complex64],
    lo: int = 0,
    hi: int = 0,
    m: int = 4,
) -> float:
    """Self-referenced EVM (dB) over an EXPLICIT window [lo, hi): each
    symbol against the stream's OWN hard decision, with the constellation
    rotation estimated from the data. References neither the transmitted
    symbols nor a lag, so it cannot be fooled by an alignment search. A
    locked matched-filter output reads EVM_dB ~ -(Es/N0)_dB (an I/Q-plane
    quantity -- no factor of two; quoting one flatters the result by 3 dB).
    Read it against ber_evm_scatter_floor_db(m), NEVER against 0 dB. The
    window is explicit because BER and EVM must be measured on the SAME
    one: a convenience back-half default scores a different window than the
    error rate did, and the two eventually disagree in a way that reads as
    a receiver defect rather than the harness bug it is. Returns 0.0 for a
    window under 20 symbols.

    Scores each symbol against the stream's OWN hard decision, with the
    constellation rotation estimated from the data — so it references
    neither the transmitted symbols nor a lag, and cannot be fooled by an
    alignment search. At a matched-filter output the error vector IS the
    complex noise, so a locked stream reads `EVM[dB] ~ -(Es/N0)[dB]`. EVM
    is an I/Q-plane quantity: there is no factor of two — that belongs to
    an I-only measurement, and quoting it flatters the result by 3 dB.

    **Pass the real `m`**, and read the result against
    ber_evm_scatter_floor_db(m), never against 0 dB.

    The window is EXPLICIT because BER and EVM must be measured on the SAME
    one. A convenience "back half" default silently scores a different
    window than the error rate did, and the two eventually disagree in a
    way that reads as a receiver defect rather than the harness bug it is.

    Parameters
    ----------
    rx : NDArray[np.complex64]
        Input.
    lo : int
        Input.
    hi : int
        Input.
    m : int
        Input.

    Returns
    -------
    float
        EVM in dB, or 0.0 ("no lock") for a window under 20 symbols.
    """

def ber_settle_from(
    budget: int,
    timing_lock: int = -1,
    carrier_lock: int = -1,
) -> int:
    """Where a steady-state measurement may start: max(budget, timing lock,
    carrier lock). The analytic budget and the receiver's own indicators
    are both fallible in the SAME direction, so whichever settles last
    decides. There was a fourth term until doppler#877: a receiver that
    handed the carrier from an NDA discriminator to a decision-directed one
    settled last of all, contributing its instant PLUS the budget again
    (measured on 8PSK: handover at symbol 2525 against a 2000-symbol
    budget, and 5.95x the coherent bound if the window started at 2000
    rather than 4525). No receiver in this library hands over any more, so
    the term went with the handover rather than remaining as an argument
    that could only be passed -1. Pass -1 for an indicator the receiver
    does not publish, which is what ber_lock_symbol() returns for 'never
    locked'. A -1 timing or carrier lock means there is NO valid
    steady-state window -- check that yourself before trusting the return.

    The POLICY for where a steady-state window may start, in one place:
    `max(budget, timing lock, carrier lock)`. The analytic budget and the
    receiver's own indicators are both fallible in the SAME direction, so
    whichever settles last decides.

    There was a fourth term until doppler#877. A receiver that handed the
    carrier from an NDA discriminator to a decision-directed one settled
    last of all — the handover fired after every lock indicator and the new
    loop then had its own transient, so it contributed `its instant + the
    budget again` (measured on 8PSK at its SER=1e-3 anchor: handover at
    symbol 2525 against a 2000-symbol budget, and 5.95x the coherent bound
    if the window started at 2000 rather than 4525). No receiver in this
    library hands over any more, so the term went with the handover rather
    than remaining as an argument that could only be passed -1.

    Pass -1 for any indicator the receiver does not publish (which is what
    ber_lock_symbol() returns for "never locked"). **A -1 timing or carrier
    lock means there is NO valid steady-state window** — check that
    yourself before trusting the return.

    Parameters
    ----------
    budget : int
        ber_settle_syms() of the loops in use.
    timing_lock : int
        ber_lock_symbol() of the timing flag, or -1.
    carrier_lock : int
        ber_lock_symbol() of the carrier flag, or -1.

    Returns
    -------
    int
        First symbol of the measurement window.
    """
