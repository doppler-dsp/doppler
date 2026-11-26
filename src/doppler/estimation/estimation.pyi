# estimation/estimation.pyi — type stubs for the estimation C extension.
from typing import Literal, final

@final
class TimingEstimate(tuple[float, float, float, float, float, int, float]):
    """One feedforward symbol-timing estimate: the eye center's place on the
    estimator's symbol clock, the symbol rate's error against the previous
    block, and how strong the symbol-rate line was.

    Attributes
    ----------
    tau : float
        Where the eye center sits on the estimator's symbol clock, in symbols, in (-1/2, 1/2]: the clock's phase at the instants the stream's power peaks. With a symbol-rate error it is the value at the middle of the block. For a pulse that is not band limited, read at a whole number of samples a symbol, it is off the eye center by where the samples fall in the symbol: up to 0.011 symbols for a rectangle at 4 (the validation report's finding `whole_n_rect_sampling_phase`). 0 when the block has no power.
    rate : float
        How far the eye center moves on the clock each symbol of the clock, in symbols: for symbols that last `sps * (1 + eps)` samples it is `eps / (1 + eps)`. From how far `tau` moved since the previous block, so 0 until `has_rate` is 1, and right only while the timing moves under half a symbol between the two blocks' middles.
    quality : float
        `|c| / sum |x|^2`, in [0, 1]: how much of the block's power is the symbol-rate line. Near 0 with no signal, with no transitions, or with a clock error that turns the line through a full cycle inside the block. 0 when the block has no power.
    power : float
        Mean `|x|^2` over the block.
    symbols : float
        The block's length in symbols of the clock: samples / sps.
    has_rate : int
        1 when a previous block exists to take `rate` against, else 0.
    eye : float
        Where an eye center sits on the INPUT's timeline: the sample index, counted from create or reset and not whole, of the eye center nearest the block's middle. Other eye centers are whole symbols (`sps` samples) either side. On a matched instance the converter's group delay and the lead-in it skipped are taken out, so the index is into the samples the estimate was handed. 0 when the block has no power.
    """

    @property
    def tau(self) -> float:
        """Where the eye center sits on the estimator's symbol clock, in
        symbols, in (-1/2, 1/2]: the clock's phase at the instants the stream's
        power peaks. With a symbol-rate error it is the value at the middle of
        the block. For a pulse that is not band limited, read at a whole number
        of samples a symbol, it is off the eye center by where the samples fall
        in the symbol: up to 0.011 symbols for a rectangle at 4 (the validation
        report's finding `whole_n_rect_sampling_phase`). 0 when the block has
        no power.
        """

    @property
    def rate(self) -> float:
        """How far the eye center moves on the clock each symbol of the clock,
        in symbols: for symbols that last `sps * (1 + eps)` samples it is `eps
        / (1 + eps)`. From how far `tau` moved since the previous block, so 0
        until `has_rate` is 1, and right only while the timing moves under half
        a symbol between the two blocks' middles.
        """

    @property
    def quality(self) -> float:
        """`|c| / sum |x|^2`, in [0, 1]: how much of the block's power is the
        symbol-rate line. Near 0 with no signal, with no transitions, or with a
        clock error that turns the line through a full cycle inside the block.
        0 when the block has no power.
        """

    @property
    def power(self) -> float:
        """Mean `|x|^2` over the block."""

    @property
    def symbols(self) -> float:
        """The block's length in symbols of the clock: samples / sps."""

    @property
    def has_rate(self) -> int:
        """1 when a previous block exists to take `rate` against, else 0."""

    @property
    def eye(self) -> float:
        """Where an eye center sits on the INPUT's timeline: the sample index,
        counted from create or reset and not whole, of the eye center nearest
        the block's middle. Other eye centers are whole symbols (`sps` samples)
        either side. On a matched instance the converter's group delay and the
        lead-in it skipped are taken out, so the index is into the samples the
        estimate was handed. 0 when the block has no power.
        """

