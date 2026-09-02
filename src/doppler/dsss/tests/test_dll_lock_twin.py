"""Fast pytest twin of the `dll_lock` characterization subject.

Imports its helpers and runs one short signal at the design floor, so the
per-push suite keeps the finding behind
`docs/design/async-dsss-receiver.md` §12.4 honest: the DLL's default
20-partial detector reads "unlocked" most of the time at Es/N0 5.7 dB on a
loop that is tracking perfectly, and the symbol-period-aided detector does
not.
"""

from doppler.dsss.tests.characterization.dll_lock.characterize import (
    PARTIALS_PER_SYMBOL,
    make_signal,
    run_trial,
)

CN0 = 40.0  # dB-Hz: Es/N0 5.7 dB, the design floor
SECONDS = 0.5


def test_default_detector_reads_unlocked_while_the_loop_tracks() -> None:
    x = make_signal(CN0, 11, SECONDS)
    r = run_trial(x, CN0, aided=False, size_looks=False)
    assert abs(r["code_rate"] - 1.0) < 1e-4  # the loop is fine
    assert r["off_fraction"] > 0.5  # the flag is not


def test_symbol_aided_detector_holds_at_the_floor() -> None:
    x = make_signal(CN0, 11, SECONDS)
    r = run_trial(x, CN0, aided=True, size_looks=True)
    assert r["window"] == 6  # floor(7.24) - 1 partials per look
    assert r["n_looks"] <= 12
    assert abs(r["code_rate"] - 1.0) < 1e-4
    assert r["off_fraction"] < 0.05
    assert r["miss_per_decision"] < 0.03
    assert PARTIALS_PER_SYMBOL > 7 and PARTIALS_PER_SYMBOL < 8
