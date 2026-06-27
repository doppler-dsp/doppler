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
            Input CF32 samples, length x_len.

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
            Input CF32 samples, length x_len.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written.
        """

    def set_acq(self, acq_code: NDArray[np.uint8], acq_reps: int) -> None:
        """Enable preamble-aided pull-in: track acq_reps periods of the (distinct) acq_code coherently before despreading the payload with the data code. Call before feeding the burst; clears when the preamble is consumed.

        Track acq_reps periods of acq_code coherently (the unmodulated, repeated
        acquisition preamble — a full ±pi phase discriminator, so the loops pull
        in even a wide residual) before switching to the data code for the
        payload. Call before feeding the burst; the acq mode clears
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

class Acquisition:
    """Create a streaming DSSS acquisition engine.

    Parameters
    ----------
    code : NDArray[np.uint8], default ...
        PN chips (0/1), length code_len.
    reps : int, default 1
        Max coherent code repetitions, the coherence ceiling (>=1).
    spc : int, default 4
        Samples per chip (>= 1).
    chip_rate : float, default 1000000.0
        Chip rate in Hz (> 0).
    cn0_dbhz : float, default 50.0
        Carrier-to-noise density in dB-Hz (> 0).
    doppler_uncertainty : float, default 0.0
        One-sided Doppler search half-range in Hz; 0 uses the full native span +/- chip_rate/(2*sf).  Must be <= span.
    pfa : float, default 1e-3
        Target system (max-of-N) false-alarm probability (0,1).
    pd : float, default 0.9
        Target detection probability (0,1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
    max_noncoh : int, default 1
        Cap on the auto-split non-coherent look count (>= 1; default 1 keeps the engine purely coherent).

    """
    def __init__(self, code: NDArray[np.uint8] = ..., reps: int = ..., spc: int = ..., chip_rate: float = ..., cn0_dbhz: float = ..., doppler_uncertainty: float = ..., pfa: float = ..., pd: float = ..., noise_mode: Literal["mean", "median", "min", "max"] = "mean", max_noncoh: int = ...) -> None: ...

    def reset(self) -> None:
        """Drain the input ring and reset the coherent accumulator.
        """

    def push(self, x: complex) -> list[tuple[int, int, float, float, float, float]]:
        """Stream raw samples; emit one event per CFAR dump above threshold.

        Buffers in, then for every complete frame applies the slow-time Doppler
        FFT, correlates against the PN reference, dumps the coherent surface
        (or, when n_noncoh > 1, accumulates |·|² over n_noncoh looks first),
        gates the peak on the auto-configured threshold, and appends an
        acq_result_t.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        list[tuple[int, int, float, float, float, float]]
            Number of events written (0 … max_results).
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def code_bins(self) -> int:
        """Code-phase hypotheses searched (= sf*spc, one code period)."""

    @property
    def doppler_bins(self) -> int:
        """Coherent depth chosen: the slow-time FFT length in code reps (<= reps)."""

    @property
    def sf(self) -> int:
        """Chips per PN segment, inferred from len(code)."""

    @property
    def spc(self) -> int:
        """Samples per chip (chip-rate oversample factor)."""

    @property
    def reps(self) -> int:
        """Max coherent code repetitions (the coherence ceiling)."""

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks per detection (1 = pure coherent)."""

    @property
    def ring_cap(self) -> int:
        """Input ring capacity in complex samples."""

    @property
    def noise_lo(self) -> int:
        """First CFAR reference bin (inclusive)."""

    @property
    def noise_hi(self) -> int:
        """Last CFAR reference bin (inclusive)."""

    @property
    def threshold(self) -> float:
        """CFAR gate on the test statistic (coherent path)."""

    @property
    def eta(self) -> float:
        """Raw per-cell Rayleigh amplitude threshold."""

    @property
    def eta_nc(self) -> float:
        """Non-coherent CFAR threshold (order-N_nc Marcum)."""

    @property
    def pfa_cell(self) -> float:
        """Bonferroni per-cell false-alarm probability over the searched cells."""

    @property
    def pd_predicted(self) -> float:
        """Predicted Pd at cn0_dbhz and the chosen grid."""

    @property
    def fs(self) -> float:
        """Sample rate (Hz) = chip_rate * spc."""

    @property
    def chip_rate(self) -> float:
        """Chip rate (Hz)."""

    @property
    def cn0_dbhz(self) -> float:
        """Carrier-to-noise density used to size the search (dB-Hz)."""

    @property
    def doppler_span_hz(self) -> float:
        """Native unambiguous Doppler half-range = +/- chip_rate/(2*sf) Hz."""

    @property
    def doppler_res_hz(self) -> float:
        """Doppler bin width = chip_rate/(sf*doppler_bins) Hz."""

    @property
    def pd(self) -> float:
        """Target detection probability."""

    @property
    def underpowered(self) -> bool:
        """True when pd_predicted < pd (the search cannot meet the target)."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Acquisition": ...

    def __exit__(self, *args: object) -> None: ...
