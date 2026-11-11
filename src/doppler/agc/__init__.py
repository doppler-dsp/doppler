"""Automatic gain control: a log-domain feedback AGC (AGC) that drives signal power toward a reference level.

It integrates the power error in dB, so convergence is exponential and
independent of the absolute input level. The loop is decimated: the detector
and integrator run once per `decim` samples with a first-order hold on the
gain between updates, so a long block costs O(n/decim) control work.

`settling_samples` answers how long it takes to settle. It runs the real
loop and counts rather than evaluating a fitted curve, so it cannot go
stale against the object; it allocates and iterates, so it is a design-time
call, never one inside a pipeline.

Examples
--------
>>> import numpy as np
>>> from doppler.agc import AGC
>>> y = AGC(ref_db=0.0, loop_bw=0.05).steps(0.1 * np.ones(200, np.complex64))
>>> bool(abs(y[-1]) > abs(y[0]))
True

A quiet input is the slow direction, and the one to budget for:

>>> from doppler.agc import settling_samples
>>> settling_samples(0.0025, 0.05, 40.0, 0.5)   # cold, 40 dB quiet
430
>>> settling_samples(0.0025, 0.05, -40.0, 0.5)  # 40 dB loud
175

"""

# agc/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .agc import AGC, settling_samples  # noqa: E402

__all__ = ["AGC", "settling_samples"]
