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
from importlib.metadata import version

import pytest

_PKG = pathlib.Path(__file__).resolve().parents[1]
_ROOT = _PKG.parents[1]
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


# --------------------------------------------------------------------- #
# The contract header: generated, but by nothing that runs again
# --------------------------------------------------------------------- #


def test_the_generated_contract_header_is_not_stale() -> None:
    """``<comp>_procglobal.h`` still says what this jm would generate.

    The header is where the three rendezvous names LIVE for a
    ``no_generate`` module -- ``dp_interrupt_pyadopt.h`` reads the macros
    rather than spelling them, precisely so a rename moves them by
    itself. What that reasoning assumed, and nobody checked, is that the
    header tracks jm. It does not: ``render_header`` is called only from
    ``jm init`` and ``jm object``, so the file is written once at
    scaffold time and never again. ``jm apply`` will not rewrite it
    (clobber it to one line and apply reports nothing), and
    ``jm status --check`` does not compare it, so a ``DO NOT EDIT`` file
    sits outside every gate jm has. Filed as just-buildit/just-makeit#1140.

    Measured on the 0.67.1 -> 0.67.2 bump, which is what this test is
    made of: gh-1134 moved the owner import from the package to the
    extension module, jm regenerated every module it writes, and left
    ``DP_INTERRUPT_GUARD_PG_OWNER`` naming the package. buffer and
    stream then adopted through the stale macro and the package
    half-loaded -- 100 collection errors.

    The behavioural test above does catch that, which is how it was
    found. This one catches it at the RIGHT ALTITUDE: it names the stale
    file and the field, on a jm bump, instead of leaving a hundred
    unrelated-looking import errors to be traced back. It also covers
    the shape doppler does not have today -- a project whose adopters
    are all jm-generated, where a stale header breaks nothing until
    someone adds a hand-written binding.

    Registration-free on both axes: the components come from
    ``process_globals`` over the merged manifest, and the expected text
    from jm's own renderer, so this cannot drift from what jm means.
    """
    _config = pytest.importorskip("just_makeit._config")
    _pg = pytest.importorskip("just_makeit._procglobal")

    cfg = _config.load(_ROOT)
    comps = _pg.process_globals(cfg)
    assert comps, (
        "no component declares process_global — either the manifest moved "
        "or jm's API did; this test is checking nothing"
    )

    stale = []
    for comp in comps:
        hdr = _NATIVE_INC / _pg.header_name(comp)
        want = _pg.render_header(cfg, comp)
        assert want, f"jm rendered no header for {comp} — its API moved"
        if not hdr.is_file() or hdr.read_text() != want:
            stale.append(hdr.relative_to(_ROOT))

    assert not stale, (
        f"generated contract header(s) stale against just-makeit "
        f"{version('just-makeit')}: {stale}. `jm apply` does NOT rewrite "
        f"them (just-buildit/just-makeit#1140) — regenerate by hand:\n"
        f'    uv run python -c "import pathlib;'
        f"from just_makeit import _config, _procglobal as p;"
        f"cfg=_config.load(pathlib.Path('.'));"
        f"[(pathlib.Path('native/inc')/p.header_name(c)).write_text("
        f'p.render_header(cfg,c)) for c in p.process_globals(cfg)]"'
    )
