"""doppler package."""

from importlib.metadata import PackageNotFoundError
from importlib.metadata import version as _version

try:
    __version__ = _version("doppler-dsp")
except PackageNotFoundError:
    __version__ = "unknown"

__all__ = []