@final
class PolynomialPhaseEstimate(tuple[float, float, float]):
    """Polynomial-phase estimate: carrier frequency, chirp rate, and a rough
    SNR confidence.

    Attributes
    ----------
    freq_norm : float
        frequency, cycles/sample, in [-0.5, 0.5).
    rate_norm : float
        chirp rate, cycles/sample^2.
    snr_db : float
        winning-row peak-to-mean (rough confidence).
    """

    @property
    def freq_norm(self) -> float:
        """frequency, cycles/sample, in [-0.5, 0.5)."""

    @property
    def rate_norm(self) -> float:
        """chirp rate, cycles/sample^2."""

    @property
    def snr_db(self) -> float:
        """winning-row peak-to-mean (rough confidence)."""

@final
class SymbolTimingEstimator:
    """Create an estimator whose symbol clock runs at sps samples a symbol.

    Parameters
    ----------
    sps : float, default 4.0
        Samples per symbol of the stream handed to it; finite and above 2
        (default: 4.0).

    Raises
    ------
    ValueError
        If construction fails. The exception message is
        ``SymbolTimingEstimator: invalid parameter (need a finite sps above
        2)``.

    Examples
    --------
    >>> from doppler.estimation import SymbolTimingEstimator
    >>> te = SymbolTimingEstimator(sps=4.0)
    >>> te.sps
    4.0

    """

    def __init__(self, sps: float = 4.0) -> None: ...
    def estimate(self, x: complex, /) -> TimingEstimate:
        """Estimate the symbol timing from one block of samples
        (matched-filtered for SymbolTimingEstimator, as they arrive for
        MatchedSymbolTimingEstimator); returns a TimingEstimate. The block is
        the whole call, so its length sets the estimate's variance; consecutive
        calls stay on one symbol clock, and from the second call on the result
        also carries the symbol rate's error.

        The block is the whole call: its length sets the estimate's variance
        and the time the estimate takes. Consecutive calls stay on one symbol
        clock, and from the second call that had power the result carries the
        symbol rate's error.

        Parameters
        ----------
        x : complex
            Matched-filtered samples, `sps` a symbol.

        Returns
        -------
        TimingEstimate
            The block's estimate.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.estimation import SymbolTimingEstimator
        >>> n = np.arange(4 * 256)
        >>> # power peaking a quarter symbol into each symbol, on any carrier
        >>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * (n / 4 - 0.25)))
        >>> x = (x * np.exp(1j * 0.3 * n)).astype(np.complex64)
        >>> e = SymbolTimingEstimator(sps=4.0).estimate(x)
        >>> round(e.tau, 3), round(e.quality, 3), e.symbols, e.has_rate
        (0.25, 0.25, 256.0, 0)

        """

    def reset(self) -> None:
        """Return the symbol clock to zero and forget the previous block and
        any block in progress; keep `sps`.

        The next estimate is then the first again: its `tau` is on a clock that
        starts at the next sample, and it carries no `rate`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.estimation import SymbolTimingEstimator
        >>> n = np.arange(1024)
        >>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * n / 4)).astype(np.complex64)
        >>> te = SymbolTimingEstimator(sps=4.0)
        >>> te.estimate(x).has_rate, te.estimate(x).has_rate
        (0, 1)
        >>> te.reset()
        >>> te.estimate(x).has_rate
        0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the SymbolTimingEstimator has already been
        destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the SymbolTimingEstimator has already been
        destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the SymbolTimingEstimator has already been
        destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def sps(self) -> float:
        """Samples per symbol of the stream handed to estimate."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "SymbolTimingEstimator":
        """Enter a context manager, returning this object.

        Lets a SymbolTimingEstimator be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        SymbolTimingEstimator
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the SymbolTimingEstimator.

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
class MatchedSymbolTimingEstimator:
    """Create an estimator with a matched filter of its own, for a stream of
    sps samples a symbol that nothing has filtered.

    Parameters
    ----------
    sps : float, default 8.0
        Samples per symbol of the input, any real number of 4 or more.
    pulse : Literal["iandd", "rrc"], default "rrc"
        The pulse the matched filter is built for.
        One of ``"iandd"``, ``"rrc"``.
    beta : float, default 0.35
        Root-raised-cosine roll-off, 0 to 1; ignored for the rectangle.
    span : int, default 8
        One-sided root-raised-cosine span in symbols, 1 or more; ignored for
        the rectangle.

    Raises
    ------
    ValueError
        If construction fails. The exception message is
        ``MatchedSymbolTimingEstimator: invalid parameter (need a finite sps of
        4 or more, pulse iandd or rrc, 0 <= beta <= 1, span >= 1)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.estimation import MatchedSymbolTimingEstimator
    >>> n = np.arange(4096)
    >>> # +1 -1 +1 ... through a root-raised cosine at 8 samples a symbol,
    >>> # with a symbol instant at sample 3 and every 8 after it
    >>> x = (0.25 * np.cos(np.pi * (n - 3.0) / 8.0)).astype(np.complex64)
    >>> te = MatchedSymbolTimingEstimator(sps=8.0, pulse="rrc")
    >>> e = te.estimate(x)
    >>> bool(abs(e.eye % 8.0 - 3.0) < 0.1), e.symbols
    (True, 490.25)

    """

    def __init__(
        self,
        sps: float = 8.0,
        pulse: Literal["iandd", "rrc"] = "rrc",
        beta: float = 0.35,
        span: int = 8,
    ) -> None: ...
    def estimate(self, x: complex, /) -> TimingEstimate:
        """Estimate the symbol timing from one block of samples
        (matched-filtered for SymbolTimingEstimator, as they arrive for
        MatchedSymbolTimingEstimator); returns a TimingEstimate. The block is
        the whole call, so its length sets the estimate's variance; consecutive
        calls stay on one symbol clock, and from the second call on the result
        also carries the symbol rate's error.

        The block is the whole call: its length sets the estimate's variance
        and the time the estimate takes. Consecutive calls stay on one symbol
        clock, and from the second call that had power the result carries the
        symbol rate's error.

        Parameters
        ----------
        x : complex
            Matched-filtered samples, `sps` a symbol.

        Returns
        -------
        TimingEstimate
            The block's estimate.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.estimation import SymbolTimingEstimator
        >>> n = np.arange(4 * 256)
        >>> # power peaking a quarter symbol into each symbol, on any carrier
        >>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * (n / 4 - 0.25)))
        >>> x = (x * np.exp(1j * 0.3 * n)).astype(np.complex64)
        >>> e = SymbolTimingEstimator(sps=4.0).estimate(x)
        >>> round(e.tau, 3), round(e.quality, 3), e.symbols, e.has_rate
        (0.25, 0.25, 256.0, 0)

        """

    def reset(self) -> None:
        """Return the symbol clock to zero and forget the previous block and
        any block in progress; keep `sps`.

        The next estimate is then the first again: its `tau` is on a clock that
        starts at the next sample, and it carries no `rate`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.estimation import SymbolTimingEstimator
        >>> n = np.arange(1024)
        >>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * n / 4)).astype(np.complex64)
        >>> te = SymbolTimingEstimator(sps=4.0)
        >>> te.estimate(x).has_rate, te.estimate(x).has_rate
        (0, 1)
        >>> te.reset()
        >>> te.estimate(x).has_rate
        0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the MatchedSymbolTimingEstimator has already
        been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the MatchedSymbolTimingEstimator has already
        been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the MatchedSymbolTimingEstimator has already been
        destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def sps(self) -> float:
        """Samples per symbol of the stream handed to estimate."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "MatchedSymbolTimingEstimator":
        """Enter a context manager, returning this object.

        Lets a MatchedSymbolTimingEstimator be used in a `with` statement so
        its C resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        MatchedSymbolTimingEstimator
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the MatchedSymbolTimingEstimator.

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
class PolynomialPhaseEstimator:
    """Create a polynomial-phase estimator.

    Parameters
    ----------
    max_len : int, default 4096
        Maximum input sequence length (>= 4).
    max_rate : float, default 0.0
        Chirp-rate search half-span (cycles/sample^2); 0 searches frequency
        only (a single FFT — near-static Doppler).

    Examples
    --------
    Create with defaults:

    >>> from doppler.estimation import PolynomialPhaseEstimator
    >>> obj = PolynomialPhaseEstimator()

    """

    def __init__(self, max_len: int = 4096, max_rate: float = 0.0) -> None: ...
    def reset(self) -> None:
        """Do nothing — the estimator keeps no running state between calls.

        A feedforward analyzer computes each estimate purely from the segment
        it is handed, so there is nothing to clear. The method exists only to
        satisfy the common object protocol; calling it is always safe and has
        no effect.

        Examples
        --------
        >>> from doppler.estimation import PolynomialPhaseEstimator
        >>> p = PolynomialPhaseEstimator(max_len=512, max_rate=0.0)
        >>> p.reset()   # no-op: an estimate depends only on the next
        >>> #           segment

        """

    def estimate(self, x: complex, /) -> PolynomialPhaseEstimate:
        """Estimate (freq, chirp-rate) of a complex sequence by dechirp and FFT
        peak search over the chirp-rate hypotheses.

        Runs the full 2-D matched-filter search in one shot: for each
        chirp-rate hypothesis the segment is dechirped and FFT-ed, and the peak
        of the resulting surface — refined sub-bin by parabolic interpolation
        on both axes — gives the estimate. With max_rate = 0 the rate axis
        collapses to a single FFT (pure Doppler) and the returned rate is
        forced to exactly 0.

        Feed a segment whose modulation has already been stripped (data-aided
        by the known symbols, or non-data-aided by the M-th-power trick —
        remembering that raising to the M-th power scales both returned values
        by M). The result carries freq_norm (cycles/sample), rate_norm
        (cycles/sample^2), and snr_db (a rough peak-to-mean confidence).

        Parameters
        ----------
        x : complex
            Complex segment (modulation already stripped by the caller).

        Returns
        -------
        PolynomialPhaseEstimate
            The estimate; all fields are zeroed if n_in is out of range.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.estimation import PolynomialPhaseEstimator
        >>> m = np.arange(512)
        >>> f, r = 0.05, 1e-5               # true Doppler + chirp rate
        >>> x = np.exp(2j*np.pi*(f*m + 0.5*r*m*m)).astype(np.complex64)
        >>> p = PolynomialPhaseEstimator(max_len=512, max_rate=5e-5)
        >>> e = p.estimate(x)                        # one-shot coherent search
        >>> round(e.freq_norm, 4), round(e.rate_norm, 7)
        (0.0501, 1e-05)

        """

    @property
    def max_len(self) -> int:
        """max input length (sizes the plan/scratch)."""

    @property
    def nfft(self) -> int:
        """zero-padded transform length: 4 * next_pow_two (max_len). The 4x is
        deliberate -- a finer frequency grid before the parabolic peak
        refinement, which matters because the input is often short (preamble
        partials, symbol streams). It also sizes `buf`, `spec` and `mag`, so
        the footprint is 4x what a bare next-pow2 would suggest.
        """

    @property
    def max_rate(self) -> float:
        """chirp-rate search half-span (cycles/sample^2)."""

    @property
    def n_rate(self) -> int:
        """number of chirp-rate hypotheses (1 if max_rate=0)."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "PolynomialPhaseEstimator":
        """Enter a context manager, returning this object.

        Lets a PolynomialPhaseEstimator be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        PolynomialPhaseEstimator
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the PolynomialPhaseEstimator.

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
