# dsss/dsss.pyi — type stubs for the dsss C extension.
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
