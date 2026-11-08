import numpy as np
import pytest

from doppler.correlation import Correlator


def test_create():
    obj = Correlator(np.zeros(1, dtype=np.complex64), dwell=1, nthreads=1)
    assert obj is not None


def test_getter_setter():
    pass  # no auto-state; add assertions for your fields


def test_reset():
    pass  # no auto-state; add assertions for your reset


def test_context_manager():
    with Correlator(np.zeros(1, dtype=np.complex64), dwell=1, nthreads=1):
        pass


def test_destroy():
    obj = Correlator(np.zeros(1, dtype=np.complex64), dwell=1, nthreads=1)
    obj.destroy()


def test_dwell_zero_is_refused_as_value_error():
    """dwell=0 never dumps, so create refuses it as ValueError (#2641),
    with the sentence that names dwell (docs/design/correlator-python.md
    §17)."""
    ref = np.zeros(8, np.complex64)
    ref[0] = 1.0
    with pytest.raises(
        ValueError,
        match="dwell must be at least 1",
    ):
        Correlator(ref=ref, dwell=0)


def test_empty_ref_is_refused_as_value_error():
    """An empty reference is refused as ValueError, never a crash.

    The one correlator once built its row plans before checking the
    length, and a length-0 FFT plan aborted the process (#2886 review).
    The check runs in a child interpreter so that a regression fails this
    test instead of killing the whole pytest run.
    """
    import subprocess
    import sys

    code = (
        "import numpy as np\n"
        "from doppler.correlation import Correlator\n"
        "try:\n"
        "    Correlator(np.zeros(0, np.complex64), dwell=1, nthreads=1)\n"
        "except ValueError:\n"
        "    print('refused')\n"
    )
    run = subprocess.run(
        [sys.executable, "-c", code], capture_output=True, text=True
    )
    assert run.returncode == 0, run.stderr
    assert run.stdout.strip() == "refused"
