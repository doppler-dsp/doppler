# compose.pyi — type stubs for the hand-written composer wrapper.
#
# Hand-owned (no_generate): the C extension is doppler.wfmgen._wfmcompose and the
# ergonomic API lives in compose.py. The doctests here are the CI-gated surface
# (pytest --doctest-glob='*.pyi'); they run against the built extension.
from __future__ import annotations

import os
from dataclasses import dataclass
from typing import Sequence

import numpy as np
from numpy.typing import NDArray

@dataclass
class Segment:
    """One waveform segment of a composed stream (mirrors the C ``wfm_segment_t``).

    The string fields ``type`` ({"tone","noise","pn","bpsk","qpsk"}), ``snr_mode``
    ({"auto","fs","ebno","esno"}) and ``lfsr`` ({"galois","fibonacci"}) map to the
    C enums; defaults match the ``wfmgen`` CLI (a clean 1024-sample 1 MS/s tone).

    Examples
    --------
    >>> from doppler.wfmgen.compose import Segment
    >>> Segment("pn", num_samples=127).type
    'pn'

    """

    type: str = ...
    fs: float = ...
    freq: float = ...
    snr: float = ...
    snr_mode: str = ...
    seed: int = ...
    sps: int = ...
    pn_length: int = ...
    pn_poly: int = ...
    lfsr: str = ...
    num_samples: int = ...
    off_samples: int = ...

class Composer:
    """Multi-segment waveform generator over a list of :class:`Segment`.

    Build from an explicit segment list, a single segment's keyword arguments, or
    a JSON spec (:meth:`from_json` / :meth:`from_file`). Pull blocks with
    :meth:`execute` or drain a finite spec with :meth:`compose`. Context manager.

    Examples
    --------
    >>> from doppler.wfmgen.compose import Composer, Segment
    >>> spec = [Segment("pn", num_samples=127), Segment("tone", num_samples=256,
    ...                                                 off_samples=64)]
    >>> x = Composer(spec).compose()
    >>> x.dtype, len(x)
    (dtype('complex64'), 447)

    """

    def __init__(
        self,
        segments: Sequence[Segment] | None = ...,
        *,
        repeat: bool = ...,
        continuous: bool = ...,
        **segment_kwargs: object,
    ) -> None: ...
    @classmethod
    def from_json(cls, json: str) -> Composer:
        """Build a composer from a JSON spec string."""
        ...

    @classmethod
    def from_file(cls, path: str | os.PathLike) -> Composer:
        """Build a composer from a JSON spec file."""
        ...

    def execute(self, n: int) -> NDArray[np.complex64]:
        """Generate up to ``n`` samples; a short/empty array marks the end.

        Examples
        --------
        >>> from doppler.wfmgen.compose import Composer
        >>> with Composer(type="noise", snr=10.0, num_samples=4096) as c:
        ...     blk = c.execute(1024)
        >>> blk.dtype, len(blk)
        (dtype('complex64'), 1024)

        """
        ...

    def compose(self, block: int = ...) -> NDArray[np.complex64]:
        """Drain a finite spec into one array (raises if ``continuous``)."""
        ...

    @property
    def segments(self) -> list[Segment]:
        """The resolved segment list (defaults filled in)."""
        ...

    @property
    def repeat(self) -> bool: ...
    @property
    def continuous(self) -> bool: ...
    def to_json(self) -> str:
        """Serialise the resolved spec to JSON (round-trips via :meth:`from_json`).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfmgen.compose import Composer, Segment
        >>> spec = [Segment("bpsk", num_samples=200), Segment("tone", freq=2e5)]
        >>> a = Composer(spec)
        >>> b = Composer.from_json(a.to_json())
        >>> bool(np.array_equal(a.compose(), b.compose()))
        True

        """
        ...

    def close(self) -> None:
        """Release the underlying C state (idempotent)."""
        ...

    def __enter__(self) -> Composer: ...
    def __exit__(self, *exc: object) -> None: ...

class Writer:
    """Stream ``complex64`` samples to a container (raw / csv / blue / sigmf).

    Pairs with :func:`doppler.wfmgen.readback.read_iq` to round-trip a file.

    Examples
    --------
    >>> import os, tempfile, numpy as np
    >>> from doppler.wfmgen.compose import Composer, Writer
    >>> from doppler.wfmgen.readback import read_iq
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
    >>> from doppler.wfmgen.compose import Composer, Writer, Reader
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
    >>> from doppler.wfmgen.compose import SampleClock
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
    >>> from doppler.wfmgen.compose import sigmf_meta, Segment
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
    >>> from doppler.wfmgen.compose import write_blue_header
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
    >>> from doppler.wfmgen.compose import rrc_taps
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
    >>> from doppler.wfmgen.compose import dsss_spread
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
    >>> from doppler.wfmgen.compose import mls_poly
    >>> hex(mls_poly(7))
    '0x41'

    """
    ...
