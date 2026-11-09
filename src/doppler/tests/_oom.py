"""Run one call under an address-space cap, in a child process.

An allocation failure is the one refusal a test cannot provoke by its
arguments: the size is fine, the machine is not. This harness makes it
happen on purpose. A child process does the setup, reads its own address
space (``VmSize`` from ``/proc/self/status``), caps it at that size plus a
small headroom (``RLIMIT_AS``), and makes exactly one call. Only an
allocation larger than the headroom can then fail, so the test chooses a
call whose own allocation is the large one, and Python itself running out
cannot pass: the outcome must be the C's declared ``MemoryError`` sentence.
The cap is then lifted and the child runs ``after``, which checks what the
refusal left behind (the engine unchanged, nothing registered).

It is a subprocess because an address-space cap is the whole process's and
cannot be undone safely inside pytest's own, and Linux only because it reads
``/proc/self/status``.

The three tests that once each carried a private copy of this harness
(doppler#2958): a carrier rebuild and a ``set_state`` that rebuilds
(``test_correlator_continuous.py``), a grid rebuild
(``test_correlator_burst.py``), and a telemetry attach
(``test_async_dsss_receiver.py``).
"""

from __future__ import annotations

import subprocess
import sys
import textwrap

import pytest

#: Skip off Linux: the harness reads ``/proc/self/status``.
linux_only = pytest.mark.skipif(
    not sys.platform.startswith("linux"), reason="reads /proc/self/status"
)

_CHILD = """\
import resource

{setup}

def _vm_bytes():
    for line in open("/proc/self/status"):
        if line.startswith("VmSize"):
            return int(line.split()[1]) * 1024

_soft, _hard = resource.getrlimit(resource.RLIMIT_AS)
resource.setrlimit(resource.RLIMIT_AS, (_vm_bytes() + {headroom}, _hard))
try:
{call}
    _outcome = "no error"
except MemoryError as _e:
    _outcome = "MemoryError: " + str(_e)
except Exception as _e:
    _outcome = type(_e).__name__ + ": " + str(_e)
resource.setrlimit(resource.RLIMIT_AS, (_soft, _hard))
print(_outcome)

{after}
"""


def run_capped(
    setup: str,
    call: str,
    *,
    headroom: int = 2 << 20,
    after: str = "",
    timeout: float = 300,
) -> subprocess.CompletedProcess[str]:
    """Run ``setup``, then ``call`` under a cap of VmSize + ``headroom``.

    Parameters
    ----------
    setup
        Code run before the cap: imports and the object under test.
    call
        The one statement made under the cap.
    headroom
        Bytes above the child's current size the cap allows. The default,
        2 MiB, suits a call whose allocation is megabytes; a call that
        allocates kilobytes needs a cap that tight.
    after
        Code run once the cap is lifted, printing what the refusal left.
    timeout
        Seconds before the child is abandoned.

    Returns
    -------
    subprocess.CompletedProcess
        Its stdout's first line is the call's outcome: ``"MemoryError:
        <sentence>"``, ``"no error"``, or ``"<Exception>: <message>"``;
        the lines after it are ``after``'s.
    """
    script = _CHILD.format(
        setup=textwrap.dedent(setup).strip(),
        headroom=int(headroom),
        call=textwrap.indent(textwrap.dedent(call).strip(), "    "),
        after=textwrap.dedent(after).strip(),
    )
    return subprocess.run(
        [sys.executable, "-c", script],
        capture_output=True,
        text=True,
        timeout=timeout,
    )
