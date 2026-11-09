# acquire/acquire.pyi — type stubs for the acquire C extension.
import os
from typing import Any, Literal, final

import numpy as np
import numpy.typing as npt
from numpy.typing import NDArray

@final
class CarrierAcquisition:
    """Create a PSD-template carrier acquisition (residual-carrier detector and
    estimator).

    Parameters
    ----------
    sample_rate_hz : float
        Sample rate of the input stream, Hz (required).
    symbol_rate_hz : float
        Symbol rate, Hz -- builds the default template (required).
    resolution_hz : float, default 0.0
        Desired FFT frequency resolution, Hz. <= 0.0 is a sentinel meaning
        "auto": symbol_rate_hz/10.0. The block is
        round(sample_rate_hz/resolution_hz) samples, floored at 3 -- a block of
        1 or 2 gives no usable estimate (#1959).
    zero_pad : int, default 4
        PSD zero-pad factor (>= 1); see psd_core.h.
    window : Literal["hann", "kaiser", "blackman-harris"], default "hann"
        The search window. `hann` is the default and the window the detection
        threshold was fit at.
        One of ``"hann"``, ``"kaiser"``, ``"blackman-harris"``.

        - ``"hann"`` — Hann: the default.
        - ``"kaiser"`` — Kaiser, shaped by `beta`.
        - ``"blackman-harris"`` — 4-term Blackman-Harris.
    beta : float, default 0.0
        Kaiser beta (ignored for hann/blackman-harris).
    psd_template : NDArray[np.float32], default ...
        Known PSD-shape template override, length must equal nfft =
        next_pow_two(block * zero_pad), block as above; NULL/length-0 means
        "not supplied" -- the default rectangular-pulse sinc^2 template (from
        symbol_rate_hz) is used.
    pfa : float, default 1e-3
        Target per-test false-alarm probability.
    pd : float, default 0.9
        Target detection probability.
    design_snr : float, default 2.0
        Assumed per-sample amplitude SNR used ONLY to precompute dwell_target
        via dp_det_n_noncoh(); not a live measurement. An optimistic guess only
        affects NON-sequential mode (which trusts this one-shot wait count
        outright) -- sequential mode's own give-up bound is max_n_blocks, not
        dwell_target, precisely so a wrong design_snr can't stop it from trying
        more blocks once real data shows it needs to.
    sequential : bool, default True
        True: test for a detection after EVERY block (the per-block CFAR ratio
        threshold -- see carrier_acq_ratio_threshold() in carrier_acq_core.c --
        tightens as more looks accumulate), stopping the moment one fires or
        max_n_blocks is reached. False: accumulate silently and test once, at
        dwell_target.
    max_n_blocks : int, default 100000
        Sequential mode's own give-up cap (ignored by non-sequential mode,
        which stops at dwell_target instead) -- deliberately a SEPARATE,
        generous bound from dwell_target; capping sequential mode at
        design_snr's own point estimate would defeat the reason to test every
        block in the first place.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.acquire import CarrierAcquisition
    >>> rng = np.random.default_rng(12345)
    >>> bits = np.where(rng.integers(0, 2, 4000), 1.0, -1.0)
    >>> data = np.repeat(bits, 8)                 # 8 samples/symbol BPSK
    >>> t = np.arange(len(data))
    >>> x = (data * np.exp(2j * np.pi * 123.0 * t / 8000.0)).astype(
    ...     np.complex64)  # residual carrier at 123 Hz
    >>> ca = CarrierAcquisition(
    ...     sample_rate_hz=8000.0, symbol_rate_hz=1000.0,
    ...     psd_template=np.array([], dtype=np.float32))
    >>> ca.steps(x)                   # fold the stream, testing each block
    >>> ca.ready                      # detection fired
    True
    >>> round(ca.residual_hz, 0)      # recovered residual carrier, Hz
    123.0

    """

    def __init__(
        self,
        sample_rate_hz: float,
        symbol_rate_hz: float,
        resolution_hz: float = 0.0,
        zero_pad: int = 4,
        window: Literal["hann", "kaiser", "blackman-harris"] = "hann",
        beta: float = 0.0,
        psd_template: npt.NDArray[np.float32] = ...,
        pfa: float = 1e-3,
        pd: float = 0.9,
        design_snr: float = 2.0,
        sequential: bool = True,
        max_n_blocks: int = 100000,
    ) -> None: ...
    def steps(self, x: npt.NDArray[np.complex64]) -> None:
        """Fold raw complex samples into the running PSD average and test for a
        detection; any chunk size across repeated calls (a partial trailing
        block carries to the next call).

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Raw complex input samples (cf32).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import CarrierAcquisition
        >>> rng = np.random.default_rng(12345)
        >>> bits = np.where(rng.integers(0, 2, 4000), 1.0, -1.0)
        >>> data = np.repeat(bits, 8)                 # 8 samples/symbol BPSK
        >>> t = np.arange(len(data))
        >>> x = (data * np.exp(2j * np.pi * 123.0 * t / 8000.0)).astype(
        ...     np.complex64)  # residual carrier at 123 Hz
        >>> ca = CarrierAcquisition(
        ...     sample_rate_hz=8000.0, symbol_rate_hz=1000.0,
        ...     psd_template=np.array([], dtype=np.float32))
        >>> ca.steps(x)                   # fold the stream, testing each block
        >>> ca.ready
        True
        >>> round(ca.residual_hz, 0)      # recovered residual carrier, Hz
        123.0

        """

    def reset(self) -> None:
        """Discard the running PSD average and detection state; counters return
        to zero.

        Use it to reuse one detector across successive captures: after a
        detection (or a give-up) the running average and counters are cleared,
        so the next steps() starts folding a fresh stream from zero.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import CarrierAcquisition
        >>> ca = CarrierAcquisition(
        ...     sample_rate_hz=8000.0, symbol_rate_hz=1000.0,
        ...     psd_template=np.array([], dtype=np.float32))
        >>> ca.steps(np.zeros(2048, dtype=np.complex64))  # accumulate looks
        >>> ca.n_blocks > 0
        True
        >>> ca.reset()                    # discard the running PSD average
        >>> ca.n_blocks
        0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the CarrierAcquisition has already been
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

        Raises ``RuntimeError`` if the CarrierAcquisition has already been
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
        ``RuntimeError`` if the CarrierAcquisition has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def ready(self) -> bool:
        """True once a detection has fired (or the dwell_target give-up cap was
        reached) -- residual_hz is only meaningful once this is true.
        """

    @property
    def residual_hz(self) -> float:
        """Sub-bin-refined residual carrier frequency estimate, Hz. Valid only
        when ready is true.
        """

    @property
    def n_blocks(self) -> int:
        """Number of n_fft-length blocks actually folded into the PSD average
        so far.
        """

    @property
    def dwell_target(self) -> int:
        """Non-sequential mode's precomputed fixed wait count, from
        det_n_noncoh(design_snr, ...) at construction. Ignored by sequential
        mode's own give-up bound -- see max_n_blocks.
        """

    @property
    def max_n_blocks(self) -> int:
        """Sequential mode's own give-up cap (independent of dwell_target) --
        the max_n_blocks constructor argument, echoed back.
        """

    @property
    def nfft(self) -> int:
        """PSD transform length (next_pow_two(n_fft*zero_pad)) -- the length
        any caller-supplied template array must match.
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

    def __enter__(self) -> "CarrierAcquisition":
        """Enter a context manager, returning this object.

        Lets a CarrierAcquisition be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        CarrierAcquisition
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the CarrierAcquisition.

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
class BurstCapture:
    """Create a burst capture: acquisition, refine and retention behind one
    push().

    Parameters
    ----------
    preamble : NDArray[np.complex64]
        One period of the preamble, preamble_len samples; not all zero, every
        sample finite. Read, not kept.
    burst_len : int, default 8192
        Samples in one burst -- what gets captured.
    reps : int, default 5
        Preamble repetitions (>= 1).
    fs : float, default 1.0
        Sample rate, Hz (> 0); 1 for normalized units.
    cn0_dbhz : float
        C/N0 the search is sized for, dB-Hz, of the preamble's mean power: any
        finite value, or NaN (DP_CORRELATOR_CN0_NONE) for no design point.
    doppler_uncertainty : float, default 0.0
        Doppler search half-range, Hz (0 = native).
    pfa : float, default 1e-3
        Target false-alarm probability, in (0, 1).
    pd : float, default 0.9
        Target detection probability, in (0, 1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        The CFAR reference: the statistic the noise floor is estimated with,
        which the detection threshold scales.
        One of ``"mean"``, ``"median"``, ``"min"``, ``"max"``.
    doppler_rate : float, default 0.0
        Doppler rate, Hz/s (>= 0), that caps the acquisition's coherent depth
        at `f_epoch/sqrt(2*doppler_rate)` repetitions (doppler#1482); 0 is no
        bound. With rate_hypotheses > 1 it is the span instead.
    rate_hypotheses : int, default 1
        Doppler-rate hypotheses to search (0 = auto, else 1 to
        DP_CORRELATOR_RATE_HYPS_MAX; auto picks the fewest the span needs, see
        dp_correlator_create_burst(), and
        dp_burst_capture_get_rate_hypotheses() reads the choice); 1 searches
        none and doppler_rate only caps the depth. Above 1 the capture
        COMPENSATES the rate: `doppler_rate` is the span `[-R, +R]`, the
        acquisition de-chirps its coherent block at this many rates across it,
        and the depth is bounded by the RESIDUAL, `R/(n - 1)` -- deeper blocks,
        more sensitivity, about `n` times the per-dwell cost, and a false-alarm
        budget split over the hypotheses (dp_correlator_create_burst() owns the
        semantics). Refine follows the engine: its coherent sum over the
        preamble's periods tries the rates too, on a finer grid because it
        integrates the whole preamble, and its Doppler cells widen by the shift
        a rate puts between the detecting dwell's middle and the preamble's --
        so a burst the engine found by rate is placed by rate. Needs a non-zero
        doppler_rate (the span). Measured: validate_acq_rate_pd.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``BurstCapture: invalid
        parameter (need a non-empty preamble with finite, non-zero energy, reps
        >= 1, fs > 0, burst_len >= 1, cn0_dbhz finite or NaN, doppler_rate >=
        0, 0 <= rate_hypotheses <= 64 (0 = auto; doppler_rate nonzero unless
        1), 0 < pfa < 1, 0 < pd < 1)``.

    Warns
    -----
    UserWarning
        Emitted after construction when ``underpowered`` holds: ``BurstCapture:
        the search cannot meet the requested pd at this cn0_dbhz and geometry
        (pd_burst < pd). It still builds a best-effort grid, so the symptom is
        bursts that are never captured rather than an error. Lower pd, raise
        cn0_dbhz, or give the preamble more repetitions.``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.acquire import BurstCapture
    >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
    >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
    >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
    >>> cap.burst_len
    512
    >>> cap.retain_span == cap.refine_span + cap.burst_len
    True

    A carrier that sweeps caps the coherent depth at its rate; searching
    Doppler-rate hypotheses compensates it, and buys the depth back:

    >>> import warnings
    >>> k = np.arange(127)
    >>> zc = np.exp(-1j * np.pi * 5 * k * (k + 1) / 127).astype(np.complex64)
    >>> def make(**kw):
    ...     with warnings.catch_warnings():
    ...         warnings.simplefilter("ignore")   # under-powered, below
    ...         return BurstCapture(zc, burst_len=2432, reps=16, fs=1.0,
    ...                             cn0_dbhz=-12.5, doppler_rate=9e-7, **kw)
    >>> make().doppler_bins, make().underpowered      # the rate caps it
    (5, True)
    >>> c = make(rate_hypotheses=9)
    >>> c.rate_hypotheses, c.doppler_bins, c.underpowered
    (9, 9, False)

    """

    def __init__(
        self,
        preamble: npt.NDArray[np.complex64],
        burst_len: int = 8192,
        reps: int = 5,
        fs: float = 1.0,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = 0.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
        doppler_rate: float = 0.0,
        rate_hypotheses: int = 1,
    ) -> None: ...
    def push(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Stream raw cf32 samples and get back the SAMPLES of every burst
        whose window has fully arrived, concatenated: burst i occupies
        burst_len samples starting at i*burst_len, and events() returns the
        matching record for each. Samples feed the embedded burst search (a
        Correlator) and are retained in a history ring; when a detection fires,
        the refine stage correlates one code period at each preamble position
        to recover the exact preamble start -- the one quantity acquisition
        structurally cannot report, since its code_phase is a lag modulo one
        code period -- and the window is emitted the moment its last sample has
        arrived. It stops there: what to DO with a burst (demodulate it, write
        it to a file, ship it to another process) is the caller's. An empty
        return is normal, not an error: it means no burst completed in this
        call. Accepts any block size -- the history ring is a contiguous window
        over the stream and is never reset between bursts, so a burst whose
        tail falls outside one call is completed by a later one.

        Windows are concatenated: burst `i` occupies `burst_len` samples
        starting at `i*burst_len`, and events() returns the matching record for
        each. Every sample of x is consumed. An empty return is normal -- it
        means no burst completed in this call.

        It aborts the process in two places, both a defect in this object and
        neither reachable from any input or from any blob set_state() accepts:
        a history ring with no room after trim (the bound in the
        implementation's trim proves room), and an acquisition call that took
        nothing while input was left (each call starts with its result empty,
        and a push with its result empty takes at least the next frame, or the
        rest as carry). Each stops rather than spinning forever -- and in a
        Python host it ends the interpreter.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input samples, x_len long.
        out : npt.NDArray[np.complex64] | None
            Written with the completed windows; may be NULL to drop.

        Returns
        -------
        NDArray[np.complex64]
            Samples written -- always a multiple of `burst_len`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> win = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> win.size % cap.burst_len        # whole windows, never a partial
        0
        >>> win.size                        # silence, so no burst completed
        0

        """

    def push_max_out(self, x_len: int, /) -> int:
        """Upper bound on samples push() can return for x_len input.

        Distinct bursts cannot overlap, so `x_len` samples complete at most
        `x_len/burst_len + 1` of them, plus whatever is already queued.

        Parameters
        ----------
        x_len : int
            Samples the next push() will be given.

        Returns
        -------
        int
            `(x_len/burst_len + 1 + queue capacity) * burst_len` samples.
        """

    def detections(
        self,
        count: int = 1,
        out: npt.NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """Every hit the search made in the last push(), unfiltered — before
        the claim rule coalesced the several detections of one preamble, and
        before the suppression window dropped the ones inside a burst already
        captured. So several rows can name one burst and a row can be a false
        alarm; that is the point. Each carries the STREAM-ABSOLUTE code epoch,
        which acquisition's own `code_phase` is not (it is a lag modulo one
        code period), plus the folded Doppler, the C/N0 lower bound and the
        CFAR statistic that gated it. Read `events()` instead for the bursts
        that survived and whose windows arrived. Valid until the next push(),
        reset() or set_state().

        BEFORE the claim rule and the suppression window: several rows can name
        one preamble, and a row can be a false alarm. That is the point -- this
        is what acquisition FOUND, and `events()` is what survived. Valid until
        the next push(), reset() or set_state().

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[Any] | None
            Destination rows; may be NULL to copy nothing (the count is still
            returned).

        Returns
        -------
        NDArray[Any]
            min(detections held, max_out): the rows copied, or with a NULL out
            the rows that would have been.

            One row per `burst_capture_detection_t` record, with fields:

            epoch : int
                Stream-absolute code epoch of the hit.
            doppler_hz : float
                Signed coarse Doppler, folded, Hz.
            cn0_dbhz : float
                C/N0 estimate from the hit, dB-Hz.
            test_stat : float
                The CFAR gating statistic, peak over noise.
            peak_mag : float
                Raw CFAR peak magnitude.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> # what the search found, against what became a burst
        >>> len(cap.detections()) >= len(cap.events())
        True

        """

    def detections_max_out(self, n: int, /) -> int:
        """Raw detections available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Ignored: the count is the last push's, not a request.

        Returns
        -------
        int
            Rows detections() can copy out now (0 before any push).
        """

    def events(
        self,
        count: int = 1,
        out: npt.NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """The event record for each burst the last push() returned. Row i
        describes the window at samples[i*burst_len ...] of that push. Valid
        until the next push(), reset() or set_state().

        Row `i` describes the window at `i*burst_len`. Valid until the next
        push(), reset() or set_state().

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[Any] | None
            Destination rows; may be NULL to copy nothing (the count is still
            returned).

        Returns
        -------
        NDArray[Any]
            min(windows of the last push, max_out): the rows copied, or with a
            NULL out the rows that would have been.

            One row per `burst_capture_event_t` record, with fields:

            preamble_start : int
                Refined stream position of the preamble.
            doppler_hz_est : float
                Signed coarse Doppler, Hz.
            doppler_res_hz : float
                Acquisition's native bin width, Hz.
            cn0_dbhz_est : float
                C/N0 estimate from the hit, dB-Hz.
            doppler_rate_hz_s : float
                Refine's winning rate hypothesis, Hz/s: the rate whose de-chirp made the whole preamble's coherent sum largest at preamble_start. One of refine's R grid points from -doppler_rate to +doppler_rate, spaced 2*doppler_rate/(R - 1); 0 without a declared rate (doppler_rate 0). Refine searches the grid whenever a rate is declared, with one acquisition hypothesis or many (doppler#2219). A hypothesis, not a fit, from the whole preamble rather than one dwell: it is the grid point nearest the carrier's rate in every event 23 dB over the design point, on or off the grid. At the design point it is in 82% to 89% of events whose rate is ON a grid point, but in 51% of those 0.4 of a step off one, where the neighbouring point is only 0.6 of a step away (validate_acq_rate_pd's event-rate rows; docs/design/burst-capture.md). Appended last, so positional initialisers do not shift (doppler#2219).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> win = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> len(cap.events()) == win.size // cap.burst_len
        True

        """

    def events_max_out(self, n: int, /) -> int:
        """Records available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Ignored: the count is the last push's, not a request.

        Returns
        -------
        int
            Event rows events() can copy out now: one per window returned.
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded burst search (a Correlator)'s search grid directly,
        bypassing the auto-sizing -- the escape hatch for a caller who wants a
        specific (doppler_bins, n_noncoh). Forwards to the engine unchanged.

        The escape hatch for a caller who wants a specific (doppler_bins,
        n_noncoh). Forwards to the engine, with one refusal of this object's
        own: a grid whose anchor can lag the preamble by more than refine
        reaches -- `n_noncoh * doppler_bins` code periods against `k_lo` -- is
        rejected rather than accepted and silently mis-refined. Acquisition
        stamps a hit at the end of the LAST accumulated look, so every look
        past the one holding the preamble moves the anchor a whole frame later;
        a burst has one frame of preamble, so `n_noncoh = 1` is the grid a
        capture wants and the sizer now always picks (doppler#1181).

        Parameters
        ----------
        doppler_bins : int
            Coherent depth to pin (see dp_correlator_configure_search_raw()).
        n_noncoh : int
            Non-coherent looks to pin; `n_noncoh * doppler_bins` must not
            exceed the refine reach.

        Raises
        ------
        ValueError
            If the C call returns ``DP_ERR_INVALID``. The exception message is
            ``configure_search_raw: doppler_bins must lie in 1..reps, n_noncoh
            in 1..DP_CORRELATOR_N_NONCOH_SAFETY_CEILING, and non-coherent looks
            (n_noncoh > 1) need rate_hypotheses 1; and n_noncoh x doppler_bins
            must not exceed the code periods refine reaches back (k_lo)``.
        MemoryError
            If the C call returns ``DP_ERR_MEMORY``. The exception message is
            ``configure_search_raw: rebuilding the engine's grid could not
            allocate; the engine keeps its prior grid``.
        ValueError
            If the C call returns any other non-zero status. The exception
            message is ``configure_search_raw failed``, with the return code
            appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> cap.configure_search_raw(4, 1)   # 4 Doppler bins, coherent only

        """

    def release(self, i: int) -> None:
        """Give back the span window `i` of the last push() claimed. An emitted
        window owns its whole span: detections inside it are the payload firing
        against the acquisition code, so they are HELD rather than reported. A
        consumer that knows better -- a demodulator whose CRC failed -- calls
        this for that window, and the held detections are searched again on the
        next push(). Unreleased, they are dropped when the next push() begins.
        Raises ValueError if `i` is not a window of the last push().

        An emitted window owns its whole span: a detection inside it is the
        payload firing against the acquisition code, not a new burst, so it is
        HELD rather than reported. Whether the window WAS a burst is a verdict
        this object cannot reach -- it stops at samples; error detection,
        whatever form the frame gives it, is the consumer's -- so a consumer
        that knows better calls this for that window, and the held detections
        are searched again on the next push(). Unreleased, they are dropped
        when the next push() begins, which is exactly the behaviour a consumer
        with no verdict always had.

        What it prevents (doppler#1181): a spurious window ending just after a
        real burst begins used to swallow that burst's first detections -- the
        receiver's own design says only a DECODED burst may own a span (§10.3,
        doppler#1004), and the capture underneath had been owning it on
        emission.

        Must be called BEFORE the next push(): `i` indexes THIS push's windows.

        Parameters
        ----------
        i : int
            Window index from the last push().

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``release failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> cap.release(0)   # no window 0 in a quiet push
        Traceback (most recent call last):
          ...
        ValueError: release failed (rc=-4)

        """

    def reset(self) -> None:
        """Return to the searching state: resets the embedded acquisition,
        drops the history ring's contents, clears every queued detection and
        every read-back, so a fresh stream cannot inherit the previous one's
        position. Construction parameters are untouched.

        Resets the embedded acquisition, rewinds the history ring, clears every
        queued detection and every read-back. Construction parameters are
        untouched; `dropped` and `n_bursts` deliberately survive, because a
        lost burst stays lost and a lifetime count is a lifetime count.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> cap.push(np.zeros(4096, dtype=np.complex64)).size
        0
        >>> cap.reset()
        >>> cap.pending
        0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the BurstCapture has already been destroyed.

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

        Raises ``RuntimeError`` if the BurstCapture has already been destroyed.

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
        length differs from `state_bytes()`, the exception listed under Raises
        if the core rejects it, and ``RuntimeError`` if the BurstCapture has
        already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.

        Raises
        ------
        MemoryError
            If the C call returns ``DP_ERR_MEMORY``. The exception message is
            ``set_state: the acquisition child's carrier needs its
            delay-Doppler map rebuilt, and the rebuild could not allocate; the
            capture is unchanged``.
        ValueError
            If the C core rejects the blob with any other non-zero status. The
            exception message is ``set_state rejected the blob``.
        """

    @property
    def preamble_start(self) -> int:
        """Stream-absolute sample index the most recent window's preamble
        starts at; 0 before the first window and after reset(). Never late is
        refine's design goal, not a guarantee (a window that began after the
        preamble has destroyed the burst): refine takes the strongest
        whole-period position over a range reaching both sides of the anchor,
        so at low C/N0 it can pick a wrong repetition, early or late.
        """

    @property
    def doppler_hz_est(self) -> float:
        """Folded/signed coarse Doppler estimate of the most recent window, Hz.
        Acquisition's own bin, mapped through dp_fftfreq -- the ONE home for
        that fold, because a consumer seeded on the wrong side of it is off by
        the full search span.
        """

    @property
    def doppler_res_hz(self) -> float:
        """Acquisition's native Doppler bin width =
        fs/(len(preamble)*coherent_bins), Hz. The width of doppler_hz_est: the
        estimate is that value +/- half of this.
        """

    @property
    def cn0_dbhz_est(self) -> float:
        """Estimated carrier-to-noise density of the most recent window
        (dB-Hz), backed out of the hit's test statistic. A LOWER BOUND: it
        tracks the true C/N0 while receiver noise dominates the CFAR estimate,
        then saturates at the code's own autocorrelation-sidelobe floor once
        the true C/N0 exceeds what this code and geometry can resolve -- a real
        ceiling, not a fault.
        """

    @property
    def doppler_rate_hz_s(self) -> float:
        """Refine's winning Doppler-rate hypothesis for the most recent window,
        Hz/s: the rate whose de-chirp made the whole preamble's coherent sum
        largest at its start, from refine's grid over [-doppler_rate,
        +doppler_rate] (at least as dense as the acquisition's, and searched
        whenever a rate is declared, even with one acquisition hypothesis). 0
        without a declared rate. A hypothesis, not a fit: it names the grid
        point nearest the carrier's rate well above the design point, and less
        often near it (validate_acq_rate_pd's event-rate rows).
        """

    @property
    def burst_len(self) -> int:
        """Samples in one emitted window -- the burst length this capture was
        built for, and the stride of a row in push()'s return.
        """

    @property
    def refine_span(self) -> int:
        """Coalescing window, in samples -- the reach over which two detections
        are ONE preamble.

        Both sides of that test are burst STARTS (resolved code epochs), so this
        bounds start-to-start separation, NOT the dead air between bursts. The two
        differ by a whole burst, and reading it as dead air costs a caller real
        airtime for nothing: the gap actually required is
        `max(0, refine_span - burst_len)`, which is 0 whenever a burst is longer than
        the refine reach (doppler#1085).
        """

    @property
    def min_gap(self) -> int:
        """Dead air to leave BETWEEN bursts, in samples — edge to edge, not
        start to start.

        Derived rather than documented as a rule the caller has to apply: a
        detection's anchor is the code epoch of whichever frame detected, and
        acquisition's framing is not aligned to the preamble, so the last frame that
        can detect sits up to `reps * code_period` past the true start. CLAIM merges
        two anchors closer than `refine_span`, so a pair survives only when
        `gap >= refine_span + reps*code_period - burst_len`.

        **Zero is a real answer** — a burst longer than `refine_span + reps*P` needs
        no gap for the claim rule's sake — but it does not mean zero is wise: a zero
        gap is a continuous stream rather than a burst link, and it measures 88% at a
        geometry where this reads 0.

        Replaces the prose `max(0, refine_span - burst_len)`, which was short by the
        whole detection-lag term: 32 samples against 528 at the C suite's geometry
        (doppler#1172).
        """

    @property
    def retain_span(self) -> int:
        """History kept per anchor, in samples -- the MINIMUM TRAILING CONTEXT.

        `refine_span` plus one whole burst. A burst closer than this to the end of
        what has been pushed is held rather than emitted, because refine cannot yet
        see the samples it needs. Feed at least this many more, or the last burst of
        a capture never comes out.
        """

    @property
    def underpowered(self) -> bool:
        """True when the search cannot meet the requested `pd` at this
        `cn0_dbhz` and geometry — `pd_burst < pd`. The grid is still built,
        best-effort, so the symptom is bursts that are never captured rather
        than a failure. Construction also emits a UserWarning; this is the same
        fact as a value, for a caller that would rather ask than catch.
        """

    @property
    def doppler_rate(self) -> float:
        """Doppler rate (Hz/s) the acquisition's coherent depth is bounded
        against: `doppler_bins <= f_epoch/sqrt(2*doppler_rate)`, so the carrier
        drifts less than half a slow-time row per block. 0 is no bound.
        """

    @property
    def rate_hypotheses(self) -> int:
        """Doppler-rate hypotheses the acquisition searches, as given at
        construction, or the count the acquisition chose when it was built with
        0 (auto: the fewest whose residual stops limiting the depth); 1 is none
        -- `doppler_rate` then only caps the coherent depth. Above 1
        `doppler_rate` is the span of the hypotheses, the depth is bounded by
        the residual `doppler_rate/(rate_hypotheses - 1)`, and refine tries the
        rates too.
        """

    @property
    def pd_predicted(self) -> float:
        """Detection probability of ONE dwell of the sized grid lying wholly
        inside the preamble, at `cn0_dbhz`. A burst gets about
        `reps/doppler_bins` dwells at an alignment it does not choose, so
        compare `pd_burst`, not this, against the `pd` that was asked for.
        """

    @property
    def pd_burst(self) -> float:
        """Detection probability of one burst at `cn0_dbhz`: every dwell its
        preamble spans, at a uniform alignment against the stream, any one
        detecting. The number behind `underpowered`, and the one to compare
        against the `pd` that was asked for. It models the ENGINE; what refine
        loses afterwards is not in the model. Measured on a Zadoff-Chu 127 x 8
        preamble, the capture delivers at least `pd_burst` at a 0.6 design
        point and at 0.9, the default `pd` (validation report §2.8). NaN with
        no design `cn0_dbhz`.
        """

    @property
    def psl_db(self) -> float:
        """The preamble's peak sidelobe level, dB: the largest lag of its
        periodic autocorrelation OUTSIDE the mainlobe, relative to the peak. A
        detection's sidelobes sit this far below it, at delays the peak list's
        exclusion does not cover, so a burst clearing the threshold by more
        than `-psl_db` also lists its own sidelobe (with `max_peaks > 1`), and
        a strong burst's sidelobe can mask a weak one there. -29.8 dB for a
        31-chip m-sequence (1/31); `-inf` for a perfect sequence such as
        Zadoff-Chu. Fixed at construction.
        """

    @property
    def eta(self) -> float:
        """Coherent detection gate: the normalised statistic a single-look
        decision must clear, from `pfa` spread across the search surface. In
        force when `n_noncoh == 1`.
        """

    @property
    def eta_nc(self) -> float:
        """Non-coherent detection gate — the one in force when `n_noncoh > 1`,
        which a burst search never chooses on its own (it reads 0 unless
        `configure_search_raw` pins looks). Higher than `eta` for the same
        `pfa`, because combining looks costs the threshold what it buys in
        sensitivity.
        """

    @property
    def straddle_loss(self) -> float:
        """Correlation kept, worst case, by a burst landing BETWEEN grid points
        rather than on one. The search is a finite grid in Doppler and code
        phase, so a real burst almost never sits on a hypothesis exactly; this
        is what that costs, and it is already priced into `pd_predicted` and
        `pd_burst`.
        """

    @property
    def doppler_bins(self) -> int:
        """Doppler hypotheses searched: the coherent depth chosen (the
        slow-time FFT length in code reps, <= reps), times the window-tile
        count when doppler_uncertainty exceeds the native span -- a tile keeps
        its own coherent depth when a design cn0_dbhz asks for one, and a hit's
        doppler_bin indexes all of window_bins * depth rows. This is the count
        to fold a hit's bin over (bin_to_signed(bin, doppler_bins) *
        doppler_res_hz is the signed Doppler); it is dp_correlator_grid_bins(),
        and the same number the burst search's doppler_bins reads.
        `configure_search_raw` pins it.
        """

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks combined per decision. Above 1 the object needs
        that many frames before it can decide at all, which is why a caller
        sweeping in short dwells has to pin it.
        """

    @property
    def code_bins(self) -> int:
        """Delay hypotheses per Doppler row: one repetition of the preamble, in
        samples (len(preamble)).
        """

    @property
    def doppler_span_hz(self) -> float:
        """Unambiguous Doppler half-range, ± this. Beyond it the per-segment
        integrate-and-dump's sinc rolloff suppresses the correlation, so a
        burst outside the span is not merely harder to find — it is nulled.
        """

    @property
    def pending(self) -> int:
        """Detections held because their burst window has NOT fully arrived.

        push() deliberately emits nothing for these: a window is returned when it is
        complete, not when it is guessed at. What this exists for is the other end --
        a caller closing a file or a socket while this is non-zero is discarding a
        burst that would have been captured, and every other read-back looks
        identical to "nothing was ever there".
        """

    @property
    def dropped(self) -> int:
        """Samples of look-back discarded while a queued detection still needed
        them, lifetime: the part of a dead detection's span -- one whose
        history is gone, so it can never be emitted -- behind the ring's tail.
        A shadowed detection is not counted. On a live stream this stays 0:
        push() never refuses input, a moved anchor is a later one, and a
        re-armed refine reads no further back than the history kept for an
        unarrived detection at or ahead of it. A resume carries all the
        look-back, so what makes it non-zero is a blob that names history it
        does not hold. A LOST BURST each, not a statistic -- it survives
        reset().
        """

    @property
    def n_bursts(self) -> int:
        """Windows emitted, lifetime."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "BurstCapture":
        """Enter a context manager, returning this object.

        Lets a BurstCapture be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        BurstCapture
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the BurstCapture.

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
class PersistentBurstCapture:
    """Create a capture whose look-back lives in a FILE.

    Parameters
    ----------
    path : str | os.PathLike
        File to back the ring with; not NULL and not empty.
    preamble : NDArray[np.complex64]
        One period of the preamble, preamble_len samples.
    burst_len : int, default 8192
        Samples in one burst -- what gets captured.
    reps : int, default 5
        Preamble repetitions (>= 1).
    fs : float, default 1.0
        Sample rate, Hz (> 0).
    cn0_dbhz : float
        C/N0 the search is sized for, dB-Hz: any finite value, or NaN
        (DP_CORRELATOR_CN0_NONE) for no design point.
    doppler_uncertainty : float, default 0.0
        Doppler search half-range, Hz (0 = native).
    pfa : float, default 1e-3
        Target false-alarm probability, in (0, 1).
    pd : float, default 0.9
        Target detection probability, in (0, 1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        The CFAR reference: the statistic the noise floor is estimated with,
        which the detection threshold scales.
        One of ``"mean"``, ``"median"``, ``"min"``, ``"max"``.
    doppler_rate : float, default 0.0
        Doppler rate, Hz/s (>= 0), that caps the acquisition's coherent depth
        at `f_epoch/sqrt(2*doppler_rate)` repetitions (doppler#1482); 0 is no
        bound. With rate_hypotheses > 1 it is the span instead.
    rate_hypotheses : int, default 1
        Doppler-rate hypotheses to search; see dp_burst_capture_create().

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``BurstCapture: invalid
        parameter (need a non-empty preamble with finite, non-zero energy, reps
        >= 1, fs > 0, burst_len >= 1, cn0_dbhz finite or NaN, doppler_rate >=
        0, 0 <= rate_hypotheses <= 64 (0 = auto; doppler_rate nonzero unless
        1), 0 < pfa < 1, 0 < pd < 1)``.

    Examples
    --------
    >>> import numpy as np, tempfile, os
    >>> from doppler.acquire import BurstCapture, PersistentBurstCapture
    >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
    >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
    >>> path = os.path.join(tempfile.mkdtemp(), "ring.cf32")
    >>> cap = PersistentBurstCapture(path, pre, burst_len=512,
    ...                             reps=4, fs=2e6)
    >>> ram = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
    >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
    >>> # the look-back is in the file, so the blob stops carrying the
    >>> # ring: twice retain_span rounded up to a power of two, 8 B a sample
    >>> ring = 1 << (2 * ram.retain_span - 1).bit_length()
    >>> ram.state_bytes() - cap.state_bytes() == ring * 8
    True
    >>> os.path.getsize(path) >= ring * 8   # rounded up to a page
    True

    """

    def __init__(
        self,
        path: str | os.PathLike,
        preamble: npt.NDArray[np.complex64],
        burst_len: int = 8192,
        reps: int = 5,
        fs: float = 1.0,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = 0.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
        doppler_rate: float = 0.0,
        rate_hypotheses: int = 1,
    ) -> None: ...
    def push(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Stream raw cf32 samples and get back the SAMPLES of every burst
        whose window has fully arrived, concatenated: burst i occupies
        burst_len samples starting at i*burst_len, and events() returns the
        matching record for each. Samples feed the embedded burst search (a
        Correlator) and are retained in a history ring; when a detection fires,
        the refine stage correlates one code period at each preamble position
        to recover the exact preamble start -- the one quantity acquisition
        structurally cannot report, since its code_phase is a lag modulo one
        code period -- and the window is emitted the moment its last sample has
        arrived. It stops there: what to DO with a burst (demodulate it, write
        it to a file, ship it to another process) is the caller's. An empty
        return is normal, not an error: it means no burst completed in this
        call. Accepts any block size -- the history ring is a contiguous window
        over the stream and is never reset between bursts, so a burst whose
        tail falls outside one call is completed by a later one.

        Windows are concatenated: burst `i` occupies `burst_len` samples
        starting at `i*burst_len`, and events() returns the matching record for
        each. Every sample of x is consumed. An empty return is normal -- it
        means no burst completed in this call.

        It aborts the process in two places, both a defect in this object and
        neither reachable from any input or from any blob set_state() accepts:
        a history ring with no room after trim (the bound in the
        implementation's trim proves room), and an acquisition call that took
        nothing while input was left (each call starts with its result empty,
        and a push with its result empty takes at least the next frame, or the
        rest as carry). Each stops rather than spinning forever -- and in a
        Python host it ends the interpreter.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input samples, x_len long.
        out : npt.NDArray[np.complex64] | None
            Written with the completed windows; may be NULL to drop.

        Returns
        -------
        NDArray[np.complex64]
            Samples written -- always a multiple of `burst_len`.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> win = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> win.size % cap.burst_len        # whole windows, never a partial
        0
        >>> win.size                        # silence, so no burst completed
        0

        """

    def push_max_out(self, x_len: int, /) -> int:
        """Upper bound on samples push() can return for x_len input.

        Distinct bursts cannot overlap, so `x_len` samples complete at most
        `x_len/burst_len + 1` of them, plus whatever is already queued.

        Parameters
        ----------
        x_len : int
            Samples the next push() will be given.

        Returns
        -------
        int
            `(x_len/burst_len + 1 + queue capacity) * burst_len` samples.
        """

    def detections(
        self,
        count: int = 1,
        out: npt.NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """Every hit the search made in the last push(), unfiltered — before
        the claim rule coalesced the several detections of one preamble, and
        before the suppression window dropped the ones inside a burst already
        captured. So several rows can name one burst and a row can be a false
        alarm; that is the point. Each carries the STREAM-ABSOLUTE code epoch,
        which acquisition's own `code_phase` is not (it is a lag modulo one
        code period), plus the folded Doppler, the C/N0 lower bound and the
        CFAR statistic that gated it. Read `events()` instead for the bursts
        that survived and whose windows arrived. Valid until the next push(),
        reset() or set_state().

        BEFORE the claim rule and the suppression window: several rows can name
        one preamble, and a row can be a false alarm. That is the point -- this
        is what acquisition FOUND, and `events()` is what survived. Valid until
        the next push(), reset() or set_state().

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[Any] | None
            Destination rows; may be NULL to copy nothing (the count is still
            returned).

        Returns
        -------
        NDArray[Any]
            min(detections held, max_out): the rows copied, or with a NULL out
            the rows that would have been.

            One row per `burst_capture_detection_t` record, with fields:

            epoch : int
                Stream-absolute code epoch of the hit.
            doppler_hz : float
                Signed coarse Doppler, folded, Hz.
            cn0_dbhz : float
                C/N0 estimate from the hit, dB-Hz.
            test_stat : float
                The CFAR gating statistic, peak over noise.
            peak_mag : float
                Raw CFAR peak magnitude.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> # what the search found, against what became a burst
        >>> len(cap.detections()) >= len(cap.events())
        True

        """

    def detections_max_out(self, n: int, /) -> int:
        """Raw detections available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Ignored: the count is the last push's, not a request.

        Returns
        -------
        int
            Rows detections() can copy out now (0 before any push).
        """

    def events(
        self,
        count: int = 1,
        out: npt.NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """The event record for each burst the last push() returned. Row i
        describes the window at samples[i*burst_len ...] of that push. Valid
        until the next push(), reset() or set_state().

        Row `i` describes the window at `i*burst_len`. Valid until the next
        push(), reset() or set_state().

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[Any] | None
            Destination rows; may be NULL to copy nothing (the count is still
            returned).

        Returns
        -------
        NDArray[Any]
            min(windows of the last push, max_out): the rows copied, or with a
            NULL out the rows that would have been.

            One row per `burst_capture_event_t` record, with fields:

            preamble_start : int
                Refined stream position of the preamble.
            doppler_hz_est : float
                Signed coarse Doppler, Hz.
            doppler_res_hz : float
                Acquisition's native bin width, Hz.
            cn0_dbhz_est : float
                C/N0 estimate from the hit, dB-Hz.
            doppler_rate_hz_s : float
                Refine's winning rate hypothesis, Hz/s: the rate whose de-chirp made the whole preamble's coherent sum largest at preamble_start. One of refine's R grid points from -doppler_rate to +doppler_rate, spaced 2*doppler_rate/(R - 1); 0 without a declared rate (doppler_rate 0). Refine searches the grid whenever a rate is declared, with one acquisition hypothesis or many (doppler#2219). A hypothesis, not a fit, from the whole preamble rather than one dwell: it is the grid point nearest the carrier's rate in every event 23 dB over the design point, on or off the grid. At the design point it is in 82% to 89% of events whose rate is ON a grid point, but in 51% of those 0.4 of a step off one, where the neighbouring point is only 0.6 of a step away (validate_acq_rate_pd's event-rate rows; docs/design/burst-capture.md). Appended last, so positional initialisers do not shift (doppler#2219).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> win = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> len(cap.events()) == win.size // cap.burst_len
        True

        """

    def events_max_out(self, n: int, /) -> int:
        """Records available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Ignored: the count is the last push's, not a request.

        Returns
        -------
        int
            Event rows events() can copy out now: one per window returned.
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded burst search (a Correlator)'s search grid directly,
        bypassing the auto-sizing -- the escape hatch for a caller who wants a
        specific (doppler_bins, n_noncoh). Forwards to the engine unchanged.

        The escape hatch for a caller who wants a specific (doppler_bins,
        n_noncoh). Forwards to the engine, with one refusal of this object's
        own: a grid whose anchor can lag the preamble by more than refine
        reaches -- `n_noncoh * doppler_bins` code periods against `k_lo` -- is
        rejected rather than accepted and silently mis-refined. Acquisition
        stamps a hit at the end of the LAST accumulated look, so every look
        past the one holding the preamble moves the anchor a whole frame later;
        a burst has one frame of preamble, so `n_noncoh = 1` is the grid a
        capture wants and the sizer now always picks (doppler#1181).

        Parameters
        ----------
        doppler_bins : int
            Coherent depth to pin (see dp_correlator_configure_search_raw()).
        n_noncoh : int
            Non-coherent looks to pin; `n_noncoh * doppler_bins` must not
            exceed the refine reach.

        Raises
        ------
        ValueError
            If the C call returns ``DP_ERR_INVALID``. The exception message is
            ``configure_search_raw: doppler_bins must lie in 1..reps, n_noncoh
            in 1..DP_CORRELATOR_N_NONCOH_SAFETY_CEILING, and non-coherent looks
            (n_noncoh > 1) need rate_hypotheses 1; and n_noncoh x doppler_bins
            must not exceed the code periods refine reaches back (k_lo)``.
        MemoryError
            If the C call returns ``DP_ERR_MEMORY``. The exception message is
            ``configure_search_raw: rebuilding the engine's grid could not
            allocate; the engine keeps its prior grid``.
        ValueError
            If the C call returns any other non-zero status. The exception
            message is ``configure_search_raw failed``, with the return code
            appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> cap.configure_search_raw(4, 1)   # 4 Doppler bins, coherent only

        """

    def release(self, i: int) -> None:
        """Give back the span window `i` of the last push() claimed. An emitted
        window owns its whole span: detections inside it are the payload firing
        against the acquisition code, so they are HELD rather than reported. A
        consumer that knows better -- a demodulator whose CRC failed -- calls
        this for that window, and the held detections are searched again on the
        next push(). Unreleased, they are dropped when the next push() begins.
        Raises ValueError if `i` is not a window of the last push().

        An emitted window owns its whole span: a detection inside it is the
        payload firing against the acquisition code, not a new burst, so it is
        HELD rather than reported. Whether the window WAS a burst is a verdict
        this object cannot reach -- it stops at samples; error detection,
        whatever form the frame gives it, is the consumer's -- so a consumer
        that knows better calls this for that window, and the held detections
        are searched again on the next push(). Unreleased, they are dropped
        when the next push() begins, which is exactly the behaviour a consumer
        with no verdict always had.

        What it prevents (doppler#1181): a spurious window ending just after a
        real burst begins used to swallow that burst's first detections -- the
        receiver's own design says only a DECODED burst may own a span (§10.3,
        doppler#1004), and the capture underneath had been owning it on
        emission.

        Must be called BEFORE the next push(): `i` indexes THIS push's windows.

        Parameters
        ----------
        i : int
            Window index from the last push().

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``release failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
        >>> cap.release(0)   # no window 0 in a quiet push
        Traceback (most recent call last):
          ...
        ValueError: release failed (rc=-4)

        """

    def reset(self) -> None:
        """Return to the searching state: resets the embedded acquisition,
        drops the history ring's contents, clears every queued detection and
        every read-back, so a fresh stream cannot inherit the previous one's
        position. Construction parameters are untouched.

        Resets the embedded acquisition, rewinds the history ring, clears every
        queued detection and every read-back. Construction parameters are
        untouched; `dropped` and `n_bursts` deliberately survive, because a
        lost burst stays lost and a lifetime count is a lifetime count.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstCapture
        >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
        >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
        >>> cap = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
        >>> cap.push(np.zeros(4096, dtype=np.complex64)).size
        0
        >>> cap.reset()
        >>> cap.pending
        0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the PersistentBurstCapture has already been
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

        Raises ``RuntimeError`` if the PersistentBurstCapture has already been
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
        length differs from `state_bytes()`, the exception listed under Raises
        if the core rejects it, and ``RuntimeError`` if the
        PersistentBurstCapture has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.

        Raises
        ------
        MemoryError
            If the C call returns ``DP_ERR_MEMORY``. The exception message is
            ``set_state: the acquisition child's carrier needs its
            delay-Doppler map rebuilt, and the rebuild could not allocate; the
            capture is unchanged``.
        ValueError
            If the C core rejects the blob with any other non-zero status. The
            exception message is ``set_state rejected the blob``.
        """

    @property
    def preamble_start(self) -> int:
        """Stream-absolute sample index the most recent window's preamble
        starts at; 0 before the first window and after reset(). Never late is
        refine's design goal, not a guarantee (a window that began after the
        preamble has destroyed the burst): refine takes the strongest
        whole-period position over a range reaching both sides of the anchor,
        so at low C/N0 it can pick a wrong repetition, early or late.
        """

    @property
    def doppler_hz_est(self) -> float:
        """Folded/signed coarse Doppler estimate of the most recent window, Hz.
        Acquisition's own bin, mapped through dp_fftfreq -- the ONE home for
        that fold, because a consumer seeded on the wrong side of it is off by
        the full search span.
        """

    @property
    def doppler_res_hz(self) -> float:
        """Acquisition's native Doppler bin width =
        fs/(len(preamble)*coherent_bins), Hz. The width of doppler_hz_est: the
        estimate is that value +/- half of this.
        """

    @property
    def cn0_dbhz_est(self) -> float:
        """Estimated carrier-to-noise density of the most recent window
        (dB-Hz), backed out of the hit's test statistic. A LOWER BOUND: it
        tracks the true C/N0 while receiver noise dominates the CFAR estimate,
        then saturates at the code's own autocorrelation-sidelobe floor once
        the true C/N0 exceeds what this code and geometry can resolve -- a real
        ceiling, not a fault.
        """

    @property
    def doppler_rate_hz_s(self) -> float:
        """Refine's winning Doppler-rate hypothesis for the most recent window,
        Hz/s: the rate whose de-chirp made the whole preamble's coherent sum
        largest at its start, from refine's grid over [-doppler_rate,
        +doppler_rate] (at least as dense as the acquisition's, and searched
        whenever a rate is declared, even with one acquisition hypothesis). 0
        without a declared rate. A hypothesis, not a fit: it names the grid
        point nearest the carrier's rate well above the design point, and less
        often near it (validate_acq_rate_pd's event-rate rows).
        """

    @property
    def burst_len(self) -> int:
        """Samples in one emitted window -- the burst length this capture was
        built for, and the stride of a row in push()'s return.
        """

    @property
    def refine_span(self) -> int:
        """Coalescing window, in samples -- the reach over which two detections
        are ONE preamble.

        Both sides of that test are burst STARTS (resolved code epochs), so this
        bounds start-to-start separation, NOT the dead air between bursts. The two
        differ by a whole burst, and reading it as dead air costs a caller real
        airtime for nothing: the gap actually required is
        `max(0, refine_span - burst_len)`, which is 0 whenever a burst is longer than
        the refine reach (doppler#1085).
        """

    @property
    def min_gap(self) -> int:
        """Dead air to leave BETWEEN bursts, in samples — edge to edge, not
        start to start.

        Derived rather than documented as a rule the caller has to apply: a
        detection's anchor is the code epoch of whichever frame detected, and
        acquisition's framing is not aligned to the preamble, so the last frame that
        can detect sits up to `reps * code_period` past the true start. CLAIM merges
        two anchors closer than `refine_span`, so a pair survives only when
        `gap >= refine_span + reps*code_period - burst_len`.

        **Zero is a real answer** — a burst longer than `refine_span + reps*P` needs
        no gap for the claim rule's sake — but it does not mean zero is wise: a zero
        gap is a continuous stream rather than a burst link, and it measures 88% at a
        geometry where this reads 0.

        Replaces the prose `max(0, refine_span - burst_len)`, which was short by the
        whole detection-lag term: 32 samples against 528 at the C suite's geometry
        (doppler#1172).
        """

    @property
    def retain_span(self) -> int:
        """History kept per anchor, in samples -- the MINIMUM TRAILING CONTEXT.

        `refine_span` plus one whole burst. A burst closer than this to the end of
        what has been pushed is held rather than emitted, because refine cannot yet
        see the samples it needs. Feed at least this many more, or the last burst of
        a capture never comes out.
        """

    @property
    def underpowered(self) -> bool:
        """True when the search cannot meet the requested `pd` at this
        `cn0_dbhz` and geometry — `pd_burst < pd`. The grid is still built,
        best-effort, so the symptom is bursts that are never captured rather
        than a failure. Construction also emits a UserWarning; this is the same
        fact as a value, for a caller that would rather ask than catch.
        """

    @property
    def doppler_rate(self) -> float:
        """Doppler rate (Hz/s) the acquisition's coherent depth is bounded
        against: `doppler_bins <= f_epoch/sqrt(2*doppler_rate)`, so the carrier
        drifts less than half a slow-time row per block. 0 is no bound.
        """

    @property
    def rate_hypotheses(self) -> int:
        """Doppler-rate hypotheses the acquisition searches, as given at
        construction, or the count the acquisition chose when it was built with
        0 (auto: the fewest whose residual stops limiting the depth); 1 is none
        -- `doppler_rate` then only caps the coherent depth. Above 1
        `doppler_rate` is the span of the hypotheses, the depth is bounded by
        the residual `doppler_rate/(rate_hypotheses - 1)`, and refine tries the
        rates too.
        """

    @property
    def pd_predicted(self) -> float:
        """Detection probability of ONE dwell of the sized grid lying wholly
        inside the preamble, at `cn0_dbhz`. A burst gets about
        `reps/doppler_bins` dwells at an alignment it does not choose, so
        compare `pd_burst`, not this, against the `pd` that was asked for.
        """

    @property
    def pd_burst(self) -> float:
        """Detection probability of one burst at `cn0_dbhz`: every dwell its
        preamble spans, at a uniform alignment against the stream, any one
        detecting. The number behind `underpowered`, and the one to compare
        against the `pd` that was asked for. It models the ENGINE; what refine
        loses afterwards is not in the model. Measured on a Zadoff-Chu 127 x 8
        preamble, the capture delivers at least `pd_burst` at a 0.6 design
        point and at 0.9, the default `pd` (validation report §2.8). NaN with
        no design `cn0_dbhz`.
        """

    @property
    def psl_db(self) -> float:
        """The preamble's peak sidelobe level, dB: the largest lag of its
        periodic autocorrelation OUTSIDE the mainlobe, relative to the peak. A
        detection's sidelobes sit this far below it, at delays the peak list's
        exclusion does not cover, so a burst clearing the threshold by more
        than `-psl_db` also lists its own sidelobe (with `max_peaks > 1`), and
        a strong burst's sidelobe can mask a weak one there. -29.8 dB for a
        31-chip m-sequence (1/31); `-inf` for a perfect sequence such as
        Zadoff-Chu. Fixed at construction.
        """

    @property
    def eta(self) -> float:
        """Coherent detection gate: the normalised statistic a single-look
        decision must clear, from `pfa` spread across the search surface. In
        force when `n_noncoh == 1`.
        """

    @property
    def eta_nc(self) -> float:
        """Non-coherent detection gate — the one in force when `n_noncoh > 1`,
        which a burst search never chooses on its own (it reads 0 unless
        `configure_search_raw` pins looks). Higher than `eta` for the same
        `pfa`, because combining looks costs the threshold what it buys in
        sensitivity.
        """

    @property
    def straddle_loss(self) -> float:
        """Correlation kept, worst case, by a burst landing BETWEEN grid points
        rather than on one. The search is a finite grid in Doppler and code
        phase, so a real burst almost never sits on a hypothesis exactly; this
        is what that costs, and it is already priced into `pd_predicted` and
        `pd_burst`.
        """

    @property
    def doppler_bins(self) -> int:
        """Doppler hypotheses searched: the coherent depth chosen (the
        slow-time FFT length in code reps, <= reps), times the window-tile
        count when doppler_uncertainty exceeds the native span -- a tile keeps
        its own coherent depth when a design cn0_dbhz asks for one, and a hit's
        doppler_bin indexes all of window_bins * depth rows. This is the count
        to fold a hit's bin over (bin_to_signed(bin, doppler_bins) *
        doppler_res_hz is the signed Doppler); it is dp_correlator_grid_bins(),
        and the same number the burst search's doppler_bins reads.
        `configure_search_raw` pins it.
        """

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks combined per decision. Above 1 the object needs
        that many frames before it can decide at all, which is why a caller
        sweeping in short dwells has to pin it.
        """

    @property
    def code_bins(self) -> int:
        """Delay hypotheses per Doppler row: one repetition of the preamble, in
        samples (len(preamble)).
        """

    @property
    def doppler_span_hz(self) -> float:
        """Unambiguous Doppler half-range, ± this. Beyond it the per-segment
        integrate-and-dump's sinc rolloff suppresses the correlation, so a
        burst outside the span is not merely harder to find — it is nulled.
        """

    @property
    def pending(self) -> int:
        """Detections held because their burst window has NOT fully arrived.

        push() deliberately emits nothing for these: a window is returned when it is
        complete, not when it is guessed at. What this exists for is the other end --
        a caller closing a file or a socket while this is non-zero is discarding a
        burst that would have been captured, and every other read-back looks
        identical to "nothing was ever there".
        """

    @property
    def dropped(self) -> int:
        """Samples of look-back discarded while a queued detection still needed
        them, lifetime: the part of a dead detection's span -- one whose
        history is gone, so it can never be emitted -- behind the ring's tail.
        A shadowed detection is not counted. On a live stream this stays 0:
        push() never refuses input, a moved anchor is a later one, and a
        re-armed refine reads no further back than the history kept for an
        unarrived detection at or ahead of it. A resume carries all the
        look-back, so what makes it non-zero is a blob that names history it
        does not hold. A LOST BURST each, not a statistic -- it survives
        reset().
        """

    @property
    def n_bursts(self) -> int:
        """Windows emitted, lifetime."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "PersistentBurstCapture":
        """Enter a context manager, returning this object.

        Lets a PersistentBurstCapture be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        PersistentBurstCapture
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the PersistentBurstCapture.

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

def bin_to_signed(bin: int, n_bins: int) -> int:
    """Map an FFT bin index to its SIGNED frequency index --
    numpy.fft.fftfreq(n) * n, exactly: 0 = DC, ascending positive to (n-1)/2,
    then wrapping negative, so an even grid's Nyquist bin is -n/2. Multiply by
    doppler_res_hz for Hz. Call this rather than writing the fold out: the
    search and its hand-off must agree on the convention, and a consumer seeded
    on the wrong side of it is off by the full search span -- a failure that
    once surfaced here as a receiver reporting tracking while decoding noise. A
    thin wrapper over dp_fftfreq_index() in clib_common.h, so C callers inline
    the same code.

    Parameters
    ----------
    bin : int
        Bin index in `[0, n_bins)`.
    n_bins : int
        Grid size.

    Returns
    -------
    int
        Signed index in `[-(n_bins/2), +((n_bins-1)/2)]`.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.acquire import bin_to_signed
    >>> [bin_to_signed(b, 8) for b in range(8)]
    [0, 1, 2, 3, -4, -3, -2, -1]
    >>> (np.fft.fftfreq(8) * 8).astype(int).tolist()   # same convention
    [0, 1, 2, 3, -4, -3, -2, -1]
    >>> bin_to_signed(4, 7)                         # odd grid: no ambiguity
    -3

    """
