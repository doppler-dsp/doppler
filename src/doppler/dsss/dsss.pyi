# dsss/dsss.pyi — type stubs for the dsss C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class Despreader:
    """Despreader component.

    Parameters
    ----------
    code : NDArray[np.uint8], default ...
        code constructor parameter.
    sf : int, default 1
        sf constructor parameter.
    sps : int, default 2
        sps constructor parameter.
    init_norm_freq : float, default 0.0
        init_norm_freq constructor parameter.
    init_chip_phase : float, default 0.0
        init_chip_phase constructor parameter.
    bn_carrier : float, default 0.05
        bn_carrier constructor parameter.
    bn_code : float, default 0.01
        bn_code constructor parameter.

    """
    def __init__(self, code: NDArray[np.uint8] = ..., sf: int = ..., sps: int = ..., init_norm_freq: float = ..., init_chip_phase: float = ..., bn_carrier: float = ..., bn_code: float = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Despread a cf32 block; emit one complex prompt symbol per code period.

        Streams: a partial symbol is carried in state across calls. Each emitted
        symbol is the complex prompt integrate-and-dump (carrier-wiped,
        code-stripped) — its sign is the BPSK decision, its phase/magnitude the
        soft information. During a `despreader_set_acq` preamble no symbols are
        emitted (the loops are pulling in); payload symbols follow.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input CF32 samples, length @p x_len.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written.

        Examples
        --------
        // seed from acquisition (norm_freq cyc/sample, chip phase in chips):
        despreader_state_t *d = despreader_create(code, n, 32, 2, f0, chip, .05, .01);
        float complex sym[256];
        size_t k = despreader_steps(d, rx, rx_len, sym, 256);
        // hard bit of sym[i] = crealf(sym[i]) >= 0
        despreader_destroy(d);

        """

    def bits(self, x: NDArray[np.complex64]) -> NDArray[np.uint8]:
        """Despread a cf32 block; emit one hard BPSK bit per code period.

        Same streaming kernel as despreader_steps(), but emits the hard decision
        `crealf(prompt) >= 0` instead of the complex symbol.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input CF32 samples, length @p x_len.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written.
        """

    def set_acq(self, acq_code: NDArray[np.uint8], acq_reps: int) -> None:
        """Enable preamble-aided pull-in: track acq_reps periods of the (distinct) acq_code coherently before despreading the payload with the data code. Call before feeding the burst; clears when the preamble is consumed.

        Track @p acq_reps periods of @p acq_code coherently (the unmodulated,
        repeated acquisition preamble — a full ±pi phase discriminator, so the
        loops pull in even a wide residual) before switching to the data code
        for the payload. Call before feeding the burst; the acq mode clears
        automatically once the preamble is consumed, and re-arms on
        despreader_reset().

        Parameters
        ----------
        acq_code : NDArray[np.uint8]
            Acquisition code (0/1), length acq_code_len; copied.
        acq_reps : int
            Number of acq-code periods in the preamble.
        """

    def reset(self) -> None:
        """Re-seed the loops to the create-time phase/frequency; preserve config.
        """

    @property
    def bn_carrier(self) -> float:
        """Carrier (Costas) loop noise bandwidth, normalized to the symbol rate."""
    @bn_carrier.setter
    def bn_carrier(self, value: float) -> None: ...

    @property
    def bn_code(self) -> float:
        """Code (DLL) loop noise bandwidth, normalized to the symbol rate."""
    @bn_code.setter
    def bn_code(self, value: float) -> None: ...

    @property
    def norm_freq(self) -> float:
        """Current carrier frequency estimate, cycles/sample."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def code_phase(self) -> float:
        """Current tracked code phase within the symbol, chips."""

    @property
    def lock_metric(self) -> float:
        """Lock indicator in [0,1] (EMA of |Re prompt|/|prompt|; ~1 = locked)."""

    @property
    def snr_est(self) -> float:
        """Post-despread SNR estimate (EMA of (Re prompt)^2 / (Im prompt)^2)."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Despreader": ...

    def __exit__(self, *args: object) -> None: ...

class Acquirer:
    """Create a streaming DSSS acquisition engine.

    Parameters
    ----------
    code : NDArray[np.uint8], default ...
        code constructor parameter.
    sf : int, default 1
        sf constructor parameter.
    sps : int, default 1
        sps constructor parameter.
    ny : int, default 16
        ny constructor parameter.
    pfa : float, default 1e-3
        pfa constructor parameter.
    pd : float, default 0.9
        pd constructor parameter.
    min_snr : float, default 0.1
        min_snr constructor parameter.
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        noise_mode constructor parameter.
    max_dwell : int, default 64
        max_dwell constructor parameter.

    """
    def __init__(self, code: NDArray[np.uint8] = ..., sf: int = ..., sps: int = ..., ny: int = ..., pfa: float = ..., pd: float = ..., min_snr: float = ..., noise_mode: Literal["mean", "median", "min", "max"] = "mean", max_dwell: int = ...) -> None: ...

    def reset(self) -> None:
        """Drain the input ring and reset the coherent accumulator.
        """

    def push(self, x: complex) -> list[tuple[int, int, float, float, float, float]]:
        """Stream raw samples; emit one event per CFAR dump above threshold.

        Buffers @p in, then for every complete frame applies the slow-time
        Doppler FFT, correlates against the PN reference, and — every @p dwell
        frames — dumps the coherent surface, gates the peak on the
        auto-configured threshold, and appends an acq_result_t.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        list[tuple[int, int, float, float, float, float]]
            Number of events written (0 … max_results).
        """

    @property
    def ny(self) -> int:
        """Ny."""

    @property
    def nx(self) -> int:
        """Nx."""

    @property
    def n(self) -> int:
        """N."""

    @property
    def sf(self) -> int:
        """Sf."""

    @property
    def sps(self) -> int:
        """Sps."""

    @property
    def dwell(self) -> int:
        """Dwell."""

    @property
    def max_dwell(self) -> int:
        """Max dwell."""

    @property
    def ring_cap(self) -> int:
        """Ring cap."""

    @property
    def noise_lo(self) -> int:
        """Noise lo."""

    @property
    def noise_hi(self) -> int:
        """Noise hi."""

    @property
    def threshold(self) -> float:
        """Threshold."""

    @property
    def eta(self) -> float:
        """Eta."""

    @property
    def pfa_cell(self) -> float:
        """Pfa cell."""

    @property
    def pd_predicted(self) -> float:
        """Pd predicted."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Acquirer": ...

    def __exit__(self, *args: object) -> None: ...
