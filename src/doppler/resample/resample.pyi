# resample/resample.pyi — type stubs for the resample C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class Resampler:
    """Create a Resampler with the built-in 4096×19 Kaiser bank. The bank provides ~60 dB alias rejection with 0.4/0.6 pass/stop normalised cutoffs. Pass rate >= 1.0 to interpolate (upsample); pass rate < 1.0 to decimate (downsample). For a custom bank use Resampler_create_custom() instead.

    Parameters
    ----------
    rate : float, default 0.0
        Output-to-input sample rate ratio (any positive float). Values >= 1.0 interpolate; values < 1.0 decimate.

    Examples
    --------
    Create with defaults:

    >>> from doppler.resample import Resampler
    >>> obj = Resampler(rate=0.0)

    """
    def __init__(self, rate: float = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Resample a block of CF32 samples at the fixed base rate. Uses the dual-mode polyphase engine: output-driven for rate >= 1 (interpolation), input-driven transposed-form for rate < 1 (decimation). State carries over between calls, so contiguous blocks produce the same result as one large block.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input samples.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length is approximately x_len * rate.

        Examples
        --------
        >>> from doppler.resample import Resampler
        >>> import numpy as np
        >>> r = Resampler(rate=2.0)
        >>> y = r.execute(np.zeros(128, dtype=np.complex64))
        >>> y.shape, y.dtype
        ((256,), dtype('complex64'))

        """

    def execute_ctrl(self, x: NDArray[np.complex64], ctrl: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Resample with per-sample additive rate deviations. Effective rate for sample i is base_rate + real(`ctrl[i]`). Uses a unified double-precision accumulator that handles both interpolation and decimation in a single code path — suitable for Doppler-shift simulation and fractional-sample timing correction. ctrl and x must have the same length.

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input samples.
        ctrl : NDArray[np.complex64]
            CF32 array, same length as x; only the real part is used as a per-sample rate addend.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length depends on accumulated rate deviations.

        Examples
        --------
        >>> from doppler.resample import Resampler
        >>> import numpy as np
        >>> r = Resampler(rate=1.0)
        >>> x = np.zeros(64, dtype=np.complex64)
        >>> ctrl = np.zeros(64, dtype=np.complex64)
        >>> y = r.execute_ctrl(x, ctrl)
        >>> y.shape, y.dtype
        ((64,), dtype('complex64'))

        """

    def reset(self) -> None:
        """Zero the delay line and phase accumulator. Rate and polyphase bank are preserved so the resampler can be resumed at the same ratio. Zeroing state eliminates transient artefacts when starting a new signal burst.

        Examples
        --------
        >>> from doppler.resample import Resampler
        >>> import numpy as np
        >>> r = Resampler(rate=2.0)
        >>> _ = r.execute(np.ones(64, dtype=np.complex64))
        >>> r.reset()
        >>> r.rate
        2.0

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def rate(self) -> float:
        """Get / set the output-to-input sample rate ratio. The setter recomputes the phase increment immediately; the delay line and phase accumulator are preserved so in-stream rate changes are glitch-free. Switching sign of (rate - 1) (i.e. crossing the boundary between interp and decim modes) requires a fresh create()."""
    @rate.setter
    def rate(self, value: float) -> None: ...

    @property
    def num_phases(self) -> int:
        """Number of polyphase branches in the filter bank. Always a power of two. The built-in bank has 4096 phases giving sub-sample timing resolution of 1/4096 of an input sample period."""

    @property
    def num_taps(self) -> int:
        """Taps per polyphase branch. Total prototype filter length is num_phases * num_taps - 1. The built-in bank uses 19 taps per branch."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Resampler": ...

    def __exit__(self, *args: object) -> None: ...

class Halfbanddecimator:
    """Create a HalfbandDecimator with caller-supplied FIR taps. Implements a 2:1 polyphase halfband decimator over CF32 IQ. The caller provides the FIR branch coefficient array h; use ``doppler.resample.kaiser_num_taps(2, atten, pb, sb)`` to size it and scipy or the built-in bank helper to design the prototype. Output length is approximately x_len / 2 per execute() call.

    Parameters
    ----------
    h : NDArray[np.float32], default ...
        Float32 FIR branch coefficients, length num_taps. Must be a symmetric halfband prototype (antisymmetric even-indexed taps zeroed).

    """
    def __init__(self, h: NDArray[np.float32] = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Decimate x by 2 using the polyphase halfband FIR filter. Processes every second input sample through the FIR branch and passes the other branch through the all-pass (zero-delay) path. State persists between calls — contiguous blocks give identical output to one large block. Output length is floor(x_len / 2).

        Parameters
        ----------
        x : NDArray[np.complex64]
            CF32 input array.  Length must be even for exact half-rate output; odd lengths write floor(x_len/2).

        Returns
        -------
        NDArray[np.complex64]
            CF32 decimated output; length == floor(x_len / 2).

        Examples
        --------
        >>> from doppler.resample import HalfbandDecimator
        >>> import numpy as np
        >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
        ...              dtype=np.float32)
        >>> hb = HalfbandDecimator(h=h)
        >>> y = hb.execute(np.zeros(100, dtype=np.complex64))
        >>> y.shape, y.dtype
        ((50,), dtype('complex64'))

        """

    def reset(self) -> None:
        """Zero all delay lines.  Coefficients and num_taps preserved. Call between signal bursts to suppress transient ringing from prior filter state. The next execute() after reset produces the same output as a freshly created decimator fed the same input.

        Examples
        --------
        >>> from doppler.resample import HalfbandDecimator
        >>> import numpy as np
        >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
        ...              dtype=np.float32)
        >>> hb = HalfbandDecimator(h=h)
        >>> _ = hb.execute(np.ones(64, dtype=np.complex64))
        >>> hb.reset()
        >>> hb.num_taps
        5

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def rate(self) -> float:
        """Fixed decimation rate — always 0.5. The halfband decimator is structurally 2:1; this property exists for API parity with Resampler and RateConverter."""

    @property
    def num_taps(self) -> int:
        """Number of FIR branch taps as passed to create. The all-pass (even-phase) branch has no taps; only the odd-phase FIR branch has length num_taps. The total prototype length is 2 * num_taps - 1."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Halfbanddecimator": ...

    def __exit__(self, *args: object) -> None: ...

class CIC:
    """Create a 4-stage, M=1 CIC decimation filter. Allocates the state struct on the heap and pre-computes the normalisation right-shift (CIC_N * log2(R) bits). All integrator and comb accumulators are zeroed; the first output arrives after R input samples. Returns NULL for invalid R or OOM.

    Parameters
    ----------
    R : int, default 16
        Decimation ratio.  Must be a power of two in `[2, 4096]`. Returns NULL for R=0, non-power-of-two, or R > 4096.

    Examples
    --------
    Create with defaults:

    >>> from doppler.resample import CIC
    >>> obj = CIC(R=16)

    """
    def __init__(self, R: int = ...) -> None: ...

    def reset(self) -> None:
        """Zero all integrator and comb accumulators; preserve R and shift. The first output sample after reset arrives after R more input samples, matching post-create behaviour. Use between signal bursts to eliminate transient artefacts caused by residual pipeline state.

        Examples
        --------
        >>> from doppler.resample import CIC
        >>> cic = CIC(R=16)
        >>> cic.reset()
        >>> cic.R
        16

        """

    def reconfigure(self, R: int) -> None:
        """Change the decimation ratio in place and reset all filter state. Recomputes the normalisation shift (CIC_N * log2(R)) and zeros all accumulators so the filter behaves exactly like a freshly created one with the new R. Silently ignores R values that are not a power-of-two in `[2, 4096]` — the state is left unchanged in that case.

        Parameters
        ----------
        R : int
            New decimation ratio.  Same constraints as cic_create().

        Examples
        --------
        >>> from doppler.resample import CIC
        >>> cic = CIC(R=4)
        >>> cic.reconfigure(8)
        >>> cic.R, cic.shift
        (8, 12)

        """

    def decimate(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Decimate a block of CF32 samples through the CIC pipeline. Each sample is converted to offset-binary UQ16, pushed through CIC_N integrators (unsigned wrapping), and when the phase counter reaches R the integrated value is passed through CIC_N M=1 comb stages and converted back to CF32.  State persists between calls. Feeding blocks that are multiples of R gives predictable output counts (exactly n_in/R samples per block).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length is floor((phase + n_in) / R).

        Examples
        --------
        >>> from doppler.resample import CIC
        >>> import numpy as np
        >>> cic = CIC(R=16)
        >>> for _ in range(4):
        ...     _ = cic.decimate(np.zeros(16, dtype=np.complex64))
        >>> y = cic.decimate(np.zeros(16, dtype=np.complex64))
        >>> y.tolist(), y.dtype
        ([0j], dtype('complex64'))

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def R(self) -> int:
        """R."""

    @property
    def shift(self) -> int:
        """Shift."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "CIC": ...

    def __exit__(self, *args: object) -> None: ...

