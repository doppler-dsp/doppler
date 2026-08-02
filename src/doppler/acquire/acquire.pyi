# acquire/acquire.pyi — type stubs for the acquire C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class CarrierAcquisition:
    """CarrierAcquisition component.

    Parameters
    ----------
    sample_rate_hz : float
        sample_rate_hz constructor parameter.
    symbol_rate_hz : float
        symbol_rate_hz constructor parameter.
    resolution_hz : float, default 0.0
        resolution_hz constructor parameter.
    zero_pad : int, default 4
        zero_pad constructor parameter.
    window : Literal["hann", "kaiser", "blackman-harris"], default "hann"
        window constructor parameter.
    beta : float, default 0.0
        beta constructor parameter.
    psd_template : NDArray[np.float32], default ...
        psd_template constructor parameter.
    pfa : float, default 1e-3
        pfa constructor parameter.
    pd : float, default 0.9
        pd constructor parameter.
    design_snr : float, default 2.0
        design_snr constructor parameter.
    sequential : bool, default True
        sequential constructor parameter.
    max_n_blocks : int, default 100000
        max_n_blocks constructor parameter.

    """
    def __init__(self, sample_rate_hz: float = ..., symbol_rate_hz: float = ..., resolution_hz: float = ..., zero_pad: int = ..., window: Literal["hann", "kaiser", "blackman-harris"] = "hann", beta: float = ..., psd_template: NDArray[np.float32] = ..., pfa: float = ..., pd: float = ..., design_snr: float = ..., sequential: bool = ..., max_n_blocks: int = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64]) -> None:
        """Fold raw complex samples into the running PSD average and test for a detection; any chunk size across repeated calls (a partial trailing block carries to the next call).

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
        ...     np.complex64)                         # residual carrier at 123 Hz
        >>> ca = CarrierAcquisition(sample_rate_hz=8000.0, symbol_rate_hz=1000.0,
        ...                         psd_template=np.array([], dtype=np.float32))
        >>> ca.steps(x)                   # fold the stream, testing each block
        >>> ca.ready
        True
        >>> round(ca.residual_hz, 0)      # recovered residual carrier, Hz
        123.0

        """

    def reset(self) -> None:
        """Discard the running PSD average and detection state; counters return to zero.

        Use it to reuse one detector across successive captures: after a
        detection (or a give-up) the running average and counters are cleared,
        so the next steps() starts folding a fresh stream from zero.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.acquire import CarrierAcquisition
        >>> ca = CarrierAcquisition(sample_rate_hz=8000.0, symbol_rate_hz=1000.0,
        ...                         psd_template=np.array([], dtype=np.float32))
        >>> ca.steps(np.zeros(2048, dtype=np.complex64))  # accumulate some looks
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
        was constructed with. Length is validated against `state_bytes()` before
        the blob is handed to the C core, and the core may reject it as well.

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
        """True once a detection has fired (or the dwell_target give-up cap was reached) -- residual_hz is only meaningful once this is true."""

    @property
    def residual_hz(self) -> float:
        """Sub-bin-refined residual carrier frequency estimate, Hz. Valid only when ready is true."""

    @property
    def n_blocks(self) -> int:
        """Number of n_fft-length blocks actually folded into the PSD average so far."""

    @property
    def dwell_target(self) -> int:
        """Non-sequential mode's precomputed fixed wait count, from det_n_noncoh(design_snr, ...) at construction. Ignored by sequential mode's own give-up bound -- see max_n_blocks."""

    @property
    def max_n_blocks(self) -> int:
        """Sequential mode's own give-up cap (independent of dwell_target) -- the max_n_blocks constructor argument, echoed back."""

    @property
    def nfft(self) -> int:
        """PSD transform length (next_pow2(n_fft*zero_pad)) -- the length any caller-supplied template array must match."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on exit.

        Idempotent: calling it again on an already-released object does nothing.
        Every other method raises ``RuntimeError`` once it has run.
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

    def __exit__(self, exc_type: object | None = ..., exc: object | None = ..., tb: object | None = ...) -> None:
        """Exit a context manager, releasing the CarrierAcquisition.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never suppresses
        one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """
