"""One flag, or the contract is a fiction.

`docs/design/io-termination.md` rests on a single sentence: *the one flag
every blocking wait in doppler consults*. That is true in C, where
everything links one archive. It is not automatic in Python, because each
extension module links the primitive **statically** and CPython loads
extensions ``RTLD_LOCAL`` -- so without deliberate binding every module
gets its own flag and a stop in one cannot reach a wait in another
(doppler#976).

That defect lived for the whole life of the ring's interrupt support and
was invisible, because the only Python setter happened to live in the same
``.so`` as the only wait anybody tested. Nothing here is clever; what it is
is EXECUTED, which is the property the original arrangement lacked.

Two checks, deliberately different in kind:

- a **behavioural** one, which is the claim a caller actually relies on;
- a **structural** one, which is registration-free, so a NEW module that
  links the primitive and forgets to bind is caught the moment it exists
  rather than when somebody notices a hang.
"""

from __future__ import annotations

import pathlib
import subprocess
import sys
import textwrap

import pytest

_PKG = pathlib.Path(__file__).resolve().parents[1]

#: How long the consumer thread is given to notice. The ring's wait checks
#: the flag every spin iteration, so a working flag is seen in microseconds
#: -- this is generous enough that a slow machine is not a failure and
#: short enough that a broken one does not stall the suite.
_NOTICE_S = 3.0


def _extensions() -> list[pathlib.Path]:
    """Every built doppler extension, discovered rather than listed."""
    return sorted(_PKG.glob("*/*.so"))


def _uses_the_flag(so: pathlib.Path) -> bool:
    """Does this module carry its own copy of the primitive?

    `nm` over the object's symbols: a module that linked dp_interrupt.c
    statically defines these, and is therefore one that must bind.
    """
    out = subprocess.run(["nm", "-C", str(so)], capture_output=True, text=True)
    if out.returncode != 0:  # nm absent (macOS CI images vary); skip
        pytest.skip("nm unavailable")
    return " dp_interrupt_own_state" in out.stdout or (
        "dp_interrupt_bind" in out.stdout
    )


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


#: The modules that carry their own flag today, doppler#976. A RATCHET:
#: it may only shrink. Both tests below are strict-xfail against it, so
#: they fail if the defect spreads AND fail if it is fixed without this
#: list being updated -- which is what stops a fix landing silently and
#: leaving a stale waiver behind.
KNOWN_UNBOUND = frozenset(
    {"buffer", "interrupt", "stream", "telemetry", "wfm"}
)


@pytest.mark.xfail(
    strict=True,
    reason=(
        "doppler#976: each extension links the interrupt primitive "
        "statically, so a stop in one module cannot reach a wait in "
        "another. doppler#977 removes the cause. When this XPASSes, the "
        "fix has landed -- delete the marker, do not widen it."
    ),
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


def test_every_module_carrying_the_flag_also_binds_it() -> None:
    """A module that links the primitive must adopt the shared state.

    Discovered over the built extensions, so a new module is covered the
    moment it exists -- there is no list here to forget to update, which
    is the failure mode this whole file exists to prevent.
    """
    carriers = [so for so in _extensions() if _uses_the_flag(so)]
    assert carriers, (
        "no extension carries the interrupt primitive — probe broken"
    )

    out = subprocess.run(
        ["nm", "-C", *[str(p) for p in carriers]],
        capture_output=True,
        text=True,
    )
    missing = []
    current = None
    binds: dict[str, bool] = {}
    for line in out.stdout.splitlines():
        if line.endswith(":") and line[:-1].endswith(".so"):
            current = pathlib.Path(line[:-1]).parent.name
            binds.setdefault(current, False)
        elif current and "dp_interrupt_bind" in line and " U " not in line:
            # defined here, and referenced by this module's own init
            binds[current] = (
                binds[current] or "dp_interrupt_rendezvous" in out.stdout
            )

    missing = {m for m, ok in binds.items() if not ok}

    # The ratchet, in both directions.
    spread = missing - KNOWN_UNBOUND
    assert not spread, (
        f"NEW module(s) carrying their own interrupt flag: {sorted(spread)}. "
        f"A stop elsewhere cannot reach their waits (doppler#976). Bind the "
        f"shared state, or land doppler#977 -- do not add them to "
        f"KNOWN_UNBOUND, which may only shrink."
    )
    fixed = KNOWN_UNBOUND - missing
    assert not fixed, (
        f"{sorted(fixed)} now bind the shared state — good. Remove them "
        f"from KNOWN_UNBOUND so the ratchet keeps its value; a waiver that "
        f"outlives its defect is how the next one hides."
    )
