# measure/measure.pyi — type stubs for the measure C extension.
#
# NB: analyze()/time_stats() return named PyStructSequence results (built in the
# hand-owned measure_ext_tonemeas.c fragment), not jm's default list[tuple]; the
# constructor is window-first to match the binding's keyword list.  This stub is
# therefore hand-maintained and allowlisted in just-makeit.toml (status_allow);
# `jm apply` regenerates it, so re-apply these edits after any apply.
from typing import Literal, NamedTuple
import numpy as np
from numpy.typing import NDArray

class ToneMetrics(NamedTuple):
    """Single-tone ADC / spectral measurement bag (one analysis pass).

    Ratio metrics (snr/sinad/thd/thd_n) are independent of the dBFS reference;
    the absolute ``*_dbfs`` levels reference a full-scale tone to 0 dBFS.
    """

    snr: float  # 10log10(P_fund / P_noise) [dB]
    sinad: float  # 10log10(P_fund / (P_noise + P_harm)) [dB]
    thd: float  # 10log10(P_harm / P_fund) [dBc]
    thd_pct: float  # 100*sqrt(P_harm / P_fund) [%]
    thd_n: float  # THD+N relative to the fundamental [dBc]; = -sinad
    sfdr_dbc: float  # fundamental - worst spur [dBc]
    sfdr_dbfs: float  # full scale - worst spur [dBFS]
    enob: float  # (sinad - 1.76) / 6.02
    enob_fs: float  # full-scale-corrected ENOB
    noise_floor_dbfs: float  # mean per-bin noise power [dBFS]
    fund_freq: float  # fundamental frequency [Hz]
    fund_dbfs: float  # fundamental level [dBFS]
    worst_spur_freq: float  # worst-spur frequency [Hz]
    worst_spur_dbc: float  # worst-spur level vs the fundamental [dBc]
    worst_spur_is_harm: int  # 1 if the worst spur is a harmonic
    rbw_hz: float  # resolution bandwidth [Hz]
    enbw_hz: float  # equivalent noise bandwidth [Hz]
    bin_hz: float  # FFT bin spacing [Hz]
    lobe_bins: int  # window main-lobe half-width L [bins]
    n_noise_bins: int  # number of bins counted as noise
    proc_gain_db: float  # FFT processing gain [dB]
    amp_uncert_db: float  # amplitude-read uncertainty bound [dB]
    floor_uncert_db: float  # noise-floor standard error [dB]

class TimeStats(NamedTuple):
    """Time-domain capture statistics (AC-coupled crest / PAPR)."""

    rms: float  # root-mean-square (DC included)
    peak: float  # peak |x - DC|
    crest_db: float  # 20log10(peak_ac / rms_ac)
    papr_db: float  # peak-to-average power ratio [dB] (= crest_db)
    dc_offset: float  # mean(x)
    fs_util_pct: float  # 100 * max|x| / full_scale

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
        Kaiser shape (ignored for Hann); ~-98 dB sidelobes at 12.
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
        """DC-centred dBFS magnitude spectrum of a capture (length ``nfft``).

        Two-sided analyser view for plotting; see the design guide.
        """

    def reset(self) -> None:
        """Reset (no-op: each analyse() call is independent)."""

    @property
    def n(self) -> int:
        """Capture / frame length."""

    @property
    def nfft(self) -> int:
        """Zero-padded transform length."""

    @property
    def fs(self) -> float:
        """Sample rate (Hz)."""

    @property
    def enbw(self) -> float:
        """Window equivalent noise bandwidth (bins)."""

    @property
    def lobe_bins(self) -> int:
        """Window main-lobe half-width L (bins)."""

    @property
    def rbw(self) -> float:
        """Resolution bandwidth = enbw * fs / n (Hz)."""

    @property
    def bin_hz(self) -> float:
        """FFT bin spacing = fs / nfft (Hz)."""

    @property
    def proc_gain_db(self) -> float:
        """FFT processing gain = 10log10(nfft / 2) (dB)."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "ToneMeasure": ...
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
