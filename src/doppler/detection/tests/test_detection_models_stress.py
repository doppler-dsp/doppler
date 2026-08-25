"""Fast pytest twin of the `models` characterization subject.

Imports the subject's own helpers and re-runs a handful of cells at a much
smaller draw count, so the per-push suite keeps the sweep honest — it proves
the helpers still import, still run, and still agree with the models at the
coarse end — without paying for the tails.

**Be clear about what this does not buy.** The full sweep resolves a 1e-4
false-alarm rate; this twin cannot see past about 1e-2, so a model that
drifted only in its tail would survive here and wait for the next
`make characterize`. That window is the category's stated trade
(`src/doppler/dsss/tests/characterization/__init__.py`), not an oversight.
"""

from __future__ import annotations

import pytest

from doppler.detection import (
    det_pd,
    det_threshold,
    det_threshold_f,
    det_threshold_noncoherent,
)

# The full dotted path is what `scripts/check_characterization.py` looks
# for to prove this file is the subject's fast twin. A
# `from ...characterization.models import characterize` binds the
# PACKAGE name and the gate would not see it, so the import below names
# the module itself and pulls one symbol out of it — which also gives
# ruff a used import rather than a bare module reference it flags.
from doppler.detection.tests.characterization.models import (
    characterize as cz,
)
from doppler.detection.tests.characterization.models.characterize import (
    N_DRAWS,
)

#: Coarse enough to run in well under a second per cell, and only ever used
#: against pfa values a draw count this size can actually resolve.
DRAWS = 40_000
assert DRAWS < N_DRAWS  # the twin must be the CHEAP one


@pytest.fixture(autouse=True)
def _small_draws(monkeypatch):
    """Every helper reads N_DRAWS from the module, so one patch covers all."""
    monkeypatch.setattr(cz, "N_DRAWS", DRAWS)


@pytest.mark.parametrize("pfa", [1e-2, 3e-2])
def test_h0_families_deliver_the_priced_rate(pfa):
    """Each family's threshold false-alarms at the rate it was sold at.

    Five sigma on the measured frequency, so a correct model cannot flake and
    a family wired to the wrong law cannot pass: the envelope and Gaussian
    thresholds differ by more than 10 % at these pfa values, which is many
    standard errors at this draw count.
    """
    for name, (hit, se) in {
        "envelope": cz.h0_envelope(pfa, 1),
        "power": cz.h0_power(pfa, 2),
        "gauss": cz.h0_gauss(pfa, 3),
        "noncoh4": cz.h0_noncoherent(pfa, 4, 4),
    }.items():
        assert abs(hit - pfa) < 5.0 * se + 1e-9, (
            f"{name}: {hit:.4e} vs {pfa:.4e}"
        )


def test_h1_pd_matches_the_model():
    """The coherent Pd curve is a frequency, not just a formula."""
    pfa = 1e-2
    eta = det_threshold(pfa)
    for dwell, snr in ((1, 1.0), (4, 0.5), (16, 0.25)):
        model = det_pd(snr, dwell, eta)
        hit, se = cz.h1_envelope(snr, dwell, pfa, 10 + dwell)
        assert abs(hit - model) < 5.0 * se + 1e-9, (
            f"dwell={dwell} snr={snr}: measured {hit:.4f} vs model {model:.4f}"
        )


def test_the_chi_square_gate_really_is_the_looser_one():
    """The §4 penalty, at the coarse end: the F gate is stricter, and the
    chi-square gate lets through materially more than it priced.

    Asserted as an inequality rather than a ratio — the 41x itself is pinned
    exactly in `native/tests/test_detection_core.c`, where no draw count is
    involved and it cannot be a statistical accident.
    """
    pfa = 3e-2
    n = 16
    hit, se, chi = cz.h0_fratio(pfa, n, 99)
    assert abs(hit - pfa) < 5.0 * se + 1e-9  # the F gate is priced right
    assert chi > 2.0 * hit  # the chi-square gate is not
    assert (
        det_threshold_f(pfa, n) * n
        > det_threshold_noncoherent(pfa, n // 2) ** 2
    )


def test_envelope_and_power_are_the_same_detector():
    """Exact, not approximate — no draws involved."""
    lines = cz.sweep_equivalence()
    worst = float(lines[-1].rsplit(":", 1)[1])
    assert worst < 1e-12, lines[-1]
