"""Where doppler's C headers live: derived once, read by every gate.

jm's schema 8 (doppler#1546) moved a project's headers from ``native/inc/``
to ``native/inc/<pkg>/``, and sixteen scripts here each spelled the old path
for themselves -- a sanctioned-home constant, a directory scan, a message.
The move broke them together, some loudly and some only once a diff touched
the file they watch. So the layout is answered here, from the manifest, the
same way jm answers it:

- ``INC_DIR`` is the ``-I`` directory (``native/inc``), unchanged by schema 8;
- ``HEADER_ROOT`` is where the headers are (``native/inc/<pkg>``), ``<pkg>``
  read from ``[project] name`` in ``just-makeit.toml``.

A regex rather than ``tomllib``: gate scripts run on the floor Python, and
``tomllib`` is 3.11.

>>> HEADER_ROOT.startswith(INC_DIR + "/")
True
>>> header("clib_common.h") == HEADER_ROOT + "/clib_common.h"
True
"""

from __future__ import annotations

import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

#: The ``-I`` directory every include is relative to.
INC_DIR = "native/inc"


def _project_key(key: str) -> str | None:
    """A string value from ``[project]`` in ``just-makeit.toml``, or None."""
    with open(os.path.join(ROOT, "just-makeit.toml"), encoding="utf-8") as f:
        text = f.read()
    project = re.search(r"^\[project\]\n(.*?)(?=^\[)", text, re.M | re.S)
    value = project and re.search(rf'^{key}\s*=\s*"([^"]+)"', project[1], re.M)
    return value[1] if value else None


def _pkg() -> str:
    name = _project_key("name")
    if not name:
        raise SystemExit("_layout: no [project] name in just-makeit.toml")
    return name


#: The package directory the headers live in (jm schema 8).
PKG = _pkg()
#: The prefix every C symbol jm derives carries (``[project] c_prefix``,
#: #1545), so a gate that greps C for an object's calls finds ``dp_fir_*``
#: as well as a pre-prefix tree's ``fir_*``. Empty when the key is unset.
C_PREFIX = _project_key("c_prefix") or ""
HEADER_ROOT = f"{INC_DIR}/{PKG}"


def header(rel: str) -> str:
    """A header's repo-relative path, from its include spelling minus the
    package: ``header("wfm/wfm_frame.h")``."""
    return f"{HEADER_ROOT}/{rel}"
