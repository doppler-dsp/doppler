"""Tests for doppler.__version__."""

import doppler


def test_version_is_string():
    assert isinstance(doppler.__version__, str)


def test_version_not_unknown():
    """Package must be installed (editable or otherwise) for version to
    resolve."""
    assert doppler.__version__ != "unknown", (
        "doppler.__version__ is 'unknown'"
        " — run `pip install -e .` or `uv sync` first"
    )


def test_version_format():
    parts = doppler.__version__.split(".")
    assert len(parts) >= 2, (
        f"Expected X.Y[.Z] version, got {doppler.__version__!r}"
    )
    assert parts[0].isdigit(), (
        f"Major version not numeric: {doppler.__version__!r}"
    )
