"""The CI-image reference gate, driven over seeded workflow YAML.

``scripts/ci_image_refs_check.py`` takes a workflow directory and a pin file
as arguments so this file can build tiny workflows in ``tmp_path`` rather than
asserting against the repo's real ``ci.yml`` — which would make every test
here fail the day someone legitimately adds a job.

The distinction under test is the one the previous gate got wrong. That scan
**skipped** ``${{ … }}`` refs, reasoning that a matrix ``image:`` line
elsewhere carried the literal it resolved to. Once the literals were removed
in favour of ``needs`` outputs (doppler#1215), that reasoning was void: it
would have walked six expressions, checked none of them, and printed OK. So
these tests care most about the cases where an expression is *wrong* — an
undeclared `needs`, a producer that never reads the pin file — because those
are exactly what a skipping scan waves through.
"""

from __future__ import annotations

import subprocess
import sys
import textwrap
from typing import TYPE_CHECKING

import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from pathlib import Path

REPO = repo_root(__file__)
SCRIPT = REPO / "scripts" / "ci_image_refs_check.py"

D2204 = "ghcr.io/x/ci@sha256:" + "a" * 64
D2404 = "ghcr.io/x/ci@sha256:" + "b" * 64

PIN = f"""\
CI_IMAGE_2204={D2204}
CI_IMAGE_2404={D2404}
CI_IMAGE_FINGERPRINT_2204={"c" * 64}
CI_IMAGE_SOURCE_HASH={"d" * 64}
"""

#: A `pin` job that genuinely reads the pin file, as the real one does.
PIN_JOB = """\
  pin:
    runs-on: ubuntu-latest
    outputs:
      image_2404: ${{ steps.read.outputs.image_2404 }}
    steps:
    - uses: actions/checkout@v7
    - id: read
      run: |
        . .github/ci-images.env
        echo "image_2404=$CI_IMAGE_2404" >> "$GITHUB_OUTPUT"
"""


def _with_pin(consumer: str) -> str:
    """Full workflow: the real-shaped ``pin`` job plus one consumer job.

    Composed rather than interpolated into one literal: ``PIN_JOB`` is already
    written at job indentation, and running it through ``dedent`` with a block
    indented for readability yields YAML that parses to no jobs at all — which
    the gate then correctly reports as zero references.
    """
    return "name: t\njobs:\n" + PIN_JOB + consumer


def _run(tmp_path: Path, workflow: str, pin: str = PIN):
    """Run the gate over one seeded workflow directory."""
    wf = tmp_path / "workflows"
    wf.mkdir(exist_ok=True)
    (wf / "ci.yml").write_text(textwrap.dedent(workflow))
    pin_file = tmp_path / "ci-images.env"
    pin_file.write_text(pin)
    return subprocess.run(
        [sys.executable, str(SCRIPT), str(wf), str(pin_file)],
        capture_output=True,
        text=True,
        cwd=REPO,
    )


def test_needs_output_from_a_real_pin_job_passes(tmp_path: Path) -> None:
    """The shape the repo actually uses."""
    r = _run(
        tmp_path,
        _with_pin(
            "  build:\n"
            "    needs: pin\n"
            "    runs-on: ubuntu-latest\n"
            "    container:\n"
            "      image: ${{ needs.pin.outputs.image_2404 }}\n"
            '    steps:\n    - run: "true"\n'
        ),
    )
    assert r.returncode == 0, r.stdout + r.stderr
    assert "OK" in r.stdout


def test_a_pinned_literal_still_passes(tmp_path: Path) -> None:
    """Literals remain legal — the rule is 'traces to the pin file'."""
    r = _run(
        tmp_path,
        f"""\
        name: t
        jobs:
          build:
            runs-on: ubuntu-latest
            container:
              image: {D2404}
            steps:
            - run: "true"
        """,
    )
    assert r.returncode == 0, r.stdout + r.stderr


def test_a_mutable_tag_fails(tmp_path: Path) -> None:
    """A tag is mutable, so the image a PR passed on need not be the one it
    merges with."""
    r = _run(
        tmp_path,
        """\
        name: t
        jobs:
          build:
            runs-on: ubuntu-latest
            container:
              image: ubuntu:24.04
            steps:
            - run: "true"
        """,
    )
    assert r.returncode == 1, r.stdout
    assert "mutable tag" in r.stdout


def test_an_unpinned_digest_fails(tmp_path: Path) -> None:
    """Pinned, but not to one of OUR digests."""
    r = _run(
        tmp_path,
        f"""\
        name: t
        jobs:
          build:
            runs-on: ubuntu-latest
            container:
              image: ghcr.io/x/ci@sha256:{"9" * 64}
            steps:
            - run: "true"
        """,
    )
    assert r.returncode == 1, r.stdout
    assert "not one of the digests" in r.stdout