class RateConverter:
    """Create a rate converter for the given output/input rate ratio. Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase Resampler stages at construction time (see file header for the selection table). Setting compensate=1 appends a closed-form Molnar-Vucic CIC droop-compensating FIR after any CIC stage, which improves passband flatness at the cost of one extra FIR stage.

    Parameters
    ----------
    rate : float, default 1.0
        Output-to-input sample rate ratio. Any positive float.
    compensate : int, default 0
        Non-zero to append a CIC passband-droop compensating FIR after any CIC stage.

    Examples
    --------
    Create with defaults:

    >>> from doppler.resample import RateConverter
    >>> obj = RateConverter(rate=1.0, compensate=0)

    """
    def __init__(self, rate: float = ..., compensate: int = ...) -> None: ...

    def execute(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Convert a block of CF32 samples through the cascade. Passes input through each stage in order, ping-ponging between two intermediate buffers. State persists between calls, so contiguous calls on sequential blocks give the same result as one large call. Output length is approximately n_in * rate.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            CF32 output array; length is approximately n_in * rate.

        Examples
        --------
        >>> from doppler.resample import RateConverter
        >>> import numpy as np
        >>> rc = RateConverter(rate=0.5, compensate=0)
        >>> y = rc.execute(np.zeros(1024, dtype=np.complex64))
        >>> y.shape, y.dtype
        ((512,), dtype('complex64'))

        """

    def reset(self) -> None:
        """Zero all sub-stage filter memories. Rate, stage count, and stage types are preserved. Processing from a reset state produces the same output as a freshly created converter fed the same input. Use between signal bursts to suppress transient artefacts from prior filter memory.

        Examples
        --------
        >>> from doppler.resample import RateConverter
        >>> rc = RateConverter(rate=0.5, compensate=0)
        >>> rc.reset()
        >>> rc.rate
        0.5

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def rate(self) -> float:
        """Get / set the output-to-input sample rate ratio. The setter rebuilds the entire cascade (new stage selection, new sub-objects) and resets all filter memories — equivalent to destroying and recreating with the new rate. Setting rate <= 0 is silently ignored."""
    @rate.setter
    def rate(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "RateConverter": ...

    def __exit__(self, *args: object) -> None: ...

class Farrow:
    """Create a Farrow interpolator.

    Parameters
    ----------
    order : Literal["linear", "parabolic", "cubic"], default "cubic"
        0 = linear, 1 = parabolic, 2 = cubic.

    Examples
    --------
    Create with defaults:

    >>> from doppler.resample import Farrow
    >>> obj = Farrow(order="cubic")

    """
    def __init__(self, order: Literal["linear", "parabolic", "cubic"] = "cubic") -> None: ...

    def delay(self, x: NDArray[np.complex64], mu: float) -> NDArray[np.complex64]:
        """Apply a constant fractional delay of `mu` samples to a cf32 block via the Farrow interpolator; output[i] is the input interpolated at i - group_delay + mu. The first group_delay samples are filling-transient.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        mu : float
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def reset(self) -> None:
        """Clear the interpolator delay line.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def group_delay(self) -> int:
        """Group delay."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Farrow": ...

    def __exit__(self, *args: object) -> None: ...

def ciccompmf(N: int, R: int, M: int) -> NDArray[np.float64]:
    """Design a CIC passband-droop compensator FIR filter. Implements the closed-form Bernoulli-series maximally-flat-error method from Molnar & Vucic (IEEE TCAS-II 58(12):926-930, 2011, DOI 10.1109/TCSII.2011.2172522). The compensator runs at the *decimated* (output) rate and should be applied after the CIC stage. DC gain is exactly 1.0. Odd M gives symmetric linear-phase taps; even M gives half-sample-shifted linear-phase taps.

    Parameters
    ----------
    N : int
        CIC filter order (number of integrator/comb stages, >= 1).
    R : int
        CIC decimation factor (>= 2).
    M : int
        Number of compensator taps in `[1, 19]` (odd or even).

    Returns
    -------
    NDArray[np.float64]
        Output.

    Examples
    --------
    >>> from doppler.resample import ciccompmf
    >>> import numpy as np
    >>> h = ciccompmf(4, 16, 5)
    >>> h.shape, h.dtype
    ((5,), dtype('float64'))
    >>> [round(float(v), 4) for v in h]
    [0.029, -0.282, 1.5061, -0.282, 0.029]

    """

def kaiser_beta(atten: float) -> float:
    """Compute the Kaiser window beta parameter from stopband attenuation. Uses the standard Kaiser-Hamming formulae: atten > 50  dB: beta = 0.1102 * (atten - 8.7) 21 <= atten <= 50 dB: beta = 0.5842*(atten-21)^0.4 + 0.07886*(atten-21) atten < 21  dB: beta = 0.0 (rectangular window)

    Parameters
    ----------
    atten : float
        Desired stopband attenuation in dB (positive value).

    Returns
    -------
    float
        Kaiser beta parameter (>= 0.0).

    Examples
    --------
    >>> from doppler.resample import kaiser_beta
    >>> round(kaiser_beta(60.0), 4)
    5.6533
    >>> kaiser_beta(20.0)
    0.0

    """

def kaiser_num_taps(num_phases: int, atten: float, pb: float, sb: float) -> int:
    """Estimate the taps-per-phase count for a polyphase Kaiser FIR bank. Applies the Kaiser length formula to the per-phase normalised prototype (pb/num_phases, sb/num_phases), rounds up to the next odd symmetrical length, then divides by num_phases to give taps per branch. The result is the minimum num_taps argument to pass to Resampler_create_custom().

    Parameters
    ----------
    num_phases : int
        Number of polyphase branches (power of two).
    atten : float
        Desired stopband attenuation in dB.
    pb : float
        Normalised passband edge (0 < pb < sb < 1).
    sb : float
        Normalised stopband edge.

    Returns
    -------
    int
        Taps per polyphase branch (>= 1).

    Examples
    --------
    >>> from doppler.resample import kaiser_num_taps
    >>> kaiser_num_taps(4096, 60.0, 0.4, 0.6)
    19

    """
