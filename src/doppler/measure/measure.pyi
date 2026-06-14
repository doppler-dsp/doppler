# measure/measure.pyi — type stubs for the measure C extension.
#
# NB: analyze()/time_stats() return named PyStructSequence results (built in the
# hand-owned measure_ext_*.c fragments), not jm's default list[tuple]; the
# constructors are window-first to match the binding keyword lists.  This stub
# is hand-maintained and allowlisted in just-makeit.toml (status_allow); `jm
# apply` regenerates it, so re-apply these edits after any apply (jm#244).
from typing import Literal, NamedTuple
import numpy as np
from numpy.typing import NDArray

class ToneMetrics(NamedTuple):
    """Single-tone ADC / spectral measurement bag (one analysis pass).

    Ratio metrics (snr/sinad/thd/thd_n) are independent of the dBFS reference;
    the absolute ``*_dbfs`` levels reference a full-scale tone to 0 dBFS.
    """

    snr: float
    sinad: float
    thd: float
    thd_pct: float
    thd_n: float
    sfdr_dbc: float
    sfdr_dbfs: float
    enob: float
    enob_fs: float
    noise_floor_dbfs: float
    fund_freq: float
    fund_dbfs: float
    worst_spur_freq: float
    worst_spur_dbc: float
    worst_spur_is_harm: int
    rbw_hz: float
    enbw_hz: float
    bin_hz: float
    lobe_bins: int
    n_noise_bins: int
    proc_gain_db: float
    amp_uncert_db: float
    floor_uncert_db: float

class TimeStats(NamedTuple):
    """Time-domain capture statistics (AC-coupled crest / PAPR)."""

    rms: float
    peak: float
    crest_db: float
    papr_db: float
    dc_offset: float
    fs_util_pct: float

class IMDMetrics(NamedTuple):
    """Two-tone intermodulation result (IMD2 / IMD3 / intercepts)."""

    f1: float
    f2: float
    p1_dbfs: float
    p2_dbfs: float
    imd2_dbc: float
    imd3_dbc: float
    imd2_freq: float
    imd3_lo_freq: float
    imd3_hi_freq: float
    toi_dbfs: float
    soi_dbfs: float
    rbw_hz: float

class NPRMetrics(NamedTuple):
    """Noise Power Ratio (notched-noise loading) result."""

    npr_db: float
    inband_psd_dbfs: float
    notch_psd_dbfs: float
    n_inband_bins: int
    n_notch_bins: int
    rbw_hz: float

class ToneMeasure:
    """Single-tone ADC / converter spectral measurement.

    Owns a window + zero-padded FFT and analyses one time-domain capture into
    the full single-tone metric bag.  Each component's power is integrated over
    its window main lobe (IEEE Std 1241) and the noise sum excludes the leakage
    bins around DC, the fundamental and each harmonic, so a full-scale tone
    reads ~0 dBFS regardless of where it lands between FFT bins.

    Parameters
    ----------
    window : Literal["hann", "kaiser"], default "kaiser"
        Analysis window.
    n : int, default 8192
        Capture / frame length the analyser expects.
    fs : float, default 1.0
        Sample rate (Hz).
    beta : float, default 12.0
        Kaiser shape (ignored for Hann).
    pad : int, default 2
        Zero-pad factor; ``nfft = next_pow2(n * pad)``.
    n_harmonics : int, default 8
        Harmonics tracked (k = 2..n_harmonics), folded into the band.
    full_scale : float, default 1.0
        Amplitude that equals 0 dBFS.
    dc_guard : int, default 0
        Extra bins excluded beyond the main lobe around DC.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.measure import ToneMeasure
    >>> m = ToneMeasure(window="kaiser", n=4096, fs=1.0, beta=12.0)
    >>> x = np.cos(2 * np.pi * 300 * np.arange(4096) / 4096).astype(np.float32)
    >>> r = m.analyze(x)
    >>> abs(r.fund_dbfs) < 0.1          # full-scale tone -> ~0 dBFS
    True
    >>> r.lobe_bins > 0 and r.n_noise_bins > 0
    True

    """

    def __init__(
        self,
        window: Literal["hann", "kaiser"] = "kaiser",
        n: int = ...,
        fs: float = ...,
        beta: float = ...,
        pad: int = ...,
        n_harmonics: int = ...,
        full_scale: float = ...,
        dc_guard: int = ...,
    ) -> None: ...
    def analyze(self, x: NDArray[np.float32]) -> ToneMetrics:
        """Analyse a real capture; returns a named :class:`ToneMetrics`.

        >>> import numpy as np
        >>> from doppler.measure import ToneMeasure
        >>> m = ToneMeasure(n=4096, beta=12.0)
        >>> x = np.cos(2 * np.pi * 200 * np.arange(4096) / 4096)
        >>> x += 0.01 * np.cos(2 * np.pi * 400 * np.arange(4096) / 4096)
        >>> round(m.analyze(x.astype(np.float32)).thd, 0)   # 2nd harm -40 dBc
        -40.0

        """

    def analyze_complex(self, x: NDArray[np.complex64]) -> ToneMetrics:
        """Analyse a complex baseband capture (two-sided spectrum).

        >>> import numpy as np
        >>> from doppler.measure import ToneMeasure
        >>> m = ToneMeasure(n=4096, beta=12.0)
        >>> i = np.arange(4096)
        >>> x = np.exp(2j * np.pi * 137 * i / 4096).astype(np.complex64)
        >>> abs(m.analyze_complex(x).fund_freq - 137 / 4096) < 2e-3
        True

        """

    def time_stats(self, x: NDArray[np.float32]) -> TimeStats:
        """Time-domain statistics of a real capture (:class:`TimeStats`).

        >>> import numpy as np
        >>> from doppler.measure import ToneMeasure
        >>> m = ToneMeasure(n=4096)
        >>> x = 0.8 * np.cos(2 * np.pi * 50 * np.arange(4096) / 4096)
        >>> round(m.time_stats(x.astype(np.float32)).crest_db, 1)
        3.0

        """

    def spectrum_dbfs(self, x: NDArray[np.float32]) -> NDArray[np.float32]:
        """DC-centred dBFS magnitude spectrum of a capture (length ``nfft``)."""

    def reset(self) -> None:
        """Reset (no-op: each analyse() call is independent)."""

    @property
    def n(self) -> int: ...
    @property
    def nfft(self) -> int: ...
    @property
    def fs(self) -> float: ...
    @property
    def enbw(self) -> float: ...
    @property
    def lobe_bins(self) -> int: ...
    @property
    def rbw(self) -> float: ...
    @property
    def bin_hz(self) -> float: ...
    @property
    def proc_gain_db(self) -> float: ...
    def destroy(self) -> None: ...
    def __enter__(self) -> "ToneMeasure": ...
    def __exit__(self, *args: object) -> None: ...

