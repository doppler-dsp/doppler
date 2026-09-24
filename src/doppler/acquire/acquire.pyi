# acquire/acquire.pyi — type stubs for the acquire C extension.
from typing import Any, final, Literal
import os
import numpy as np
from numpy.typing import NDArray

@final
class CarrierAcquisition:
    """Create a carrier_acq instance.

    Parameters
    ----------
    sample_rate_hz : float
        Sample rate of the input stream, Hz (required).
    symbol_rate_hz : float
        Symbol rate, Hz -- builds the default template (required).
    resolution_hz : float, default 0.0
        Desired FFT frequency resolution, Hz. <= 0.0 is a sentinel meaning
        "auto": symbol_rate_hz/10.0.
    zero_pad : int, default 4
        PSD zero-pad factor (>= 1); see psd_core.h.
    window : Literal["hann", "kaiser", "blackman-harris"], default "hann"
        Enum index; 0=hann, 1=kaiser, 2=blackman-harris.
    beta : float, default 0.0
        Kaiser beta (ignored for hann/blackman-harris).
    psd_template : NDArray[np.float32], default ...
        Known PSD-shape template override, length must equal nfft =
        next_pow_two(round(sample_rate_hz /resolution_hz) * zero_pad);
        NULL/length-0 means "not supplied" -- the default rectangular-pulse
        sinc^2 template (from symbol_rate_hz) is used.
    pfa : float, default 1e-3
        Target per-test false-alarm probability.
    pd : float, default 0.9
        Target detection probability.
    design_snr : float, default 2.0
        Assumed per-sample amplitude SNR used ONLY to precompute dwell_target
        via det_n_noncoh(); not a live measurement. An optimistic guess only
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
        psd_template: NDArray[np.float32] = ...,
        pfa: float = 1e-3,
        pd: float = 0.9,
        design_snr: float = 2.0,
        sequential: bool = True,
        max_n_blocks: int = 100000,
    ) -> None: ...

    def steps(self, x: NDArray[np.complex64]) -> None:
        """Fold raw complex samples into the running PSD average and test for a
        detection; any chunk size across repeated calls (a partial trailing
        block carries to the next call).

        Parameters
        ----------
        x : NDArray[np.complex64]
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

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

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

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

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
class Acquisition:
    """Create a continuous-mode acquisition engine: always wideband
    window-tiling, allowing a block-coherent depth inside the tiles to
    accommodate waveforms with code-only windows.

    Parameters
    ----------
    code : NDArray[np.uint8]
        PN chips (0/1), length code_len.
    spc : int, default 4
        Samples per chip (>= 1).
    chip_rate : float, default 1000000.0
        Chip rate in Hz (> 0).
    symbol_rate : float, default 1000.0
        Continuous data-symbol rate in Hz; <= 0 means no known clock.
        Diagnostic only (exposed via acq_state_t::epochs_per_symbol), doesn't
        feed sizing: this engine never coherently combines regardless of the
        data-modulation clock.
    cn0_dbhz : float, default 50.0
        Carrier-to-noise density in dB-Hz: any finite value. A continuous
        engine needs one -- its non-coherent looks cannot be chosen without a
        target.
    doppler_uncertainty : float, default 0.0
        One-sided Doppler search half-range in Hz; 0 uses the full native span
        +/- chip_rate/(2*sf) (still window-tiled, at window_bins=1).
    pfa : float, default 1e-3
        Target system (max-of-N) false-alarm probability (0,1).
    pd : float, default 0.9
        Target detection probability (0,1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
    code_only_epochs : int, default 1
        Whole code-only epochs a waveform's code-only window holds at any chip
        phase (floor(W_symbols * chips_per_symbol / sf) - 1; design §2.1) --
        the engine allows a coherent depth to accommodate such waveforms. 1 =
        no window: a coherent depth of 1.
    doppler_rate : float, default 0.0
        Doppler rate in Hz/s the coherent depth is bounded against (the drift
        over one block stays inside half a slow-time row); 0 leaves the window
        as the only bound.

    Warns
    -----
    UserWarning
        Emitted after construction when ``underpowered`` holds: ``Acquisition
        is under-powered: pd_predicted < pd at this cn0_dbhz. Raise cn0_dbhz or
        narrow doppler_uncertainty.``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.acquire import Acquisition
    >>> from doppler.wfm import PN, mls_poly
    >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
    ...                      length=5).generate(31)).astype(np.uint8)
    >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
    ...     np.complex64)
    >>> burst = np.tile(np.roll(s0, 17), 23).astype(np.complex64)
    >>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)
    >>> a.push(burst)[0][:2]    # detects (Doppler-window bin, code phase)
    (0, 17)
    >>> a.coherent_bins            # no window given: one epoch
    1
    >>> b = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0,
    ...                 code_only_epochs=7)
    >>> b.coherent_bins            # (7 + 1) // 2: a whole block fits
    4

    """
    def __init__(
        self,
        code: NDArray[np.uint8],
        spc: int = 4,
        chip_rate: float = 1000000.0,
        symbol_rate: float = 1000.0,
        cn0_dbhz: float = 50.0,
        doppler_uncertainty: float = 0.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
        code_only_epochs: int = 1,
        doppler_rate: float = 0.0,
    ) -> None: ...

    def reset(self) -> None:
        """Drain the input ring and reset the coherent accumulator.

        Discards any buffered samples that have not yet completed a frame and
        clears the non-coherent power accumulator and dwell bookkeeping, so the
        next push() begins a fresh search from an empty ring. The construction
        parameters — grid, thresholds, and PN reference — are untouched; only
        the in-flight streaming state is dropped.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> burst = np.tile(np.roll(s0, 17), 23).astype(np.complex64)
        >>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)
        >>> _ = a.push(burst[:100])   # a partial frame, buffered mid-stream
        >>> a.reset()                 # drop it before it can bias a detection
        >>> a.push(burst)[0][:2]      # (Doppler bin, code phase)
        (0, 17)

        """

    def push(
        self,
        x: complex,
    ) -> list[tuple[int, int, float, float, float, float, int]]:
        """Stream raw samples; emit one event per CFAR dump above threshold.

        Buffers x, then for every complete frame applies the slow-time Doppler
        FFT, correlates against the PN reference, dumps the coherent surface
        (or, when n_noncoh > 1, accumulates |·|² over n_noncoh looks first),
        gates the peak on the auto-configured threshold, and appends an
        acq_result_t. Each event carries the peak's Doppler bin and code phase
        (the two search axes), its CFAR statistic, and an estimated C/N0 — see
        acq_result_t.

        Parameters
        ----------
        x : complex
            Raw input, interleaved CF32, n_in complex samples.

        Returns
        -------
        list[tuple[int, int, float, float, float, float, int]]
            Number of events written (0 … max_results).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0,
        ...                 doppler_uncertainty=40e3)
        >>> fs = 1e6 * 4                    # sample rate = chip_rate * spc
        >>> t = np.arange(a.code_bins * a.n_noncoh)
        >>> carrier = np.exp(2j * np.pi * (a.doppler_res_hz / fs) * t)
        >>> sig = (np.tile(np.roll(s0, 17), a.n_noncoh)
        ...        * carrier).astype(np.complex64)
        >>> a.push(sig)[0][:2]              # (Doppler-window bin, code phase)
        (1, 17)

        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the search grid directly, bypassing both auto-sizing searches --
        the advanced escape hatch (mirrors
        Dll.configure_lock_raw/Costas.configure_lock). Resizes every
        buffer/plan that depends on the grid (the slow-time FFT, the code
        correlator, the reference, and every per-frame scratch buffer),
        re-derives the threshold ladder for the pinned grid from the same
        physics __init__ used, and clears in-flight accumulation (ring
        contents, the non-coherent power accumulator, dwell bookkeeping) --
        call between push() calls, never a substitute for one. Raises
        ValueError if doppler_bins is outside [1, reps] or n_noncoh is outside
        [1, 256] (the internal non-coherent-look safety-valve ceiling).

        Resizes every buffer/plan that depends on the grid (the slow-time FFT,
        the code correlator, the reference, and every per-frame scratch
        buffer), re-derives the threshold ladder for the pinned grid from the
        same physics acq_create_burst()/acq_create_continuous() used, and
        clears in-flight accumulation (ring contents, the non-coherent power
        accumulator, dwell bookkeeping) — call between push() calls, never a
        substitute for one.

        Parameters
        ----------
        doppler_bins : int
            Coherent depth to pin, in `[1, reps]`.
        n_noncoh : int
            Non-coherent look count to pin, in `[1,
            ACQ_N_NONCOH_SAFETY_CEILING]`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=50.0)
        >>> a.configure_search_raw(doppler_bins=1, n_noncoh=4)  # pin the grid
        >>> a.doppler_bins, a.n_noncoh
        (1, 4)
        >>> burst = np.tile(np.roll(s0, 17), 4).astype(np.complex64)
        >>> a.push(burst)[0][:2]      # detects at the pinned grid
        (0, 17)

        """

    def set_max_peaks(self, n: int) -> None:
        """How many peaks a dwell may report -- the peak list's capacity
        (docs/design/async-dsss-receiver.md section 7.1). One (the default) is
        the classic gated maximum. More lists every peak above the same gate,
        strongest first, with an exclusion zone of one Doppler bin by one chip
        around each (one emitter's main lobe, so its own shoulders are not the
        next peak) and the two-epoch rule for a peak at an already-listed code
        phase (a data transition inside the epoch splits one emitter into twins
        at its own code phase on other tiles; such a peak is held for one dwell
        and listed only if it is still there, at the same tile, on the next).
        Each listed peak is one record from push(), all of a dwell's sharing
        samples_consumed and noise_est; a held twin takes one of the n slots
        that dwell but is not reported. The threshold does not change with n.
        Raises ValueError outside 1..64. Clears the held candidates.

        One (the default) is the classic detector -- the maximum of the
        surface, gated. More is the list of docs/design/async-dsss-receiver.md
        §7.1: every peak above the same gate, strongest first, each with an
        exclusion zone of one Doppler bin by the reference's first
        autocorrelation null around it (one chip for a PN code; one emitter's
        main lobe, so its own shoulders are not the next peak), and the
        two-epoch rule for a peak at an already-listed code phase -- a data
        transition inside the epoch splits one emitter into twins at its own
        code phase on other tiles, so such a peak is held for one dwell and
        listed only if it was there, at the same tile, on the previous one.
        Each listed peak is one acq_result_t from acq_push(), all of a dwell's
        sharing its `samples_consumed` and `noise_est`. A held twin takes a
        slot of the `n` for that dwell but is not reported. The threshold does
        not change: a second peak is another draw from the same cells against
        the same union bound. Clears the held candidates.

        Parameters
        ----------
        n : int
            1 … ACQ_MAX_PEAKS.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_max_peaks failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> code = (np.arange(31) * 5 % 2).astype(np.uint8)
        >>> a = Acquisition(code, spc=2, chip_rate=1e6, symbol_rate=1e3,
        ...                 cn0_dbhz=50.0, doppler_uncertainty=50e3)
        >>> a.max_peaks
        1
        >>> a.set_max_peaks(8)
        >>> a.max_peaks
        8

        """

    def set_carrier_freq_hz(self, carrier_freq_hz: float) -> None:
        """Couple the code clock to the carrier: the chip rate dilates by
        doppler_hz / carrier_freq_hz, and the engine accounts for it -- every
        window tile's epochs are shifted along the code axis by the drift the
        tile's own frequency implies before the slow-time transform (inside a
        coherent block of D epochs), and the hand-off advances the hit's code
        phase by the drift over half the dwell (design section 12.11, 12.12).
        Config, not running state: not in the state blob, so a resumed engine
        wants it set again. 0.0 (the default) = uncoupled, the engine as it ran
        without it. Raises ValueError for a negative or non-finite value.

        A physically-coupled Doppler moves the code as well as the carrier --
        100 chips/s at 20 ppm of 5 Mcps -- and the engine's two long
        integrations both smear over it (doppler#1256, #1254):

        - **Inside a coherent block** of D epochs every tile's epoch
          correlations are shifted along the code axis by the drift the tile's
          own frequency implies, `f_tile / carrier` chips per chip, aligned to
          the block's middle, before the slow-time transform (a linear phase on
          each epoch's product, exact to a fraction of a sample). Measured at
          SPEC's 20 ppm with D = 154 (3.1 chips of drift across the block):
          without it the block's peak is 13 dB down and 3 chips wide and the
          depth detects nothing at 34 dB-Hz; with it the block reads as a still
          one.
        - **The hand-off** (acq_build_handoff()) advances the hit's code phase
          by the drift over half the dwell -- the non-coherent sum's peak is
          the phase at the dwell's middle, the seed is wanted at its end: 0.9
          chip at the 40 dB-Hz floor, past a refine loop's pull-in.

        Config, not running state: it is not in the state blob, so a resumed
        engine wants it set again by its holder, as at create. Default 0.0
        (uncoupled) is the engine exactly as it ran without it.

        Parameters
        ----------
        carrier_freq_hz : float
            RF carrier, Hz; 0.0 = uncoupled.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_carrier_freq_hz failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> a = Acquisition(code, spc=2, chip_rate=5e6, symbol_rate=2700.0,
        ...                 cn0_dbhz=45.0, doppler_uncertainty=50e3)
        >>> a.carrier_freq_hz
        0.0
        >>> a.set_carrier_freq_hz(2.5e9)
        >>> a.carrier_freq_hz
        2500000000.0

        """

    def set_threads(self, n: int) -> None:
        """Set how many threads the searcher fans its tiles across (design
        §2.3: a roll per thread on persistent workers).

        A continuous engine is created with a pool of the machine's online
        cores when it has more than one tile; a burst engine, and a single-tile
        one, run serially. This sets the count: 0 auto-selects the online core
        count, 1 runs everything on the calling thread, n runs on n workers
        (the caller included). The workers are created here, once, and parked
        between pushes; nothing is created per push. The surface is
        bit-identical at every count -- the tiles are independent after the one
        forward transform and each writes its own rows -- so this changes the
        cost of a push and nothing about its result. Setup path, never hot; not
        while another thread is inside push().

        Parameters
        ----------
        n : int
            Thread count; 0 = online cores, 1 = serial.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_threads failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(
        ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
        >>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0,
        ...                 doppler_uncertainty=4000.0)
        >>> a.threads >= 1               # a pool, sized to the machine
        True
        >>> a.set_threads(1)
        >>> a.threads
        1

        """

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context and register the engine's
        probes on it (design §2.4).

        Registers ten probes, emitted once per DECIDED dwell (a coherent dump,
        or the dwell that completes `n_noncoh` looks) and further thinned by
        decim: "<prefix>.stat" (the dwell's test statistic — the strongest cell
        against the CFAR reference, in the units the gate is set in),
        "<prefix>.gate" (that gate: `threshold` on the coherent path, `eta_nc`
        on the non-coherent one — plotted together they show exactly where a
        hit fired), "<prefix>.noise" (the CFAR reference `noise_est`),
        "<prefix>.peak" (the strongest cell's raw value), "<prefix>.row" and
        "<prefix>.col" (its native Doppler row and code-phase column — a
        surface coordinate, not a physical unit; acq_surface_doppler_hz() and
        acq_surface_chip_phase() convert), "<prefix>.n_peaks" (picks in the
        dwell, held twins included), "<prefix>.n_held" (picks held as
        same-code-phase twins rather than listed, §7.1), "<prefix>.conc" (the
        strongest pick's concentration — see `peak_conc`: its main lobe's power
        over its whole column's, near 1 for one clean emitter even when it
        straddles two tiles, about 0.5 when a data transition splits it into
        twins two or more tiles away, lower still when a coherent block
        straddles data — the discriminator between one emitter's splatter and a
        second emitter) and "<prefix>.hit" (1 when the gate fired). Passing
        NULL detaches. Setup path, never hot; the context is borrowed and must
        outlive the attachment (SPSC rules in dp_tlm/dp_tlm_core.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "acq" or "ch0.acq".
        decim : int
            Emit every decim-th decided dwell; >= 1.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_telemetry failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.telemetry import Telemetry
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(
        ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
        >>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0)
        >>> tlm = Telemetry(1 << 12)
        >>> a.set_telemetry(tlm, "acq")
        >>> sorted(tlm.probe_names)[:3]
        ['acq.col', 'acq.conc', 'acq.gate']
        >>> x = np.zeros(a.n_noncoh * 511 * 2 * 3, dtype=np.complex64)
        >>> _ = a.push(x)
        >>> len(tlm.read()) % 10      # ten records per decided dwell
        0

        """

    def surface(self, out: NDArray[np.float32]) -> int:
        """The last decided dwell's surface, in the gate's own units.

        Copies the surface the last dwell was decided on into out, row-major
        `surface_rows` (Doppler: tiles, or interpolated slow-time rows) by
        `code_bins` (code phase), every cell divided by the same CFAR reference
        the gate used — so a cell reads as its own test statistic, to a float
        rounding (the SIMD build's fast-math may take a reciprocal in this loop
        and a divide in the gate's), and the gate (`threshold`, or `eta_nc` on
        the non-coherent path) is a flat plane on a plot. The engine keeps this
        only while `keep_surface` is set (a caller sets it, or
        acq_set_surface_sink() does): set it, push, then read. `surface_at`
        says which dwell it is; a time-decimated record is the caller reading
        every k-th dwell, or a sink with `decim`.

        Parameters
        ----------
        out : NDArray[np.float32]
            At least `surface_rows * code_bins` floats.

        Returns
        -------
        int
            Cells written (`surface_rows * code_bins`), or 0 when no dwell has
            been decided with `keep_surface` set, or out is too small.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(
        ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
        >>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0)
        >>> a.keep_surface = 1
        >>> x = np.zeros(a.n_noncoh * 511 * 2, dtype=np.complex64)
        >>> _ = a.push(x)
        >>> s = np.empty(a.surface_rows * a.code_bins, dtype=np.float32)
        >>> a.surface(s) == s.size
        True
        >>> s.reshape(a.surface_rows, a.code_bins).shape == (a.surface_rows, 1022)
        True

        """

    def surface_doppler_hz(self, out: NDArray[np.float64]) -> int:
        """The surface's Doppler axis: the frequency of each row, in Hz.

        One value per surface row, the fold and scale a hit's `doppler_hz_est`
        uses (dp_fftfreq_index() times `doppler_res_hz`, on the interpolated
        grid where the slow-time axis is interpolated), so a plot of
        acq_surface() carries the same axis a DetectionEvent reports on.

        Parameters
        ----------
        out : NDArray[np.float64]
            At least `surface_rows` doubles.

        Returns
        -------
        int
            Values written (`surface_rows`), or 0 if out is too small.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(
        ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
        >>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0,
        ...                 doppler_uncertainty=4000.0)
        >>> f = np.empty(a.surface_rows, dtype=np.float64)
        >>> a.surface_doppler_hz(f) == a.surface_rows
        True
        >>> bool(f[0] == 0.0 and f.min() < 0.0 < f.max())
        True

        """

    def surface_chip_phase(self, out: NDArray[np.float64]) -> int:
        """The surface's code-phase axis: the chip phase of each column.

        One value per surface column, in chips, the same mapping
        acq_build_handoff() applies to a hit's `code_phase` — so a plotted peak
        sits at the chip phase the DetectionEvent would carry.

        Parameters
        ----------
        out : NDArray[np.float64]
            At least `code_bins` doubles.

        Returns
        -------
        int
            Values written (`code_bins`), or 0 if out is too small.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(
        ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
        >>> a = Acquisition(code, spc=2, chip_rate=1e6, cn0_dbhz=50.0)
        >>> c = np.empty(a.code_bins, dtype=np.float64)
        >>> a.surface_chip_phase(c) == a.code_bins
        True
        >>> bool(c[0] == 0.0 and c[1] == 510.5)
        True

        """

    def surface_complex(self, out: NDArray[np.complex64]) -> int:
        """The last decided dwell's surface, complex: amplitude and carrier
        phase per cell, before the magnitude the gate reads.

        Copies the coherent sum the last dwell was decided on into out,
        row-major `surface_rows` x `code_bins` like acq_surface(), in the
        correlation's own units rather than the gate's. A cell's phase is the
        carrier at the block's middle, relative to its tile's centre; its
        neighbours along the code axis are complex early and late arms, so a
        tracker can form the coherent discriminator `Re(conj(P) (L - E)) /
        |P|^2`, which the magnitude surface cannot
        (docs/design/async-dsss-receiver-measurements.md §12.21). Coherent path
        only: a non-coherent dwell (`n_noncoh > 1`) is a power sum with no
        phase, and reads 0.

        Parameters
        ----------
        out : NDArray[np.complex64]
            At least `surface_rows * code_bins` complex floats.

        Returns
        -------
        int
            Cells written (`surface_rows * code_bins`), or 0 when no dwell has
            been decided, the path is non-coherent, or out is too small.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> a = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=70.0)
        >>> a.n_noncoh                 # one look: the dwell is the coherent dump
        1
        >>> _ = a.push(np.roll(s0, 17).astype(np.complex64))
        >>> sc = np.empty(a.surface_rows * a.code_bins, dtype=np.complex64)
        >>> a.surface_complex(sc) == sc.size
        True
        >>> int(np.argmax(np.abs(sc)) % a.code_bins)   # the peak's code phase
        17

        """

    def block_prompt(
        self,
        tile: int,
        col: int,
        out: NDArray[np.complex64],
    ) -> int:
        """One cell's column of the last whole block: the per-epoch complex
        correlations at a code phase, the despread stream at epoch rate.

        The block-coherent engine gathers every tile's correlation row for
        `coherent_bins` epochs before its slow-time transform (file doc, design
        §2.3). This copies the `coherent_bins` values at column col of tile
        tile into out, in epoch order: each epoch's complex prompt at that code
        phase, rolled to the tile's centre frequency and shifted to the block's
        middle by the tile's code-rate hypothesis, so the phase is continuous
        along the column for an emitter at the tile's centre and rotates at its
        offset from it. At an emitter's cell this is what a despreader
        produces, one value per epoch, phase included
        (docs/design/async-dsss-receiver-measurements.md §12.21). Valid once a
        block is whole, until the next epoch is pushed.

        Parameters
        ----------
        tile : int
            Tile index, `0 … window_bins-1` (native FFT order, the order the
            surface's rows are cut in).
        col : int
            Code-phase column, `0 … code_bins-1`.
        out : NDArray[np.complex64]
            At least `coherent_bins` complex floats.

        Returns
        -------
        int
            Values written (`coherent_bins`), or 0 at `coherent_bins == 1` (no
            block is gathered), while a block is partial, for an index out of
            range, or when out is too small.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> b = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=60.0,
        ...                 doppler_uncertainty=40e3, code_only_epochs=7)
        >>> b.coherent_bins                    # (7 + 1) // 2
        4
        >>> blk = np.tile(np.roll(s0, 17), b.coherent_bins).astype(np.complex64)
        >>> _ = b.push(blk)
        >>> p = np.empty(b.coherent_bins, dtype=np.complex64)
        >>> b.block_prompt(0, 17, p) == b.coherent_bins
        True
        >>> bool(np.abs(p).min() > 0.99 * np.abs(p).max())  # every epoch's prompt
        True
        >>> b.block_prompt(0, 17, np.empty(1, dtype=np.complex64))  # too small
        0

        """

    def block_raw(self, out: NDArray[np.complex64]) -> int:
        """The last whole block's raw samples, as pushed.

        Copies the `coherent_bins * code_bins` samples the block-coherent
        engine gathered for its last whole block into out, epoch by epoch in
        stream order — the samples acq_push() consumed, untouched. Kept for the
        tile-edge re-ask (acq_resolve_tile_alias()); exposed so a tracker can
        re-correlate them at any code phase, rate or symbol boundary the
        engine's own grid does not have — a symbol-rate despreader at the
        tracked timing runs on exactly this
        (docs/design/async-dsss-receiver-measurements.md §12.21). Valid once a
        block is whole, until the next epoch is pushed.

        Parameters
        ----------
        out : NDArray[np.complex64]
            At least `coherent_bins * code_bins` complex floats.

        Returns
        -------
        int
            Samples written (`coherent_bins * code_bins`), or 0 at
            `coherent_bins == 1`, while a block is partial, or when out is too
            small.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import Acquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> b = Acquisition(code, spc=4, chip_rate=1e6, cn0_dbhz=60.0,
        ...                 doppler_uncertainty=40e3, code_only_epochs=7)
        >>> blk = np.tile(np.roll(s0, 17), b.coherent_bins).astype(np.complex64)
        >>> _ = b.push(blk)
        >>> raw = np.empty(b.coherent_bins * b.code_bins, dtype=np.complex64)
        >>> b.block_raw(raw) == raw.size
        True
        >>> bool(np.array_equal(raw, blk))      # the samples as pushed
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Acquisition has already been destroyed.

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

        Raises ``RuntimeError`` if the Acquisition has already been destroyed.

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
        ``RuntimeError`` if the Acquisition has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def max_peaks(self) -> int:
        """The peak list's capacity per dwell (1 = the classic gated maximum);
        set with set_max_peaks().
        """

    @property
    def carrier_freq_hz(self) -> float:
        """RF carrier the Doppler is physically coupled to, Hz (0.0 =
        uncoupled); set with set_carrier_freq_hz().
        """

    @property
    def code_bins(self) -> int:
        """Code-phase hypotheses searched (= sf*spc, one code period)."""

    @property
    def doppler_bins(self) -> int:
        """Native Doppler bins this engine searches: the window-tile count
        times the block-coherent depth inside each tile (`coherent_bins`), one
        uniform grid of `doppler_res_hz` over the tiled span in FFT-bin order
        -- what a hit's `doppler_bin` indexes.
        """

    @property
    def coherent_bins(self) -> int:
        """The block-coherent depth D inside every window tile (design §2.3):
        epochs per block and Doppler rows per tile. 1 without a code-only
        window.
        """

    @property
    def sf(self) -> int:
        """Chips per PN segment, inferred from len(code)."""

    @property
    def spc(self) -> int:
        """Samples per chip (chip-rate oversample factor)."""

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks per detection (1 = pure coherent)."""

    @property
    def ring_cap(self) -> int:
        """Input ring capacity in complex samples."""

    @property
    def noise_lo(self) -> int:
        """First CFAR reference bin (inclusive)."""

    @property
    def noise_hi(self) -> int:
        """Last CFAR reference bin (inclusive)."""

    @property
    def threshold(self) -> float:
        """CFAR gate on the test statistic (coherent path)."""

    @property
    def eta(self) -> float:
        """Raw per-cell Rayleigh amplitude threshold."""

    @property
    def eta_nc(self) -> float:
        """Non-coherent CFAR threshold (order-N_nc Marcum)."""

    @property
    def pfa_cell(self) -> float:
        """Bonferroni per-cell false-alarm probability over the searched
        cells.
        """

    @property
    def pd_predicted(self) -> float:
        """Predicted Pd at cn0_dbhz and the chosen grid: the average Pd over
        the straddle priors (slow-time scalloping, intra-segment rotation,
        code-phase sample offset - quadrature over uniform priors), matching
        what the Monte-Carlo characterization measures rather than the on-grid
        best case.
        """

    @property
    def straddle_loss(self) -> float:
        """Mean amplitude derating of the correlation peak from grid straddle
        (slow-time Doppler scalloping x intra-segment rotation x code-phase
        sample offset, each averaged over a uniform prior) - a diagnostic
        summary; 20*log10(straddle_loss) is the loss in dB. Sizing and
        pd_predicted average Pd itself over the priors (Pd at this mean
        amplitude would overstate the mean Pd).
        """

    @property
    def fs(self) -> float:
        """Sample rate (Hz) = chip_rate * spc."""

    @property
    def chip_rate(self) -> float:
        """Chip rate (Hz)."""

    @property
    def cn0_dbhz(self) -> float:
        """Carrier-to-noise density used to size the search (dB-Hz)."""

    @property
    def doppler_span_hz(self) -> float:
        """Native unambiguous Doppler half-range = +/- chip_rate/(2*sf) Hz."""

    @property
    def doppler_res_hz(self) -> float:
        """Doppler bin width = chip_rate/(sf*doppler_bins) Hz."""

    @property
    def pd(self) -> float:
        """Target detection probability."""

    @property
    def underpowered(self) -> bool:
        """True when pd_predicted < pd -- the search cannot meet the target pd
        at this cn0_dbhz and geometry. The engine still builds a best-effort
        grid rather than failing; because C cannot raise a Python warning from
        a successful create, construction also emits a UserWarning in this
        case.
        """

    @property
    def symbol_rate(self) -> float:
        """Continuous data-symbol rate (Hz) this engine was built with --
        diagnostic only, doesn't feed sizing (this engine never coherently
        combines regardless).
        """

    @property
    def epochs_per_symbol(self) -> float:
        """(chip_rate/sf)/symbol_rate -- code epochs per data symbol; 0 when
        symbol_rate is 0.
        """

    @property
    def threads(self) -> int:
        """Workers the searcher fans its tiles across, the calling thread
        included (design §2.3); 1 = serial. Set with set_threads(); a
        continuous engine with more than one tile starts at the machine's
        online core count.
        """

    @property
    def keep_surface(self) -> int:
        """1 keeps every decided dwell's surface for `surface()` (normalised
        into the gate's units at each decision); 0 (the default) costs nothing.
        `set_surface_sink()` in C sets it.
        """
    @keep_surface.setter
    def keep_surface(self, value: int) -> None: ...

    @property
    def surface_rows(self) -> int:
        """Rows of the surface `surface()` returns: the Doppler axis in surface
        units (tiles x interpolated slow-time rows); its columns are
        `code_bins`.
        """

    @property
    def surface_at(self) -> int:
        """`samples_consumed` of the dwell whose surface `surface()` returns (0
        until one has been captured).
        """

    @property
    def n_peaks(self) -> int:
        """Picks in the last decided dwell, held twins included."""

    @property
    def n_held(self) -> int:
        """Picks of the last decided dwell held as same-code-phase twins rather
        than listed (design §7.1).
        """

    @property
    def peak_conc(self) -> float:
        """Concentration of the last dwell's strongest peak: the power of its
        main lobe (its row and one either side, the exclusion zone's width)
        over the total power of its code-phase column across every Doppler row
        and tile. Near 1 for a clean single emitter, even one straddling two
        tiles; about 0.5 when a data transition splits it into twins two or
        more tiles away; lower when a coherent block straddles data (design
        §2.4).
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


    def __enter__(self) -> "Acquisition":
        """Enter a context manager, returning this object.

        Lets a Acquisition be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Acquisition
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Acquisition.

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
class BurstAcquisition:
    """Create a burst-mode acquisition engine for any repeated preamble, given
    as its samples (forwards to acq_create_burst() -- see its doc comment in
    acq_core.h for the full physics).

    Parameters
    ----------
    preamble : NDArray[np.complex64]
        One period of the preamble, preamble_len samples; not all zero, every
        sample finite.
    reps : int, default 1
        Max coherent repetitions (>= 1).
    fs : float, default 1.0
        Sample rate in Hz (> 0); 1 for normalized units.
    cn0_dbhz : float
        Design carrier-to-noise density in dB-Hz, of the preamble's mean power:
        any finite value, or NaN (ACQ_CN0_NONE) for no design point -- size for
        the whole preamble.
    doppler_uncertainty : float, default 0.0
        One-sided Doppler search half-range in Hz.
    pfa : float, default 1e-3
        Target system false-alarm probability (0,1).
    pd : float, default 0.9
        Target detection probability (0,1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
    doppler_rate : float, default 0.0
        Doppler rate in Hz/s (>= 0) that caps the coherent depth at
        `f_epoch/sqrt(2*doppler_rate)` repetitions (doppler#1482); 0 is no
        bound.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``BurstAcquisition:
        invalid parameter (need a non-empty preamble with finite, non-zero
        energy, reps >= 1, fs > 0, cn0_dbhz finite or NaN, doppler_uncertainty
        >= 0, doppler_rate >= 0, 0 < pfa < 1, 0 < pd < 1)``.

    Warns
    -----
    UserWarning
        Emitted after construction when ``underpowered`` holds:
        ``BurstAcquisition is under-powered: pd_burst < pd at this
        reps/cn0_dbhz. Raise reps or cn0_dbhz, or narrow
        doppler_uncertainty.``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.cvt import bin_to_nrz
    >>> from doppler.acquire import BurstAcquisition
    >>> from doppler.wfm import PN, mls_poly
    >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
    ...                      length=5).generate(31)).astype(np.uint8)
    >>> nrz = np.zeros(31, np.float32)
    >>> _ = bin_to_nrz(code & 1, nrz)
    >>> s0 = np.repeat(nrz, 4).astype(np.complex64)    # 4 samples a chip
    >>> burst = np.tile(np.roll(s0, 17), 24)
    >>> b = BurstAcquisition(s0, reps=8, fs=4e6, cn0_dbhz=50.0)
    >>> b.push(burst)[0][:2]      # detects (Doppler bin, delay in samples)
    (0, 17)

    Any repeated preamble is searched the same way -- here a 127-sample
    Zadoff-Chu sequence, in normalized units:

    >>> k = np.arange(127)
    >>> zc = np.exp(-1j * np.pi * 5 * k * (k + 1) / 127).astype(
    ...     np.complex64)
    >>> z = BurstAcquisition(zc, reps=8)
    >>> z.code_bins                # samples per repetition
    127
    >>> z.push(np.tile(np.roll(zc, 40), 10))[0][:2]
    (0, 40)

    """
    def __init__(
        self,
        preamble: NDArray[np.complex64],
        reps: int = 1,
        fs: float = 1.0,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = 0.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
        doppler_rate: float = 0.0,
    ) -> None: ...

    def reset(self) -> None:
        """Drain the input ring and reset the coherent accumulator.

        Forwards to acq_reset() on the embedded engine: discards any buffered
        samples that have not yet completed a frame and clears the non-coherent
        power accumulator and dwell bookkeeping, so the next push() begins a
        fresh search from an empty ring. Construction parameters are untouched.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstAcquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
        >>> b = BurstAcquisition(s0, reps=8, fs=4e6, cn0_dbhz=50.0)
        >>> _ = b.push(burst[:100])   # a partial frame, buffered mid-stream
        >>> b.reset()                 # drop it before it can bias a detection
        >>> b.push(burst)[0][:2]      # (Doppler bin, delay in samples)
        (0, 17)

        """

    def push(
        self,
        x: complex,
    ) -> list[tuple[int, int, float, float, float, float, int]]:
        """Stream raw samples; emit one event per CFAR dump above threshold.

        Forwards to acq_push() on the embedded engine (see its doc comment in
        acq_core.h for the framing/CFAR mechanics). Each event carries the
        peak's Doppler bin and code phase (the two search axes), its CFAR
        statistic, and an estimated C/N0 — see acq_result_t.

        Parameters
        ----------
        x : complex
            Raw input, interleaved CF32, n_in complex samples.

        Returns
        -------
        list[tuple[int, int, float, float, float, float, int]]
            Number of events written (0 … max_results).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstAcquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
        >>> b = BurstAcquisition(s0, reps=8, fs=4e6, cn0_dbhz=50.0)
        >>> b.push(burst)[0][:2]      # (Doppler bin, delay in samples)
        (0, 17)

        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the search grid directly, bypassing both auto-sizing searches --
        the advanced escape hatch (mirrors
        Dll.configure_lock_raw/Costas.configure_lock). Resizes every
        buffer/plan that depends on the grid (the slow-time FFT, the code
        correlator, the reference, and every per-frame scratch buffer),
        re-derives the threshold ladder for the pinned grid from the same
        physics __init__ used, and clears in-flight accumulation (ring
        contents, the non-coherent power accumulator, dwell bookkeeping) --
        call between push() calls, never a substitute for one. Raises
        ValueError if doppler_bins is outside [1, reps] or n_noncoh is outside
        [1, 256] (the internal non-coherent-look safety-valve ceiling).

        Forwards to acq_configure_search_raw() on the embedded engine (see its
        doc comment in acq_core.h): resizes every grid-dependent buffer/plan,
        re-derives the threshold ladder for the pinned grid, and clears
        in-flight accumulation — call between push() calls, never a substitute
        for one.

        Parameters
        ----------
        doppler_bins : int
            Coherent depth to pin, in `[1, reps]`.
        n_noncoh : int
            Non-coherent look count to pin, in `[1,
            ACQ_N_NONCOH_SAFETY_CEILING]`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstAcquisition
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> b = BurstAcquisition(s0, reps=8, fs=4e6, cn0_dbhz=50.0)
        >>> b.configure_search_raw(doppler_bins=4, n_noncoh=2)  # pin the grid
        >>> b.doppler_bins, b.n_noncoh
        (4, 2)
        >>> burst = np.tile(np.roll(s0, 17), 8).astype(np.complex64)
        >>> b.push(burst)[0][:2]      # detects at the pinned grid
        (0, 17)

        """

    def set_max_peaks(self, n: int) -> None:
        """How many peaks a dwell may report -- the peak list's capacity
        (docs/design/async-dsss-receiver.md section 7.1). One (the default) is
        the classic gated maximum. More lists every peak above the same gate,
        strongest first, with an exclusion zone of one Doppler bin by one chip
        around each (one emitter's main lobe, so its own shoulders are not the
        next peak) and the two-epoch rule for a peak at an already-listed code
        phase (a data transition inside the epoch splits one emitter into twins
        at its own code phase on other tiles; such a peak is held for one dwell
        and listed only if it is still there, at the same tile, on the next).
        Each listed peak is one record from push(), all of a dwell's sharing
        samples_consumed and noise_est; a held twin takes one of the n slots
        that dwell but is not reported. The threshold does not change with n.
        Raises ValueError outside 1..64. Clears the held candidates.

        Forwards to acq_set_max_peaks() on the embedded engine (see its doc
        comment in acq_core.h): one is the classic gated maximum; more is the
        list of docs/design/async-dsss-receiver.md §7.1 -- every peak above the
        same gate, strongest first, an exclusion zone of one Doppler bin by the
        reference's first autocorrelation null (one chip for a PN code) around
        each, and the two-epoch rule for a peak at an already-listed code
        phase. Each listed peak is one result from push().

        Parameters
        ----------
        n : int
            1 … ACQ_MAX_PEAKS.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``set_max_peaks failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import BurstAcquisition
        >>> code = (np.arange(31) * 5 % 2).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> b = BurstAcquisition(s0, reps=8, fs=4e6, cn0_dbhz=50.0)
        >>> b.set_max_peaks(4)
        >>> b.max_peaks
        4

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the BurstAcquisition has already been
        destroyed.

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

        Raises ``RuntimeError`` if the BurstAcquisition has already been
        destroyed.

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
        ``RuntimeError`` if the BurstAcquisition has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def max_peaks(self) -> int:
        """The peak list's capacity per dwell (1 = the classic gated maximum);
        set with set_max_peaks().
        """

    @property
    def code_bins(self) -> int:
        """Delay hypotheses searched: one repetition of the preamble, in
        samples (len(preamble)).
        """

    @property
    def doppler_bins(self) -> int:
        """Coherent depth chosen: the slow-time FFT length in code reps (<=
        reps), unless doppler_uncertainty exceeds the native span, in which
        case this reports the wideband window-tile count instead (coherent
        depth forced to 1 -- see acq_core.h's file doc comment).
        """

    @property
    def reps(self) -> int:
        """Max coherent code repetitions (the coherence ceiling)."""

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks per detection (1 = pure coherent)."""

    @property
    def ring_cap(self) -> int:
        """Input ring capacity in complex samples."""

    @property
    def noise_lo(self) -> int:
        """First CFAR reference bin (inclusive)."""

    @property
    def noise_hi(self) -> int:
        """Last CFAR reference bin (inclusive)."""

    @property
    def threshold(self) -> float:
        """CFAR gate on the test statistic (coherent path)."""

    @property
    def eta(self) -> float:
        """Raw per-cell Rayleigh amplitude threshold."""

    @property
    def eta_nc(self) -> float:
        """Non-coherent CFAR threshold (order-N_nc Marcum)."""

    @property
    def pfa_cell(self) -> float:
        """Bonferroni per-cell false-alarm probability over the searched
        cells.
        """

    @property
    def pd_predicted(self) -> float:
        """Predicted Pd at cn0_dbhz and the chosen grid: the average Pd over
        the straddle priors (slow-time scalloping, intra-segment rotation,
        code-phase sample offset - quadrature over uniform priors), matching
        what the Monte-Carlo characterization measures rather than the on-grid
        best case.
        """

    @property
    def pd_burst(self) -> float:
        """Predicted Pd of one burst of `reps` repetitions at cn0_dbhz: the
        dwells are aligned to the stream, so the preamble lands at a uniform
        offset and spans about reps/doppler_bins of them, whole or partial, and
        the burst is detected when any one is. The number the sizer meets `pd`
        with and `underpowered` is set from; pd_predicted is one aligned dwell.
        NaN with no design cn0_dbhz or with n_noncoh > 1.
        """

    @property
    def straddle_loss(self) -> float:
        """Mean amplitude derating of the correlation peak from grid straddle
        (slow-time Doppler scalloping x intra-segment rotation x code-phase
        sample offset, each averaged over a uniform prior) - a diagnostic
        summary; 20*log10(straddle_loss) is the loss in dB. Sizing and
        pd_predicted average Pd itself over the priors (Pd at this mean
        amplitude would overstate the mean Pd).
        """

    @property
    def fs(self) -> float:
        """Sample rate (Hz) the preamble was given at."""

    @property
    def doppler_rate(self) -> float:
        """Doppler rate (Hz/s) the coherent depth is bounded against:
        coherent_bins <= f_epoch/sqrt(2*doppler_rate). 0 is no bound.
        """

    @property
    def cn0_dbhz(self) -> float:
        """Carrier-to-noise density used to size the search (dB-Hz)."""

    @property
    def doppler_span_hz(self) -> float:
        """Native unambiguous Doppler half-range = +/- fs/(2*len(preamble))
        Hz.
        """

    @property
    def doppler_res_hz(self) -> float:
        """Doppler bin width = fs/(len(preamble)*doppler_bins) Hz."""

    @property
    def pd(self) -> float:
        """Target detection probability."""

    @property
    def underpowered(self) -> bool:
        """True when pd_burst < pd -- the search cannot meet the target pd at
        this cn0_dbhz and geometry. The engine still builds a best-effort grid
        rather than failing; because C cannot raise a Python warning from a
        successful create, construction also emits a UserWarning in this
        case.
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


    def __enter__(self) -> "BurstAcquisition":
        """Enter a context manager, returning this object.

        Lets a BurstAcquisition be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        BurstAcquisition
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the BurstAcquisition.

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
        finite value, or NaN (ACQ_CN0_NONE) for no design point.
    doppler_uncertainty : float, default 0.0
        Doppler search half-range, Hz (0 = native).
    pfa : float, default 1e-3
        Target false-alarm probability, in (0, 1).
    pd : float, default 0.9
        Target detection probability, in (0, 1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR reference: 0=mean, 1=median, 2=min, 3=max.
    doppler_rate : float, default 0.0
        Doppler rate, Hz/s (>= 0), that caps the acquisition's coherent depth
        at `f_epoch/sqrt(2*doppler_rate)` repetitions (doppler#1482); 0 is no
        bound.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``BurstCapture: invalid
        parameter (need a non-empty preamble with finite, non-zero energy, reps
        >= 1, fs > 0, burst_len >= 1, cn0_dbhz finite or NaN, doppler_rate >=
        0, 0 < pfa < 1, 0 < pd < 1)``.

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

    """
    def __init__(
        self,
        preamble: NDArray[np.complex64],
        burst_len: int = 8192,
        reps: int = 5,
        fs: float = 1.0,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = 0.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
        doppler_rate: float = 0.0,
    ) -> None: ...

    def push(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Stream raw cf32 samples and get back the SAMPLES of every burst
        whose window has fully arrived, concatenated: burst i occupies
        burst_len samples starting at i*burst_len, and events() returns the
        matching record for each. Samples feed the embedded BurstAcquisition
        and are retained in a history ring; when a detection fires, the refine
        stage correlates one code period at each preamble position to recover
        the exact preamble start -- the one quantity acquisition structurally
        cannot report, since its code_phase is a lag modulo one code period --
        and the window is emitted the moment its last sample has arrived. It
        stops there: what to DO with a burst (demodulate it, write it to a
        file, ship it to another process) is the caller's. An empty return is
        normal, not an error: it means no burst completed in this call. Accepts
        any block size -- the history ring is a contiguous window over the
        stream and is never reset between bursts, so a burst whose tail falls
        outside one call is completed by a later one.

        Windows are concatenated: burst `i` occupies `burst_len` samples
        starting at `i*burst_len`, and events() returns the matching record for
        each. Every sample of x is consumed. An empty return is normal -- it
        means no burst completed in this call.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples, x_len long.
        out : NDArray[np.complex64] | None
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

    def push_max_out(self, x_len: int) -> int:
        """Upper bound on samples push() can return for x_len input.

        Distinct bursts cannot overlap, so `x_len` samples complete at most

        `x_len/burst_len + 1` of them, plus whatever is already queued.

        Parameters
        ----------
        x_len : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def detections(
        self,
        count: int = 1,
        out: NDArray[Any] | None = None,
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
        out : NDArray[Any] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[Any]
            Output.

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

    def detections_max_out(self, n: int) -> int:
        """Raw detections available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def events(
        self,
        count: int = 1,
        out: NDArray[Any] | None = None,
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
        out : NDArray[Any] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[Any]
            Output.

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

    def events_max_out(self, n: int) -> int:
        """Records available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded BurstAcquisition's search grid directly, bypassing
        the auto-sizing -- the escape hatch for a caller who wants a specific
        (doppler_bins, n_noncoh). Forwards to the engine unchanged.

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
            Input.
        n_noncoh : int
            Input.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

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
            Input.

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
        untouched; `dropped` deliberately survives, because a lost burst stays
        lost.

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

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the BurstCapture has already been destroyed.

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
        ``RuntimeError`` if the BurstCapture has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def preamble_start(self) -> int:
        """Stream-absolute sample index the most recent window's preamble
        starts at. NEVER LATE: a window that began after the preamble has
        destroyed the burst, so the refine stage's obligation is to be
        early-or-exact and to say how early.
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
    def burst_len(self) -> int:
        """Samples in one emitted window -- the burst length this capture was
        built for, and the stride of a row in push()'s return.
        """

    @property
    def refine_span(self) -> int:
        """Coalescing window, in samples -- the reach over which two detections
        are ONE preamble. Both sides of that test are burst STARTS (resolved
        code epochs), so this bounds start-to-start separation, NOT the dead
        air between bursts. The two differ by a whole burst, and reading it as
        dead air costs a caller real airtime for nothing: the gap actually
        required is `max(0, refine_span - burst_len)`, which is 0 whenever a
        burst is longer than the refine reach (doppler#1085).
        """

    @property
    def min_gap(self) -> int:
        """Dead air to leave BETWEEN bursts, in samples — edge to edge, not
        start to start. Derived rather than documented as a rule the caller has
        to apply: a detection's anchor is the code epoch of whichever frame
        detected, and acquisition's framing is not aligned to the preamble, so
        the last frame that can detect sits up to `reps * code_period` past the
        true start. CLAIM merges two anchors closer than `refine_span`, so a
        pair survives only when `gap >= refine_span + reps*code_period -
        burst_len`. **Zero is a real answer** — a burst longer than
        `refine_span + reps*P` needs no gap for the claim rule's sake — but it
        does not mean zero is wise: a zero gap is a continuous stream rather
        than a burst link, and it measures 88% at a geometry where this reads
        0. Replaces the prose `max(0, refine_span - burst_len)`, which was
        short by the whole detection-lag term: 32 samples against 528 at the C
        suite's geometry (doppler#1172).
        """

    @property
    def retain_span(self) -> int:
        """History kept per anchor, in samples -- the MINIMUM TRAILING CONTEXT.
        `refine_span` plus one whole burst. A burst closer than this to the end
        of what has been pushed is held rather than emitted, because refine
        cannot yet see the samples it needs. Feed at least this many more, or
        the last burst of a capture never comes out.
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
        """Doppler hypotheses searched: the coherent depth the sizer chose
        (bounded by `reps`), or, when `doppler_uncertainty` exceeds the native
        span, the window-tile count. The same number
        `BurstAcquisition.doppler_bins` reads. `configure_search_raw` pins
        it.
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
        push() deliberately emits nothing for these: a window is returned when
        it is complete, not when it is guessed at. What this exists for is the
        other end -- a caller closing a file or a socket while this is non-zero
        is discarding a burst that would have been captured, and every other
        read-back looks identical to "nothing was ever there".
        """

    @property
    def dropped(self) -> int:
        """Samples the history ring refused, lifetime. A LOST BURST each, not a
        statistic -- it survives reset().
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
        (ACQ_CN0_NONE) for no design point.
    doppler_uncertainty : float, default 0.0
        Doppler search half-range, Hz (0 = native).
    pfa : float, default 1e-3
        Target false-alarm probability, in (0, 1).
    pd : float, default 0.9
        Target detection probability, in (0, 1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR reference: 0=mean, 1=median, 2=min, 3=max.
    doppler_rate : float, default 0.0
        Doppler rate, Hz/s (>= 0), that caps the acquisition's coherent depth
        at `f_epoch/sqrt(2*doppler_rate)` repetitions (doppler#1482); 0 is no
        bound.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``BurstCapture: invalid
        parameter (need a non-empty preamble with finite, non-zero energy, reps
        >= 1, fs > 0, burst_len >= 1, cn0_dbhz finite or NaN, doppler_rate >=
        0, 0 < pfa < 1, 0 < pd < 1)``.

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
    >>> import numpy as np, tempfile, os
    >>> from doppler.acquire import BurstCapture, PersistentBurstCapture
    >>> code = np.array([1, 1, 1, 0, 1, 0, 0], dtype=np.uint8)
    >>> pre = np.repeat(np.where(code, -1.0, 1.0), 2).astype(np.complex64)
    >>> path = os.path.join(tempfile.mkdtemp(), "ring.cf32")
    >>> cap = PersistentBurstCapture(path, pre, burst_len=512,
    ...                             reps=4, fs=2e6)
    >>> ram = BurstCapture(pre, burst_len=512, reps=4, fs=2e6)
    >>> _ = cap.push(np.zeros(4096, dtype=np.complex64))
    >>> # the look-back is in the file, so the blob stops carrying it
    >>> ram.state_bytes() - cap.state_bytes() == ram.retain_span * 8
    True
    >>> os.path.getsize(path) > 0
    True

    """
    def __init__(
        self,
        path: str | os.PathLike,
        preamble: NDArray[np.complex64],
        burst_len: int = 8192,
        reps: int = 5,
        fs: float = 1.0,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = 0.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
        doppler_rate: float = 0.0,
    ) -> None: ...

    def push(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Stream raw cf32 samples and get back the SAMPLES of every burst
        whose window has fully arrived, concatenated: burst i occupies
        burst_len samples starting at i*burst_len, and events() returns the
        matching record for each. Samples feed the embedded BurstAcquisition
        and are retained in a history ring; when a detection fires, the refine
        stage correlates one code period at each preamble position to recover
        the exact preamble start -- the one quantity acquisition structurally
        cannot report, since its code_phase is a lag modulo one code period --
        and the window is emitted the moment its last sample has arrived. It
        stops there: what to DO with a burst (demodulate it, write it to a
        file, ship it to another process) is the caller's. An empty return is
        normal, not an error: it means no burst completed in this call. Accepts
        any block size -- the history ring is a contiguous window over the
        stream and is never reset between bursts, so a burst whose tail falls
        outside one call is completed by a later one.

        Windows are concatenated: burst `i` occupies `burst_len` samples
        starting at `i*burst_len`, and events() returns the matching record for
        each. Every sample of x is consumed. An empty return is normal -- it
        means no burst completed in this call.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples, x_len long.
        out : NDArray[np.complex64] | None
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

    def push_max_out(self, x_len: int) -> int:
        """Upper bound on samples push() can return for x_len input.

        Distinct bursts cannot overlap, so `x_len` samples complete at most

        `x_len/burst_len + 1` of them, plus whatever is already queued.

        Parameters
        ----------
        x_len : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def detections(
        self,
        count: int = 1,
        out: NDArray[Any] | None = None,
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
        out : NDArray[Any] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[Any]
            Output.

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

    def detections_max_out(self, n: int) -> int:
        """Raw detections available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def events(
        self,
        count: int = 1,
        out: NDArray[Any] | None = None,
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
        out : NDArray[Any] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[Any]
            Output.

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

    def events_max_out(self, n: int) -> int:
        """Records available from the last push(). n is ignored.

        Parameters
        ----------
        n : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded BurstAcquisition's search grid directly, bypassing
        the auto-sizing -- the escape hatch for a caller who wants a specific
        (doppler_bins, n_noncoh). Forwards to the engine unchanged.

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
            Input.
        n_noncoh : int
            Input.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``configure_search_raw failed``, with the return code appended
            (gh-869).

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
            Input.

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
        untouched; `dropped` deliberately survives, because a lost burst stays
        lost.

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

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

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

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the PersistentBurstCapture has already been
        destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def preamble_start(self) -> int:
        """Stream-absolute sample index the most recent window's preamble
        starts at. NEVER LATE: a window that began after the preamble has
        destroyed the burst, so the refine stage's obligation is to be
        early-or-exact and to say how early.
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
    def burst_len(self) -> int:
        """Samples in one emitted window -- the burst length this capture was
        built for, and the stride of a row in push()'s return.
        """

    @property
    def refine_span(self) -> int:
        """Coalescing window, in samples -- the reach over which two detections
        are ONE preamble. Both sides of that test are burst STARTS (resolved
        code epochs), so this bounds start-to-start separation, NOT the dead
        air between bursts. The two differ by a whole burst, and reading it as
        dead air costs a caller real airtime for nothing: the gap actually
        required is `max(0, refine_span - burst_len)`, which is 0 whenever a
        burst is longer than the refine reach (doppler#1085).
        """

    @property
    def min_gap(self) -> int:
        """Dead air to leave BETWEEN bursts, in samples — edge to edge, not
        start to start. Derived rather than documented as a rule the caller has
        to apply: a detection's anchor is the code epoch of whichever frame
        detected, and acquisition's framing is not aligned to the preamble, so
        the last frame that can detect sits up to `reps * code_period` past the
        true start. CLAIM merges two anchors closer than `refine_span`, so a
        pair survives only when `gap >= refine_span + reps*code_period -
        burst_len`. **Zero is a real answer** — a burst longer than
        `refine_span + reps*P` needs no gap for the claim rule's sake — but it
        does not mean zero is wise: a zero gap is a continuous stream rather
        than a burst link, and it measures 88% at a geometry where this reads
        0. Replaces the prose `max(0, refine_span - burst_len)`, which was
        short by the whole detection-lag term: 32 samples against 528 at the C
        suite's geometry (doppler#1172).
        """

    @property
    def retain_span(self) -> int:
        """History kept per anchor, in samples -- the MINIMUM TRAILING CONTEXT.
        `refine_span` plus one whole burst. A burst closer than this to the end
        of what has been pushed is held rather than emitted, because refine
        cannot yet see the samples it needs. Feed at least this many more, or
        the last burst of a capture never comes out.
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
        """Doppler hypotheses searched: the coherent depth the sizer chose
        (bounded by `reps`), or, when `doppler_uncertainty` exceeds the native
        span, the window-tile count. The same number
        `BurstAcquisition.doppler_bins` reads. `configure_search_raw` pins
        it.
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
        push() deliberately emits nothing for these: a window is returned when
        it is complete, not when it is guessed at. What this exists for is the
        other end -- a caller closing a file or a socket while this is non-zero
        is discarding a burst that would have been captured, and every other
        read-back looks identical to "nothing was ever there".
        """

    @property
    def dropped(self) -> int:
        """Samples the history ring refused, lifetime. A LOST BURST each, not a
        statistic -- it survives reset().
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
    numpy.fft.fftfreq(n) * n, exactly: 0 = DC, ascending positive to
    (n-1)/2, then wrapping negative, so an even grid's Nyquist bin is -n/2.
    Multiply by doppler_res_hz for Hz. Call this rather than writing the
    fold out: the search and its hand-off must agree on the convention, and
    a consumer seeded on the wrong side of it is off by the full search
    span -- a failure that once surfaced here as a receiver reporting
    tracking while decoding noise. A thin wrapper over dp_fftfreq_index()
    in clib_common.h, so C callers inline the same code.

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
