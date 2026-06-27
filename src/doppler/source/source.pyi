# source/source.pyi — type stubs for the source C extension.
import numpy as np
from numpy.typing import NDArray

class NCO:
    """Create an NCO instance. Allocates and initialises the phase accumulator to zero, converts norm_freq to the integer phase_inc = floor(frac(norm_freq) × 2^32), and stores nmax for scaled output.  The NCO is immediately ready to call nco_steps_u32 / nco_steps_u32_scaled / nco_steps_u32_ovf.

    Parameters
    ----------
    norm_freq : float, default 0.0
        Normalised frequency in cycles per sample. Any real value; only the fractional part matters. Negative values fold correctly (−0.25 → 3×2^30).
    nmax : int, default 0
        Wrap target for nco_steps_u32_scaled. Pass 0 to return the raw 32-bit accumulator.

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import NCO
    >>> obj = NCO(norm_freq=0.0, nmax=0)

    """
    def __init__(self, norm_freq: float = ..., nmax: int = ...) -> None: ...

    def reset(self) -> None:
        """Zero the phase accumulator. Sets phase to 0 so the next nco_steps_u32 call starts from the beginning of the cycle.  norm_freq, phase_inc, and nmax are unchanged; the NCO is ready to generate samples again immediately.

        Examples
        --------
        >>> from doppler.source import NCO
        >>> nco = NCO(0.25, 0)
        >>> _ = nco.steps_u32(2)
        >>> nco.phase
        2147483648
        >>> nco.reset()
        >>> nco.phase
        0
        >>> nco.norm_freq
        0.25

        """

    def steps_u32(self) -> NDArray[np.uint32]:
        """Advance n samples; write raw uint32 accumulator values. Each element is the phase value BEFORE the increment fires, so `out[0]` is the phase at the moment of the call.  The accumulator wraps silently at 2^32, giving the full-resolution integer ramp that the scaled and carry variants derive from.  Returns n.

        Returns
        -------
        NDArray[np.uint32]
            n (always).

        Examples
        --------
        >>> from doppler.source import NCO
        >>> nco = NCO(0.25, 0)
        >>> out = nco.steps_u32(4)
        >>> out.dtype
        dtype('uint32')
        >>> out.tolist()
        [0, 1073741824, 2147483648, 3221225472]

        """

    def steps_u32_scaled(self) -> NDArray[np.uint32]:
        """Advance n samples; values scaled to `[0, nmax)`. Uses the branchless fixed-point identity `out[i]` = (uint64_t)phase * nmax >> 32 to map the full accumulator range uniformly onto [0, nmax) without a modulo operation.  When nmax == 0 falls back to the raw accumulator (identical to nco_steps_u32).  Useful for polyphase filter bank indexing and direct LUT addressing.  Returns n.

        Returns
        -------
        NDArray[np.uint32]
            n (always).

        Examples
        --------
        >>> from doppler.source import NCO
        >>> nco = NCO(0.25, 4)
        >>> out = nco.steps_u32_scaled(4)
        >>> out.dtype
        dtype('uint32')
        >>> out.tolist()
        [0, 1, 2, 3]

        """

    def steps_u32_ovf(self) -> tuple[NDArray[np.uint32], NDArray[np.uint8]]:
        """Advance n samples; write raw phase values and per-sample carry. Identical to nco_steps_u32 for the phase array, but simultaneously fills a parallel uint8 carry buffer: `out1[i]` is 1 if the add that produced `out[i]`'s post-increment phase wrapped past 2^32, else 0. The carry marks the exact boundary of one input period and is the primitive for polyphase sample-clock and rational resampling engines. Returns n.

        Returns
        -------
        tuple[NDArray[np.uint32], NDArray[np.uint8]]
            n (always).

        Examples
        --------
        >>> from doppler.source import NCO
        >>> nco = NCO(0.5, 0)
        >>> ph, carry = nco.steps_u32_ovf(4)
        >>> ph.tolist()
        [0, 2147483648, 0, 2147483648]
        >>> carry.tolist()
        [0, 1, 0, 1]
        >>> carry.dtype
        dtype('uint8')

        """

    @property
    def norm_freq(self) -> float:
        """Normalised frequency (read/write). Setting norm_freq recomputes phase_inc = floor(frac(v) × 2^32) and takes effect on the next nco_steps_* call; phase is NOT reset."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def phase(self) -> int:
        """Current phase accumulator value (read/write). Reading returns the current integer phase in `[0, 2^32)`.  Writing overrides the accumulator directly, allowing arbitrary phase offsets without re-creating the NCO."""
    @phase.setter
    def phase(self, value: int) -> None: ...

    @property
    def phase_inc(self) -> int:
        """Per-sample phase increment (read-only). Derived from norm_freq as floor(frac(norm_freq) × 2^32).  Updated automatically whenever norm_freq is written.  A freq of 0.25 gives phase_inc = 1073741824 (0x40000000)."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "NCO": ...

    def __exit__(self, *args: object) -> None: ...

