"""Validate that every ``wfmgen --record`` output conforms to the canonical
wfmgen JSON Schema (``docs/schema/wfmgen.schema.json``).

Two complementary coverage strategies:

* **Live invocations** — run the real C binary with a distinct flag
  combination, capture ``--record`` JSON, and assert it validates.  Catches
  drift between the schema and what the serialiser actually emits across every
  waveform type, SNR mode, pulse shape, and LFSR convention.

* **Static instances** — hand-crafted JSON objects that exercise schema paths
  not reachable from a single terminating invocation (``repeat``/``continuous``
  flags, ``sum`` segments, the ``continuous`` top-level key, deliberate
  invalids).  Confirms the schema accepts or rejects these correctly.
"""

from __future__ import annotations

import json
import subprocess
from pathlib import Path
from typing import Any

import pytest

from doppler.wfm import cli

# ---------------------------------------------------------------------------
# Schema fixture
# ---------------------------------------------------------------------------

_SCHEMA_PATH = Path(__file__).parents[4] / "docs/schema/wfmgen.schema.json"


@pytest.fixture(scope="module")
def validator():
    from jsonschema import Draft202012Validator

    return Draft202012Validator(json.loads(_SCHEMA_PATH.read_text()))


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _bin() -> str:
    return cli._runnable()


def _record(tmp_path: Path, *flags: str) -> dict[str, Any]:
    """Run wfmgen with *flags* + ``--output /dev/null --record FILE``."""
    rec = tmp_path / "record.json"
    p = subprocess.run(
        [_bin(), *flags, "--output", "/dev/null", "--record", str(rec)],
        capture_output=True,
    )
    assert p.returncode == 0, p.stderr.decode()
    return json.loads(rec.read_text())


# ---------------------------------------------------------------------------
# Live-invocation cases
# One row per flag combination that produces a finite, terminating run.
# repeat/continuous are excluded here — they loop forever; covered statically.
# ---------------------------------------------------------------------------

_LIVE_CASES: list[tuple[str, list[str]]] = [
    # ── tone variants ───────────────────────────────────────────────────────
    ("tone_basic", ["--type", "tone", "--count", "64"]),
    (
        "tone_freq_fs",
        ["--type", "tone", "--freq", "1e5", "--fs", "1e6", "--count", "64"],
    ),
    ("tone_headroom", ["--type", "tone", "--count", "64", "--headroom", "6"]),
    ("tone_level", ["--type", "tone", "--count", "64", "--level", "-6"]),
    ("tone_snr", ["--type", "tone", "--snr", "20", "--count", "64"]),
    # ── noise ───────────────────────────────────────────────────────────────
    ("noise_basic", ["--type", "noise", "--count", "64"]),
    (
        "noise_snr_fs",
        [
            "--type",
            "noise",
            "--snr",
            "10",
            "--snr-mode",
            "fs",
            "--count",
            "64",
        ],
    ),
    # ── PN sequence ─────────────────────────────────────────────────────────
    ("pn_basic", ["--type", "pn", "--count", "64"]),
    (
        "pn_fibonacci",
        [
            "--type",
            "pn",
            "--lfsr",
            "fibonacci",
            "--pn-length",
            "10",
            "--count",
            "64",
        ],
    ),
    # ── BPSK ────────────────────────────────────────────────────────────────
    ("bpsk_rect", ["--type", "bpsk", "--sps", "4", "--count", "64"]),
    (
        "bpsk_rrc",
        [
            "--type",
            "bpsk",
            "--sps",
            "4",
            "--pulse",
            "rrc",
            "--rrc-beta",
            "0.35",
            "--rrc-span",
            "8",
            "--count",
            "64",
        ],
    ),
    (
        "bpsk_ebno",
        [
            "--type",
            "bpsk",
            "--sps",
            "4",
            "--snr",
            "10",
            "--snr-mode",
            "ebno",
            "--count",
            "64",
        ],
    ),
    # ── QPSK ────────────────────────────────────────────────────────────────
    ("qpsk_rect", ["--type", "qpsk", "--sps", "4", "--count", "64"]),
    (
        "qpsk_esno",
        [
            "--type",
            "qpsk",
            "--sps",
            "4",
            "--snr",
            "12",
            "--snr-mode",
            "esno",
            "--count",
            "64",
        ],
    ),
    # ── chirp ───────────────────────────────────────────────────────────────
    (
        "chirp",
        [
            "--type",
            "chirp",
            "--freq",
            "0",
            "--f-end",
            "4e5",
            "--fs",
            "1e6",
            "--count",
            "64",
        ],
    ),
    # ── bits ────────────────────────────────────────────────────────────────
    (
        "bits_bpsk",
        [
            "--type",
            "bits",
            "--bits",
            "10110010",
            "--modulation",
            "bpsk",
            "--sps",
            "4",
            "--count",
            "64",
        ],
    ),
    (
        "bits_qpsk_rrc",
        [
            "--type",
            "bits",
            "--bits",
            "10110010",
            "--modulation",
            "qpsk",
            "--sps",
            "4",
            "--pulse",
            "rrc",
            "--rrc-beta",
            "0.35",
            "--rrc-span",
            "8",
            "--count",
            "64",
        ],
    ),
    (
        "bits_none",
        [
            "--type",
            "bits",
            "--bits",
            "10110010",
            "--modulation",
            "none",
            "--sps",
            "4",
            "--count",
            "64",
        ],
    ),
]


