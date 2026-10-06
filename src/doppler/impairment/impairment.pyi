# impairment/impairment.pyi — type stubs for the impairment C extension.
from typing import final
import numpy as np
import numpy.typing as npt
from numpy.typing import NDArray

@final
class DopplerChannel:
    """DopplerChannel component.

    Parameters
    ----------
    fs : float, default 1000000.0
        fs constructor parameter.
    carrier_hz : float, default 0.0
        carrier_hz constructor parameter.
    doppler_ppm : float, default 0.0
        doppler_ppm constructor parameter.
    doppler_rate_ppm_s : float, default 0.0
        doppler_rate_ppm_s constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.impairment import DopplerChannel
    >>> obj = DopplerChannel(
    ...     fs=1000000.0,
    ...     carrier_hz=0.0,
    ...     doppler_ppm=0.0,
    ...     doppler_rate_ppm_s=0.0,
    ... )

    """

    def __init__(
        self,
        fs: float = 1000000.0,
        carrier_hz: float = 0.0,
        doppler_ppm: float = 0.0,
        doppler_rate_ppm_s: float = 0.0,
    ) -> None: ...
    def execute(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Apply clock Doppler to a block of complex baseband.

        Resamples x by `1/(1+d(t))` and multiplies the result by the coherent
        carrier `exp(j*2*pi*fc*excess(t))`. State persists across calls, so
        feeding a stream in blocks gives the same samples as one large call
        (subject to `DOPPLER_CHANNEL_MAX_BLOCK`).

        Output length is approximately `x_len/(1+d)` and varies by a sample
        from call to call as the fractional resampling accumulator crosses —
        that variation is the dilation itself, not a defect.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input block.
        out : npt.NDArray[np.complex64] | None
            Output buffer.

        Returns
        -------
        NDArray[np.complex64]
            Samples written to out.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.impairment import DopplerChannel
        >>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9, doppler_ppm=20.0)
        >>> y = ch.execute(np.ones(1000, dtype=np.complex64))
        >>> y.shape                   # 20 ppm is 0.02 samples over this block
        (1000,)
        >>> round(ch.offset_hz, 1)    # fc * d = 2.5e9 * 20e-6, in Hz
        50000.0

        """

    def execute_max_out(self) -> int:
        """Upper bound on the output of one execute() call.

        Assumes an input of at most `DOPPLER_CHANNEL_MAX_BLOCK` samples — see
        that

        macro for why the bound cannot depend on the actual input length.

        Returns
        -------
        int
            Output.
        """

    def execute_profile(
        self,
        x: npt.NDArray[np.complex64],
        ppm: npt.NDArray[np.float64],
    ) -> NDArray[np.complex64]:
        """Apply a per-sample Doppler PROFILE to a block of complex baseband.

        The array form of dp_doppler_channel_execute(): instead of the
        create-time `(doppler_ppm, doppler_rate_ppm_s)` closed form -- a
        straight line, which a real pass is not -- the Doppler is supplied as
        one value per INPUT sample. The length contract is the one
        `dp_resamp_execute_ctrl()` underneath already has (`ctrl` parallel to
        `in`), so a profile is handed to the resampler rather than reduced to
        fit it.

        **The profile is ABSOLUTE.** `ppm[i]` is the total instantaneous
        Doppler at input sample `i`; the create-time scalars do not add to it.
        They cancel exactly rather than by convention: the resampler's rate is
        `base + ctrl`, and this fills `ctrl = ratio(ppm[i]) - base` with the
        same `base` it was built with. Creating with zeros and supplying a
        profile is the ordinary use.

        **The carrier is read off the resampler, not integrated beside it.**
        The excess delay at output `k` is `(p_k - k + 1)/fs`, where `p_k` is
        the input position the resampler's own accumulator reports for that
        output (dp_resamp_execute_ctrl_pos()). So the carrier
        `exp(j*2*pi*fc*excess)` is derived from the dilation the resampler
        actually performed:

        - It cannot disagree with the dilation, and there is no second
          accumulator to drift from the first.
        - It cannot depend on how the stream was chunked: nothing here maps a
          profile index to an output index, which is what made the first
          attempt at this (reverted) chunk-dependent. Output `k` has the
          carrier of the position it was interpolated at, whatever call it fell
          in.
        - It has no cancellation: the integer part of `p_k - k` is exact, and
          only a fraction below one input sample is floating point.

        Mixing the two calls on one stream is permitted and coherent -- both
        advance the same clocks -- but a stream a profile has driven reports
        dp_doppler_channel_get_offset_hz() from the profile, since the closed
        form no longer describes it. The profile is in the serialized state
        (layout version 2) only as that last value; the carrier needs none.

        A sign change mid-record is the point: no `(doppler_ppm,
        doppler_rate_ppm_s)` pair produces it.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input CF32 samples, x_len of them.
        ppm : npt.NDArray[np.float64]
            Doppler in ppm, parallel to x.

        Returns
        -------
        NDArray[np.complex64]
            Samples written. 0 if any pointer is NULL, if ppm_len differs from
            x_len, or if any profile sample is non-finite or below -5e5 ppm (a
            scale under 1/2: past the 2x expansion the output is sized for, and
            at -1e6 ppm time stops, which create() already refuses for the
            scalar). All checked over the whole profile BEFORE any output is
            produced, so a bad call writes nothing rather than a valid prefix.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.impairment import DopplerChannel
        >>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9)
        >>> n = 1000
        >>> ppm = np.where(np.arange(n) < n // 2, 20.0, -20.0)
        >>> y = ch.execute_profile(np.ones(n, dtype=np.complex64), ppm)
        >>> y.shape          # closing then opening: the record STRETCHES overall
        (1001,)
        >>> round(ch.offset_hz, 1)   # fc * d at the last profile sample
        -50000.0

        """

    def reset(self) -> None:
        """Reset DopplerChannel to its post-create state.

        Zeroes both sample clocks (so `elapsed_s` and the carrier phase restart
        at zero) and clears the resampler's delay line and fractional
        accumulator. The configured
        `fs`/`carrier_hz`/`doppler_ppm`/`doppler_rate_ppm_s` are kept.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.impairment import DopplerChannel
        >>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9, doppler_ppm=20.0)
        >>> _ = ch.execute(np.ones(1000, dtype=np.complex64))
        >>> round(ch.elapsed_s, 6)    # receive time consumed: 1000 / 1e6
        0.001
        >>> ch.reset()                # both sample clocks back to zero
        >>> ch.elapsed_s
        0.0

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the DopplerChannel has already been
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

        Raises ``RuntimeError`` if the DopplerChannel has already been
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
        ``RuntimeError`` if the DopplerChannel has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def fs(self) -> float:
        """Fs."""

    @property
    def carrier_hz(self) -> float:
        """Carrier hz."""

    @property
    def doppler_ppm(self) -> float:
        """Doppler ppm."""

    @property
    def doppler_rate_ppm_s(self) -> float:
        """Doppler rate ppm s."""

    @property
    def elapsed_s(self) -> float:
        """Receive time in seconds consumed so far, the `t` every Doppler
        quantity is evaluated at. Advances by `n/fs` per `execute(x)` call and
        is zeroed by `reset()`.
        """

    @property
    def delay_samples(self) -> float:
        """The resampler's group delay in samples (10.5 for the built-in bank),
        constant and in addition to the dilation: output `k` at receive time `t
        = k/fs` carries the input at `t + excess(t) - delay_samples/fs`. A
        receiver started at the input's phase is this far from the peak.
        """

    @property
    def offset_hz(self) -> float:
        """Instantaneous carrier offset `fc * d(t)` in Hz at the current
        `elapsed_s` -- the frequency a receiver would have to tune out right
        now. Read-only diagnostic; with a non-zero `doppler_rate_ppm_s` it
        ramps as the stream advances.
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

    def __enter__(self) -> "DopplerChannel":
        """Enter a context manager, returning this object.

        Lets a DopplerChannel be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        DopplerChannel
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the DopplerChannel.

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
