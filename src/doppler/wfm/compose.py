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
from typing import Sequence

import numpy as np
from numpy.typing import NDArray

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

# ── string-enum ↔ C-int tables (must match native/src/app/wfmgen.c) ──────────
_TYPES = ("tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits")
_MODES = ("auto", "fs", "ebno", "esno")
_PULSES = ("rect", "rrc")  # PSK pulse shape → C: rect (hold) / rrc (FIR)
_BITMODS = ("none", "bpsk", "qpsk")  # bits-pattern modulation → C bit_mod
_STYPES = ("cf32", "cf64", "ci32", "ci16", "ci8")
_FTYPES = ("raw", "csv", "blue", "sigmf")
_ENDIANS = ("le", "be")
_LFSRS = ("galois", "fibonacci")


def _idx(name: str, table: tuple[str, ...], what: str) -> int:
    try:
        return table.index(name)
    except ValueError:
        raise ValueError(
            f"{what} must be one of {table!r}, got {name!r}"
        ) from None


def _src_tuple(s) -> tuple:
    """A source's 16-field ``_SOURCE_FMT`` tuple for the transport marshal."""
    return (
        _idx(s.type, _TYPES, "type"),
        float(s.freq),
        float(s.snr),
        _idx(s.snr_mode, _MODES, "snr_mode"),
        int(s.seed),
        int(s.sps),
        int(s.pn_length),
        int(s.pn_poly),
        _idx(s.lfsr, _LFSRS, "lfsr"),
        float(s.level),
        float(s.f_end),
        _idx(s.modulation, _BITMODS, "modulation"),
        s.bits,
        _idx(s.pulse, _PULSES, "pulse"),
        float(s.rrc_beta),
        int(s.rrc_span),
    )


def _seg_tuple(seg) -> tuple:
    """A segment's transport tuple: flat ``_SEG_FMT`` (1 source) else nested.

    Mirrors the C round-trip (``wfmcompose_py.c``): ``n_sources == 1`` is the
    flat single-source form, otherwise ``(num, off, fs, [source-tuples])``.
    """
    srcs = list(seg.sources)
    if len(srcs) == 1:
        s = srcs[0]
        return (
            _idx(s.type, _TYPES, "type"),
            float(seg.fs),
            float(s.freq),
            float(s.snr),
            _idx(s.snr_mode, _MODES, "snr_mode"),
            int(s.seed),
            int(s.sps),
            int(s.pn_length),
            int(s.pn_poly),
            _idx(s.lfsr, _LFSRS, "lfsr"),
            int(seg.num_samples),
            int(seg.off_samples),
            float(s.level),
            float(s.f_end),
            _idx(s.modulation, _BITMODS, "modulation"),
            s.bits,
            _idx(s.pulse, _PULSES, "pulse"),
            float(s.rrc_beta),
            int(s.rrc_span),
        )
    return (
        int(seg.num_samples),
        int(seg.off_samples),
        float(seg.fs),
        [_src_tuple(s) for s in srcs],
    )


# ── module-level helpers ─────────────────────────────────────────────────────


def sigmf_meta(
    *,
    sample_type: str = "cf32",
    endian: str = "le",
    fs: float = 1e6,
    fc: float = 0.0,
    segments: Sequence[Segment],
) -> str:
    """Build the SigMF ``.sigmf-meta`` JSON for a composed capture.

    The capture's segments become per-segment SigMF annotations; pair this with
    a ``Writer(..., file_type="sigmf")`` writing the ``.sigmf-data`` companion.
    """
    return _c.sigmf_meta_json(
        _idx(sample_type, _STYPES, "sample_type"),
        _idx(endian, _ENDIANS, "endian"),
        float(fs),
        float(fc),
        [_seg_tuple(s) for s in segments],
    )


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


def rrc_taps(beta: float, sps: int, span: int) -> NDArray[np.float32]:
    """Root-raised-cosine pulse-shaping taps.

    Returns ``2*span*sps + 1`` float32 taps for roll-off ``beta``, ``sps``
    samples per symbol, and a ``±span``-symbol support.

    Examples
    --------
    >>> from doppler.wfm.compose import rrc_taps
    >>> t = rrc_taps(0.35, 4, 6)
    >>> t.dtype, len(t)
    (dtype('float32'), 49)
    """
    return _c.rrc_taps(float(beta), int(sps), int(span))


def dsss_spread(
    syms: NDArray[np.complex64], code: NDArray[np.uint8], sf: int
) -> NDArray[np.complex64]:
    """Direct-sequence spread ``syms`` by the ``±1`` chip ``code`` (length ≥ ``sf``).

    Each symbol is repeated against the first ``sf`` chips, yielding
    ``len(syms) * sf`` output chips.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.wfm.compose import dsss_spread
    >>> syms = np.array([1+0j, -1+0j], dtype=np.complex64)
    >>> code = np.array([0, 1, 0, 1], dtype=np.uint8)
    >>> dsss_spread(syms, code, 4).shape
    (8,)
    """
    return _c.dsss_spread(syms, code, int(sf))
