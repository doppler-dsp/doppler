# resample/resample.pyi — type stubs for the resample C extension.
from typing import Any, final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class Resampler:
    """Create a Resampler with the built-in 4096×19 Kaiser bank. The bank
    provides ~60 dB alias rejection with 0.4/0.6 pass/stop normalised cutoffs.
    Pass rate >= 1.0 to interpolate (upsample); pass rate < 1.0 to decimate
    (downsample). For a custom bank use Resampler_create_custom() instead.

    Parameters
    ----------
    rate : float, default 0.0
        Output-to-input sample rate ratio (any positive float). Values >= 1.0
        interpolate; values < 1.0 decimate.

    Examples
    --------
    >>> from doppler.resample import Resampler
    >>> import numpy as np
    >>> r = Resampler(rate=2.0)
    >>> r.num_phases, r.num_taps
    (4096, 19)
    >>> r.rate
    2.0

    """
    def __init__(self, rate: float = ...) -> None: ...

    def execute(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Resample a block of CF32 samples at the fixed base rate. Uses the
        dual-mode polyphase engine: output-driven for rate >= 1
        (interpolation), input-driven transposed-form for rate < 1
        (decimation). State carries over between calls, so contiguous blocks
        produce the same result as one large block.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input samples.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length is approximately x_len * rate, capped at
            max_out.

        Examples
        --------
        >>> from doppler.resample import Resampler
        >>> import numpy as np
        >>> r = Resampler(rate=2.0)
        >>> y = r.execute(np.zeros(128, dtype=np.complex64))
        >>> y.shape, y.dtype
        ((256,), dtype('complex64'))

        """

    def execute_max_out(self) -> int:
        """Always returns RESAMPLER_MAX_OUT.

        Returns
        -------
        int
            Output.
        """

    def execute_ctrl(
        self,
        x: NDArray[np.complex64],
        ctrl: NDArray[np.complex64],
    ) -> NDArray[np.complex64]:
        """Resample with per-sample additive rate deviations. Effective rate
        for sample i is base_rate + real(`ctrl[i]`). Uses a unified
        double-precision accumulator that handles both interpolation and
        decimation in a single code path — suitable for Doppler-shift
        simulation and fractional-sample timing correction. ctrl and x must
        have the same length.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input samples.
        ctrl : NDArray[np.complex64]
            CF32 array, same length as x; only the real part is used as a
            per-sample rate addend.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length depends on accumulated rate deviations,
            capped at max_out.

        Examples
        --------
        >>> from doppler.resample import Resampler
        >>> import numpy as np
        >>> r = Resampler(rate=1.0)
        >>> x = np.zeros(64, dtype=np.complex64)
        >>> ctrl = np.zeros(64, dtype=np.complex64)
        >>> y = r.execute_ctrl(x, ctrl)
        >>> y.shape, y.dtype
        ((64,), dtype('complex64'))

        """

    def reset(self) -> None:
        """Zero the delay line and phase accumulator. Rate and polyphase bank
        are preserved so the resampler can be resumed at the same ratio.
        Zeroing state eliminates transient artefacts when starting a new signal
        burst.

        Examples
        --------
        >>> from doppler.resample import Resampler
        >>> import numpy as np
        >>> r = Resampler(rate=2.0)
        >>> _ = r.execute(np.ones(64, dtype=np.complex64))
        >>> r.reset()
        >>> r.rate
        2.0

        """

    def execute_ctrl_max_out(self) -> int:
        """Max samples ``execute_ctrl()`` can emit, for any input block.

        The bound is the resampler's fixed internal capacity
        (``RESAMPLER_MAX_OUT``), not a function of the block about to be
        passed — which is why this accessor takes no argument.

        Returns
        -------
        int
            Capacity, in samples, to allocate for an ``out=`` buffer.

        Examples
        --------
        >>> from doppler.resample import Resampler
        >>> Resampler(rate=2.0).execute_ctrl_max_out()
        65536

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Resampler has already been destroyed.

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

        Raises ``RuntimeError`` if the Resampler has already been destroyed.

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
        ``RuntimeError`` if the Resampler has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def rate(self) -> float:
        """Get / set the output-to-input sample rate ratio. The setter
        recomputes the phase increment immediately; the delay line and phase
        accumulator are preserved so in-stream rate changes are glitch-free.
        Switching sign of (rate - 1) (i.e. crossing the boundary between interp
        and decim modes) requires a fresh create().
        """
    @rate.setter
    def rate(self, value: float) -> None: ...

    @property
    def num_phases(self) -> int:
        """Number of polyphase branches in the filter bank. Always a power of
        two. The built-in bank has 4096 phases giving sub-sample timing
        resolution of 1/4096 of an input sample period.
        """

    @property
    def num_taps(self) -> int:
        """Taps per polyphase branch. Total prototype filter length is
        num_phases * num_taps - 1. The built-in bank uses 19 taps per
        branch.
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


    def __enter__(self) -> "Resampler":
        """Enter a context manager, returning this object.

        Lets a Resampler be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Resampler
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Resampler.

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
class HalfbandDecimator:
    """Create a HalfbandDecimator with caller-supplied FIR taps. Implements a
    2:1 polyphase halfband decimator over CF32 IQ. The caller provides the FIR
    branch coefficient array h; use ``doppler.resample.kaiser_num_taps(2,
    atten, pb, sb)`` to size it and scipy or the built-in bank helper to design
    the prototype. Output length is approximately x_len / 2 per execute() call.

    Parameters
    ----------
    h : NDArray[np.float32]
        Float32 FIR branch coefficients, length num_taps. Must be a symmetric
        halfband prototype (antisymmetric even-indexed taps zeroed).

    Examples
    --------
    >>> from doppler.resample import HalfbandDecimator
    >>> import numpy as np
    >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
    ...              dtype=np.float32)
    >>> hb = HalfbandDecimator(h=h)
    >>> hb.num_taps, hb.rate
    (5, 0.5)

    """
    def __init__(self, h: NDArray[np.float32]) -> None: ...

    def execute(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Decimate x by 2 using the polyphase halfband FIR filter. Processes
        every second input sample through the FIR branch and passes the other
        branch through the all-pass (zero-delay) path. State persists between
        calls — contiguous blocks give identical output to one large block.
        Output length is floor(x_len / 2).

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input array. Length must be even for exact half-rate output;
            odd lengths write floor(x_len/2).

        Returns
        -------
        NDArray[np.complex64]
            CF32 decimated output; length is min(floor(x_len / 2), max_out).

        Examples
        --------
        >>> from doppler.resample import HalfbandDecimator
        >>> import numpy as np
        >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
        ...              dtype=np.float32)
        >>> hb = HalfbandDecimator(h=h)
        >>> y = hb.execute(np.zeros(100, dtype=np.complex64))
        >>> y.shape, y.dtype
        ((50,), dtype('complex64'))

        """

    def execute_max_out(self) -> int:
        """Always returns HBDECIM_MAX_OUT.

        Returns
        -------
        int
            Output.
        """

    def reset(self) -> None:
        """Zero all delay lines. Coefficients and num_taps preserved. Call
        between signal bursts to suppress transient ringing from prior filter
        state. The next execute() after reset produces the same output as a
        freshly created decimator fed the same input.

        Examples
        --------
        >>> from doppler.resample import HalfbandDecimator
        >>> import numpy as np
        >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
        ...              dtype=np.float32)
        >>> hb = HalfbandDecimator(h=h)
        >>> _ = hb.execute(np.ones(64, dtype=np.complex64))
        >>> hb.reset()
        >>> hb.num_taps
        5

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the HalfbandDecimator has already been
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

        Raises ``RuntimeError`` if the HalfbandDecimator has already been
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
        ``RuntimeError`` if the HalfbandDecimator has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def rate(self) -> float:
        """Fixed decimation rate — always 0.5. The halfband decimator is
        structurally 2:1; this property exists for API parity with Resampler
        and RateConverter.
        """

    @property
    def num_taps(self) -> int:
        """Number of FIR branch taps as passed to create. The all-pass
        (even-phase) branch has no taps; only the odd-phase FIR branch has
        length num_taps. The total prototype length is 2 * num_taps - 1.
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


    def __enter__(self) -> "HalfbandDecimator":
        """Enter a context manager, returning this object.

        Lets a HalfbandDecimator be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        HalfbandDecimator
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the HalfbandDecimator.

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
class CIC:
    """Create a 4-stage, M=1 CIC decimation filter. Allocates the state struct
    on the heap and pre-computes the normalisation right-shift (CIC_N * log2(R)
    bits). All integrator and comb accumulators are zeroed; the first output
    arrives after R input samples. Returns NULL for invalid R or OOM. Input
    amplitude is bounded: |Re| and |Im| <= 1.0. A component beyond +-1.0 is
    clipped at the boundary before any filtering; the sample stream gives no
    sign of it, so check the sticky clipped flag. Unlike doppler's
    floating-point blocks this one is not scale-free -- scale the input into
    range first.

    Parameters
    ----------
    R : int, default 16
        Decimation ratio. Must be a power of two in `[2, 4096]`. Returns NULL
        for R=0, non-power-of-two, or R > 4096.

    Examples
    --------
    >>> from doppler.resample import CIC
    >>> cic = CIC(R=16)
    >>> cic.R, cic.shift
    (16, 16)

    """
    def __init__(self, R: int = ...) -> None: ...

    def reset(self) -> None:
        """Zero all integrator and comb accumulators; preserve R and shift. The
        first output sample after reset arrives after R more input samples,
        matching post-create behaviour. Use between signal bursts to eliminate
        transient artefacts caused by residual pipeline state.

        Examples
        --------
        >>> from doppler.resample import CIC
        >>> cic = CIC(R=16)
        >>> cic.reset()
        >>> cic.R
        16

        """

    def reconfigure(self, R: int) -> None:
        """Change the decimation ratio in place and reset all filter state.
        Recomputes the normalisation shift (CIC_N * log2(R)) and zeros all
        accumulators so the filter behaves exactly like a freshly created one
        with the new R. Silently ignores R values that are not a power-of-two
        in `[2, 4096]` — the state is left unchanged in that case.

        Parameters
        ----------
        R : int
            New decimation ratio. Same constraints as cic_create().

        Examples
        --------
        >>> from doppler.resample import CIC
        >>> cic = CIC(R=4)
        >>> cic.reconfigure(8)
        >>> cic.R, cic.shift
        (8, 12)

        """

    def decimate(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Decimate a block of CF32 samples through the CIC pipeline. Each
        sample is converted to offset-binary UQ16, pushed through CIC_N
        integrators (unsigned wrapping), and when the phase counter reaches R
        the integrated value is passed through CIC_N M=1 comb stages and
        converted back to CF32. State persists between calls. Feeding blocks
        that are multiples of R gives predictable output counts (exactly n_in/R
        samples per block).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length is min(floor((phase + n_in) / R),
            max_out).

        Notes
        -----
        **Input amplitude is bounded: |Re| and |Im| <= 1.0.** A component
        beyond +-1.0 is clipped at the boundary before filtering; the sample
        stream gives no sign of it, so check the sticky clipped flag. Scale the
        input into range first; see the file header.

        Examples
        --------
        >>> from doppler.resample import CIC
        >>> import numpy as np
        >>> cic = CIC(R=16)
        >>> for _ in range(4):
        ...     _ = cic.decimate(np.zeros(16, dtype=np.complex64))
        >>> y = cic.decimate(np.zeros(16, dtype=np.complex64))
        >>> y.tolist(), y.dtype
        ([0j], dtype('complex64'))

        """

    def decimate_max_out(self) -> int:
        """Upper bound on decimate output — returns 0 (lazy-alloc signal).

        The Python extension allocates n_in elements on the first call.

        Since n_in >= ceil(n_in/R) = n_out for all R >= 1, the buffer is

        always large enough as long as block size stays consistent.

        Returns
        -------
        int
            Output.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the CIC has already been destroyed.

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

        Raises ``RuntimeError`` if the CIC has already been destroyed.

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
        ``RuntimeError`` if the CIC has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def R(self) -> int:
        """R."""

    @property
    def shift(self) -> int:
        """Shift."""

    @property
    def clipped(self) -> bool:
        """True if any input component has exceeded the +-1.0 bound since the
        last reset(). Sticky, and free to read: the CIC's boundary comparisons
        run on every sample anyway, so it records something the sample stream
        cannot tell you -- a clipped stream still looks entirely plausible
        (finite, no NaN, merely distorted), so this flag is the only reliable
        check.
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


    def __enter__(self) -> "CIC":
        """Enter a context manager, returning this object.

        Lets a CIC be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        CIC
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the CIC.

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
class RateConverter:
    """Create a rate converter for the given output/input rate ratio. Selects
    the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase Resampler
    stages at construction time (see file header for the selection table).
    Setting compensate=1 appends a closed-form Molnar-Vucic CIC
    droop-compensating FIR after any CIC stage, which improves passband
    flatness at the cost of one extra FIR stage.

    Parameters
    ----------
    rate : float, default 1.0
        Output-to-input sample rate ratio. Any positive float.
    compensate : int, default 0
        Non-zero to append a CIC passband-droop compensating FIR after any CIC
        stage.

    Examples
    --------
    >>> from doppler.resample import RateConverter
    >>> rc = RateConverter(rate=0.5, compensate=0)
    >>> rc.rate
    0.5

    """
    def __init__(self, rate: float = ..., compensate: int = ...) -> None: ...

    def execute(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Convert a block of CF32 samples through the cascade. Passes input
        through each stage in order, ping-ponging between two intermediate
        buffers. State persists between calls, so contiguous calls on
        sequential blocks give the same result as one large call. Output length
        is approximately n_in * rate.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length is approximately n_in * rate.

        Examples
        --------
        >>> from doppler.resample import RateConverter
        >>> import numpy as np
        >>> rc = RateConverter(rate=0.5, compensate=0)
        >>> y = rc.execute(np.zeros(1024, dtype=np.complex64))
        >>> y.shape, y.dtype
        ((512,), dtype('complex64'))

        """

    def execute_max_out(self) -> int:
        """Upper bound on execute output for a standard 65536-sample block.

        Returns (size_t)(65536 * max(rate, 1.0)) + 2. The Python extension uses

        this to pre-allocate the output buffer on the first execute call.

        Returns
        -------
        int
            Output.
        """

    def execute_ctrl(
        self,
        x: NDArray[np.complex64],
        ctrl: float,
    ) -> NDArray[np.complex64]:
        """Convert a block, steering the cascade's fractional stage by ctrl.

        The control-port form of RateConverter_execute(): the fixed integer
        stages (HalfbandDecimator / CIC) run unchanged, and the scalar rate
        deviation ctrl is forwarded to the **terminal polyphase Resampler
        stage's** accumulator (via resamp_execute_ctrl_push) — so its effective
        rate becomes `stage_rate + ctrl` for this call. This exposes the
        fractional tail's control port that RateConverter_execute() hides: a
        timing/rate-tracking loop can decimate a high input rate cheaply
        through the HB/CIC stages and then arbitrary-rate + strobe-align in the
        last stage, updating ctrl per block.

        `ctrl` is referenced to the terminal stage's (post-decimation) rate,
        not the overall rate. It is meaningful only when the cascade actually
        ends in a Resampler stage; a pure integer HB/CIC cascade has no
        fractional stage to steer, so this **falls through to
        RateConverter_execute()** (ctrl ignored).

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input block.
        ctrl : float
            Rate deviation added to the terminal Resampler stage's rate.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length tracks the accumulated effective rate.

        Examples
        --------
        >>> from doppler.resample import RateConverter
        >>> import numpy as np
        >>> rc = RateConverter(rate=0.8, compensate=0)  # -> Resampler(0.8)
        >>> x = np.ones(1000, dtype=np.complex64)
        >>> rc.execute_ctrl(x, 0.0).shape[0]    # base rate: 1000 -> 800
        800
        >>> rc2 = RateConverter(rate=0.8, compensate=0)
        >>> rc2.execute_ctrl(x, 0.05).shape[0]  # +ctrl speeds the tail up
        850

        """

    def execute_ctrl_push(
        self,
        x: complex,
        ctrl: float,
    ) -> NDArray[np.complex64]:
        """Push ONE input sample; emit whatever outputs it completes.

        The per-input streaming form of RateConverter_execute_ctrl(), and the
        only form a closed loop can use: a block call must know its whole
        `ctrl` history up front, whereas a timing loop computes each correction
        *from* the outputs already emitted. Feeding a stream one sample at a
        time through this reproduces RateConverter_execute_ctrl() on the same
        block bit-for-bit when ctrl is held constant (the cascade is
        block-boundary invariant), so the cheap block form stays correct for
        open-loop use.

        The integer HB/CIC stages consume the sample and emit at most one
        intermediate sample each; the terminal Resampler stage then emits 0
        outputs (a decimator between strobes — the common case), 1, or several
        (an interpolator). A cascade with no terminal Resampler ignores ctrl.

        Parameters
        ----------
        x : complex
            One CF32 input sample.
        ctrl : float
            Rate deviation added to the terminal stage's rate for this input
            (referenced to the terminal, post-decimation rate).

        Returns
        -------
        NDArray[np.complex64]
            CF32 array of the outputs completed by this input (0, 1, or more).

        Examples
        --------
        >>> from doppler.resample import RateConverter
        >>> import numpy as np
        >>> rc = RateConverter(rate=0.8, compensate=0)  # -> Resampler(0.8)
        >>> x = (np.arange(10, dtype=np.float32) + 1).astype(np.complex64)
        >>> # a decimator emits 0 between strobes, 1 on a strobe:
        >>> [rc.execute_ctrl_push(complex(v), 0.0).shape[0] for v in x]
        [0, 1, 1, 1, 1, 0, 1, 1, 1, 1]

        """

    def reset(self) -> None:
        """Zero all sub-stage filter memories. Rate, stage count, and stage
        types are preserved. Processing from a reset state produces the same
        output as a freshly created converter fed the same input. Use between
        signal bursts to suppress transient artefacts from prior filter memory.

        Examples
        --------
        >>> from doppler.resample import RateConverter
        >>> rc = RateConverter(rate=0.5, compensate=0)
        >>> rc.reset()
        >>> rc.rate
        0.5

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the RateConverter has already been
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

        Raises ``RuntimeError`` if the RateConverter has already been
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
        ``RuntimeError`` if the RateConverter has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def rate(self) -> float:
        """Get / set the output-to-input sample rate ratio. The setter rebuilds
        the entire cascade (new stage selection, new sub-objects) and resets
        all filter memories — equivalent to destroying and recreating with the
        new rate. Setting rate <= 0 is silently ignored.
        """
    @rate.setter
    def rate(self, value: float) -> None: ...

    @property
    def clipped(self) -> bool:
        """True if any planned CIC stage has clipped its input since the last
        `reset()`. The cascade inherits the CIC's input bound (`|Re|`, `|Im| <=
        1.0`) whenever `stages` names a CIC -- any decimation by 8 or more. The
        clip is invisible in the samples (finite, no NaN, merely distorted), so
        this is the only reliable check, and it is free: the boundary
        comparisons run on every sample regardless. Always False for a cascade
        with no CIC stage -- those plans are scale-free.
        """

    @property
    def narrow_pulse(self) -> bool:
        """True when a rectangular pulse was selected with fewer than four
        output samples per symbol, where its matched filter degenerates to a
        2-3 tap sum. Construction also raises a UserWarning; this is the same
        diagnostic to pull rather than catch. Always False for `pulse="rrc"`
        and for a plain converter.
        """

    @property
    def stages(self) -> list[str]:
        """Stage labels for the planned cascade, e.g. `['CIC(8)',
        'Resampler(0.8)']`. A terminal stage carrying a pulse-shaped bank names
        its pulse: `'Resampler(0.923077,rrc)'`.
        """

    @property
    def bank_shape(self) -> list[int]:
        """`[num_phases, num_taps]` of the terminal polyphase stage, or `[]`
        when the cascade ends in an integer decimator and so has no bank to
        describe. `num_taps` is the per-output MAC count and, times
        `num_phases`, the bank's size in floats. With a pulse selected it is
        set by the terminal stage's rate rather than the input rate -- which is
        what keeps a matched filter affordable at a high input
        samples-per-symbol: the same 34 taps per arm at 4 samples/symbol and at
        256, where filtering at the input rate would need 4225.
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


    def __enter__(self) -> "RateConverter":
        """Enter a context manager, returning this object.

        Lets a RateConverter be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        RateConverter
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the RateConverter.

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
class MatchedRateConverter:
    """Create a rate converter for the given output/input rate ratio. Selects
    the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase Resampler
    stages at construction time (see file header for the selection table).
    Setting compensate=1 appends a closed-form Molnar-Vucic CIC
    droop-compensating FIR after any CIC stage, which improves passband
    flatness at the cost of one extra FIR stage.

    Parameters
    ----------
    rate : float, default 1.0
        Output-to-input sample rate ratio. Any positive float.
    compensate : int, default 1
        Non-zero to append a CIC passband-droop compensating FIR after any CIC
        stage.
    pulse : Literal["iandd", "rrc"], default "rrc"
        pulse constructor parameter.
    beta : float, default 0.35
        beta constructor parameter.
    span : int, default 8
        span constructor parameter.
    pulse_sps : float, default 2.0
        pulse_sps constructor parameter.
    num_phases : int, default 1024
        num_phases constructor parameter.

    Examples
    --------
    >>> from doppler.resample import RateConverter
    >>> rc = RateConverter(rate=0.5, compensate=0)
    >>> rc.rate
    0.5

    """
    def __init__(
        self,
        rate: float = ...,
        compensate: int = ...,
        pulse: Literal["iandd", "rrc"] = "rrc",
        beta: float = ...,
        span: int = ...,
        pulse_sps: float = ...,
        num_phases: int = ...,
    ) -> None: ...

    def execute(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Convert a block of CF32 samples through the cascade. Passes input
        through each stage in order, ping-ponging between two intermediate
        buffers. State persists between calls, so contiguous calls on
        sequential blocks give the same result as one large call. Output length
        is approximately n_in * rate.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length is approximately n_in * rate.

        Examples
        --------
        >>> from doppler.resample import RateConverter
        >>> import numpy as np
        >>> rc = RateConverter(rate=0.5, compensate=0)
        >>> y = rc.execute(np.zeros(1024, dtype=np.complex64))
        >>> y.shape, y.dtype
        ((512,), dtype('complex64'))

        """

    def execute_max_out(self) -> int:
        """Upper bound on execute output for a standard 65536-sample block.

        Returns (size_t)(65536 * max(rate, 1.0)) + 2. The Python extension uses

        this to pre-allocate the output buffer on the first execute call.

        Returns
        -------
        int
            Output.
        """

    def execute_ctrl(
        self,
        x: NDArray[np.complex64],
        ctrl: float,
    ) -> NDArray[np.complex64]:
        """Convert a block, steering the cascade's fractional stage by ctrl.

        The control-port form of RateConverter_execute(): the fixed integer
        stages (HalfbandDecimator / CIC) run unchanged, and the scalar rate
        deviation ctrl is forwarded to the **terminal polyphase Resampler
        stage's** accumulator (via resamp_execute_ctrl_push) — so its effective
        rate becomes `stage_rate + ctrl` for this call. This exposes the
        fractional tail's control port that RateConverter_execute() hides: a
        timing/rate-tracking loop can decimate a high input rate cheaply
        through the HB/CIC stages and then arbitrary-rate + strobe-align in the
        last stage, updating ctrl per block.

        `ctrl` is referenced to the terminal stage's (post-decimation) rate,
        not the overall rate. It is meaningful only when the cascade actually
        ends in a Resampler stage; a pure integer HB/CIC cascade has no
        fractional stage to steer, so this **falls through to
        RateConverter_execute()** (ctrl ignored).

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input block.
        ctrl : float
            Rate deviation added to the terminal Resampler stage's rate.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length tracks the accumulated effective rate.

        Examples
        --------
        >>> from doppler.resample import RateConverter
        >>> import numpy as np
        >>> rc = RateConverter(rate=0.8, compensate=0)  # -> Resampler(0.8)
        >>> x = np.ones(1000, dtype=np.complex64)
        >>> rc.execute_ctrl(x, 0.0).shape[0]    # base rate: 1000 -> 800
        800
        >>> rc2 = RateConverter(rate=0.8, compensate=0)
        >>> rc2.execute_ctrl(x, 0.05).shape[0]  # +ctrl speeds the tail up
        850

        """

    def execute_ctrl_push(
        self,
        x: complex,
        ctrl: float,
    ) -> NDArray[np.complex64]:
        """Push ONE input sample; emit whatever outputs it completes.

        The per-input streaming form of RateConverter_execute_ctrl(), and the
        only form a closed loop can use: a block call must know its whole
        `ctrl` history up front, whereas a timing loop computes each correction
        *from* the outputs already emitted. Feeding a stream one sample at a
        time through this reproduces RateConverter_execute_ctrl() on the same
        block bit-for-bit when ctrl is held constant (the cascade is
        block-boundary invariant), so the cheap block form stays correct for
        open-loop use.

        The integer HB/CIC stages consume the sample and emit at most one
        intermediate sample each; the terminal Resampler stage then emits 0
        outputs (a decimator between strobes — the common case), 1, or several
        (an interpolator). A cascade with no terminal Resampler ignores ctrl.

        Parameters
        ----------
        x : complex
            One CF32 input sample.
        ctrl : float
            Rate deviation added to the terminal stage's rate for this input
            (referenced to the terminal, post-decimation rate).

        Returns
        -------
        NDArray[np.complex64]
            CF32 array of the outputs completed by this input (0, 1, or more).

        Examples
        --------
        >>> from doppler.resample import RateConverter
        >>> import numpy as np
        >>> rc = RateConverter(rate=0.8, compensate=0)  # -> Resampler(0.8)
        >>> x = (np.arange(10, dtype=np.float32) + 1).astype(np.complex64)
        >>> # a decimator emits 0 between strobes, 1 on a strobe:
        >>> [rc.execute_ctrl_push(complex(v), 0.0).shape[0] for v in x]
        [0, 1, 1, 1, 1, 0, 1, 1, 1, 1]

        """

    def reset(self) -> None:
        """Zero all sub-stage filter memories. Rate, stage count, and stage
        types are preserved. Processing from a reset state produces the same
        output as a freshly created converter fed the same input. Use between
        signal bursts to suppress transient artefacts from prior filter memory.

        Examples
        --------
        >>> from doppler.resample import RateConverter
        >>> rc = RateConverter(rate=0.5, compensate=0)
        >>> rc.reset()
        >>> rc.rate
        0.5

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the MatchedRateConverter has already been
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

        Raises ``RuntimeError`` if the MatchedRateConverter has already been
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
        ``RuntimeError`` if the MatchedRateConverter has already been
        destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def rate(self) -> float:
        """Get / set the output-to-input sample rate ratio. The setter rebuilds
        the entire cascade (new stage selection, new sub-objects) and resets
        all filter memories — equivalent to destroying and recreating with the
        new rate. Setting rate <= 0 is silently ignored.
        """
    @rate.setter
    def rate(self, value: float) -> None: ...

    @property
    def clipped(self) -> bool:
        """True if any planned CIC stage has clipped its input since the last
        `reset()`. The cascade inherits the CIC's input bound (`|Re|`, `|Im| <=
        1.0`) whenever `stages` names a CIC -- any decimation by 8 or more. The
        clip is invisible in the samples (finite, no NaN, merely distorted), so
        this is the only reliable check, and it is free: the boundary
        comparisons run on every sample regardless. Always False for a cascade
        with no CIC stage -- those plans are scale-free.
        """

    @property
    def narrow_pulse(self) -> bool:
        """True when a rectangular pulse was selected with fewer than four
        output samples per symbol, where its matched filter degenerates to a
        2-3 tap sum. Construction also raises a UserWarning; this is the same
        diagnostic to pull rather than catch. Always False for `pulse="rrc"`
        and for a plain converter.
        """

    @property
    def stages(self) -> list[str]:
        """Stage labels for the planned cascade, e.g. `['CIC(8)',
        'Resampler(0.8)']`. A terminal stage carrying a pulse-shaped bank names
        its pulse: `'Resampler(0.923077,rrc)'`.
        """

    @property
    def bank_shape(self) -> list[int]:
        """`[num_phases, num_taps]` of the terminal polyphase stage, or `[]`
        when the cascade ends in an integer decimator and so has no bank to
        describe. `num_taps` is the per-output MAC count and, times
        `num_phases`, the bank's size in floats. With a pulse selected it is
        set by the terminal stage's rate rather than the input rate -- which is
        what keeps a matched filter affordable at a high input
        samples-per-symbol: the same 34 taps per arm at 4 samples/symbol and at
        256, where filtering at the input rate would need 4225.
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


    def __enter__(self) -> "MatchedRateConverter":
        """Enter a context manager, returning this object.

        Lets a MatchedRateConverter be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        MatchedRateConverter
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the MatchedRateConverter.

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
class Farrow:
    """Create a Farrow interpolator.

    Parameters
    ----------
    order : Literal["linear", "parabolic", "cubic"], default "cubic"
        0 = linear, 1 = parabolic, 2 = cubic.

    Examples
    --------
    Create with defaults:

    >>> from doppler.resample import Farrow
    >>> obj = Farrow(order="cubic")

    """
    def __init__(
        self,
        order: Literal["linear", "parabolic", "cubic"] = "cubic",
    ) -> None: ...

    def delay(
        self,
        x: NDArray[np.complex64],
        mu: float,
    ) -> NDArray[np.complex64]:
        """Apply a constant fractional delay of `mu` samples to a cf32 block
        via the Farrow interpolator; output[i] is the input interpolated at i -
        group_delay + mu. The first group_delay samples are filling-transient.

        Pushes each input sample through the delay line and evaluates the
        interpolator at the same fixed offset, so the whole block is delayed by
        a constant, non-integer amount. Output sample i is the input
        interpolated at `i - group_delay + mu`, i.e. the stream shifted later
        by `group_delay - mu` samples; the first group_delay outputs are the
        delay-line filling transient and should be discarded. Because the
        offset is held constant this is the open-loop use of the interpolator —
        a timing loop instead steers mu per sample via
        farrow_push()/farrow_eval().

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input samples.
        mu : float
            Fractional delay in samples; the offset in `[0,1)` into the
            interpolation interval (values outside extrapolate).

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array, same length as x, each sample delayed by
            `group_delay - mu`.

        Examples
        --------
        >>> from doppler.resample import Farrow
        >>> import numpy as np
        >>> f = Farrow(order="cubic")
        >>> x = np.arange(8, dtype=np.complex64)   # a ramp: exact interp
        >>> y = f.delay(x, 0.5)                  # delay group_delay - 0.5
        >>> [round(float(v.real), 4) for v in y]  # first 2 are transient
        [0.0, -0.0625, 0.4375, 1.5, 2.5, 3.5, 4.5, 5.5]

        """

    def reset(self) -> None:
        """Clear the interpolator delay line.

        Zeroes the 4-tap delay line so the next block starts from a filling
        transient again, exactly as a freshly created interpolator would. The
        order (linear / parabolic / cubic) is preserved, so the same object can
        be reused across independent bursts without rebuilding the polynomial.
        Call it between unrelated signal segments to stop the tail of one
        leaking into the head of the next.

        Examples
        --------
        >>> from doppler.resample import Farrow
        >>> import numpy as np
        >>> f = Farrow(order="cubic")
        >>> _ = f.delay(np.ones(8, dtype=np.complex64), 0.25)  # leaves state
        >>> f.reset()                                 # back to pristine
        >>> x = np.arange(8, dtype=np.complex64)
        >>> f.delay(x, 0.5)[3:].real.tolist()   # steady part: ramp - 1.5
        [1.5, 2.5, 3.5, 4.5, 5.5]

        """

    def delay_max_out(self) -> int:
        """Extra capacity ``delay()`` needs beyond the input length: none.

        An ``out=`` buffer must hold ``max(delay_max_out(), len(x))``
        elements. ``delay()`` emits at most one sample per input sample,
        so it imposes no requirement of its own and this returns 0 —
        ``len(x)`` alone is the exact bound. A non-zero value here means
        the opposite: that a method can emit *more* than it is given
        (``Resampler.execute_ctrl_max_out`` interpolates, ``FFT`` pads to
        ``n``), which is the only case the accessor exists to cover.

        Returns
        -------
        int
            0 — size an ``out=`` buffer from ``len(x)``.

        Examples
        --------
        >>> from doppler.resample import Farrow
        >>> Farrow().delay_max_out()
        0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Farrow has already been destroyed.

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

        Raises ``RuntimeError`` if the Farrow has already been destroyed.

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
        ``RuntimeError`` if the Farrow has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def group_delay(self) -> int:
        """Group delay."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "Farrow":
        """Enter a context manager, returning this object.

        Lets a Farrow be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Farrow
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Farrow.

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
class HalfbandDecimatorQ15:
    """Allocate and initialise a fixed-point halfband 2:1 decimator. The FIR
    branch coefficients are supplied as float and converted internally to Q15
    with a x0.5 polyphase rate scaling. The full halfband prototype is sparse
    (every other tap is zero); supply only the non-zero FIR branch taps, not
    the full sparse prototype.

    Parameters
    ----------
    h : NDArray[np.float32]
        Float FIR branch coefficients of length num_taps. Must be symmetric
        (`h[k]` == `h[num_taps-1-k]`).

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.resample import HalfbandDecimatorQ15
    >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
    >>> dec = HalfbandDecimatorQ15(h)
    >>> dec.num_taps
    3
    >>> dec.rate
    0.5

    """
    def __init__(self, h: NDArray[np.float32]) -> None: ...

    def execute(
        self,
        x: NDArray[np.int16],
        out: NDArray[np.int16] | None = None,
    ) -> NDArray[np.int16]:
        """Decimate a block of interleaved IQ int16 samples by 2. Input must be
        interleaved int16_t IQ pairs (I₀ Q₀ I₁ Q₁ …); pass a 1-D array of
        2*n_complex elements. Each pair of complex input samples produces one
        complex output sample, so an array of length 2N yields at most N output
        pairs (2N int16 output values). If n_in is odd the trailing IQ pair is
        buffered and consumed on the next call.

        Parameters
        ----------
        x : NDArray[np.int16]
            Input.

        Returns
        -------
        NDArray[np.int16]
            min(available, max_out) COMPLEX samples -- twice that many int16_t
            values.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.resample import HalfbandDecimatorQ15
        >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
        >>> dec = HalfbandDecimatorQ15(h)
        >>> x = np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0], dtype=np.int16)
        >>> y = dec.execute(x)
        >>> y.dtype
        dtype('int16')
        >>> y.shape
        (4,)
        >>> y.tolist()
        [0, 0, 625, 0]

        """

    def execute_max_out(self) -> int:
        """Maximum output samples for a given input length.

        Returns 0 to trigger the lazy-alloc path in the Python glue: the

        output buffer is sized to n_in on first call (always sufficient for
        2:1).

        Returns
        -------
        int
            Output.
        """

    def reset(self) -> None:
        """Zero all delay rings and clear the pending-sample flag. After a
        reset the decimator behaves identically to a freshly constructed
        instance: the four dual-write delay rings are zeroed and has_pending is
        cleared, so no partial IQ pair carries over. Call this between
        unrelated signal segments to prevent inter-segment leakage.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.resample import HalfbandDecimatorQ15
        >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
        >>> dec = HalfbandDecimatorQ15(h)
        >>> x = np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0], dtype=np.int16)
        >>> _ = dec.execute(x)
        >>> dec.reset()
        >>> y = dec.execute(x)
        >>> y.tolist()
        [0, 0, 625, 0]

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the HalfbandDecimatorQ15 has already been
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

        Raises ``RuntimeError`` if the HalfbandDecimatorQ15 has already been
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
        ``RuntimeError`` if the HalfbandDecimatorQ15 has already been
        destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def num_taps(self) -> int:
        """FIR branch length as supplied to the constructor. This is the count
        of non-zero symmetric taps in the FIR branch, not the full sparse
        halfband prototype length. Useful for introspection when chaining
        multiple stages with programmatically computed filter banks.
        """

    @property
    def rate(self) -> float:
        """The sample-rate reduction factor; always 0.5 for 2:1 decimation.
        Exposed as a read-only property so pipelines can query the rate of each
        stage programmatically without hard-coding the 2:1 assumption.
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


    def __enter__(self) -> "HalfbandDecimatorQ15":
        """Enter a context manager, returning this object.

        Lets a HalfbandDecimatorQ15 be used in a `with` statement so its C
        resources are released deterministically on exit rather than at
        collection time.

        Returns
        -------
        HalfbandDecimatorQ15
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the HalfbandDecimatorQ15.

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

