"""`gen_doc_versions` stamps a version and leaves a file extension alone.

Its token used to take a greedy `[A-Za-z0-9.]*` suffix, which swallowed
`.tar.gz` along with the version: `.../archive/refs/tags/v0.79.0.tar.gz`
became `.../v0.79.0` on the first stamp (doppler#2856). Pages carry versioned
filenames everywhere (tarballs, `.deb`s, zips), so the token is held here.
"""

from __future__ import annotations

import sys

import pytest

from doppler.tests._repo import repo_root

sys.path.insert(0, str(repo_root(__file__) / "scripts"))
import gen_doc_versions as g


@pytest.mark.parametrize(
    ("text", "want"),
    [
        ("v0.79.0.tar.gz", "vX.tar.gz"),
        ("archive/refs/tags/v0.79.0.tar.gz", "archive/refs/tags/vX.tar.gz"),
        (
            "doppler-0.79.0-linux-x86_64.tar.gz",
            "doppler-X-linux-x86_64.tar.gz",
        ),
        ("doppler-0.79.0-windows-x86_64.zip", "doppler-X-windows-x86_64.zip"),
        ("0.79.0.zip", "X.zip"),
        (
            "libdoppler-dsp0.79_0.79.0_amd64.deb",
            "libdoppler-dsp0.79_X_amd64.deb",
        ),
        ("0.80.0rc1.tar.gz", "X.tar.gz"),
        ("0.80.0rc1", "X"),
        ("0.80.0.dev4", "X"),
        ("see v0.79.0.", "see vX."),  # a sentence-final period stays
        ("(0.79.0).", "(X)."),
        ("0.79.0.apk", "X.apk"),  # no list of extensions to forget
        ("v0.79.0 \\", "vX \\"),
        ("127.0.0.1", "127.0.0.1"),  # four parts: an address, not a version
        ("0.39.0.1", "0.39.0.1"),
    ],
)
def test_the_token_stops_at_a_file_extension(text: str, want: str) -> None:
    assert g.VERSION_TOKEN.sub("X", text) == want


def test_a_stamp_keeps_the_extension() -> None:
    page = (
        f"{g.START}\n```sh\n"
        "curl -L .../archive/refs/tags/v0.1.0.tar.gz\n```\n"
        f"{g.END}\n"
    )
    out = g.render(page, "9.8.7")
    assert "archive/refs/tags/v9.8.7.tar.gz" in out
