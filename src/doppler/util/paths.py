"""Paths that start at the calling script, not at the working directory.

A script that writes a figure or a capture under a bare file name writes it
into whatever directory the run started in: from the repo root it lands in
the repo root, from the script's own folder it lands beside the script. The
same run puts its output in two places depending on where it was typed.
:func:`fromto` takes that choice away from the working directory: the start
is the calling script's own directory unless the caller names another, and
the destination is joined onto it with pathlib's ``/``.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    import os

__all__ = ["fromto"]


def fromto(
    to: str | os.PathLike[str] | None = None,
    here: str | os.PathLike[str] | None = None,
) -> Path:
    """The path from ``here`` to ``to``: where a script writes an artifact.

    The start, ``here``, defaults to the directory of the file that called
    ``fromto``, so a bare file name lands beside the calling script whatever
    directory the run started in. The destination is joined onto the start
    with pathlib's ``/``, which returns its right operand unchanged when that
    is absolute. So one call covers both cases a script has: no destination
    given (its default name, beside itself) and a destination given (used as
    given).

    Parameters
    ----------
    to : str or os.PathLike, optional
        The destination. A relative one is joined onto ``here``. An absolute
        one is returned unchanged. ``None`` returns ``here`` itself, the
        directory, for a script that writes several files into it.
    here : str or os.PathLike, optional
        The start. Default: the directory of the calling file, read from the
        caller's ``__file__`` and resolved to an absolute path.

    Returns
    -------
    pathlib.Path
        ``Path(here) / to``, or ``Path(here)`` when ``to`` is ``None``.
        Nothing is created or checked on disk; the caller makes any
        directory it writes into.

    Raises
    ------
    ValueError
        ``here`` is omitted and the caller has no ``__file__`` (a REPL, or
        ``python -c``), so there is no script to start from.

    Notes
    -----
    A path someone types on a command line is relative to the working
    directory by convention. Resolve it where it is read
    (``Path(arg).resolve()``): it is then absolute, and ``fromto`` returns it
    as given. Only the default is anchored at the script.

    Examples
    --------
    A relative destination lands under the start, an absolute one where it
    says, and no destination is the start itself:

    >>> from doppler.util import fromto
    >>> fromto("dll_lock.png", here="/runs").as_posix()
    '/runs/dll_lock.png'
    >>> fromto("/tmp/dll_lock.png", here="/runs").as_posix()
    '/tmp/dll_lock.png'
    >>> fromto(here="/runs").as_posix()
    '/runs'

    With no ``here``, the start is the directory of the file making the
    call:

    >>> from pathlib import Path
    >>> fromto("x.png") == Path(__file__).resolve().parent / "x.png"
    True
    """
    if here is None:
        # The caller's module globals, not the working directory: this frame
        # is fromto's own, so one level up is whoever called it.
        caller = sys._getframe(1).f_globals.get("__file__")
        if caller is None:
            raise ValueError(
                "fromto: the caller has no __file__ (a REPL, or python -c), "
                "so there is no script to start from; pass here="
            )
        here = Path(caller).resolve().parent
    start = Path(here)
    return start if to is None else start / to
