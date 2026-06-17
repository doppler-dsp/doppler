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

class Writer:
    """Stream ``complex64`` samples to a container (raw / csv / blue / sigmf).

    Pairs with :func:`doppler.wfm.readback.read_iq` to round-trip a file.

    Examples
    --------
    >>> import os, tempfile, numpy as np
    >>> from doppler.wfm.compose import Composer, Writer
    >>> from doppler.wfm.readback import read_iq
    >>> x = Composer(type="tone", freq=1e5, num_samples=512).compose()
    >>> p = os.path.join(tempfile.mkdtemp(), "cap.cf32")
    >>> with Writer(p, sample_type="cf32") as w:
    ...     _ = w.write(x)
    >>> bool(np.allclose(read_iq(p, "cf32"), x))
    True

    """

    def __init__(
        self,
        path: str | os.PathLike,
        *,
        file_type: str = ...,
        sample_type: str = ...,
        endian: str = ...,
        fs: float = ...,
        fc: float = ...,
        total: int = ...,
    ) -> None: ...
    def write(self, iq: NDArray[np.complex64]) -> int:
        """Write a block of samples; returns the number written."""
        ...

    def close(self) -> None:
        """Flush, patch any header, and close the file (idempotent)."""
        ...

    def __enter__(self) -> Writer: ...
    def __exit__(self, *exc: object) -> None: ...

class Reader:
    """Read a capture back to ``complex64`` — the dual of :class:`Writer`.

    Auto-detects the container (BLUE magic / ``.sigmf-meta`` sidecar / ``.csv`` /
    raw); BLUE and SigMF recover sample type, byte order, ``fs`` and ``fc`` from
    metadata, while raw / CSV use the ``sample_type`` / ``endian`` hints. All
    parsing and conversion is in C.

    Examples
    --------
    >>> import tempfile, os, numpy as np
    >>> from doppler.wfm.compose import Composer, Writer, Reader
    >>> x = Composer(type="tone", freq=1e5, num_samples=512).compose()
    >>> p = os.path.join(tempfile.mkdtemp(), "cap.blue")
    >>> with Writer(p, file_type="blue", fs=1e6) as w:
    ...     _ = w.write(x)
    >>> with Reader(p) as r:
    ...     y = r.read_all()
    ...     print(r.file_type, int(r.fs), bool(np.allclose(y, x)))
    blue 1000000 True

    """

    def __init__(
        self,
        path: str | os.PathLike,
        *,
        sample_type: str = ...,
        endian: str = ...,
    ) -> None: ...
    @property
    def file_type(self) -> str: ...
    @property
    def sample_type(self) -> str: ...
    @property
    def endian(self) -> str: ...
    @property
    def fs(self) -> float: ...
    @property
    def fc(self) -> float: ...
    @property
    def num_samples(self) -> int: ...
    def read(self, n: int) -> NDArray[np.complex64]:
        """Read up to ``n`` samples; a short/empty array marks EOF."""
        ...

    def read_all(self, block: int = ...) -> NDArray[np.complex64]:
        """Drain the whole capture into one ``complex64`` array."""
        ...

    def close(self) -> None:
        """Close the file (idempotent)."""
        ...

    def __enter__(self) -> Reader: ...
    def __exit__(self, *exc: object) -> None: ...

class ZmqSink:
    """Publish ``complex64`` samples over a ZeroMQ PUB socket (POSIX only).

    Each :meth:`send` frames the block with its sample rate / centre frequency and
    the chosen ``sample_type`` wire format. Raises ``NotImplementedError`` on
    platforms without the sink (Windows).
    """

    def __init__(self, endpoint: str, *, sample_type: str = ...) -> None: ...
    def send(
        self, iq: NDArray[np.complex64], fs: float, fc: float = ...
    ) -> None:
        """Publish a block tagged with its sample rate and centre frequency."""
        ...

    def close(self) -> None:
        """Close the publisher (idempotent)."""
        ...

    def __enter__(self) -> ZmqSink: ...
    def __exit__(self, *exc: object) -> None: ...

class SampleClock:
    """Pace and timestamp a stream against an ideal ``fs``-Hz clock (POSIX).

    :meth:`pace` sleeps so each block leaves at ``epoch + n/fs`` (throttling a
    producer to real time); :meth:`stamp` returns the ideal UNIX-epoch-ns time
    of the next sample. One drift-free timeline: deadlines are recomputed from
    the cumulative sample count, so jitter never accumulates into drift.
    Underruns (producer can't keep up) are counted; ``resync=True`` re-anchors
    to now instead of keeping the unreachable schedule. Raises
    ``NotImplementedError`` off POSIX (Windows).

    Examples
    --------
    >>> from doppler.wfm.compose import SampleClock
    >>> clk = SampleClock(fs=1e6)
    >>> _ = clk.pace(1000)            # advance 1000 samples (~1 ms) and wait
    >>> clk.samples
    1000
    >>> isinstance(clk.stamp(), int)
    True

    """

    def __init__(self, fs: float, *, resync: bool = ...) -> None: ...
    def pace(self, count: int) -> float:
        """Advance ``count`` samples, sleep to the deadline; returns slack (s)."""
        ...

    def stamp(self) -> int:
        """Ideal UNIX-epoch-ns timestamp of the next sample (index ``n``)."""
        ...

    def reset(self) -> None:
        """Re-anchor to now and zero the counters (fresh clock at n=0)."""
        ...

    def resync(self) -> None:
        """Drop accumulated lateness; pace forward from now (keeps ``n``)."""
        ...

    @property
    def samples(self) -> int: ...
    @property
    def underruns(self) -> int: ...
    @property
    def max_lateness(self) -> float: ...

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

def mls_poly(n: int) -> int:
    """Maximal-length-sequence primitive polynomial for an LFSR of length ``n``.

    Valid for ``n`` in 2..64 (returns 0 otherwise).

    Examples
    --------
    >>> from doppler.wfm.compose import mls_poly
    >>> hex(mls_poly(7))
    '0x41'

    """
    ...