@pytest.mark.parametrize(
    "case_id,flags",
    _LIVE_CASES,
    ids=[c[0] for c in _LIVE_CASES],
)
def test_record_validates(case_id: str, flags: list[str], validator, tmp_path):
    spec = _record(tmp_path, *flags)
    validator.validate(spec)


def test_json_template_validates(validator):
    """``wfmgen json-template`` covers tone + bits-with-rrc + sum."""
    p = subprocess.run([_bin(), "json-template"], capture_output=True)
    assert p.returncode == 0
    validator.validate(json.loads(p.stdout))


def test_from_file_record_validates(validator, tmp_path):
    """``--from-file`` round-trip exercises the ``sum`` segment path."""
    tpl = tmp_path / "tpl.json"
    rec = tmp_path / "record.json"
    subprocess.run([_bin(), "json-template", str(tpl)], check=True)
    p = subprocess.run(
        [
            _bin(),
            "--from-file",
            str(tpl),
            "--output",
            "/dev/null",
            "--record",
            str(rec),
        ],
        capture_output=True,
    )
    assert p.returncode == 0, p.stderr.decode()
    validator.validate(json.loads(rec.read_text()))


# ---------------------------------------------------------------------------
# Static schema-acceptance tests
# Hand-crafted instances for paths unreachable from a terminating invocation.
# ---------------------------------------------------------------------------

# A minimal valid inline segment reused across cases.
_SEG: dict[str, Any] = {
    "type": "tone",
    "fs": 1e6,
    "freq": 0.0,
    "snr": 100.0,
    "snr_mode": "auto",
    "seed": 1,
    "sps": 8,
    "pn_length": 7,
    "pn_poly": 0,
    "lfsr": "galois",
    "num_samples": 1024,
    "off_samples": 0,
}

_VALID_STATICS: list[tuple[str, dict[str, Any]]] = [
    (
        "repeat_true",
        {
            "version": 1,
            "repeat": True,
            "continuous": False,
            "segments": [_SEG],
        },
    ),
    (
        "continuous_true",
        {
            "version": 1,
            "repeat": False,
            "continuous": True,
            "segments": [_SEG],
        },
    ),
    ("headroom_field", {"version": 1, "headroom": 3.0, "segments": [_SEG]}),
    (
        "sum_segment",
        {
            "version": 1,
            "segments": [
                {
                    "fs": 1e6,
                    "num_samples": 1024,
                    "off_samples": 0,
                    "sum": [
                        {
                            "type": "bpsk",
                            "freq": 0.0,
                            "snr": 20.0,
                            "snr_mode": "auto",
                            "seed": 1,
                            "sps": 8,
                            "pn_length": 7,
                            "pn_poly": 0,
                            "lfsr": "galois",
                        },
                        {
                            "type": "tone",
                            "freq": 1e5,
                            "snr": 10.0,
                            "snr_mode": "auto",
                            "seed": 2,
                            "sps": 8,
                            "pn_length": 7,
                            "pn_poly": 0,
                            "lfsr": "galois",
                            "level": -10.0,
                        },
                    ],
                }
            ],
        },
    ),
    (
        "chirp_f_end",
        {
            "version": 1,
            "segments": [
                {
                    **_SEG,
                    "type": "chirp",
                    "f_end": 4e5,
                }
            ],
        },
    ),
    (
        "lfsr_fibonacci",
        {"version": 1, "segments": [{**_SEG, "lfsr": "fibonacci"}]},
    ),
    (
        "bits_with_pattern",
        {
            "version": 1,
            "segments": [
                {
                    **_SEG,
                    "type": "bits",
                    "modulation": "qpsk",
                    "pattern": "10110010",
                    "pulse": "rrc",
                    "rrc_beta": 0.35,
                    "rrc_span": 8,
                }
            ],
        },
    ),
    ("level_nonzero", {"version": 1, "segments": [{**_SEG, "level": -6.0}]}),
]

_INVALID_STATICS: list[tuple[str, dict[str, Any]]] = [
    ("missing_version", {"segments": [_SEG]}),
    ("wrong_version", {"version": 2, "segments": [_SEG]}),
    ("empty_segments", {"version": 1, "segments": []}),
    (
        "type_and_sum_conflict",
        {"version": 1, "segments": [{**_SEG, "sum": [{"type": "noise"}]}]},
    ),
    (
        "unknown_top_level_key",
        {"version": 1, "extra": True, "segments": [_SEG]},
    ),
    (
        "bad_type_enum",
        {"version": 1, "segments": [{**_SEG, "type": "sawtooth"}]},
    ),
    (
        "bad_snr_mode",
        {"version": 1, "segments": [{**_SEG, "snr_mode": "rms"}]},
    ),
    (
        "positive_level",
        {"version": 1, "segments": [{**_SEG, "level": 3.0}]},
    ),  # level must be <= 0
]


@pytest.mark.parametrize(
    "case_id,instance",
    _VALID_STATICS,
    ids=[c[0] for c in _VALID_STATICS],
)
def test_static_valid(case_id: str, instance: dict, validator):
    validator.validate(instance)


@pytest.mark.parametrize(
    "case_id,instance",
    _INVALID_STATICS,
    ids=[c[0] for c in _INVALID_STATICS],
)
def test_static_invalid(case_id: str, instance: dict, validator):
    from jsonschema import ValidationError

    with pytest.raises(ValidationError):
        validator.validate(instance)
