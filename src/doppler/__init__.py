"""doppler package."""
from importlib.metadata import version as _version, PackageNotFoundError

try:
    __version__ = _version("doppler")
except PackageNotFoundError:
    __version__ = "unknown"
