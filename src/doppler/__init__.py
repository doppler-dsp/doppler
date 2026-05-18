"""doppler package."""
from importlib.metadata import version as _version, PackageNotFoundError

try:
    __version__ = _version("doppler-dsp")
except PackageNotFoundError:
    __version__ = "unknown"
