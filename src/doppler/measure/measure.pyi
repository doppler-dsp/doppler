# measure/measure.pyi — type stubs for the measure C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class ToneMetrics(tuple[float, float, float, float, float, float, float, float, float, float, float, float, float, float, int, float, float, float, int, int, float, float, float]):
    """Single-tone ADC/converter dynamic metrics (SNR, SINAD, THD, SFDR, ENOB)
    plus the analysis-grid accuracy fields.

    Attributes
    ----------
    snr : float
        SNR = 10log10(P_fund / P_noise) (dB).
    sinad : float
        SINAD = 10log10(fund/(noise+harm)) (dB).
    thd : float
        THD = 10log10(P_harm / P_fund) (dBc).
    thd_pct : float
        THD = 100 sqrt(P_harm / P_fund) (%).
    thd_n : float
        THD+N = 10log10((noise+harm)/fund) = -SINAD.
    sfdr_dbc : float
        SFDR: fundamental - worst spur (dBc).
    sfdr_dbfs : float
        SFDR: full scale - worst spur (dBFS).
    enob : float
        ENOB = (SINAD - 1.76)/6.02.
    enob_fs : float
        Full-scale-corrected ENOB.
    noise_floor_dbfs : float
        Mean per-bin noise power (dBFS).
    fund_freq : float
        Fundamental frequency (Hz).
    fund_dbfs : float
        Fundamental level (dBFS).
    worst_spur_freq : float
        Worst spur frequency (Hz).
    worst_spur_dbc : float
        Worst spur level vs the fundamental (dBc).
    worst_spur_is_harm : int
        1 if the worst spur is a harmonic, else 0.
    rbw_hz : float
        Resolution bandwidth = enbw*fs/n (Hz).
    enbw_hz : float
        Equivalent noise bandwidth (Hz) (= rbw_hz).
    bin_hz : float
        FFT bin spacing = fs/nfft (Hz).
    lobe_bins : int
        Window main-lobe half-width L (bins).
    n_noise_bins : int
        Number of bins counted as noise.
    proc_gain_db : float
        FFT processing gain = 10log10(nfft/2) (dB).
    amp_uncert_db : float
        Amplitude-read uncertainty bound (dB).
    floor_uncert_db : float
        Noise-floor standard error (dB).
    """

    @property
    def snr(self) -> float:
        """SNR = 10log10(P_fund / P_noise) (dB)."""

    @property
    def sinad(self) -> float:
        """SINAD = 10log10(fund/(noise+harm)) (dB)."""

    @property
    def thd(self) -> float:
        """THD = 10log10(P_harm / P_fund) (dBc)."""

    @property
    def thd_pct(self) -> float:
        """THD = 100 sqrt(P_harm / P_fund) (%)."""

    @property
    def thd_n(self) -> float:
        """THD+N = 10log10((noise+harm)/fund) = -SINAD."""

    @property
    def sfdr_dbc(self) -> float:
        """SFDR: fundamental - worst spur (dBc)."""

    @property
    def sfdr_dbfs(self) -> float:
        """SFDR: full scale - worst spur (dBFS)."""

    @property
    def enob(self) -> float:
        """ENOB = (SINAD - 1.76)/6.02."""

    @property
    def enob_fs(self) -> float:
        """Full-scale-corrected ENOB."""

    @property
    def noise_floor_dbfs(self) -> float:
        """Mean per-bin noise power (dBFS)."""

    @property
    def fund_freq(self) -> float:
        """Fundamental frequency (Hz)."""

    @property
    def fund_dbfs(self) -> float:
        """Fundamental level (dBFS)."""

    @property
    def worst_spur_freq(self) -> float:
        """Worst spur frequency (Hz)."""

    @property
    def worst_spur_dbc(self) -> float:
        """Worst spur level vs the fundamental (dBc)."""

    @property
    def worst_spur_is_harm(self) -> int:
        """1 if the worst spur is a harmonic, else 0."""

    @property
    def rbw_hz(self) -> float:
        """Resolution bandwidth = enbw*fs/n (Hz)."""

    @property
    def enbw_hz(self) -> float:
        """Equivalent noise bandwidth (Hz) (= rbw_hz)."""

    @property
    def bin_hz(self) -> float:
        """FFT bin spacing = fs/nfft (Hz)."""

    @property
    def lobe_bins(self) -> int:
        """Window main-lobe half-width L (bins)."""

    @property
    def n_noise_bins(self) -> int:
        """Number of bins counted as noise."""

    @property
    def proc_gain_db(self) -> float:
        """FFT processing gain = 10log10(nfft/2) (dB)."""

    @property
    def amp_uncert_db(self) -> float:
        """Amplitude-read uncertainty bound (dB)."""

    @property
    def floor_uncert_db(self) -> float:
        """Noise-floor standard error (dB)."""