class IMDMeasure:
    """Two-tone intermodulation (IMD2 / IMD3) and third-order intercept.

    Drive two equal tones f1<f2; :meth:`analyze` finds them as the two strongest
    lobes and integrates the IMD2 (f2-f1) and IMD3 (2f1-f2, 2f2-f1) products
    over their window main lobes.

    Examples
    --------
    >>> from doppler.measure import IMDMeasure
    >>> m = IMDMeasure(n=8192, fs=1.0)
    >>> m.nfft
    16384

    """

    def __init__(
        self,
        window: Literal["hann", "kaiser"] = "kaiser",
        n: int = ...,
        fs: float = ...,
        beta: float = ...,
        pad: int = ...,
        full_scale: float = ...,
    ) -> None: ...
    def analyze(self, x: NDArray[np.float32]) -> IMDMetrics:
        """Two-tone IMD/TOI of a real capture (named :class:`IMDMetrics`)."""

    def reset(self) -> None:
        """Reset (no-op: each analyse() call is independent)."""

    @property
    def n(self) -> int: ...
    @property
    def nfft(self) -> int: ...
    @property
    def fs(self) -> float: ...
    def destroy(self) -> None: ...
    def __enter__(self) -> "IMDMeasure": ...
    def __exit__(self, *args: object) -> None: ...

class NPRMeasure:
    """Notched-noise Noise Power Ratio.

    Drive the system with band-limited noise containing a deep notch; NPR is the
    ratio of the mean in-band noise PSD to the mean PSD that folds into the
    notch.  The band/notch geometry is an :meth:`analyze` argument, so one
    estimator sweeps several notch placements.

    Examples
    --------
    >>> from doppler.measure import NPRMeasure
    >>> m = NPRMeasure(window="kaiser", n=8192, fs=1.0)
    >>> m.nfft
    16384

    """

    def __init__(
        self,
        window: Literal["hann", "kaiser"] = "kaiser",
        n: int = ...,
        fs: float = ...,
        beta: float = ...,
        pad: int = ...,
        full_scale: float = ...,
    ) -> None: ...
    def analyze(
        self,
        x: NDArray[np.float32],
        active_lo: float,
        active_hi: float,
        notch_lo: float,
        notch_hi: float,
        guard_hz: float,
    ) -> NPRMetrics:
        """NPR of a notched-noise capture (named :class:`NPRMetrics`).

        ``active_lo/hi`` bound the loaded noise band, ``notch_lo/hi`` the notch,
        and ``guard_hz`` is a keep-out around the notch edges (all Hz).
        """

    def reset(self) -> None:
        """Reset (no-op: each analyse() call is independent)."""

    @property
    def n(self) -> int: ...
    @property
    def nfft(self) -> int: ...
    @property
    def fs(self) -> float: ...
    @property
    def rbw(self) -> float: ...
    def destroy(self) -> None: ...
    def __enter__(self) -> "NPRMeasure": ...
    def __exit__(self, *args: object) -> None: ...

def measure_min_samples(
    fs: float, target_rbw: float, window: int, beta: float
) -> int:
    """Samples needed to reach a target RBW (window 0=hann, 1=kaiser)."""

def measure_rec_nfft(n: int, pad: int) -> int:
    """Recommended zero-padded transform length: next_pow2(n * pad)."""

def measure_proc_gain(nfft: int) -> float:
    """FFT processing gain in dB: 10*log10(nfft / 2)."""

def dp_coherent_freq(fs: float, f_target: float, N: int) -> float:
    """Nearest leakage-free coherent test frequency (J cycles, J coprime N)."""
