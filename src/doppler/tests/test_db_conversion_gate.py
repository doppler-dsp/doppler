"""The one-dB-converter gate, exercised over a seeded tree.

`scripts/check_db_conversion_sites.py` (`make lint-db-conversion`) holds a dB
spectrum to one converter, `dp_power_to_db_f32` (#2094, #2108). Each case
writes a throwaway `native/src` holding the three allowed sites, adds one
change, and runs the gate over it with ``--root``.
"""

from __future__ import annotations

import subprocess
import sys
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "check_db_conversion_sites.py"

#: One own-log10 statement in each file the gate allows one in.
ALLOWED = {
    "native/src/psd/psd_core.c": (
        "void f (float *out, const double *lp, size_t nb) {\n"
        "  for (size_t b = 0; b < nb; b++)\n"
        "    out[b] = (float)(10.0 * log10 (lp[b]));\n}\n"
    ),
    "native/src/spectral/magnitude_db_cf32.c": (
        "void g (float *out, const float *m, size_t n) {\n"
        "  for (size_t k = 0; k < n; k++)\n"
        "    out[k] = 20.0f * log10f (m[k]);\n}\n"
    ),
    "native/src/spectral/magnitude_db_cf64.c": (
        "void h (float *out, const double *m, size_t n) {\n"
        "  for (size_t k = 0; k < n; k++)\n"
        "    out[k] = (float)(20.0 * log10 (m[k]));\n}\n"
    ),
}


def _seed(root: Path, extra: dict[str, str]) -> None:
    for rel, text in {**ALLOWED, **extra}.items():
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    (root / "native" / "inc").mkdir(parents=True, exist_ok=True)


def _gate(root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(root)],
        capture_output=True,
        text=True,
        check=False,
    )


def test_the_tree_holds() -> None:
    r = subprocess.run(
        [sys.executable, str(SCRIPT)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stdout


def test_the_allowed_sites_alone_pass(tmp_path: Path) -> None:
    _seed(tmp_path, {})
    r = _gate(tmp_path)
    assert r.returncode == 0, r.stdout


@pytest.mark.parametrize(
    "text",
    [
        # one line, as the measurement spectra had it
        "void s (float *o, const float *p, size_t n) {\n"
        "  for (size_t i = 0; i < n; i++) o[i] = 10.0f * log10f (p[i]);\n}\n",
        # wrapped over three lines after its loop header, as specan had it
        "void s (float *out, const float *p, size_t n) {\n"
        "  for (size_t i = 0; i < n; i++)\n"
        "    out[i] = 10.0f\n"
        "             * log10f (p[i] + 1e-20f)\n"
        "             + 3.0f;\n}\n",
    ],
    ids=["one-line", "wrapped"],
)
def test_a_new_converter_fails(tmp_path: Path, text: str) -> None:
    _seed(tmp_path, {"native/src/meas/meas_core.c": text})
    r = _gate(tmp_path)
    assert r.returncode == 1, r.stdout
    assert "native/src/meas/meas_core.c" in r.stdout


@pytest.mark.parametrize(
    "text",
    [
        # a scalar measurement keeps double, by rule
        "double snr (double a, double b) {\n"
        "  double r = 10.0 * log10 (a / b);\n  return r;\n}\n",
        # prose about a conversion is not one
        "/* out[i] = 10 * log10 (p[i]) was the old way */\nint x;\n",
        "// out[i] = 10 * log10 (p[i]);\nint y;\n",
        # an indexed READ inside a scalar's log10 is not an assignment
        "double peak (const float *p, size_t k) {\n"
        "  return 10.0 * log10 (p[k]);\n}\n",
    ],
    ids=["scalar", "block-comment", "line-comment", "indexed-read"],
)
def test_out_of_scope_passes(tmp_path: Path, text: str) -> None:
    _seed(tmp_path, {"native/src/meas/meas_core.c": text})
    r = _gate(tmp_path)
    assert r.returncode == 0, r.stdout


def test_jm_glue_is_not_scanned(tmp_path: Path) -> None:
    glue = "void e (float *o, float *p) { o[0] = log10f (p[0]); }\n"
    _seed(tmp_path, {"native/src/meas/meas_ext_meas.c": glue})
    assert _gate(tmp_path).returncode == 0


def test_an_allowed_file_may_not_grow(tmp_path: Path) -> None:
    rel = "native/src/psd/psd_core.c"
    grown = ALLOWED[rel] + (
        "void s (float *o, const float *p, size_t n) {\n"
        "  for (size_t i = 0; i < n; i++) o[i] = 10.0f * log10f (p[i]);\n}\n"
    )
    _seed(tmp_path, {rel: grown})
    r = _gate(tmp_path)
    assert r.returncode == 1, r.stdout
    assert rel in r.stdout


def test_a_converted_site_lowers_the_allowance(tmp_path: Path) -> None:
    rel = "native/src/spectral/magnitude_db_cf32.c"
    _seed(tmp_path, {rel: "int converted;\n"})
    r = _gate(tmp_path)
    assert r.returncode == 1, r.stdout
    assert "lower its allowance" in r.stdout