def ciccompmf(N: int, R: int, M: int) -> NDArray[np.float64]:
    """Design a CIC passband-droop compensator FIR filter. Implements the
    closed-form Bernoulli-series maximally-flat-error method from Molnar &
    Vucic (IEEE TCAS-II 58(12):926-930, 2011, DOI
    10.1109/TCSII.2011.2172522). The compensator runs at the *decimated*
    (output) rate and should be applied after the CIC stage. DC gain is
    exactly 1.0. Odd M gives symmetric linear-phase taps; even M gives
    half-sample-shifted linear-phase taps.

    Parameters
    ----------
    N : int
        CIC filter order (number of integrator/comb stages, >= 1).
    R : int
        CIC decimation factor (>= 2).
    M : int
        Number of compensator taps in `[1, 19]` (odd or even).

    Returns
    -------
    NDArray[np.float64]
        Output.

    Examples
    --------
    >>> from doppler.resample import ciccompmf
    >>> import numpy as np
    >>> h = ciccompmf(4, 16, 5)
    >>> h.shape, h.dtype
    ((5,), dtype('float64'))
    >>> [round(float(v), 4) for v in h]
    [0.029, -0.282, 1.5061, -0.282, 0.029]

    """

def kaiser_beta(atten: float) -> float:
    """Compute the Kaiser window beta parameter from stopband attenuation.
    Uses the standard Kaiser-Hamming formulae: atten > 50 dB: beta = 0.1102
    * (atten - 8.7) 21 <= atten <= 50 dB: beta = 0.5842*(atten-21)^0.4 +
    0.07886*(atten-21) atten < 21 dB: beta = 0.0 (rectangular window)

    Parameters
    ----------
    atten : float
        Desired stopband attenuation in dB (positive value).

    Returns
    -------
    float
        Kaiser beta parameter (>= 0.0).

    Examples
    --------
    >>> from doppler.resample import kaiser_beta
    >>> round(kaiser_beta(60.0), 4)
    5.6533
    >>> kaiser_beta(20.0)
    0.0

    """

def kaiser_num_taps(
    num_phases: int,
    atten: float,
    pb: float,
    sb: float,
) -> int:
    """Estimate the taps-per-phase count for a polyphase Kaiser FIR bank.
    Applies the Kaiser length formula to the per-phase normalised prototype
    (pb/num_phases, sb/num_phases), rounds up to the next odd symmetrical
    length, then divides by num_phases to give taps per branch. The result
    is the minimum num_taps argument to pass to Resampler_create_custom().

    Parameters
    ----------
    num_phases : int
        Number of polyphase branches (power of two).
    atten : float
        Desired stopband attenuation in dB.
    pb : float
        Normalised passband edge (0 < pb < sb < 1).
    sb : float
        Normalised stopband edge.

    Returns
    -------
    int
        Taps per polyphase branch (>= 1).

    Examples
    --------
    >>> from doppler.resample import kaiser_num_taps
    >>> kaiser_num_taps(4096, 60.0, 0.4, 0.6)
    19

    """