@final
class TimeStats(tuple[float, float, float, float, float, float]):
    """AC-coupled time-domain capture statistics (crest factor / PAPR).

    Attributes
    ----------
    rms : float
        Root-mean-square amplitude (DC included).
    peak : float
        Peak deviation, max|x - DC|.
    crest_db : float
        Crest factor, 20log10(peak_ac / rms_ac) (dB).
    papr_db : float
        Peak-to-average power ratio (= crest) (dB).
    dc_offset : float
        DC offset, mean(x).
    fs_util_pct : float
        Full-scale use, 100*max|x|/full_scale (%).
    """

    @property
    def rms(self) -> float:
        """Root-mean-square amplitude (DC included)."""

    @property
    def peak(self) -> float:
        """Peak deviation, max|x - DC|."""

    @property
    def crest_db(self) -> float:
        """Crest factor, 20log10(peak_ac / rms_ac) (dB)."""

    @property
    def papr_db(self) -> float:
        """Peak-to-average power ratio (= crest) (dB)."""

    @property
    def dc_offset(self) -> float:
        """DC offset, mean(x)."""

    @property
    def fs_util_pct(self) -> float:
        """Full-scale use, 100*max|x|/full_scale (%)."""

@final
class NPRMetrics(tuple[float, float, float, int, int, float]):
    """Noise-power-ratio metrics from a notched-noise-loading test.

    Attributes
    ----------
    npr_db : float
        NPR = 10log10(in-band PSD / notch PSD) (dB).
    inband_psd_dbfs : float
        Mean in-band noise power per bin (dBFS).
    notch_psd_dbfs : float
        Mean power folded into the notch (dBFS).
    n_inband_bins : int
        Bins averaged in the active band.
    n_notch_bins : int
        Bins averaged inside the notch.
    rbw_hz : float
        Resolution bandwidth = enbw*fs/n (Hz).
    """

    @property
    def npr_db(self) -> float:
        """NPR = 10log10(in-band PSD / notch PSD) (dB)."""

    @property
    def inband_psd_dbfs(self) -> float:
        """Mean in-band noise power per bin (dBFS)."""

    @property
    def notch_psd_dbfs(self) -> float:
        """Mean power folded into the notch (dBFS)."""

    @property
    def n_inband_bins(self) -> int:
        """Bins averaged in the active band."""

    @property
    def n_notch_bins(self) -> int:
        """Bins averaged inside the notch."""

    @property
    def rbw_hz(self) -> float:
        """Resolution bandwidth = enbw*fs/n (Hz)."""

@final
class IMDMetrics(tuple[float, float, float, float, float, float, float, float, float, float, float, float]):
    """Two-tone intermodulation metrics (IMD2, IMD3, second/third-order
    intercepts).

    Attributes
    ----------
    f1 : float
        Lower tone frequency (Hz).
    f2 : float
        Upper tone frequency (Hz).
    p1_dbfs : float
        Lower tone level (dBFS).
    p2_dbfs : float
        Upper tone level (dBFS).
    imd2_dbc : float
        2nd-order product (f2-f1) vs mean tone (dBc).
    imd3_dbc : float
        Worst 3rd-order product vs mean tone (dBc).
    imd2_freq : float
        2nd-order product frequency (Hz).
    imd3_lo_freq : float
        3rd-order (2f1-f2) product frequency (Hz).
    imd3_hi_freq : float
        3rd-order (2f2-f1) product frequency (Hz).
    toi_dbfs : float
        Third-order intercept (dBFS).
    soi_dbfs : float
        Second-order intercept (dBFS).
    rbw_hz : float
        Resolution bandwidth = enbw*fs/n (Hz).
    """

    @property
    def f1(self) -> float:
        """Lower tone frequency (Hz)."""

    @property
    def f2(self) -> float:
        """Upper tone frequency (Hz)."""

    @property
    def p1_dbfs(self) -> float:
        """Lower tone level (dBFS)."""

    @property
    def p2_dbfs(self) -> float:
        """Upper tone level (dBFS)."""

    @property
    def imd2_dbc(self) -> float:
        """2nd-order product (f2-f1) vs mean tone (dBc)."""

    @property
    def imd3_dbc(self) -> float:
        """Worst 3rd-order product vs mean tone (dBc)."""

    @property
    def imd2_freq(self) -> float:
        """2nd-order product frequency (Hz)."""

    @property
    def imd3_lo_freq(self) -> float:
        """3rd-order (2f1-f2) product frequency (Hz)."""

    @property
    def imd3_hi_freq(self) -> float:
        """3rd-order (2f2-f1) product frequency (Hz)."""

    @property
    def toi_dbfs(self) -> float:
        """Third-order intercept (dBFS)."""

    @property
    def soi_dbfs(self) -> float:
        """Second-order intercept (dBFS)."""

    @property
    def rbw_hz(self) -> float:
        """Resolution bandwidth = enbw*fs/n (Hz)."""

