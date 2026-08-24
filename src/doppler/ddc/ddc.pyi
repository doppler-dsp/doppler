# ddc/ddc.pyi — type stubs for the ddc C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class DDC:
    """Create a complex-input Digital Down-Converter. Allocates internal state
    for the LO and RateConverter cascade. The RateConverter selects the
    cheapest multi-stage decimation chain (CIC + optional halfband + polyphase
    resampler) for the given rate.

    Parameters
    ----------
    norm_freq : float, default 0.0
        LO frequency in cycles/sample at the input rate. Set to -f_carrier to
        shift a carrier at f_carrier to DC. Any real value is accepted.
    rate : float, default 0.25
        Output rate / input rate. Must be > 0. Values >= 1 are up-sampling;
        typical use is decimation (0 < rate < 1).

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``DDC: invalid
        parameter (need rate > 0, 0 <= beta <= 1, span >= 1, pulse_sps > 0,
        num_phases a power of two >= 2)``.

    Examples
    --------
    >>> from doppler.ddc import DDC
    >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
    >>> ddc.norm_freq
    -0.1
    >>> ddc.rate
    0.25

    """
    def __init__(self, norm_freq: float = ..., rate: float = ...) -> None: ...

    def execute(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Mix input block with LO, then rate-convert.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input block; accepted as float32 (auto-cast).
        out : NDArray[np.complex64] | None
            CF32 output buffer (C-only, hidden from Python).

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written (C-only).

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
        >>> t = np.arange(4096)
        >>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
        >>> y = ddc.execute(x)
        >>> y.shape
        (1024,)
        >>> y.dtype
        dtype('complex64')
        >>> round(float(abs(y[500])), 2)   # shifted to DC; amplitude ≈ 1
        1.0

        """

    def execute_max_out(self, x_len: int) -> int:
        """Maximum output samples one execute() of x_len inputs can produce.

        A DDC decimates (or passes at unity), so the output never exceeds the

        input length: returns x_len. The binding sizes the output buffer to
        this

        per-call bound and resizes down to the actual count (gh-607).

        Parameters
        ----------
        x_len : int
            Number of input samples the matching execute() call sees.

        Returns
        -------
        int
            x_len (a safe upper bound on the produced samples).
        """

    def execute_ctrl(
        self,
        x: NDArray[np.complex64],
        rate_ctrl: float,
        freq_ctrl: float,
    ) -> NDArray[np.complex64]:
        """Mix and resample a block, steering both control ports.

        The control-port form of ddc_execute(): the LO advances by `phase_inc +
        freq_ctrl` on every sample of this block, and the cascade's terminal
        stage runs at `stage_rate + rate_ctrl`. Neither deviation is persisted
        — the centre norm_freq and rate are untouched — so a tracking loop
        passes its full filter output on every call and the DDC holds no loop
        state of its own.

        Feeding a stream through ddc_execute_ctrl_push() one sample at a time
        reproduces this call bit-for-bit when both controls are held constant,
        so the cheap block form stays correct for open-loop use (a fixed
        Doppler offset, a rate trim) and the push form is what a closed loop
        uses.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input block.
        rate_ctrl : float
            Rate deviation added to the terminal Resampler stage's rate.
            Referenced to the terminal (post-decimation) rate, not the overall
            rate; ignored by a plan whose last stage is an integer HB/CIC with
            nothing to steer.
        freq_ctrl : float
            Frequency deviation added to the LO, in cycles/sample at the INPUT
            rate (any sign).

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written.

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=0.0, rate=0.25)   # LO centred at DC
        >>> t = np.arange(4096)
        >>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
        >>> y = ddc.execute_ctrl(x, 0.0, -0.1)    # freq_ctrl steers +0.1 to DC
        >>> y.shape
        (1024,)
        >>> round(float(abs(y[100:].mean())), 2)  # settled output sits at DC
        1.0

        """

    def execute_ctrl_push(
        self,
        x: complex,
        rate_ctrl: float,
        freq_ctrl: float,
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Push ONE input sample; emit whatever outputs it completes.

        The per-input streaming form of ddc_execute_ctrl(), and the only form a
        closed loop can use: a block call has to know its whole control history
        up front, whereas a carrier or timing loop computes each correction
        *from* the outputs already emitted. Both loops close once per symbol,
        so both ports need this form.

        The mix costs one LO step per input; the cascade then emits 0 outputs
        (the common decimating case, between strobes), 1, or several.

        Parameters
        ----------
        x : complex
            One CF32 input sample.
        rate_ctrl : float
            Rate deviation for this input (terminal-stage rate).
        freq_ctrl : float
            Frequency deviation for this input, cycles/sample at the input
            rate.
        out : NDArray[np.complex64] | None
            Output buffer for any emitted samples.

        Returns
        -------
        NDArray[np.complex64]
            Number of outputs written (0, 1, or more).

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
        >>> t = np.arange(64)
        >>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
        >>> outs = [ddc.execute_ctrl_push(complex(s), 0.0, 0.0) for s in x]
        >>> int(sum(len(o) for o in outs))   # 64 inputs, rate 1/4 -> 16 outs
        16
        >>> [len(o) for o in outs[:4]]        # 0 outs until a strobe completes
        [0, 0, 0, 1]

        """

    def execute_ctrl_push_max_out(self) -> int:
        """Largest number of samples execute_ctrl_push() can return in the
        current state.

        Size an `out=` buffer with this before calling execute_ctrl_push(), or
        use it to allocate one up front. The bound is this object's own: what
        it depends on is a property of the algorithm, so a header block on
        execute_ctrl_push_max_out() replaces this text.

        Returns
        -------
        int
            Upper bound on the output length; the actual call may return fewer.
        """

    def reset(self) -> None:
        """Zero LO phase and filter history.

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=0.0, rate=0.25)
        >>> x = np.ones(64, dtype=np.complex64)
        >>> y1 = ddc.execute(x)
        >>> ddc.reset()
        >>> y2 = ddc.execute(x)
        >>> bool(np.array_equal(y1, y2))
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the DDC has already been destroyed.

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

        Raises ``RuntimeError`` if the DDC has already been destroyed.

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
        ``RuntimeError`` if the DDC has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def norm_freq(self) -> float:
        """Return the current LO normalised frequency (cycles/sample)."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def rate(self) -> float:
        """Return the configured output/input rate ratio (read-only). The rate
        is fixed at create time; change it by destroying and recreating the DDC
        with the new value.
        """

    @property
    def clipped(self) -> bool:
        """Has the cascade's CIC clipped its input since the last reset?"""

    @property
    def narrow_pulse(self) -> bool:
        """Is this object's rectangular matched filter degenerately narrow?"""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "DDC":
        """Enter a context manager, returning this object.

        Lets a DDC be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        DDC
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the DDC.

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
class MatchedDDC:
    """Create a complex-input Digital Down-Converter. Allocates internal state
    for the LO and RateConverter cascade. The RateConverter selects the
    cheapest multi-stage decimation chain (CIC + optional halfband + polyphase
    resampler) for the given rate.

    Parameters
    ----------
    norm_freq : float, default 0.0
        LO frequency in cycles/sample at the input rate. Set to -f_carrier to
        shift a carrier at f_carrier to DC. Any real value is accepted.
    rate : float, default 0.25
        Output rate / input rate. Must be > 0. Values >= 1 are up-sampling;
        typical use is decimation (0 < rate < 1).
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

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``DDC: invalid
        parameter (need rate > 0, 0 <= beta <= 1, span >= 1, pulse_sps > 0,
        num_phases a power of two >= 2)``.

    Examples
    --------
    >>> from doppler.ddc import DDC
    >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
    >>> ddc.norm_freq
    -0.1
    >>> ddc.rate
    0.25

    """
    def __init__(
        self,
        norm_freq: float = ...,
        rate: float = ...,
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
        """Mix input block with LO, then rate-convert.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input block; accepted as float32 (auto-cast).
        out : NDArray[np.complex64] | None
            CF32 output buffer (C-only, hidden from Python).

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written (C-only).

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
        >>> t = np.arange(4096)
        >>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
        >>> y = ddc.execute(x)
        >>> y.shape
        (1024,)
        >>> y.dtype
        dtype('complex64')
        >>> round(float(abs(y[500])), 2)   # shifted to DC; amplitude ≈ 1
        1.0

        """

    def execute_max_out(self, x_len: int) -> int:
        """Maximum output samples one execute() of x_len inputs can produce.

        A DDC decimates (or passes at unity), so the output never exceeds the

        input length: returns x_len. The binding sizes the output buffer to
        this

        per-call bound and resizes down to the actual count (gh-607).

        Parameters
        ----------
        x_len : int
            Number of input samples the matching execute() call sees.

        Returns
        -------
        int
            x_len (a safe upper bound on the produced samples).
        """

    def execute_ctrl(
        self,
        x: NDArray[np.complex64],
        rate_ctrl: float,
        freq_ctrl: float,
    ) -> NDArray[np.complex64]:
        """Mix and resample a block, steering both control ports.

        The control-port form of ddc_execute(): the LO advances by `phase_inc +
        freq_ctrl` on every sample of this block, and the cascade's terminal
        stage runs at `stage_rate + rate_ctrl`. Neither deviation is persisted
        — the centre norm_freq and rate are untouched — so a tracking loop
        passes its full filter output on every call and the DDC holds no loop
        state of its own.

        Feeding a stream through ddc_execute_ctrl_push() one sample at a time
        reproduces this call bit-for-bit when both controls are held constant,
        so the cheap block form stays correct for open-loop use (a fixed
        Doppler offset, a rate trim) and the push form is what a closed loop
        uses.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input block.
        rate_ctrl : float
            Rate deviation added to the terminal Resampler stage's rate.
            Referenced to the terminal (post-decimation) rate, not the overall
            rate; ignored by a plan whose last stage is an integer HB/CIC with
            nothing to steer.
        freq_ctrl : float
            Frequency deviation added to the LO, in cycles/sample at the INPUT
            rate (any sign).

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written.

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=0.0, rate=0.25)   # LO centred at DC
        >>> t = np.arange(4096)
        >>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
        >>> y = ddc.execute_ctrl(x, 0.0, -0.1)    # freq_ctrl steers +0.1 to DC
        >>> y.shape
        (1024,)
        >>> round(float(abs(y[100:].mean())), 2)  # settled output sits at DC
        1.0

        """

    def execute_ctrl_push(
        self,
        x: complex,
        rate_ctrl: float,
        freq_ctrl: float,
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Push ONE input sample; emit whatever outputs it completes.

        The per-input streaming form of ddc_execute_ctrl(), and the only form a
        closed loop can use: a block call has to know its whole control history
        up front, whereas a carrier or timing loop computes each correction
        *from* the outputs already emitted. Both loops close once per symbol,
        so both ports need this form.

        The mix costs one LO step per input; the cascade then emits 0 outputs
        (the common decimating case, between strobes), 1, or several.

        Parameters
        ----------
        x : complex
            One CF32 input sample.
        rate_ctrl : float
            Rate deviation for this input (terminal-stage rate).
        freq_ctrl : float
            Frequency deviation for this input, cycles/sample at the input
            rate.
        out : NDArray[np.complex64] | None
            Output buffer for any emitted samples.

        Returns
        -------
        NDArray[np.complex64]
            Number of outputs written (0, 1, or more).

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
        >>> t = np.arange(64)
        >>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
        >>> outs = [ddc.execute_ctrl_push(complex(s), 0.0, 0.0) for s in x]
        >>> int(sum(len(o) for o in outs))   # 64 inputs, rate 1/4 -> 16 outs
        16
        >>> [len(o) for o in outs[:4]]        # 0 outs until a strobe completes
        [0, 0, 0, 1]

        """

    def execute_ctrl_push_max_out(self) -> int:
        """Largest number of samples execute_ctrl_push() can return in the
        current state.

        Size an `out=` buffer with this before calling execute_ctrl_push(), or
        use it to allocate one up front. The bound is this object's own: what
        it depends on is a property of the algorithm, so a header block on
        execute_ctrl_push_max_out() replaces this text.

        Returns
        -------
        int
            Upper bound on the output length; the actual call may return fewer.
        """

    def reset(self) -> None:
        """Zero LO phase and filter history.

        Examples
        --------
        >>> from doppler.ddc import DDC
        >>> import numpy as np
        >>> ddc = DDC(norm_freq=0.0, rate=0.25)
        >>> x = np.ones(64, dtype=np.complex64)
        >>> y1 = ddc.execute(x)
        >>> ddc.reset()
        >>> y2 = ddc.execute(x)
        >>> bool(np.array_equal(y1, y2))
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the MatchedDDC has already been destroyed.

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

        Raises ``RuntimeError`` if the MatchedDDC has already been destroyed.

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
        ``RuntimeError`` if the MatchedDDC has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def norm_freq(self) -> float:
        """Return the current LO normalised frequency (cycles/sample)."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def rate(self) -> float:
        """Return the configured output/input rate ratio (read-only). The rate
        is fixed at create time; change it by destroying and recreating the DDC
        with the new value.
        """

    @property
    def clipped(self) -> bool:
        """Has the cascade's CIC clipped its input since the last reset?"""

    @property
    def narrow_pulse(self) -> bool:
        """Is this object's rectangular matched filter degenerately narrow?"""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "MatchedDDC":
        """Enter a context manager, returning this object.

        Lets a MatchedDDC be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        MatchedDDC
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the MatchedDDC.

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
class Ddcr:
    """Create a real-input Digital Down-Converter (Architecture D2). The signal
    chain is: halfband R2C (2:1, bakes in +fs/4 shift) -> fine LO mix at the
    intermediate rate (fs_in/2) -> RateConverter -> CF32 output. The halfband
    stage uses +-1/0 coefficients (no multiplications) and puts the fine LO and
    the cascade at fs_in/2. That is worth ~1.1-1.7x in a whole receiver (it
    halves the rate ahead of the polyphase matched filter, so the gain grows
    with samples/symbol) and close to nothing for the front end alone -- see
    the file header for the measurements. Use it because the input IS real.

    Parameters
    ----------
    norm_freq : float, default 0.0
        Fine NCO frequency at the intermediate rate (fs_in/2, cycles/sample).
        To tune a real tone at normalised input frequency f_c to DC, set
        norm_freq = -(2*f_c + 0.5).
    rate : float, default 0.25
        Total output/input rate. Must be in (0, 0.5) because the halfband
        pre-decimates by 2.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``Ddcr: invalid
        parameter (need 0 < rate < 0.5, 0 <= beta <= 1, span >= 1, pulse_sps >
        0, num_phases a power of two >= 2)``.

    Examples
    --------
    >>> from doppler.ddc import Ddcr
    >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
    >>> ddcr.norm_freq
    -0.7
    >>> ddcr.rate
    0.25

    """
    def __init__(self, norm_freq: float = ..., rate: float = ...) -> None: ...

    def execute(
        self,
        x: NDArray[np.float32],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Down-convert a block of real float32 samples to CF32 baseband.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.
        out : NDArray[np.complex64] | None
            CF32 output buffer (C-only, hidden from Python).

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written (C-only).

        Examples
        --------
        >>> from doppler.ddc import Ddcr
        >>> import numpy as np
        >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
        >>> t = np.arange(4096)
        >>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)
        >>> out = np.empty(len(x), dtype=np.complex64)
        >>> y = ddcr.execute(x, out)
        >>> y.shape
        (1024,)
        >>> y.dtype
        dtype('complex64')
        >>> round(float(abs(y[500])), 2)   # analytic signal of a unit cosine
        1.0

        """

    def execute_max_out(self) -> int:
        """Upper bound on one execute call's output, or 0 to let the caller
        size it from the input block (a decimator never exceeds its input).

        Returns
        -------
        int
            Output.
        """

    def execute_ctrl(
        self,
        x: NDArray[np.float32],
        rate_ctrl: float,
        freq_ctrl: float,
    ) -> NDArray[np.complex64]:
        """Process a real block, steering both control ports.

        The control-port form of ddcr_execute(); see ddc_execute_ctrl() for the
        semantics, which are identical except for where the LO lives.

        Parameters
        ----------
        x : NDArray[np.float32]
            Real float32 input block.
        rate_ctrl : float
            Rate deviation added to the terminal Resampler stage's rate
            (referenced to the terminal, post-decimation rate).
        freq_ctrl : float
            Frequency deviation added to the fine LO, in cycles/sample at the
            INTERMEDIATE rate (fs_in/2) — the halfband has already decimated by
            two by the time the mix happens, so a discriminator working in
            cycles per ADC sample must be doubled before it lands here.

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written.

        Examples
        --------
        >>> from doppler.ddc import Ddcr
        >>> import numpy as np
        >>> ddcr = Ddcr(norm_freq=-0.5, rate=0.25)  # LO 0.2 short of tune
        >>> t = np.arange(4096)
        >>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)
        >>> y = ddcr.execute_ctrl(x, 0.0, -0.2)     # ctrl completes the tune
        >>> y.shape
        (1024,)
        >>> round(float(abs(y[100:].mean())), 2)    # real tone -> DC, amp 1.0
        1.0

        """

    def execute_ctrl_push(
        self,
        x: float,
        rate_ctrl: float,
        freq_ctrl: float,
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Push ONE real input sample; emit whatever outputs it completes.

        The per-input streaming form of ddcr_execute_ctrl(), for a closed loop.
        The halfband consumes two inputs per intermediate sample, so every
        other push does no mixing and emits nothing at all — the LO advances
        (and its control is applied) once per *intermediate* sample, which is
        the rate the LO runs at.

        Parameters
        ----------
        x : float
            One real float32 input sample.
        rate_ctrl : float
            Rate deviation for this input (terminal-stage rate).
        freq_ctrl : float
            Frequency deviation, cycles/sample at fs_in/2.
        out : NDArray[np.complex64] | None
            Output buffer for any emitted samples.

        Returns
        -------
        NDArray[np.complex64]
            Number of outputs written (0, 1, or more).

        Examples
        --------
        >>> from doppler.ddc import Ddcr
        >>> import numpy as np
        >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
        >>> x = np.cos(2 * np.pi * 0.1 * np.arange(128)).astype(np.float32)
        >>> outs = [ddcr.execute_ctrl_push(float(s), 0.0, 0.0) for s in x]
        >>> int(sum(len(o) for o in outs))  # 128 real inputs, rate 1/4 -> 32
        32
        >>> [len(o) for o in outs[:4]]      # halfband: 0 until a strobe
        [0, 0, 0, 1]

        """

    def execute_ctrl_push_max_out(self) -> int:
        """Bound for ONE pushed input: `ceil(rate) + 1` output periods.
        Non-zero because the push form has no input block to size from.

        Returns
        -------
        int
            Output.
        """

    def reset(self) -> None:
        """Zero halfband history, LO phase and filter history.

        Examples
        --------
        >>> from doppler.ddc import Ddcr
        >>> import numpy as np
        >>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
        >>> x = np.ones(64, dtype=np.float32)
        >>> out = np.empty(64, dtype=np.complex64)
        >>> y1 = ddcr.execute(x, out).copy()
        >>> ddcr.reset()
        >>> y2 = ddcr.execute(x, out)
        >>> bool(np.array_equal(y1, y2))
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Ddcr has already been destroyed.

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

        Raises ``RuntimeError`` if the Ddcr has already been destroyed.

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
        ``RuntimeError`` if the Ddcr has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def norm_freq(self) -> float:
        """Return the current fine NCO normalised frequency at the intermediate
        rate (fs_in/2, cycles/sample).
        """
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def rate(self) -> float:
        """Return the total configured rate (fs_out / fs_in, read-only). This
        is the end-to-end ratio from ADC input to CF32 output. Change it by
        destroying and recreating the DDCR.
        """

    @property
    def clipped(self) -> bool:
        """Has the cascade's CIC clipped its input since the last reset?"""

    @property
    def narrow_pulse(self) -> bool:
        """Is this object's rectangular matched filter degenerately narrow?"""

    def close(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
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


    def __enter__(self) -> "Ddcr":
        """Enter a context manager, returning this object.

        Lets a Ddcr be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Ddcr
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Ddcr.

        Equivalent to calling `close()`. Returns ``None``, so an exception
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
class MatchedDdcr:
    """Create a real-input Digital Down-Converter (Architecture D2). The signal
    chain is: halfband R2C (2:1, bakes in +fs/4 shift) -> fine LO mix at the
    intermediate rate (fs_in/2) -> RateConverter -> CF32 output. The halfband
    stage uses +-1/0 coefficients (no multiplications) and puts the fine LO and
    the cascade at fs_in/2. That is worth ~1.1-1.7x in a whole receiver (it
    halves the rate ahead of the polyphase matched filter, so the gain grows
    with samples/symbol) and close to nothing for the front end alone -- see
    the file header for the measurements. Use it because the input IS real.

    Parameters
    ----------
    norm_freq : float, default 0.0
        Fine NCO frequency at the intermediate rate (fs_in/2, cycles/sample).
        To tune a real tone at normalised input frequency f_c to DC, set
        norm_freq = -(2*f_c + 0.5).
    rate : float, default 0.25
        Total output/input rate. Must be in (0, 0.5) because the halfband
        pre-decimates by 2.
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

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``Ddcr: invalid
        parameter (need 0 < rate < 0.5, 0 <= beta <= 1, span >= 1, pulse_sps >
        0, num_phases a power of two >= 2)``.

    Examples
    --------
    >>> from doppler.ddc import Ddcr
    >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
    >>> ddcr.norm_freq
    -0.7
    >>> ddcr.rate
    0.25

    """
    def __init__(
        self,
        norm_freq: float = ...,
        rate: float = ...,
        pulse: Literal["iandd", "rrc"] = "rrc",
        beta: float = ...,
        span: int = ...,
        pulse_sps: float = ...,
        num_phases: int = ...,
    ) -> None: ...

    def execute(
        self,
        x: NDArray[np.float32],
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Down-convert a block of real float32 samples to CF32 baseband.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.
        out : NDArray[np.complex64] | None
            CF32 output buffer (C-only, hidden from Python).

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written (C-only).

        Examples
        --------
        >>> from doppler.ddc import Ddcr
        >>> import numpy as np
        >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
        >>> t = np.arange(4096)
        >>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)
        >>> out = np.empty(len(x), dtype=np.complex64)
        >>> y = ddcr.execute(x, out)
        >>> y.shape
        (1024,)
        >>> y.dtype
        dtype('complex64')
        >>> round(float(abs(y[500])), 2)   # analytic signal of a unit cosine
        1.0

        """

    def execute_max_out(self) -> int:
        """Upper bound on one execute call's output, or 0 to let the caller
        size it from the input block (a decimator never exceeds its input).

        Returns
        -------
        int
            Output.
        """

    def execute_ctrl(
        self,
        x: NDArray[np.float32],
        rate_ctrl: float,
        freq_ctrl: float,
    ) -> NDArray[np.complex64]:
        """Process a real block, steering both control ports.

        The control-port form of ddcr_execute(); see ddc_execute_ctrl() for the
        semantics, which are identical except for where the LO lives.

        Parameters
        ----------
        x : NDArray[np.float32]
            Real float32 input block.
        rate_ctrl : float
            Rate deviation added to the terminal Resampler stage's rate
            (referenced to the terminal, post-decimation rate).
        freq_ctrl : float
            Frequency deviation added to the fine LO, in cycles/sample at the
            INTERMEDIATE rate (fs_in/2) — the halfband has already decimated by
            two by the time the mix happens, so a discriminator working in
            cycles per ADC sample must be doubled before it lands here.

        Returns
        -------
        NDArray[np.complex64]
            Number of output samples written.

        Examples
        --------
        >>> from doppler.ddc import Ddcr
        >>> import numpy as np
        >>> ddcr = Ddcr(norm_freq=-0.5, rate=0.25)  # LO 0.2 short of tune
        >>> t = np.arange(4096)
        >>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)
        >>> y = ddcr.execute_ctrl(x, 0.0, -0.2)     # ctrl completes the tune
        >>> y.shape
        (1024,)
        >>> round(float(abs(y[100:].mean())), 2)    # real tone -> DC, amp 1.0
        1.0

        """

    def execute_ctrl_push(
        self,
        x: float,
        rate_ctrl: float,
        freq_ctrl: float,
        out: NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Push ONE real input sample; emit whatever outputs it completes.

        The per-input streaming form of ddcr_execute_ctrl(), for a closed loop.
        The halfband consumes two inputs per intermediate sample, so every
        other push does no mixing and emits nothing at all — the LO advances
        (and its control is applied) once per *intermediate* sample, which is
        the rate the LO runs at.

        Parameters
        ----------
        x : float
            One real float32 input sample.
        rate_ctrl : float
            Rate deviation for this input (terminal-stage rate).
        freq_ctrl : float
            Frequency deviation, cycles/sample at fs_in/2.
        out : NDArray[np.complex64] | None
            Output buffer for any emitted samples.

        Returns
        -------
        NDArray[np.complex64]
            Number of outputs written (0, 1, or more).

        Examples
        --------
        >>> from doppler.ddc import Ddcr
        >>> import numpy as np
        >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
        >>> x = np.cos(2 * np.pi * 0.1 * np.arange(128)).astype(np.float32)
        >>> outs = [ddcr.execute_ctrl_push(float(s), 0.0, 0.0) for s in x]
        >>> int(sum(len(o) for o in outs))  # 128 real inputs, rate 1/4 -> 32
        32
        >>> [len(o) for o in outs[:4]]      # halfband: 0 until a strobe
        [0, 0, 0, 1]

        """

    def execute_ctrl_push_max_out(self) -> int:
        """Bound for ONE pushed input: `ceil(rate) + 1` output periods.
        Non-zero because the push form has no input block to size from.

        Returns
        -------
        int
            Output.
        """

    def reset(self) -> None:
        """Zero halfband history, LO phase and filter history.

        Examples
        --------
        >>> from doppler.ddc import Ddcr
        >>> import numpy as np
        >>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
        >>> x = np.ones(64, dtype=np.float32)
        >>> out = np.empty(64, dtype=np.complex64)
        >>> y1 = ddcr.execute(x, out).copy()
        >>> ddcr.reset()
        >>> y2 = ddcr.execute(x, out)
        >>> bool(np.array_equal(y1, y2))
        True

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the MatchedDdcr has already been destroyed.

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

        Raises ``RuntimeError`` if the MatchedDdcr has already been destroyed.

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
        ``RuntimeError`` if the MatchedDdcr has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def norm_freq(self) -> float:
        """Return the current fine NCO normalised frequency at the intermediate
        rate (fs_in/2, cycles/sample).
        """
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def rate(self) -> float:
        """Return the total configured rate (fs_out / fs_in, read-only). This
        is the end-to-end ratio from ADC input to CF32 output. Change it by
        destroying and recreating the DDCR.
        """

    @property
    def clipped(self) -> bool:
        """Has the cascade's CIC clipped its input since the last reset?"""

    @property
    def narrow_pulse(self) -> bool:
        """Is this object's rectangular matched filter degenerately narrow?"""

    def close(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
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


    def __enter__(self) -> "MatchedDdcr":
        """Enter a context manager, returning this object.

        Lets a MatchedDdcr be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        MatchedDdcr
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the MatchedDdcr.

        Equivalent to calling `close()`. Returns ``None``, so an exception
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
