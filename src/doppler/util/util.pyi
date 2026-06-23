# util/util.pyi — type stubs for the util C extension.
def square_clip(y: complex, lin: float) -> complex:
    """Square-clip a complex sample: clip the real and imaginary parts independently to [-lin, lin] (a square region in the IQ plane).

    Parameters
    ----------
    y : complex
        Complex CF32 input sample.
    lin : float
        Per-component clip threshold (linear amplitude, >= 0). Values outside `[-lin, lin]` are clamped; values on the boundary are preserved exactly.

    Returns
    -------
    complex
        Sample with each component limited to `[-lin, lin]`.

    Examples
    --------
    >>> from doppler.util import square_clip
    >>> square_clip(0.5+0.25j, 1.0)   # within bounds, passed through
    (0.5+0.25j)
    >>> square_clip(2.0+0.5j, 1.0)    # real clipped, imag unchanged
    (1+0.5j)
    >>> square_clip(3.0-4.0j, 1.0)    # both components clipped
    (1-1j)
    >>> square_clip(0.5+0.5j, 0.25)   # smaller threshold clips both
    (0.25+0.25j)
    >>> square_clip(-2.0+0.0j, 1.0)   # negative real clipped
    (-1+0j)

    """
