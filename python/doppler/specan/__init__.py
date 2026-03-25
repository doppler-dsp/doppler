"""
doppler.specan — live spectrum analyzer.

Launch from the command line:

    uvx --from doppler-dsp doppler-specan

Or if you have the package installed:

    doppler-specan --port 8765 --fft-size 2048
"""

from .server import app, main  # noqa: F401

__all__ = ["app", "main"]