class LO:
    """Create an LO instance. Allocates state, sets phase to 0, and derives phase_inc from norm_freq.  Initialises the shared 65536-entry float LUT on the first call (single-threaded concern: call lo_create() before spawning threads that share LO instances).

    Parameters
    ----------
    norm_freq : float, default 0.0
        Normalised frequency in cycles per sample. Any real value; only the fractional part matters.

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import LO
    >>> obj = LO(norm_freq=0.0)

    """
    def __init__(self, norm_freq: float = ...) -> None: ...

    def reset(self) -> None:
        """Zero the phase accumulator. Sets phase to 0 so the next lo_steps call starts at angle 0 (1+0j). norm_freq and phase_inc are unchanged.

        Examples
        --------
        >>> from doppler.source import LO
        >>> lo = LO(0.25)
        >>> _ = lo.steps(2)
        >>> lo.phase
        2147483648
        >>> lo.reset()
        >>> lo.phase
        0
        >>> lo.norm_freq
        0.25

        """

    def steps(self) -> NDArray[np.complex64]:
        """Generate n CF32 phasors at the current norm_freq. Each sample is cos(θ) + j·sin(θ) where θ is the phase BEFORE the accumulator is advanced, giving a unit-magnitude complex sinusoid via the 65536-entry LUT.  SFDR ≈ 96 dBc.  Returns n.

        Returns
        -------
        NDArray[np.complex64]
            n (always).

        Examples
        --------
        >>> from doppler.source import LO
        >>> lo = LO(0.25)
        >>> out = lo.steps(4)
        >>> out.dtype
        dtype('complex64')
        >>> out.shape
        (4,)
        >>> [round(float(abs(c)), 4) for c in out]
        [1.0, 1.0, 1.0, 1.0]

        """

    def steps_ctrl(self, ctrl: NDArray[np.float32]) -> NDArray[np.complex64]:
        """Generate CF32 phasors with per-sample FM deviation. For each sample i, `ctrl[i]`'s fractional part is converted to a delta phase-increment (delta = floor(frac(`ctrl[i]`) × 2^32)) that is added on top of the base phase_inc for that one step only.  The base norm_freq and phase_inc are NOT modified; the deviation is transient per sample, making this the natural API for FM synthesis and frequency-hopping.  Output length equals ctrl_len.  Returns ctrl_len.

        Parameters
        ----------
        ctrl : NDArray[np.float32]
            Float32 array of per-sample normalised-frequency deviations.  Only the fractional part of each element contributes.

        Returns
        -------
        NDArray[np.complex64]
            ctrl_len (always).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.source import LO
        >>> lo = LO(0.25)
        >>> ctrl = np.zeros(4, dtype=np.float32)
        >>> out = lo.steps_ctrl(ctrl)
        >>> out.dtype
        dtype('complex64')
        >>> out.shape
        (4,)
        >>> [round(float(abs(c)), 4) for c in out]
        [1.0, 1.0, 1.0, 1.0]

        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def norm_freq(self) -> float:
        """Normalised frequency (read/write). Setting norm_freq recomputes phase_inc = floor(frac(v) × 2^32) and takes effect on the next lo_steps call; phase is NOT reset."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def phase(self) -> int:
        """Current phase accumulator value (read/write). Returns the current integer phase in `[0, 2^32)`.  Writing overrides the accumulator directly for phase-coherent frequency switching."""
    @phase.setter
    def phase(self, value: int) -> None: ...

    @property
    def phase_inc(self) -> int:
        """Per-sample phase increment (read-only). Derived from norm_freq as floor(frac(norm_freq) × 2^32).  A freq of 0.25 gives phase_inc = 1073741824 (0x40000000)."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "LO": ...

    def __exit__(self, *args: object) -> None: ...

class AWGN:
    """Create an AWGN generator. Allocates state, seeds the xoshiro256++ RNG via SplitMix64, and sets up both the scalar and the AVX2 parallel streams.  The initial seed is stored so awgn_reset() can reproduce the exact same stream.

    Parameters
    ----------
    seed : int, default 0
        64-bit RNG seed.  Two generators with different seeds produce statistically independent noise streams.
    amplitude : float, default 1.0
        Per-component (Re, Im) standard deviation.  Must be ≥ 0; total complex power = 2 × amplitude².

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import AWGN
    >>> obj = AWGN(seed=0, amplitude=1.0)

    """
    def __init__(self, seed: int = ..., amplitude: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset RNG to the seed supplied at create time. Re-runs the SplitMix64 seeding procedure with the original seed so the next awgn_generate() call produces exactly the same samples as the first call after awgn_create().  amplitude is not changed.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.source import AWGN
        >>> gen = AWGN(seed=0, amplitude=1.0)
        >>> first = gen.generate(4)
        >>> gen.reset()
        >>> second = gen.generate(4)
        >>> bool(np.all(first == second))
        True

        """

    def generate(self) -> NDArray[np.complex64]:
        """Generate n complex CF32 AWGN samples. Uses Box-Muller with xoshiro256++ to fill `out` with independent complex Gaussians: Re and Im each have zero mean and standard deviation `amplitude`.  Total complex power = 2 × amplitude². The AVX2 path processes 8 samples in parallel when available.

        Returns
        -------
        NDArray[np.complex64]
            n (always).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.source import AWGN
        >>> gen = AWGN(seed=0, amplitude=1.0)
        >>> out = gen.generate(1024)
        >>> out.dtype
        dtype('complex64')
        >>> out.shape
        (1024,)
        >>> round(float(np.var(out.real)), 1)
        1.0
        >>> round(float(np.var(out.imag)), 1)
        1.0

        """

    def reseed(self, seed: int) -> complex:
        """Reseed the RNG and reset all xoshiro256++ state. Equivalent to calling awgn_destroy() and awgn_create(seed, amplitude) but reuses the existing allocation.  amplitude is unchanged.

        Parameters
        ----------
        seed : int
            New 64-bit RNG seed.

        Returns
        -------
        complex
            Output.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.source import AWGN
        >>> gen = AWGN(seed=0, amplitude=1.0)
        >>> gen.reseed(42)
        >>> out1 = gen.generate(4)
        >>> gen2 = AWGN(seed=42, amplitude=1.0)
        >>> out2 = gen2.generate(4)
        >>> bool(np.all(out1 == out2))
        True

        """

    @property
    def amplitude(self) -> float:
        """Return the current amplitude (per-component std dev)."""
    @amplitude.setter
    def amplitude(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AWGN": ...

    def __exit__(self, *args: object) -> None: ...
