# analyzer/analyzer.pyi — type stubs for the analyzer C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class Specan:
    """Create a natural-parameter spectrum analyzer.

    Parameters
    ----------
    fs : float
        fs constructor parameter (required).
    span : float
        span constructor parameter (required).
    rbw : float
        rbw constructor parameter (required).
    src_center : float, default 0.0
        src_center constructor parameter.
    center : float, default 0.0
        center constructor parameter.
    offset_db : float, default 0.0
        offset_db constructor parameter.
    full_scale : float, default 1.0
        full_scale constructor parameter.
    bits : int, default 0
        bits constructor parameter.
    window : Literal["hann", "kaiser"], default "kaiser"
        window constructor parameter.
    navg : int, default 1
        navg constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.analyzer import Specan
    >>> obj = Specan(fs=2.048e6, span=200e3, rbw=500.0, src_center=0.0, center=0.0, offset_db=0.0, full_scale=1.0, bits=0, window="kaiser", navg=1)

    """
    def __init__(self, fs: float, span: float, rbw: float, src_center: float = ..., center: float = ..., offset_db: float = ..., full_scale: float = ..., bits: int = ..., window: Literal["hann", "kaiser"] = "kaiser", navg: int = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.float32]:
        """Mix, decimate, average; return one DC-centred dB display frame, or None.

        Feeds @p x through the Ddc, buffers the decimated output, and once
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
        >>> sa.execute(np.zeros(64, dtype=np.complex64)) is None  # too few samples
        True
        >>> frame = sa.execute(np.zeros(65536, dtype=np.complex64))
        >>> frame.shape, frame.dtype
        ((801,), dtype('float32'))

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
        """Drop pending samples and the running average; zero LO/filter history.
        """

    @property
    def fs_out(self) -> float:
        """Fs out."""

    @property
    def span(self) -> float:
        """Span."""

    @property
    def rbw(self) -> float:
        """Rbw."""

    @property
    def center(self) -> float:
        """Center."""

    @property
    def beta(self) -> float:
        """Beta."""

    @property
    def n(self) -> int:
        """N."""

    @property
    def nfft(self) -> int:
        """Nfft."""

    @property
    def navg(self) -> int:
        """Navg."""

    @property
    def display_size(self) -> int:
        """Display size."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Specan": ...

    def __exit__(self, *args: object) -> None: ...
