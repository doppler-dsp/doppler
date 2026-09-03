"""Telemetry: lightweight in-band probes (Telemetry) that record loop internals -- error, control, lock -- to a sink for offline analysis, and the run's events (EventLog) as SigMF annotations beside them.

Examples
--------
>>> from doppler.telemetry import Telemetry
>>> t = Telemetry()
>>> t.probe('loop.err')
0"""

# telemetry/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .telemetry import Telemetry, MemoryCapture, Capture, TelemetryStats, EventLog  # noqa: E402

__all__ = ["Telemetry", "MemoryCapture", "Capture", "TelemetryStats", "EventLog"]
