# source/source.pyi — type stubs for the source C extension.
import numpy as np
from numpy.typing import NDArray

class NCO:
    """NCO component.

    Parameters
    ----------
    norm_freq : float, default 0.0
        norm_freq constructor parameter.
    nmax : int, default 0
        nmax constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import NCO
    >>> obj = NCO(0.0, 0)

    """
    def __init__(self, norm_freq: float = ..., nmax: int = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def steps_u32(self) -> NDArray[np.uint32]:
        """Steps u32."""

    def steps_u32_scaled(self) -> NDArray[np.uint32]:
        """Steps u32 scaled."""

    def steps_u32_ovf(self) -> tuple[NDArray[np.uint32], NDArray[np.uint8]]:
        """Steps u32 ovf."""

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def phase(self) -> int:
        """Phase."""
    @phase.setter
    def phase(self, value: int) -> None: ...

    @property
    def phase_inc(self) -> int:
        """Phase inc."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "NCO": ...

    def __exit__(self, *args: object) -> None: ...

class LO:
    """LO component.

    Parameters
    ----------
    norm_freq : float, default 0.0
        norm_freq constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.source import LO
    >>> obj = LO(0.0)

    """
    def __init__(self, norm_freq: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset state to post-create defaults."""

    def steps(self) -> NDArray[np.complex64]:
        """Steps."""

    def steps_ctrl(self, ctrl: NDArray[np.float32]) -> NDArray[np.complex64]:
        """Steps ctrl."""

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def phase(self) -> int:
        """Phase."""
    @phase.setter
    def phase(self, value: int) -> None: ...

    @property
    def phase_inc(self) -> int:
        """Phase inc."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "LO": ...

    def __exit__(self, *args: object) -> None: ...

class AWGN:
    """Additive White Gaussian Noise generator.

    Generates complex CF32 noise where real and imaginary parts are
    independent zero-mean Gaussians, each with standard deviation
    ``amplitude``.  Total complex power = 2 × amplitude².

    The implementation uses xoshiro256++ for random number generation and
    the Box-Muller transform to map uniform samples to Gaussians.  Phase
    is drawn from a 2¹⁶-entry sin/cos LUT (~96 dBc SFDR), giving a flat
    noise floor suitable for any SNR of practical interest.

    An AVX-512 fast path runs 8 independent xoshiro256++ streams in
    parallel via 512-bit SIMD and uses glibc libmvec ``_ZGVdN8v_logf``
    for 8-wide vectorised ``log``.  The scalar fallback is used on CPUs
    without AVX-512.  Path selection is automatic at runtime.

    Parameters
    ----------
    seed : int, optional
        64-bit RNG seed.  Two generators with different seeds produce
        uncorrelated noise streams.  Default 0.
    amplitude : float, optional
        Per-component (Re, Im) standard deviation.  Must be ≥ 0.
        Default 1.0.

    Examples
    --------
    Basic generation:

    >>> from doppler.source import AWGN
    >>> import numpy as np
    >>> g = AWGN(seed=42, amplitude=1.0)
    >>> noise = g.generate(65536)
    >>> noise.dtype
    dtype('complex64')
    >>> abs(np.std(np.real(noise)) - 1.0) < 0.05
    True

    Reproducible output — same seed, same stream:

    >>> a = AWGN(seed=7); b = AWGN(seed=7)
    >>> np.array_equal(a.generate(256), b.generate(256))
    True

    Amplitude retune (RNG state unaffected):

    >>> g = AWGN(seed=0, amplitude=0.5)
    >>> abs(np.std(np.real(g.generate(65536))) - 0.5) < 0.02
    True
    """
    def __init__(self, seed: int = ..., amplitude: float = ...) -> None: ...

    def reset(self) -> None:
        """Reset the RNG to the seed supplied at construction time.

        Filter state (none for AWGN) and RNG state are both reset.
        Successive calls to ``generate()`` after ``reset()`` return the
        same sequence as after construction.

        Examples
        --------
        >>> from doppler.source import AWGN
        >>> import numpy as np
        >>> g = AWGN(seed=1)
        >>> first = g.generate(256)
        >>> g.reset()
        >>> np.array_equal(first, g.generate(256))
        True
        """

    def generate(self, n: int = ...) -> NDArray[np.complex64]:
        """Generate *n* complex CF32 AWGN samples.

        Each call advances the RNG by 2*n 64-bit words (scalar path) or
        the equivalent number of vectorised steps (AVX-512 path).
        Successive calls produce a continuous, non-repeating noise stream
        until ``reset()`` or ``reseed()`` is called.

        Parameters
        ----------
        n : int
            Number of complex output samples.  Must be ≥ 1.

        Returns
        -------
        NDArray[np.complex64]
            Shape ``(n,)``, dtype ``complex64``.  Real and imaginary parts
            are independent N(0, amplitude²) random variables.

        Examples
        --------
        >>> from doppler.source import AWGN
        >>> g = AWGN(seed=0, amplitude=1.0)
        >>> y = g.generate(4096)
        >>> y.shape
        (4096,)
        >>> y.dtype
        dtype('complex64')
        """

    def reseed(self, seed: int) -> None:
        """Replace the RNG seed and reset state.

        Equivalent to destroying the generator and creating a new one
        with ``AWGN(seed=seed, amplitude=self.amplitude)``.  The
        ``amplitude`` property is unchanged.

        Parameters
        ----------
        seed : int
            New 64-bit RNG seed.

        Examples
        --------
        >>> from doppler.source import AWGN
        >>> import numpy as np
        >>> g = AWGN(seed=1)
        >>> old = g.generate(64)
        >>> g.reseed(999)
        >>> not np.array_equal(old, g.generate(64))
        True
        """

    @property
    def amplitude(self) -> float:
        """Per-component (Re, Im) standard deviation.

        Changing this does not reset or disturb the RNG state — only the
        scale applied inside ``generate()`` changes from the next call
        onward.

        Returns
        -------
        float
            Current amplitude (≥ 0).
        """
    @amplitude.setter
    def amplitude(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AWGN": ...

    def __exit__(self, *args: object) -> None: ...
