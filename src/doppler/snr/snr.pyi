# snr/snr.pyi — type stubs for the snr C extension.
import numpy as np
from numpy.typing import NDArray
def snr_data_aided_db(soft: NDArray[np.complex64], sign_bits: NDArray[np.uint8]) -> float:
    """Data-aided Es/N0 (dB): strip the known sign, Es/N0 = a^2 / mean(|z-a|^2).

    Strips the known transmitted sign (``soft[i] * (sign_bits[i] ? -1 :
    1)``), then Es/N0 = (mean signal amplitude)^2 / (mean residual power).
    Scale-invariant (works regardless of the caller's symbol normalization)
    and polarity-invariant (a global sign flip in ``soft`` changes nothing,
    since the amplitude is squared) -- so it needs no resolution of an
    absolute-phase ambiguity a tracking loop may carry.

    Parameters
    ----------
    soft : NDArray[np.complex64]
        Despread complex symbols.
    sign_bits : NDArray[np.uint8]
        Known transmitted bits (0/1; 0 -> +1, 1 -> -1).

    Returns
    -------
    float
        Es/N0 in dB over ``min(soft_len, sign_bits_len)`` paired samples, or NaN if that count is 0 or the residual power is exactly 0.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.snr import snr_data_aided_db
    >>> rng = np.random.default_rng(0)
    >>> bits = (rng.random(2000) > 0.5).astype(np.uint8)
    >>> sign = np.where(bits, -1.0, 1.0).astype(np.complex64)
    >>> noise = (0.1 * (rng.standard_normal(2000)
    ...          + 1j * rng.standard_normal(2000))).astype(np.complex64)
    >>> soft = (sign + noise).astype(np.complex64)
    >>> round(float(snr_data_aided_db(soft, bits)), 1)
    17.1

    """

def snr_m2m4_db(x: NDArray[np.complex64]) -> float:
    """Non-data-aided moment-based (M2M4) Es/N0 (dB) for a constant-modulus signal in AWGN.

    M2M4 estimator (Pauluzzi & Beaulieu 2000) for a constant-modulus signal
    (BPSK/QPSK/M-PSK) in circular complex AWGN: no known symbols required.

    Parameters
    ----------
    x : NDArray[np.complex64]
        Complex baseband samples (post-carrier-lock; residual phase does not bias the moment-based estimate).

    Returns
    -------
    float
        Es/N0 in dB, 0-linear for pure noise, +inf for a noiseless constant-modulus signal, or NaN if x_len is 0 or the block has zero power.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.snr import snr_m2m4_db
    >>> rng = np.random.default_rng(0)
    >>> bits = (rng.random(2000) > 0.5).astype(np.uint8)
    >>> sign = np.where(bits, -1.0, 1.0).astype(np.complex64)
    >>> noise = (0.1 * (rng.standard_normal(2000)
    ...          + 1j * rng.standard_normal(2000))).astype(np.complex64)
    >>> x = (sign + noise).astype(np.complex64)
    >>> round(float(snr_m2m4_db(x)), 1)
    17.1

    """

def snr_data_aided_db_series(soft: NDArray[np.complex64], sign_bits: NDArray[np.uint8], window: int) -> NDArray[np.float64]:
    """Sliding-window data-aided Es/N0 (dB) vs index, for visualizing drift.

    Same estimator as snr_data_aided_db(), applied to a ``[i - window/2, i +
    window/2]`` window centered (clamped at the edges) on each output index
    -- for visualizing SNR drift vs time/index rather than reading one
    block-average scalar.

    Parameters
    ----------
    soft : NDArray[np.complex64]
        Despread complex symbols.
    sign_bits : NDArray[np.uint8]
        Known transmitted bits (0/1).
    window : int
        Window width in samples.

    Returns
    -------
    NDArray[np.float64]
        Output.
    """

def snr_m2m4_db_series(x: NDArray[np.complex64], window: int) -> NDArray[np.float64]:
    """Sliding-window blind (M2M4) Es/N0 (dB) vs index, for visualizing drift.

    Same estimator as snr_m2m4_db(), applied to a ``[i - window/2, i +
    window/2]`` window centered (clamped at the edges) on each output index.

    Parameters
    ----------
    x : NDArray[np.complex64]
        Complex baseband samples.
    window : int
        Window width in samples.

    Returns
    -------
    NDArray[np.float64]
        Output.
    """