def test_needs_not_declared_fails(tmp_path: Path) -> None:
    """THE case the old skipping scan waved through.

    Without ``needs: pin`` the expression resolves to the empty string at
    runtime and the job fails far from here, with a message about an invalid
    image rather than about this mistake.
    """
    r = _run(
        tmp_path,
        _with_pin(
            "  build:\n"
            "    runs-on: ubuntu-latest\n"
            "    container:\n"
            "      image: ${{ needs.pin.outputs.image_2404 }}\n"
            '    steps:\n    - run: "true"\n'
        ),
    )
    assert r.returncode == 1, r.stdout
    assert "not in this job's needs" in r.stdout


def test_producer_that_never_reads_the_pin_fails(tmp_path: Path) -> None:
    """A job named ``pin`` is not the same as a job that reads the pin.

    This is the check that keeps the indirection honest: without it, someone
    could hardcode a digest in the producer and the gate would approve of the
    consumer pointing at it — reintroducing the second home this whole change
    exists to remove.
    """
    r = _run(
        tmp_path,
        f"""\
        name: t
        jobs:
          pin:
            runs-on: ubuntu-latest
            outputs:
              image_2404: ${{{{ steps.read.outputs.image_2404 }}}}
            steps:
            - id: read
              run: echo "image_2404={D2404}" >> "$GITHUB_OUTPUT"
          build:
            needs: pin
            runs-on: ubuntu-latest
            container:
              image: ${{{{ needs.pin.outputs.image_2404 }}}}
            steps:
            - run: "true"
        """,
    )
    assert r.returncode == 1, r.stdout
    assert "never reads" in r.stdout


def test_undeclared_output_fails(tmp_path: Path) -> None:
    """The producer reads the pin but does not publish the name asked for."""
    r = _run(
        tmp_path,
        _with_pin(
            "  build:\n"
            "    needs: pin\n"
            "    runs-on: ubuntu-latest\n"
            "    container:\n"
            "      image: ${{ needs.pin.outputs.image_2204 }}\n"
            '    steps:\n    - run: "true"\n'
        ),
    )
    assert r.returncode == 1, r.stdout
    assert "declares no output" in r.stdout


def test_matrix_include_is_resolved_not_skipped(tmp_path: Path) -> None:
    """``${{ matrix.image }}`` must be followed into the include entries.

    A bad digest hidden in an include entry is precisely what the old scan
    caught only by accident — and would have stopped catching.
    """
    r = _run(
        tmp_path,
        f"""\
        name: t
        jobs:
          build:
            runs-on: ubuntu-latest
            strategy:
              matrix:
                include:
                  - os: a
                    image: {D2404}
                  - os: b
                    image: ghcr.io/x/ci@sha256:{"9" * 64}
            container:
              image: ${{{{ matrix.image }}}}
            steps:
            - run: "true"
        """,
    )
    assert r.returncode == 1, r.stdout
    assert "matrix.include" in r.stdout


def test_an_unresolvable_expression_fails(tmp_path: Path) -> None:
    """Refuse to guess, rather than wave it through."""
    r = _run(
        tmp_path,
        """\
        name: t
        jobs:
          build:
            runs-on: ubuntu-latest
            container:
              image: ${{ env.SOMETHING }}
            steps:
            - run: "true"
        """,
    )
    assert r.returncode == 1, r.stdout
    assert "refuses to guess" in r.stdout


def test_zero_references_is_a_failure(tmp_path: Path) -> None:
    """A scan that matches nothing has not passed — it has not run.

    This is the exact way the old scan would have died silently once the
    literals were replaced by expressions it skipped.
    """
    r = _run(
        tmp_path,
        """\
        name: t
        jobs:
          build:
            runs-on: ubuntu-latest
            steps:
            - run: "true"
        """,
    )
    assert r.returncode == 1, r.stdout
    assert "has not passed" in r.stdout


def test_services_images_are_checked_too(tmp_path: Path) -> None:
    """A service container runs our code's dependencies; it is a ref too."""
    r = _run(
        tmp_path,
        """\
        name: t
        jobs:
          build:
            runs-on: ubuntu-latest
            services:
              broker:
                image: nats:latest
            steps:
            - run: "true"
        """,
    )
    assert r.returncode == 1, r.stdout
    assert "services.broker.image" in r.stdout


@pytest.mark.parametrize("missing", ["absent", "empty"])
def test_a_pin_file_that_says_nothing_fails(
    tmp_path: Path, missing: str
) -> None:
    """An unreadable pin must not read as 'nothing to check'."""
    body = "" if missing == "empty" else "# no rows\n"
    r = _run(
        tmp_path,
        """\
        name: t
        jobs:
          build:
            runs-on: ubuntu-latest
            container:
              image: ubuntu:24.04
            steps:
            - run: "true"
        """,
        pin=body,
    )
    assert r.returncode == 1, r.stdout
    assert "no CI_IMAGE_" in r.stdout
