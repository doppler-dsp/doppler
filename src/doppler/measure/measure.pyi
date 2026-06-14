# measure/measure.pyi — type stubs for the measure C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class ToneMeasure:
    """Create a ToneMeasure analyser.

    Parameters
    ----------
    n : int, default 8192
        n constructor parameter.
    fs : float, default 1.0
        fs constructor parameter.
    window : Literal["hann", "kaiser"], default "kaiser"
        window constructor parameter.
    beta : float, default 12.0
        beta constructor parameter.
    pad : int, default 2
        pad constructor parameter.
    n_harmonics : int, default 8
        n_harmonics constructor parameter.
    full_scale : float, default 1.0
        full_scale constructor parameter.
    dc_guard : int, default 0
        dc_guard constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.measure import ToneMeasure
    >>> obj = ToneMeasure(n=8192, fs=1.0, window="kaiser", beta=12.0, pad=2, n_harmonics=8, full_scale=1.0, dc_guard=0)

    """
    def __init__(self, n: int = ..., fs: float = ..., window: Literal["hann", "kaiser"] = "kaiser", beta: float = ..., pad: int = ..., n_harmonics: int = ..., full_scale: float = ..., dc_guard: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset (no-op: the analyser is stateless between calls).
        """

    def analyze(self, x: float) -> tuple[float, float, float, float, float, float, float, float, float, float, float, float, float, float, int, float, float, float, int, int, float, float, float]:
        """Analyze a real time-domain capture; returns a ToneMetrics result.

        Parameters
        ----------
        x : float
            Input.

        Returns
        -------
        tuple[float, float, float, float, float, float, float, float, float, float, float, float, float, float, int, float, float, float, int, int, float, float, float]
            the metric record (by value).
        """

    def analyze_complex(self, x: complex) -> tuple[float, float, float, float, float, float, float, float, float, float, float, float, float, float, int, float, float, float, int, int, float, float, float]:
        """Analyze a complex baseband capture (two-sided spectrum).

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        tuple[float, float, float, float, float, float, float, float, float, float, float, float, float, float, int, float, float, float, int, int, float, float, float]
            Output.
        """

    def time_stats(self, x: float) -> tuple[float, float, float, float, float, float]:
        """Time-domain stats: RMS, peak, crest/PAPR, DC offset, FS utilisation.

        Parameters
        ----------
        x : float
            Input.

        Returns
        -------
        tuple[float, float, float, float, float, float]
            Output.
        """

    def spectrum_dbfs(self, x: NDArray[np.float32]) -> NDArray[np.float32]:
        """DC-centred dBFS magnitude spectrum of a capture (length nfft, for plots).

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.

        Returns
        -------
        NDArray[np.float32]
            Number of samples written (nfft).
        """

    @property
    def n(self) -> int:
        """N."""

    @property
    def nfft(self) -> int:
        """Nfft."""

    @property
    def fs(self) -> float:
        """Fs."""

    @property
    def enbw(self) -> float:
        """Enbw."""

    @property
    def lobe_bins(self) -> int:
        """Lobe bins."""

    @property
    def rbw(self) -> float:
        """Rbw."""

    @property
    def bin_hz(self) -> float:
        """Bin hz."""

    @property
    def proc_gain_db(self) -> float:
        """Proc gain db."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "ToneMeasure": ...

    def __exit__(self, *args: object) -> None: ...

class NPRMeasure:
    """Create an NPRMeasure analyser.

    Parameters
    ----------
    n : int, default 8192
        n constructor parameter.
    fs : float, default 1.0
        fs constructor parameter.
    window : Literal["hann", "kaiser"], default "kaiser"
        window constructor parameter.
    beta : float, default 12.0
        beta constructor parameter.
    pad : int, default 2
        pad constructor parameter.
    full_scale : float, default 1.0
        full_scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.measure import NPRMeasure
    >>> obj = NPRMeasure(n=8192, fs=1.0, window="kaiser", beta=12.0, pad=2, full_scale=1.0)

    """
    def __init__(self, n: int = ..., fs: float = ..., window: Literal["hann", "kaiser"] = "kaiser", beta: float = ..., pad: int = ..., full_scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset (no-op: each analyze() call is independent).
        """

    def analyze(self, x: float, active_lo: float, active_hi: float, notch_lo: float, notch_hi: float, guard_hz: float = 0.0) -> tuple[float, float, float, int, int, float]:
        """NPR of a notched-noise capture over [active_lo,active_hi] with a notch [notch_lo,notch_hi] (Hz) and guard keep-out.

        Parameters
        ----------
        x : float
            Real time-domain capture.
        active_lo : float
            Active noise band lower edge (Hz).
        active_hi : float
            Active noise band upper edge (Hz).
        notch_lo : float
            Notch lower edge (Hz).
        notch_hi : float
            Notch upper edge (Hz).
        guard_hz : float
            Keep-out around the notch edges (Hz).

        Returns
        -------
        tuple[float, float, float, int, int, float]
            the NPR metric record (by value).
        """

    @property
    def n(self) -> int:
        """N."""

    @property
    def nfft(self) -> int:
        """Nfft."""

    @property
    def fs(self) -> float:
        """Fs."""

    @property
    def rbw(self) -> float:
        """Rbw."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "NPRMeasure": ...

    def __exit__(self, *args: object) -> None: ...

class IMDMeasure:
    """Create an IMDMeasure analyser.

    Parameters
    ----------
    n : int, default 8192
        n constructor parameter.
    fs : float, default 1.0
        fs constructor parameter.
    window : Literal["hann", "kaiser"], default "kaiser"
        window constructor parameter.
    beta : float, default 12.0
        beta constructor parameter.
    pad : int, default 2
        pad constructor parameter.
    full_scale : float, default 1.0
        full_scale constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.measure import IMDMeasure
    >>> obj = IMDMeasure(n=8192, fs=1.0, window="kaiser", beta=12.0, pad=2, full_scale=1.0)

    """
    def __init__(self, n: int = ..., fs: float = ..., window: Literal["hann", "kaiser"] = "kaiser", beta: float = ..., pad: int = ..., full_scale: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset (no-op: each analyze() call is independent).
        """

    def analyze(self, x: float) -> tuple[float, float, float, float, float, float, float, float, float, float, float, float]:
        """Two-tone IMD/TOI of a real capture (finds the two strongest tones).

        Parameters
        ----------
        x : float
            Input.

        Returns
        -------
        tuple[float, float, float, float, float, float, float, float, float, float, float, float]
            the IMD metric record (by value; zeroed if no two tones are found).
        """

    @property
    def n(self) -> int:
        """N."""

    @property
    def nfft(self) -> int:
        """Nfft."""

    @property
    def fs(self) -> float:
        """Fs."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "IMDMeasure": ...

    def __exit__(self, *args: object) -> None: ...

def measure_min_samples(fs: float, target_rbw: float, window: int, beta: float) -> int:
    """Samples needed to reach a target RBW (window 0=hann, 1=kaiser)."""

def measure_rec_nfft(n: int, pad: int) -> int:
    """Recommended zero-padded transform length: next_pow2(n * pad)."""

def measure_proc_gain(nfft: int) -> float:
    """FFT processing gain in dB: 10*log10(nfft / 2)."""

def dp_coherent_freq(fs: float, f_target: float, N: int) -> float:
    """Nearest leakage-free coherent test frequency (J cycles, J coprime N)."""