@final
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
        Amplitude that equals 0 dBFS (> 0). Ignored if bits > 0.
    bits : int, default 0
        ADC depth: bits>0 sets the 0-dBFS reference to 2^(bits-1) and, unless
        overridden, the dynamic-range target (6.02*bits + 1.76 + headroom).
    dynamic_range_db : float, default 0.0
        Explicit sidelobe/dynamic-range target (dB); used when > 0, else
        derived from bits (or a deep default when both are 0).
    dc_guard : int, default 0
        Extra bins excluded beyond L around DC.

    Examples
    --------
    Create with defaults:

    >>> from doppler.measure import ToneMeasure
    >>> obj = ToneMeasure(
    ...     n=8192,
    ...     fs=1.0,
    ...     n_harmonics=8,
    ...     full_scale=1.0,
    ...     bits=0,
    ...     dynamic_range_db=0.0,
    ...     dc_guard=0,
    ... )

    """
    def __init__(
        self,
        n: int = ...,
        fs: float = ...,
        n_harmonics: int = ...,
        full_scale: float = ...,
        bits: int = ...,
        dynamic_range_db: float = ...,
        dc_guard: int = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Reset the analyser (a no-op: it holds no state between calls).

        Every analyze() / analyze_complex() / time_stats() / spectrum_dbfs()
        call re-windows and re-transforms its own capture from scratch, so
        there is nothing carried between calls to clear. The method exists only
        so ToneMeasure honours the same reset() contract as every other doppler
        object, letting a generic pipeline reset each stage uniformly.

        Examples
        --------
        >>> from doppler.measure import ToneMeasure
        >>> m = ToneMeasure(n=4096, fs=1.0)
        >>> m.reset()            # stateless: provided only for API uniformity
        >>> m.reset() is None    # returns nothing; safe to call anytime
        True

        """

    def analyze(self, x: float) -> ToneMetrics:
        """Analyze a real time-domain capture; returns a ToneMetrics result.

        Parameters
        ----------
        x : float
            Input.

        Returns
        -------
        ToneMetrics
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
        >>> abs(r.fund_dbfs) < 0.1, round(r.thd, 1)  # 0 dBFS tone, THD -40
        (True, -40.0)

        """

    def analyze_complex(self, x: complex) -> ToneMetrics:
        """Analyze a complex baseband capture (two-sided spectrum).

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        ToneMetrics
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

    def time_stats(self, x: float) -> TimeStats:
        """Time-domain stats: RMS, peak, crest/PAPR, DC offset, FS utilisation.

        Parameters
        ----------
        x : float
            Input.

        Returns
        -------
        TimeStats
            Output.

        Examples
        --------
        >>> from doppler.measure import ToneMeasure
        >>> import numpy as np
        >>> t = np.arange(4096)
        >>> x = (0.8*np.cos(2*np.pi*50*t/4096)).astype(np.float32)
        >>> ts = ToneMeasure(n=4096, fs=1.0).time_stats(x)
        >>> round(ts.crest_db, 2), round(ts.fs_util_pct, 0)  # crest ~3.01 dB
        (3.01, 80.0)

        """

    def spectrum_dbfs(
        self,
        x: NDArray[np.float32],
        out: NDArray[np.float32] | None = None,
    ) -> NDArray[np.float32]:
        """DC-centred dBFS magnitude spectrum of a capture (length nfft, for
        plots).

        The windowed, zero-padded magnitude spectrum behind the metrics, laid
        out DC-centred (fftshifted) and normalised to dBFS so it drops straight
        under an analyzer trace. Use it to eyeball where the fundamental,
        harmonics and spurs that analyze() quantifies actually sit.

        Parameters
        ----------
        x : NDArray[np.float32]
            Real time-domain capture (length x_len).

        Returns
        -------
        NDArray[np.float32]
            DC-centred dBFS magnitude spectrum, one value per FFT bin (nfft).

        Examples
        --------
        >>> from doppler.measure import ToneMeasure
        >>> import numpy as np
        >>> t = np.arange(4096)
        >>> x = np.cos(2*np.pi*300*t/4096).astype(np.float32)  # full-scale
        >>> s = ToneMeasure(n=4096, fs=1.0).spectrum_dbfs(x)  # DC-centred dBFS
        >>> s.shape                     # zero-padded to next power of two
        (8192,)
        >>> round(float(s.max()), 1)   # two real images, ~6 dB each
        -6.0

        """

    def spectrum_dbfs_max_out(self) -> int:
        """Capacity (== nfft) of the spectrum_dbfs output buffer.

        Returns
        -------
        int
            Output.
        """

    @property
    def n(self) -> int:
        """Window / frame length (samples)."""

    @property
    def nfft(self) -> int:
        """Zero-padded transform length."""

    @property
    def fs(self) -> float:
        """Sample rate, Hz."""

    @property
    def enbw(self) -> float:
        """Equivalent noise bandwidth, bins."""

    @property
    def lobe_bins(self) -> int:
        """Window main-lobe half-width L (bins)."""

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
        """FFT bin spacing = fs/nfft (Hz)."""

    @property
    def proc_gain_db(self) -> float:
        """FFT processing gain = 10log10(nfft/2) (dB)."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "ToneMeasure":
        """Enter a context manager, returning this object.

        Lets a ToneMeasure be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        ToneMeasure
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the ToneMeasure.

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
class NPRMeasure:
    """Create an NPRMeasure analyser (auto Kaiser window).

    Parameters
    ----------
    n : int, default 8192
        Capture/frame length (>= 2).
    fs : float, default 1.0
        Sample rate (Hz, > 0).
    full_scale : float, default 1.0
        Amplitude that equals 0 dBFS (> 0). Ignored if bits > 0.
    bits : int, default 0
        ADC depth: bits>0 sets the 0-dBFS reference to 2^(bits-1) and, unless
        overridden, the dynamic-range target.
    dynamic_range_db : float, default 0.0
        Explicit sidelobe/dynamic-range target (dB); used when > 0, else
        derived from bits.

    Examples
    --------
    Create with defaults:

    >>> from doppler.measure import NPRMeasure
    >>> obj = NPRMeasure(
    ...     n=8192,
    ...     fs=1.0,
    ...     full_scale=1.0,
    ...     bits=0,
    ...     dynamic_range_db=0.0,
    ... )

    """
    def __init__(
        self,
        n: int = ...,
        fs: float = ...,
        full_scale: float = ...,
        bits: int = ...,
        dynamic_range_db: float = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Reset the analyser (a no-op: each analyze() call is independent).

        Every analyze() / spectrum_dbfs() call re-windows and re-transforms its
        own capture from scratch, so nothing is carried between calls to clear.
        The method exists only so NPRMeasure honours the same reset() contract
        as every other doppler object, letting a generic pipeline reset each
        stage uniformly.

        Examples
        --------
        >>> from doppler.measure import NPRMeasure
        >>> m = NPRMeasure(n=8192, fs=1.0)
        >>> m.reset()            # stateless: provided only for API uniformity
        >>> m.reset() is None    # returns nothing; safe to call anytime
        True

        """

    def analyze(
        self,
        x: float,
        active_lo: float,
        active_hi: float,
        notch_lo: float,
        notch_hi: float,
        guard_hz: float = 0.0,
    ) -> NPRMetrics:
        """NPR of a notched-noise capture over [active_lo,active_hi] with a
        notch [notch_lo,notch_hi] (Hz) and guard keep-out.

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
        NPRMetrics
            the NPR metric record (by value).

        Examples
        --------
        >>> from doppler.measure import NPRMeasure
        >>> import numpy as np
        >>> rng = np.random.default_rng(0)
        >>> n = 1 << 15
        >>> F = np.fft.rfft(rng.standard_normal(n))
        >>> f = np.fft.rfftfreq(n)
        >>> F[(f < 0.05) | (f > 0.45)] = 0  # band-limit to [0.05,0.45]
        >>> F[(f >= 0.20) & (f <= 0.25)] *= 10**(-50/20)   # notch 50 dB deep
        >>> x = np.fft.irfft(F, n)
        >>> x = (0.3*x/np.std(x)).astype(np.float32)
        >>> r = NPRMeasure(n=n, fs=1.0).analyze(
        ...     x, 0.05, 0.45, 0.20, 0.25, 0.01)
        >>> 45 < r.npr_db < 55, r.notch_psd_dbfs < r.inband_psd_dbfs
        (True, True)

        """

    def spectrum_dbfs(
        self,
        x: NDArray[np.float32],
        out: NDArray[np.float32] | None = None,
    ) -> NDArray[np.float32]:
        """DC-centred dBFS magnitude spectrum of a capture (length nfft, for
        plots).

        The same windowed, zero-padded PSD the NPR metrics are read off, laid
        out DC-centred (fftshifted) and normalised to dBFS for an
        analyzer-display backdrop. Use it to see the notch and the active band
        that analyze() integrates over.

        Parameters
        ----------
        x : NDArray[np.float32]
            Real time-domain capture (length x_len).

        Returns
        -------
        NDArray[np.float32]
            DC-centred dBFS magnitude spectrum, one value per FFT bin (nfft).

        Examples
        --------
        >>> from doppler.measure import NPRMeasure
        >>> import numpy as np
        >>> rng = np.random.default_rng(0)
        >>> x = (0.3*rng.standard_normal(8192)).astype(np.float32)  # noise
        >>> s = NPRMeasure(n=8192, fs=1.0).spectrum_dbfs(x)  # DC-centred dBFS
        >>> s.shape                                          # zero-padded nfft
        (16384,)
        >>> round(float(np.median(s)), 0)   # broadband floor, below 0 dBFS
        -48.0

        """

    def spectrum_dbfs_max_out(self) -> int:
        """Capacity (== nfft) of the spectrum_dbfs output buffer.

        Returns
        -------
        int
            Output.
        """

    @property
    def n(self) -> int:
        """Window / frame length (samples)."""

    @property
    def nfft(self) -> int:
        """Zero-padded transform length."""

    @property
    def fs(self) -> float:
        """Sample rate, Hz."""

    @property
    def rbw(self) -> float:
        """Rbw."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "NPRMeasure":
        """Enter a context manager, returning this object.

        Lets a NPRMeasure be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        NPRMeasure
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the NPRMeasure.

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
class IMDMeasure:
    """Create an IMDMeasure analyser (auto Kaiser window).

    Parameters
    ----------
    n : int, default 8192
        Capture/frame length (>= 2).
    fs : float, default 1.0
        Sample rate (Hz, > 0).
    full_scale : float, default 1.0
        Amplitude that equals 0 dBFS (> 0). Ignored if bits > 0.
    bits : int, default 0
        ADC depth: bits>0 sets the 0-dBFS reference to 2^(bits-1) and, unless
        overridden, the dynamic-range target.
    dynamic_range_db : float, default 0.0
        Explicit sidelobe/dynamic-range target (dB); used when > 0, else
        derived from bits.

    Examples
    --------
    Create with defaults:

    >>> from doppler.measure import IMDMeasure
    >>> obj = IMDMeasure(
    ...     n=8192,
    ...     fs=1.0,
    ...     full_scale=1.0,
    ...     bits=0,
    ...     dynamic_range_db=0.0,
    ... )

    """
    def __init__(
        self,
        n: int = ...,
        fs: float = ...,
        full_scale: float = ...,
        bits: int = ...,
        dynamic_range_db: float = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Reset the analyser (a no-op: each analyze() call is independent).

        Every analyze() / spectrum_dbfs() call re-windows and re-transforms its
        own capture from scratch, so nothing is carried between calls to clear.
        The method exists only so IMDMeasure honours the same reset() contract
        as every other doppler object, letting a generic pipeline reset each
        stage uniformly.

        Examples
        --------
        >>> from doppler.measure import IMDMeasure
        >>> m = IMDMeasure(n=4096, fs=1.0)
        >>> m.reset()            # stateless: provided only for API uniformity
        >>> m.reset() is None    # returns nothing; safe to call anytime
        True

        """

    def analyze(self, x: float) -> IMDMetrics:
        """Two-tone IMD/TOI of a real capture (finds the two strongest tones).

        Parameters
        ----------
        x : float
            Input.

        Returns
        -------
        IMDMetrics
            the IMD metric record (by value; zeroed if no two tones are found).

        Examples
        --------
        >>> from doppler.measure import IMDMeasure
        >>> import numpy as np
        >>> t = np.arange(4096)
        >>> # two equal tones at 200 & 250 cycles, plus 3rd-order
        >>> # products 40 dB down
        >>> x = (np.cos(2*np.pi*200*t/4096) + np.cos(2*np.pi*250*t/4096)
        ...      + 0.01*np.cos(2*np.pi*150*t/4096)
        ...      + 0.01*np.cos(2*np.pi*300*t/4096)).astype(np.float32)
        >>> r = IMDMeasure(n=4096, fs=1.0).analyze(x)
        >>> round(r.f1, 4), round(r.f2, 4), round(r.imd3_dbc, 0)
        (0.0488, 0.061, -40.0)

        """

    def spectrum_dbfs(
        self,
        x: NDArray[np.float32],
        out: NDArray[np.float32] | None = None,
    ) -> NDArray[np.float32]:
        """DC-centred dBFS magnitude spectrum of a capture (length nfft, for
        plots).

        The same windowed, zero-padded PSD the IMD metrics are read off, laid
        out DC-centred (fftshifted) and normalised to dBFS for an
        analyzer-display backdrop. Use it to see the two fundamentals and the
        intermodulation products that analyze() integrates.

        Parameters
        ----------
        x : NDArray[np.float32]
            Real time-domain capture (length x_len).

        Returns
        -------
        NDArray[np.float32]
            DC-centred dBFS magnitude spectrum, one value per FFT bin (nfft).

        Examples
        --------
        >>> from doppler.measure import IMDMeasure
        >>> import numpy as np
        >>> t = np.arange(4096)
        >>> x = (0.5*np.cos(2*np.pi*200*t/4096)
        ...      + 0.5*np.cos(2*np.pi*250*t/4096)).astype(np.float32)
        >>> s = IMDMeasure(n=4096, fs=1.0).spectrum_dbfs(x)  # DC-centred dBFS
        >>> s.shape
        (8192,)
        >>> round(float(s.max()), 1)   # each tone splits into two images
        -12.0

        """

    def spectrum_dbfs_max_out(self) -> int:
        """Capacity (== nfft) of the spectrum_dbfs output buffer.

        Returns
        -------
        int
            Output.
        """

    @property
    def n(self) -> int:
        """Window / frame length (samples)."""

    @property
    def nfft(self) -> int:
        """Zero-padded transform length."""

    @property
    def fs(self) -> float:
        """Sample rate, Hz."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "IMDMeasure":
        """Enter a context manager, returning this object.

        Lets a IMDMeasure be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        IMDMeasure
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the IMDMeasure.

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

def measure_min_samples(
    fs: float,
    target_rbw: float,
    bits: int,
    dynamic_range_db: float,
    complex_input: int,
) -> int:
    """Samples for a target RBW (auto Kaiser from bits/dynamic_range_db;
    target_rbw<=0 -> span/1000).

    Plans a capture for the same auto-Kaiser window the measurement objects
    use: the dynamic-range target (from dynamic_range_db, else bits)
    selects the Kaiser beta, whose ENBW (measured via kaiser_enbw) sets the
    bins-per-RBW. RBW = ENBW * fs / n, so n = ceil(ENBW * fs / target_rbw).

    Parameters
    ----------
    fs : float
        Sample rate (Hz, > 0).
    target_rbw : float
        Desired resolution bandwidth (Hz). When <= 0 it defaults to
        span/1000, where span = fs/2 for real captures and fs for complex
        (complex_input).
    bits : int
        ADC depth: sets the dynamic-range target when no explicit override
        is given.
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
    """Nearest leakage-free coherent test frequency (J cycles, J coprime
    N).

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
