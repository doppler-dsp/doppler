"""Multi-segment waveform composition, file writers, and a ZMQ sink.

This is the Python face of the C ``wfmgen`` composer subsystem — the same engine
behind the ``wfmgen`` CLI, exposed here as classes. A :class:`Composer` strings
:class:`Segment` specs (tone / noise / PN / BPSK / QPSK, each with its own
on-time and trailing gap) into one stream, optionally looping (``repeat``) or
running forever (``continuous``). :class:`Writer` serialises samples to the same
containers as the CLI (raw interleaved I/Q, CSV, BLUE type-1000, SigMF), and
:class:`ZmqSink` publishes them over ZeroMQ. The composer's resolved spec
round-trips through JSON (:meth:`Composer.to_json` / :meth:`Composer.from_json`),
so a capture is fully reproducible.

The samples come back as ``complex64`` arrays; pair :class:`Writer` with
:func:`doppler.wfm.readback.read_iq` to round-trip a file.

The ``Synth`` / ``Segment`` / ``Timeline`` / ``Composer`` ergonomic types are the
**jm-generated** CPython types in ``doppler.wfm.wfm_compose`` (the composer lives
entirely in the ``.so``; ``jm`` owns the binding). They are **re-exported
verbatim** below — there is no Python wrapper layer: standalone sample generation
(:meth:`Synth.steps`), the ``pattern`` / ``f_start`` input sugar, the flat
single-source :class:`Segment` view, :meth:`Composer.stream`, and the resolved
:meth:`Composer.to_dict` are all generated. Only the container writers/readers
(BLUE / SigMF / ZMQ / sample-clock) stay hand-written here, over the transport
binding ``_wfmcompose``.

Examples
--------
>>> from doppler.wfm.compose import Composer, Segment
>>> spec = [Segment("pn", num_samples=127, pn_length=7),
...         Segment("tone", freq=1e5, num_samples=256, off_samples=64)]
>>> x = Composer(spec).compose()
>>> x.dtype, len(x)  # 127 (pn) + 256 (tone) + 64 (gap)
(dtype('complex64'), 447)
"""

from __future__ import annotations

import os


# _wfmcompose: transport binding (sigmf/DSP helpers — the transport classes are
# now the generated kind="handle" types below).
from . import _wfmcompose as _c

# The transport surface is now the generated kind="handle" types — re-export
# them through compose so `doppler.wfm.compose.Writer` (etc.) stays the import
# path. (Realtime pacing now lives in C as Composer.stream(realtime=fs); the
# hand-written paced() helper is retired.)
from .sample_clock import SampleClock  # noqa: F401  (re-export)
from .wfm_reader import Reader  # noqa: F401  (re-export)
from .wfm_sink import ZmqSink  # noqa: F401  (re-export)
from .wfm_writer import Writer  # noqa: F401  (re-export)

# The composer OO surface IS the generated .so type — re-export it verbatim, no
# Python wrapper. Synth/Segment/Timeline/Composer carry standalone generation,
# the pattern/f_start aliases, the flat Segment view, stream() and to_dict().
from .wfm_compose import (  # noqa: F401  (re-export)
    Composer,
    Segment,
    Synth,
    Timeline,
    bits,
    bpsk,
    chirp,
    noise,
    pn,
    qpsk,
    tone,
)

# string-enum ↔ C-int tables for the remaining hand binding (write_blue_header);
# must match native/src/app/wfmgen.c and the manifest [[enum]] stype/endian.
#
# This is a third copy of that enum SSOT (doppler#179 review #8). It survives
# because write_blue_header is the last unmigrated hand binding: turning it into
# a generated `jm function` would let the enum strings resolve against the single
# manifest [[enum]] SSOT, deleting these tables — but that needs `path` + `enum`
# arg support in jm's module-function generator, which it does not yet have
# (tracked as just-makeit#353). Until then, the mapping has to live here.
_STYPES = ("cf32", "cf64", "ci32", "ci16", "ci8")
_ENDIANS = ("le", "be")


def _idx(name: str, table: tuple[str, ...], what: str) -> int:
    try:
        return table.index(name)
    except ValueError:
        raise ValueError(
            f"{what} must be one of {table!r}, got {name!r}"
        ) from None


# ── module-level helpers ─────────────────────────────────────────────────────

# sigmf_meta is now the generated Composer.to_sigmf() method (delegated
# serializer over the resolved segments) — call Composer(spec).to_sigmf(...).


def write_blue_header(
    path: str | os.PathLike,
    *,
    sample_type: str = "cf32",
    endian: str = "le",
    fs: float = 1e6,
    fc: float = 0.0,
    total: int,
    data_start: float = 0.0,
    detached: bool = True,
) -> None:
    """Write a standalone BLUE type-1000 HCB header (the detached ``.hdr``).

    The 512-byte header carries the ``"BLUE"`` magic, byte order, ``data_size``
    (``total`` × bytes-per-sample), the type-1000 tag and ``xdelta = 1/fs``;
    pair it with a detached ``.det`` body of raw interleaved I/Q.

    Examples
    --------
    >>> import os, tempfile
    >>> from doppler.wfm.compose import write_blue_header
    >>> p = os.path.join(tempfile.mkdtemp(), "cap.hdr")
    >>> write_blue_header(p, sample_type="cf32", fs=1e6, total=512)
    >>> with open(p, "rb") as f:
    ...     head = f.read()
    >>> head[:4], len(head)
    (b'BLUE', 512)
    """
    _c.blue_write_hcb(
        os.fspath(path),
        _idx(sample_type, _STYPES, "sample_type"),
        _idx(endian, _ENDIANS, "endian"),
        float(fs),
        float(fc),
        float(data_start),
        int(total),
        bool(detached),
    )


# rrc_taps / dsss_spread are now generated `variable_output` module functions
# (doppler.wfm.rrc_taps / .dsss_spread, over native/src/wfm/{rrc_taps,dsss_spread}.c).
