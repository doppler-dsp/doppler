# detection/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .detection import marcum_q, det_threshold, det_pd, det_dwell, det_snr, det_threshold_power, det_pd_power, det_dwell_power, det_snr_power, det_threshold_noncoherent, det_pd_noncoherent, det_n_noncoh  # noqa: E402

__all__ = ["marcum_q", "det_threshold", "det_pd", "det_dwell", "det_snr", "det_threshold_power", "det_pd_power", "det_dwell_power", "det_snr_power", "det_threshold_noncoherent", "det_pd_noncoherent", "det_n_noncoh"]
