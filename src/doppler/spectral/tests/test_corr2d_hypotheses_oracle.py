"""Claim 1 of ``docs/design/corr2d-hypotheses.md``: the prototype's numpy
reference is the shipped ``DelayDoppler``.

Every other claim on that page is pure numpy and runs in ``make
docs-check``. This one needs the compiled extension, which the Docs job does
not build, so it runs here, where the Python suite has it. The page's claims
table names this file as the claim's home.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path

from doppler.tests._repo import repo_root

# Walked up, not counted: the coverage job runs a copy of this package two
# levels deeper (doppler/tests/_repo.py says why).
PROTOTYPE = (
    repo_root() / "docs/design/corr2d-hypotheses-measurements/prototype.py"
)


def test_the_numpy_reference_is_the_shipped_delay_doppler() -> None:
    spec = importlib.util.spec_from_file_location("prototype", PROTOTYPE)
    assert spec is not None and spec.loader is not None
    proto = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(proto)
    assert Path(__file__).name == proto.ORACLE_TEST
    err = proto.oracle_error()
    assert err < 2e-5, f"max rel err {err:.1e} (float32 kernel, 2e-5)"
