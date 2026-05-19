# util/util.pyi — type stubs for the util C extension.
def square_clip(y: complex, lin: float) -> complex:
    """Square-clip a complex sample: clip the real and imaginary parts independently to [-lin, lin] (a square region in the IQ plane)."""
