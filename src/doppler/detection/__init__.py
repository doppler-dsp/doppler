"""Detection primitives: a portable lock detector (LockDet) applying level and time hysteresis to any scalar lock metric.

Examples
--------
>>> from doppler.detection import LockDet
>>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
>>> [d.step(2.0), d.step(2.0)]
[0, 1]"""

# detection/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .detection import marcum_q, det_threshold, det_pd, det_dwell, det_snr, det_threshold_power, det_pd_power, det_dwell_power, det_snr_power, det_threshold_noncoherent, det_pd_noncoherent, det_n_noncoh, det_ema_alpha, LockDet, det_verify_count, det_verify_delay, det_threshold_f  # noqa: E402

__all__ = ["marcum_q", "det_threshold", "det_pd", "det_dwell", "det_snr", "det_threshold_power", "det_pd_power", "det_dwell_power", "det_snr_power", "det_threshold_noncoherent", "det_pd_noncoherent", "det_n_noncoh", "det_ema_alpha", "LockDet", "det_verify_count", "det_verify_delay", "det_threshold_f"]
