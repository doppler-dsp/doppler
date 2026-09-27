"""An example project's find_package(doppler) must carry a stamped floor.

doppler#1583: uno-q's bare ``find_package(doppler REQUIRED)`` accepted a
stale 0.56.0 install, which then failed at compile time as a missing
header. ``scripts/gen_doc_versions.py`` now refuses a call outside a
``doc-version`` region, and stamps the version inside one at each release.
"""

from __future__ import annotations

import importlib.util

from doppler.tests._repo import repo_root

_SPEC = importlib.util.spec_from_file_location(
    "gen_doc_versions",
    repo_root(__file__) / "scripts" / "gen_doc_versions.py",
)
assert _SPEC is not None and _SPEC.loader is not None
gen = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(gen)

REGION = (
    "    # <!-- doc-version:start -->\n"
    "    find_package(doppler 0.1.0 REQUIRED)\n"
    "    # <!-- doc-version:end -->\n"
)


def test_a_bare_call_is_refused_with_its_line():
    text = "project(x C)\nfind_package(doppler REQUIRED)\n"
    assert gen.unfloored(text) == [2]


def test_a_call_inside_a_region_passes_and_is_restamped():
    assert gen.unfloored(REGION) == []
    assert "find_package(doppler 9.8.7 REQUIRED)" in gen.render(
        REGION, "9.8.7"
    )


def test_a_region_elsewhere_does_not_cover_a_bare_call():
    assert gen.unfloored(REGION + "find_package(doppler REQUIRED)\n") == [4]


def test_every_example_project_is_scanned():
    names = {p.parent.name for p in gen.example_cmakelists()}
    assert {"uno-q", "consumer"} <= names
