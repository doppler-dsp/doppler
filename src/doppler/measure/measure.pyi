# measure/measure.pyi — type stubs for the measure C extension.
import numpy as np
from numpy.typing import NDArray

class ToneMeasure:
    """Create a ToneMeasure analyser (auto Kaiser window).

    Parameters
    ----------
    n : int, default 8192
        Capture/frame length (>= 2).
    fs : float, default 1.0
        Sample rate (Hz, > 0).
    n_harmonics : int, default 8
        Harmonics to track (k = 2..n_harmonics).
    full_scale : float, default 1.0
        Amplitude that equals 0 dBFS (> 0).  Ignored if bits > 0.
    bits : int, default 0
        ADC depth: bits>0 sets the 0-dBFS reference to 2^(bits-1) and, unless overridden, the dynamic-range target (6.02*bits + 1.76 + headroom).
    dynamic_range_db : float, default 0.0
        Explicit sidelobe/dynamic-range target (dB); used when > 0, else derived from bits (or a deep default when both are 0).
    dc_guard : int, default 0
        Extra bins excluded beyond L around DC.

    Examples
    --------
    Create with defaults:

    >>> from doppler.measure import ToneMeasure
    >>> obj = ToneMeasure(n=8192, fs=1.0, n_harmonics=8, full_scale=1.0, bits=0, dynamic_range_db=0.0, dc_guard=0)

    """
    def __init__(self, n: int = ..., fs: float = ..., n_harmonics: int = ..., full_scale: float = ..., bits: int = ..., dynamic_range_db: float = ..., dc_guard: int = ...) -> None: ...

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

        Examples
        --------
        >>> from doppler.measure import ToneMeasure
        >>> import numpy as np
        >>> n, t = 4096, np.arange(4096)
        >>> # full-scale tone at 300 cycles + a 2nd harmonic 40 dB down
        >>> x = (np.cos(2*np.pi*300*t/n)
        ...      + 0.01*np.cos(2*np.pi*600*t/n)).astype(np.float32)
        >>> r = ToneMeasure(n=n, fs=1.0).analyze(x)
        >>> type(r).__name__
        'ToneMetrics'
        >>> abs(r.fund_dbfs) < 0.1, round(r.thd, 1)   # 0 dBFS tone, THD -40 dBc
        (True, -40.0)

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

        Examples
        --------
        >>> from doppler.measure import ToneMeasure
        >>> import numpy as np
        >>> i = np.arange(4096)
        >>> x = np.exp(2j*np.pi*137*i/4096).astype(np.complex64)
        >>> r = ToneMeasure(n=4096, fs=1.0).analyze_complex(x)
        >>> round(r.fund_freq, 4), abs(r.fund_dbfs) < 0.2
        (0.0334, True)

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

        Examples
        --------
        >>> from doppler.measure import ToneMeasure
        >>> import numpy as np
        >>> t = np.arange(4096)
        >>> x = (0.8*np.cos(2*np.pi*50*t/4096)).astype(np.float32)
        >>> ts = ToneMeasure(n=4096, fs=1.0).time_stats(x)
        >>> round(ts.crest_db, 2), round(ts.fs_util_pct, 0)   # sine crest ~3.01 dB
        (3.01, 80.0)

        """

    def spectrum_dbfs(self, x: NDArray[np.float32], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
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

    def spectrum_dbfs_max_out(self) -> int:
        """Max output length spectrum_dbfs() can produce for the current state."""

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
    def spur_guard_bins(self) -> int:
        """Spur guard bins."""

    @property
    def beta(self) -> float:
        """Beta."""

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
    """Create an NPRMeasure analyser (auto Kaiser window).

    Parameters
    ----------
    n : int, default 8192
        Capture/frame length (>= 2).
    fs : float, default 1.0
        Sample rate (Hz, > 0).
    full_scale : float, default 1.0
        Amplitude that equals 0 dBFS (> 0).  Ignored if bits > 0.
    bits : int, default 0
        ADC depth: bits>0 sets the 0-dBFS reference to 2^(bits-1) and, unless overridden, the dynamic-range target.
    dynamic_range_db : float, default 0.0
        Explicit sidelobe/dynamic-range target (dB); used when > 0, else derived from bits.

    Examples
    --------
    Create with defaults:

    >>> from doppler.measure import NPRMeasure
    >>> obj = NPRMeasure(n=8192, fs=1.0, full_scale=1.0, bits=0, dynamic_range_db=0.0)

    """
    def __init__(self, n: int = ..., fs: float = ..., full_scale: float = ..., bits: int = ..., dynamic_range_db: float = ...) -> None: ...

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

        Examples
        --------
        >>> from doppler.measure import NPRMeasure
        >>> import numpy as np
        >>> rng = np.random.default_rng(0)
        >>> n = 1 << 15
        >>> F = np.fft.rfft(rng.standard_normal(n))
        >>> f = np.fft.rfftfreq(n)
        >>> F[(f < 0.05) | (f > 0.45)] = 0                 # band-limit to [0.05,0.45]
        >>> F[(f >= 0.20) & (f <= 0.25)] *= 10**(-50/20)   # notch 50 dB deep
        >>> x = np.fft.irfft(F, n)
        >>> x = (0.3*x/np.std(x)).astype(np.float32)
        >>> r = NPRMeasure(n=n, fs=1.0).analyze(x, 0.05, 0.45, 0.20, 0.25, 0.01)
        >>> 45 < r.npr_db < 55, r.notch_psd_dbfs < r.inband_psd_dbfs
        (True, True)

        """

    def spectrum_dbfs(self, x: NDArray[np.float32], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """DC-centred dBFS magnitude spectrum of a capture (length nfft, for plots).

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.

        Returns
        -------
        NDArray[np.float32]
            Output.
        """

    def spectrum_dbfs_max_out(self) -> int:
        """Max output length spectrum_dbfs() can produce for the current state."""

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
    """Create an IMDMeasure analyser (auto Kaiser window).

    Parameters
    ----------
    n : int, default 8192
        Capture/frame length (>= 2).
    fs : float, default 1.0
        Sample rate (Hz, > 0).
    full_scale : float, default 1.0
        Amplitude that equals 0 dBFS (> 0).  Ignored if bits > 0.
    bits : int, default 0
        ADC depth: bits>0 sets the 0-dBFS reference to 2^(bits-1) and, unless overridden, the dynamic-range target.
    dynamic_range_db : float, default 0.0
        Explicit sidelobe/dynamic-range target (dB); used when > 0, else derived from bits.

    Examples
    --------
    Create with defaults:

    >>> from doppler.measure import IMDMeasure
    >>> obj = IMDMeasure(n=8192, fs=1.0, full_scale=1.0, bits=0, dynamic_range_db=0.0)

    """
    def __init__(self, n: int = ..., fs: float = ..., full_scale: float = ..., bits: int = ..., dynamic_range_db: float = ...) -> None: ...

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

        Examples
        --------
        >>> from doppler.measure import IMDMeasure
        >>> import numpy as np
        >>> t = np.arange(4096)
        >>> # two equal tones at 200 & 250 cycles + 3rd-order products 40 dB down
        >>> x = (np.cos(2*np.pi*200*t/4096) + np.cos(2*np.pi*250*t/4096)
        ...      + 0.01*np.cos(2*np.pi*150*t/4096)
        ...      + 0.01*np.cos(2*np.pi*300*t/4096)).astype(np.float32)
        >>> r = IMDMeasure(n=4096, fs=1.0).analyze(x)
        >>> round(r.f1, 4), round(r.f2, 4), round(r.imd3_dbc, 0)
        (0.0488, 0.061, -40.0)

        """

    def spectrum_dbfs(self, x: NDArray[np.float32], out: NDArray[np.float32] | None = None) -> NDArray[np.float32]:
        """DC-centred dBFS magnitude spectrum of a capture (length nfft, for plots).

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.

        Returns
        -------
        NDArray[np.float32]
            Output.
        """

    def spectrum_dbfs_max_out(self) -> int:
        """Max output length spectrum_dbfs() can produce for the current state."""

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

def measure_min_samples(fs: float, target_rbw: float, bits: int, dynamic_range_db: float, complex_input: int) -> int:
    """Samples for a target RBW (auto Kaiser from bits/dynamic_range_db; target_rbw<=0 -> span/1000).

    Plans a capture for the same auto-Kaiser window the measurement objects
    use: the dynamic-range target (from dynamic_range_db, else bits) selects
    the Kaiser beta, whose ENBW (measured via kaiser_enbw) sets the
    bins-per-RBW. RBW = ENBW * fs / n, so n = ceil(ENBW * fs / target_rbw).

    Parameters
    ----------
    fs : float
        Sample rate (Hz, > 0).
    target_rbw : float
        Desired resolution bandwidth (Hz).  When <= 0 it defaults to span/1000, where span = fs/2 for real captures and fs for complex (complex_input).
    bits : int
        ADC depth: sets the dynamic-range target when no explicit override is given.
    dynamic_range_db : float
        Explicit dynamic-range target (dB); used when > 0.
    complex_input : int
        Non-zero if the capture is complex (span = fs).

    Returns
    -------
    int
        Required capture length, or 0 on bad args.
    """

def measure_rec_nfft(n: int, pad: int) -> int:
    """Recommended zero-padded transform length: next_pow2(n * pad).

    Parameters
    ----------
    n : int
        Input.
    pad : int
        Input.

    Returns
    -------
    int
        Output.
    """

def measure_proc_gain(nfft: int) -> float:
    """FFT processing gain in dB: 10*log10(nfft / 2).

    Parameters
    ----------
    nfft : int
        Input.

    Returns
    -------
    float
        Output.
    """

def dp_coherent_freq(fs: float, f_target: float, N: int) -> float:
    """Nearest leakage-free coherent test frequency (J cycles, J coprime N).

    Snaps `f_target` to `J * fs / N` where J is the nearest integer cycle
    count that is coprime with N — an integer number of cycles in the
    capture (no leakage) with J coprime to N (so quantisation-noise
    correlation is minimised).

    Parameters
    ----------
    fs : float
        Input.
    f_target : float
        Input.
    N : int
        Input.

    Returns
    -------
    float
        The coherent frequency (Hz), or 0 on bad args.
    """
