"""One flag, or the contract is a fiction.

`docs/design/io-termination.md` rests on a single sentence: *the one flag
every blocking wait in doppler consults*. That is true in C, where
everything links one archive. It is not automatic in Python, because each
extension module links the primitive **statically** and CPython loads
extensions ``RTLD_LOCAL`` -- so without a deliberate rendezvous every module
gets its own flag and a stop in one cannot reach a wait in another
(doppler#976).

That defect lived for the whole life of the ring's interrupt support and
was invisible, because the only Python setter happened to live in the same
``.so`` as the only wait anybody tested. Nothing here is clever; what it is
is EXECUTED, which is the property the original arrangement lacked.

The rendezvous itself is just-makeit's ``process_global = true`` on the
``dp_interrupt_guard`` component: the owning module publishes a ``PyCapsule``
over the state and every other linking module adopts the pointer in its
``PyInit_``. doppler writes the two accessors it names
(``native/src/dp_interrupt.c``) and, for its two ``no_generate`` modules,
the adopt call itself (``native/inc/dp_interrupt_pyadopt.h``).

Two checks, deliberately different in kind:

- a **behavioural** one, which is the claim a caller actually relies on;
- a **structural** one, which is registration-free, so a NEW module that
  links the primitive and never joins the rendezvous is caught the moment
  it exists rather than when somebody notices a hang.
"""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys
import textwrap

import pytest

from doppler.tests._repo import repo_root

_PKG = pathlib.Path(__file__).resolve().parents[1]
#: The checkout, found by walking up for its markers rather than by
#: counting directories: `make coverage` runs this file from a COPY of
#: the package at build-cov/pkg/doppler, two levels deeper, where every
#: fixed-depth root lands inside build-cov/. See doppler.tests._repo.
_ROOT = repo_root()
_NATIVE_INC = _ROOT / "native" / "inc"

#: How long the consumer thread is given to notice. The ring's wait checks
#: the flag every spin iteration, so a working flag is seen in microseconds
#: -- this is generous enough that a slow machine is not a failure and
#: short enough that a broken one does not stall the suite.
_NOTICE_S = 3.0

#: Defined in native/src/dp_interrupt.c. A module carrying this symbol has
#: linked the primitive statically and therefore holds a flag of its own,
#: which is what makes it a module that MUST join the rendezvous.
_STORAGE_SYMBOL = b"dp_interrupt_own_state"


def _capsule_name() -> bytes:
    """The rendezvous capsule name, read from just-makeit's own header.

    Derived rather than spelled, because it is jm's invention: it is
    project-and-component-qualified, so renaming the component moves it.
    A literal here would keep passing against the old name while every
    module quietly stopped sharing a flag -- the failure this file exists
    to catch, arrived at through its own test.
    """
    hdr = (
        _NATIVE_INC / "dp_interrupt_guard" / "dp_interrupt_guard_procglobal.h"
    )
    if not hdr.is_file():  # pragma: no cover - a build this test can't judge
        pytest.skip(f"{hdr} absent: run `make jm-apply`")
    m = re.search(
        r'#define\s+DP_INTERRUPT_GUARD_PG_CAPSULE\s+"([^"]+)"',
        hdr.read_text(),
    )
    assert m, f"no PG_CAPSULE define in {hdr} — jm's contract moved"
    return m.group(1).encode()


def _extensions() -> list[pathlib.Path]:
    """Every built doppler extension, discovered rather than listed."""
    return sorted(_PKG.glob("*/*.so"))


# --------------------------------------------------------------------- #
# The behavioural check: what a caller relies on
# --------------------------------------------------------------------- #

_PROBE = textwrap.dedent(
    """
    import threading, time, sys
    import numpy as np
    from doppler.interrupt import Interrupt
    from doppler.buffer import F32Buffer

    it = Interrupt(np.array([], dtype=np.int32))
    buf = F32Buffer(1024)          # nothing is ever written to it
    out = {}

    def consumer():
        try:
            buf.wait(64)
            out["r"] = "returned"
        except KeyboardInterrupt:
            out["r"] = "interrupted"
        except BaseException as e:
            out["r"] = type(e).__name__

    t = threading.Thread(target=consumer, daemon=True)
    t.start()
    time.sleep(0.2)
    it.interrupt()
    t.join(timeout=%(notice)s)
    print(out.get("r", "SPINNING"))
    """
)


def test_a_stop_in_one_module_reaches_a_wait_in_another() -> None:
    """The claim, end to end, across two extension modules.

    Run in a SUBPROCESS on purpose: the ring's wait is an unbounded spin
    when the flag does not reach it, so a failure here would otherwise
    wedge the whole suite rather than fail it. The subprocess is killed,
    reported, and the run continues.
    """
    try:
        done = subprocess.run(
            [sys.executable, "-c", _PROBE % {"notice": _NOTICE_S}],
            capture_output=True,
            text=True,
            timeout=_NOTICE_S + 15.0,
        )
    except subprocess.TimeoutExpired:
        pytest.fail(
            "the consumer never came back: a stop requested through "
            "doppler.interrupt did not reach a ring wait in doppler.buffer, "
            "so the two modules are not sharing one flag (doppler#976)"
        )

    verdict = done.stdout.strip().splitlines()[-1] if done.stdout else ""
    assert verdict == "interrupted", (
        f"the ring wait answered {verdict!r} rather than being interrupted; "
        f"stderr={done.stderr[-400:]!r}"
    )


# --------------------------------------------------------------------- #
# The structural check: registration-free, catches the NEXT module
# --------------------------------------------------------------------- #


def test_every_module_carrying_the_flag_joins_the_rendezvous() -> None:
    """A module that links the primitive must share the one state.

    Discovered over the built extensions, so a new module is covered the
    moment it exists -- there is no list here to forget to update, which
    is the failure mode this whole file exists to prevent. In particular
    there is no waiver list: the previous version of this test carried a
    five-module ratchet, and a waiver that outlives its defect is how the
    next one hides.

    Both roles are proved by the same evidence, deliberately. The owner
    module publishes the capsule and an adopter reads it, so the capsule
    NAME is a string constant in either one's `.so`; a module that linked
    the primitive and joined nothing has the storage and not the name.
    Reading the binary is what makes this registration-free -- there is no
    manifest to consult and nothing to declare.
    """
    capsule = _capsule_name()
    carriers = [
        so for so in _extensions() if _STORAGE_SYMBOL in so.read_bytes()
    ]
    assert carriers, (
        "no extension carries the interrupt primitive — probe broken, or "
        "the extensions are not built (`make build`)"
    )

    missing = sorted(
        so.parent.name for so in carriers if capsule not in so.read_bytes()
    )
    assert not missing, (
        f"module(s) carrying their own interrupt flag without joining the "
        f"rendezvous: {missing}. A stop requested elsewhere cannot reach "
        f"their waits (doppler#976). A module just-makeit generates gets "
        f"the rendezvous from `process_global` in objects/"
        f"dp_interrupt_guard.toml; a `no_generate` module has to call "
        f"dp_interrupt_pyadopt() in its own PyInit_, as buffer and stream "
        f"do. Do NOT add a waiver list here."
    )
