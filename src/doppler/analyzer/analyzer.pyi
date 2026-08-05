# analyzer/analyzer.pyi — type stubs for the analyzer C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class Specan:
    """Create a natural-parameter spectrum analyzer.

    Parameters
    ----------
    fs : float
        Input sample rate (Hz). Must be > 0.
    span : float
        Display span (Hz). Must be > 0.
    rbw : float
        Resolution bandwidth (Hz). Must be > 0.
    src_center : float, default 0.0
        Source center frequency (Hz); the input band is centred here, so the
        analyzer mixes (center − src_center) to DC.
    center : float, default 0.0
        Desired display center frequency (Hz).
    offset_db : float, default 0.0
        Additive dB offset on the display spectrum, applied on top of dBFS
        (e.g. a dBm calibration the application computes from a reference
        level).
    full_scale : float, default 1.0
        Amplitude that reads 0 dBFS (> 0). Ignored if bits > 0.
    bits : int, default 0
        ADC depth: bits>0 sets the 0-dBFS reference to 2^(bits-1) in the shared
        PSD core (the single source of truth for the dBFS reference).
    window : Literal["hann", "kaiser"], default "kaiser"
        Window index: 0 = Hann, 1 = Kaiser (RBW-trimmable).
    navg : int, default 1
        Segments averaged per emitted frame (>= 1).

    Examples
    --------
    >>> from doppler.analyzer import Specan
    >>> sa = Specan(fs=2.048e6, span=200e3, rbw=500.0)
    >>> sa.fs_out
    256000.0
    >>> sa.nfft == 2 * sa.n
    True

    """
    def __init__(
        self,
        fs: float,
        span: float,
        rbw: float,
        src_center: float = ...,
        center: float = ...,
        offset_db: float = ...,
        full_scale: float = ...,
        bits: int = ...,
        window: Literal["hann", "kaiser"] = "kaiser",
        navg: int = ...,
    ) -> None: ...

    def execute(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.float32] | None = None,
    ) -> NDArray[np.float32]:
        """Mix, decimate, average; return one DC-centred dB display frame, or
        None.

        Feeds x through the Ddc, buffers the decimated output, and once
        `n·navg` decimated samples are available windows + FFTs + averages them
        into a fresh frame, crops the central ±span/2 band and writes it in dB
        (+ ref_db). Returns 0 (writing nothing) until a frame is ready — the
        binding maps that to Python ``None``.

        Parameters
        ----------
        x : NDArray[np.complex64]
            cf32 input block (C-only; the binding passes it).

        Returns
        -------
        NDArray[np.float32]
            Display bins written (disp_n), or 0 if no frame is ready yet.

        Examples
        --------
        >>> from doppler.analyzer import Specan
        >>> import numpy as np
        >>> sa = Specan(fs=2.048e6, span=200e3, rbw=500.0, navg=1)
        >>> sa.execute(np.zeros(64, dtype=np.complex64)) is None  # too few
        True
        >>> frame = sa.execute(np.zeros(65536, dtype=np.complex64))
        >>> frame.shape, frame.dtype
        ((801,), dtype('float32'))

        """

    def execute_max_out(self) -> int:
        """Output capacity hint for specan_execute(); equals disp_n.

        Returns
        -------
        int
            Output.
        """

    def retune(self, center: float) -> None:
        """Move the display center frequency (seamless LO retune; no rebuild).

        Updates the Ddc LO phase increment (seamless across blocks — no
        resampler or window reset) and drops pending samples so the next frame
        reflects only the new tuning. Changing the span or RBW requires a
        destroy + create (the decimation rate and window length change).

        Parameters
        ----------
        center : float
            New display center frequency (Hz).
        """

    def reset(self) -> None:
        """Drop pending samples and the running average; zero LO/filter
        history.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Specan has already been destroyed.

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

        Raises ``RuntimeError`` if the Specan has already been destroyed.

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
        ``RuntimeError`` if the Specan has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def fs_out(self) -> float:
        """Decimated rate, Hz (= span·1.28, ≤ fs_in)."""

    @property
    def span(self) -> float:
        """Display span, Hz."""

    @property
    def rbw(self) -> float:
        """Requested resolution bandwidth, Hz."""

    @property
    def center(self) -> float:
        """Display center frequency, Hz."""

    @property
    def beta(self) -> float:
        """Kaiser beta realising rbw."""

    @property
    def n(self) -> int:
        """Segment / window length (samples)."""

    @property
    def nfft(self) -> int:
        """Zero-padded transform length."""

    @property
    def navg(self) -> int:
        """Segments averaged per emitted frame."""

    @property
    def display_size(self) -> int:
        """Display size."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "Specan":
        """Enter a context manager, returning this object.

        Lets a Specan be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Specan
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Specan.

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
