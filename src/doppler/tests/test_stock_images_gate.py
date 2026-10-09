"""The stock-image gate, exercised over seeded trees.

CI pulled stock images (debian, ubuntu, python, almalinux, fedora, nats) from
Docker Hub by bare name, anonymously, and with several PRs in flight its 429
rate limit failed the Docker, glibc and package legs of unrelated PRs
(doppler#1950). `scripts/check_stock_images.py` refuses a stock image named
any way but through the Makefile's STOCK_REGISTRY. Each case seeds a tree
and points `--root` at it; the last runs the real make target on the tree.
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
SCRIPT = REPO / "scripts" / "check_stock_images.py"
DIGEST = "sha256:" + "9" * 64


def _run(tmp_path: Path, files: dict[str, str], *args: str):
    for rel, text in files.items():
        f = tmp_path / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_text(text, encoding="utf-8")
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--root", str(tmp_path), *args],
        capture_output=True,
        text=True,
    )


# ── refused: a stock image by its Docker Hub name ────────────────────────────


@pytest.mark.parametrize(
    ("rel", "text", "said"),
    [
        ("Dockerfile", "FROM debian:bookworm\n", "FROM debian:bookworm"),
        (
            "deploy/Dockerfile.x",
            "FROM docker.io/library/debian:bookworm AS b\n",
            "docker.io/library/debian",
        ),
        # The BuildKit frontend is a Docker Hub pull too.
        (
            "ci.Dockerfile",
            "# syntax=docker/dockerfile:1\nARG STOCK_REGISTRY\n"
            "FROM ${STOCK_REGISTRY}/debian:bookworm\n",
            "syntax=docker/dockerfile:1",
        ),
        # What `FROM ${BASE}` pulls is BASE's default.
        (
            "Dockerfile",
            "ARG BASE=ubuntu:24.04\nFROM ${BASE}\n",
            "FROM ubuntu:24.04",
        ),
        ("Dockerfile", "FROM x.io/a:b\nCOPY --from=nats:2.10 /n /n\n", "nats"),
        ("scripts/a.sh", "docker run --rm debian:stable true\n", "debian"),
        (
            "scripts/a.sh",
            'image=nats:2.10\ndocker run -d --name n "$image" -js\n',
            "nats:2.10",
        ),
        # The package-smoke shape: a loop over a make list of distros.
        (
            "Makefile",
            "DISTROS ?= almalinux:8 fedora:latest\n"
            "RUN = for d in $(DISTROS); do \\\n"
            '\t    net=; docker run --rm $$net -v "$(CURDIR)":/w $$d \\\n'
            "\t        true; done\n",
            "fedora:latest",
        ),
        (
            "Makefile",
            "IMG = ubuntu:24.04\nx:\n\tdocker run --rm $(IMG) true\n",
            "ubuntu:24.04",
        ),
        (
            ".github/workflows/w.yml",
            "jobs:\n  a:\n    steps:\n      - run: docker pull alpine:3\n",
            "alpine:3",
        ),
        (
            ".github/workflows/w.yml",
            "jobs:\n  a:\n    steps:\n      - uses: docker://alpine:3\n",
            "docker://alpine:3",
        ),
    ],
)
def test_a_docker_hub_stock_image_is_named(
    tmp_path: Path, rel: str, text: str, said: str
) -> None:
    r = _run(tmp_path, {rel: text})
    assert r.returncode == 1, r.stdout
    assert f"{rel}:" in r.stdout
    assert said in r.stdout


# ── fine: through the registry, another registry, our own, or a stage ───────


@pytest.mark.parametrize(
    ("rel", "text"),
    [
        (
            "Dockerfile",
            "ARG STOCK_REGISTRY\nFROM ${STOCK_REGISTRY}/debian:bookworm AS b\n"
            "FROM b\nCOPY --from=b /x /x\nCOPY --from=0 /y /y\n",
        ),
        (
            "Dockerfile",
            "ARG STOCK_REGISTRY\nARG BASE=${STOCK_REGISTRY}/ubuntu:24.04\n"
            "FROM ${BASE}\n",
        ),
        ("Dockerfile", f"FROM ghcr.io/x/ci@{DIGEST}\n"),
        ("Dockerfile", "FROM quay.io/pypa/manylinux_2_28_x86_64\n"),
        # The repo's own image, named for DOCKER_IMAGE (--own-prefix).
        ("Dockerfile", "ARG G=doppler-glibc228:dev\nFROM ${G}\n"),
        # No default: the build-arg is the caller's to pass.
        ("Dockerfile", "ARG BASE\nFROM ${BASE}\n"),
        (
            "scripts/a.sh",
            'image="${STOCK_REGISTRY:?use make}/nats:2.10"\n'
            'docker run -d "$image" -js\n',
        ),
        (
            "Makefile",
            "STOCK_REGISTRY ?= public.ecr.aws/docker/library\n"
            "DISTROS ?= $(STOCK_REGISTRY)/fedora:latest $(CI_IMAGE_2404)\n"
            "RUN = for d in $(DISTROS); do docker run --rm $$d true; done\n"
            "OWN = $(DOCKER_IMAGE)-pkg:local\nDOCKER_IMAGE ?= doppler\n"
            "x:\n\tdocker run --rm $(call CHECKOUT,b) $(OWN) make\n",
        ),
        # An argument nothing here resolves is opaque, not a finding.
        ("scripts/a.sh", 'docker run --rm "${1}" cat /etc/os-release\n'),
        ("scripts/a.sh", "# docker run --rm debian:stable -- a comment\n"),
    ],
)
def test_a_registry_own_or_stage_reference_passes(
    tmp_path: Path, rel: str, text: str
) -> None:
    # A baseline reference, so a case that holds none (an opaque argument, a
    # comment) is judged on its own shape, not refused as an empty scan.
    base = {"base/Dockerfile": f"FROM ghcr.io/x/ci@{DIGEST}\n"}
    r = _run(tmp_path, {**base, rel: text})
    assert r.returncode == 0, r.stdout


def test_a_vendored_file_is_listed_not_failed(tmp_path: Path) -> None:
    """standard.mk's VENDORED_FILES are canonical's to fix."""
    files = {
        "docker/ci.Dockerfile": "# syntax=docker/dockerfile:1\nFROM a.io/b\n",
    }
    r = _run(tmp_path, files, "--vendored", "docker/ci.Dockerfile x.mk")
    assert r.returncode == 0, r.stdout
    assert "[vendored] docker/ci.Dockerfile:1" in r.stdout


def test_a_scan_that_finds_no_image_has_not_passed(tmp_path: Path) -> None:
    r = _run(tmp_path, {"scripts/a.sh": "echo hello\n"})
    assert r.returncode == 1, r.stdout
    assert "nothing checked" in r.stdout


def test_the_live_tree_passes_through_make() -> None:
    """The real target, so the vendored list is the Makefile's own."""
    r = subprocess.run(
        ["make", "-s", "-C", str(REPO), "lint-stock-images"],
        capture_output=True,
        text=True,
    )
    assert r.returncode == 0, r.stdout + r.stderr
    assert "check_stock_images: OK" in r.stdout
