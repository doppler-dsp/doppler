# detection/detection.pyi — type stubs for the detection C extension.
def marcum_q(m: int, a: float, b: float) -> float:
    """Marcum q."""

def det_threshold(pfa: float) -> float:
    """Det threshold."""

def det_pd(snr: float, dwell: int, threshold: float) -> float:
    """Det pd."""

def det_dwell(snr: float, pd_min: float, pfa: float, max_dwell: int) -> int:
    """Det dwell."""

def det_snr(dwell: int, pd_min: float, pfa: float) -> float:
    """Det snr."""

def det_threshold_power(pfa: float) -> float:
    """Det threshold power."""

def det_pd_power(snr_power: float, dwell: int, power_threshold: float) -> float:
    """Det pd power."""

def det_dwell_power(snr_power: float, pd_min: float, pfa: float, max_dwell: int) -> int:
    """Det dwell power."""

def det_snr_power(dwell: int, pd_min: float, pfa: float) -> float:
    """Det snr power."""
