# compose.pyi — type stubs for the hand-written composer wrapper.
#
# Hand-owned (no_generate): the C extension is doppler.wfm._wfmcompose and the
# ergonomic API lives in compose.py. The doctests here are the CI-gated surface
# (pytest --doctest-glob='*.pyi'); they run against the built extension.
from __future__ import annotations

import os
from typing import Iterable, Iterator, Sequence

import numpy as np
from numpy.typing import NDArray

# The composer OO surface is the generated .so type (stubs in
# wfm_compose.pyi) — re-exported verbatim, no Python wrapper.
from .wfm_compose import (
    Composer as Composer,
    Segment as Segment,
    Synth as Synth,
    Timeline as Timeline,
    bits as bits,
    bpsk as bpsk,
    chirp as chirp,
    noise as noise,
    pn as pn,
    qpsk as qpsk,
    tone as tone,
)

# Transport handles are the generated kind="handle" .so types — their
# authoritative stubs live in wfm_writer.pyi / wfm_reader.pyi / wfm_sink.pyi /
# sample_clock.pyi. compose.py re-imports them for one import path, so re-export
# (don't redefine) here to keep the surface in sync with the generated API.
from .sample_clock import SampleClock as SampleClock
from .wfm_reader import Reader as Reader
from .wfm_sink import ZmqSink as ZmqSink
from .wfm_writer import Writer as Writer

def paced(
    blocks: Iterable[NDArray[np.complex64]], fs: float
) -> Iterator[NDArray[np.complex64]]:
    """Pace an iterable of sample blocks to real time against an ``fs``-Hz clock.

    The transport-side equivalent of ``wfmgen --realtime``: wrap any block
    iterator (typically :meth:`Composer.stream`) so each block is yielded
    unchanged, then sleeps to its real-time deadline via :class:`SampleClock`.

    >>> from doppler.wfm.compose import Composer, paced
    >>> comp = Composer(type="tone", freq=1e5, num_samples=512)
    >>> sum(len(b) for b in paced(comp.stream(256), fs=1e6))
    512

    """

def sigmf_meta(
    *,
    sample_type: str = ...,
    endian: str = ...,
    fs: float = ...,
    fc: float = ...,
    segments: Sequence[Segment],
) -> str:
    """Build the SigMF ``.sigmf-meta`` JSON for a composed capture.

    Examples
    --------
    >>> import json
    >>> from doppler.wfm.compose import sigmf_meta, Segment
    >>> meta = sigmf_meta(fs=1e6, segments=[Segment("tone", num_samples=64)])
    >>> json.loads(meta)["global"]["core:sample_rate"]
    1000000

    """
    ...

def write_blue_header(
    path: str | os.PathLike,
    *,
    sample_type: str = ...,
    endian: str = ...,
    fs: float = ...,
    fc: float = ...,
    total: int,
    data_start: float = ...,
    detached: bool = ...,
) -> None:
    """Write a standalone BLUE type-1000 HCB header (the detached ``.hdr``).

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
    ...

def rrc_taps(beta: float, sps: int, span: int) -> NDArray[np.float32]:
    """Root-raised-cosine pulse-shaping taps (``2*span*sps + 1`` of them).

    Examples
    --------
    >>> from doppler.wfm.compose import rrc_taps
    >>> t = rrc_taps(0.35, 4, 6)
    >>> t.dtype, len(t)
    (dtype('float32'), 49)

    """
    ...

def dsss_spread(
    syms: NDArray[np.complex64], code: NDArray[np.uint8], sf: int
) -> NDArray[np.complex64]:
    """Direct-sequence spread ``syms`` by a ``±1`` chip ``code`` (length ≥ ``sf``).

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.wfm.compose import dsss_spread
    >>> syms = np.array([1 + 0j, -1 + 0j], dtype=np.complex64)
    >>> dsss_spread(syms, np.array([0, 1, 0, 1], dtype=np.uint8), 4).shape
    (8,)

    """
    ...
