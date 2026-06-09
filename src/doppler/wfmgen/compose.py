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
:func:`doppler.wfmgen.readback.read_iq` to round-trip a file.

Examples
--------
>>> from doppler.wfmgen.compose import Composer, Segment
>>> spec = [Segment("pn", num_samples=127, pn_length=7),
...         Segment("tone", freq=1e5, num_samples=256, off_samples=64)]
>>> x = Composer(spec).compose()
>>> x.dtype, len(x)  # 127 (pn) + 256 (tone) + 64 (gap)
(dtype('complex64'), 447)
"""

from __future__ import annotations

import os
from dataclasses import dataclass
from typing import Sequence

import numpy as np
from numpy.typing import NDArray

from . import _wfmcompose as _c

# ── string-enum ↔ C-int tables (must match native/src/app/wfmgen.c) ──────────
_TYPES = ("tone", "noise", "pn", "bpsk", "qpsk")
_MODES = ("auto", "fs", "ebno", "esno")
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


@dataclass
class Segment:
    """One waveform segment of a composed stream.

    Fields mirror the C ``wfm_segment_t`` and the ``wfmgen`` CLI flags; the
    string fields (``type``, ``snr_mode``, ``lfsr``) are mapped to the C enums.
    Defaults match the CLI: a clean 1024-sample baseband tone at 1 MS/s.

    Parameters
    ----------
    type : {"tone", "noise", "pn", "bpsk", "qpsk"}
        Waveform kind.
    fs : float
        Sample rate (Hz).
    freq : float
        Carrier/offset frequency (Hz); normalised by ``fs`` internally.
    snr : float
        SNR in dB under ``snr_mode``; the default (100 dB) is numerically clean.
    snr_mode : {"auto", "fs", "ebno", "esno"}
        How ``snr`` is interpreted; ``auto`` resolves per ``type``.
    seed : int
        PRNG / LFSR seed (reproducible by default).
    sps : int
        Samples per symbol (PSK) or per chip (PN).
    pn_length : int
        LFSR register length (PN/PSK data source).
    pn_poly : int
        LFSR polynomial; ``0`` selects the maximal-length poly for ``pn_length``.
    lfsr : {"galois", "fibonacci"}
        LFSR topology.
    num_samples : int
        On-time length of the segment (samples).
    off_samples : int
        Trailing zero gap after the segment (samples).
    """

    type: str = "tone"
    fs: float = 1e6
    freq: float = 0.0
    snr: float = 100.0
    snr_mode: str = "auto"
    seed: int = 1
    sps: int = 8
    pn_length: int = 7
    pn_poly: int = 0
    lfsr: str = "galois"
    num_samples: int = 1024
    off_samples: int = 0

    def _tuple(self) -> tuple:
        """Fixed-order 12-tuple consumed by the C binding (enums → ints)."""
        return (
            _idx(self.type, _TYPES, "type"),
            float(self.fs),
            float(self.freq),
            float(self.snr),
            _idx(self.snr_mode, _MODES, "snr_mode"),
            int(self.seed),
            int(self.sps),
            int(self.pn_length),
            int(self.pn_poly),
            _idx(self.lfsr, _LFSRS, "lfsr"),
            int(self.num_samples),
            int(self.off_samples),
        )

    @classmethod
    def _from_tuple(cls, t: tuple) -> "Segment":
        return cls(
            type=_TYPES[t[0]],
            fs=t[1],
            freq=t[2],
            snr=t[3],
            snr_mode=_MODES[t[4]],
            seed=t[5],
            sps=t[6],
            pn_length=t[7],
            pn_poly=t[8],
            lfsr=_LFSRS[t[9]],
            num_samples=t[10],
            off_samples=t[11],
        )


class Composer:
    """A multi-segment waveform generator over a list of :class:`Segment`.

    Construct from an explicit segment list, from a single segment's keyword
    arguments, or from a JSON spec (:meth:`from_json` / :meth:`from_file`). Pull
    samples a block at a time with :meth:`execute`, or drain a finite spec to one
    array with :meth:`compose`. Usable as a context manager.

    Parameters
    ----------
    segments : Sequence[Segment] or None
        The segments to compose. If ``None``, a single segment is built from the
        remaining keyword arguments (see :class:`Segment`).
    repeat : bool, optional
        Loop the whole sequence after the last segment.
    continuous : bool, optional
        Never finish (implies ``repeat``); :meth:`compose` then raises.
    **segment_kwargs
        Used to build a single :class:`Segment` when ``segments`` is ``None``.

    Examples
    --------
    >>> from doppler.wfmgen.compose import Composer
    >>> with Composer(type="noise", snr=10.0, num_samples=4096) as c:
    ...     blk = c.execute(1024)
    >>> blk.dtype, len(blk)
    (dtype('complex64'), 1024)
    """

    def __init__(
        self,
        segments: Sequence[Segment] | None = None,
        *,
        repeat: bool = False,
        continuous: bool = False,
        **segment_kwargs,
    ) -> None:
        if segments is None:
            segments = [Segment(**segment_kwargs)]
        elif segment_kwargs:
            raise TypeError(
                "pass either a segments list or single-segment kwargs, not both"
            )
        tuples = [s._tuple() for s in segments]
        self._cap = _c.composer_create(tuples, bool(repeat), bool(continuous))

    @classmethod
    def from_json(cls, json: str) -> "Composer":
        """Build a composer from a JSON spec string (the ``--from-file`` schema)."""
        self = cls.__new__(cls)
        self._cap = _c.composer_from_json(json)
        return self

    @classmethod
    def from_file(cls, path: str | os.PathLike) -> "Composer":
        """Build a composer from a JSON spec file."""
        self = cls.__new__(cls)
        self._cap = _c.composer_from_file(os.fspath(path))
        return self

    def execute(self, n: int) -> NDArray[np.complex64]:
        """Generate up to ``n`` samples; a shorter (or empty) array marks the end."""
        return _c.composer_execute(self._cap, int(n))

    def compose(self, block: int = 4096) -> NDArray[np.complex64]:
        """Drain a finite spec into a single array (raises if ``continuous``)."""
        if self.continuous:
            raise ValueError(
                "cannot compose() a continuous spec; use execute()"
            )
        chunks = []
        while True:
            blk = self.execute(block)
            if len(blk) == 0:
                break
            chunks.append(blk)
        if not chunks:
            return np.empty(0, dtype=np.complex64)
        return np.concatenate(chunks)

    @property
    def _resolved(self) -> tuple[list[Segment], bool, bool]:
        segs, repeat, continuous = _c.composer_segments(self._cap)
        return [Segment._from_tuple(t) for t in segs], repeat, continuous

    @property
    def segments(self) -> list[Segment]:
        """The resolved segment list (defaults filled in)."""
        return self._resolved[0]

    @property
    def repeat(self) -> bool:
        return self._resolved[1]

    @property
    def continuous(self) -> bool:
        return self._resolved[2]

    def to_json(self) -> str:
        """Serialise the resolved spec to JSON (round-trips via :meth:`from_json`)."""
        segs, repeat, continuous = self._resolved
        return _c.spec_to_json([s._tuple() for s in segs], repeat, continuous)

    def close(self) -> None:
        """Release the underlying C state (idempotent)."""
        _c.composer_destroy(self._cap)

    def __enter__(self) -> "Composer":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


class Writer:
    """Stream ``complex64`` samples to a container file.

    Wraps the C writer for the four containers the CLI supports. For ``"sigmf"``
    the samples land in ``path`` (use ``<name>.sigmf-data``) and the companion
    metadata is produced by :func:`sigmf_meta`; for detached BLUE use
    :func:`write_blue_header`.

    Parameters
    ----------
    path : str or PathLike
        Output file (opened in binary mode; truncated).
    file_type : {"raw", "csv", "blue", "sigmf"}
        Output container.
    sample_type : {"cf32", "cf64", "ci32", "ci16", "ci8"}
        On-disk element type; integer types are quantised from unit-scale.
    endian : {"le", "be"}
        Byte order for multi-byte integer types (ignored for CSV).
    fs, fc : float
        Sample rate and centre frequency (Hz), recorded in BLUE/SigMF metadata.
    total : int, optional
        Expected sample count (lets seekable BLUE headers be patched on close).

    Examples
    --------
    >>> import tempfile, os
    >>> from doppler.wfmgen.compose import Composer, Writer
    >>> from doppler.wfmgen.readback import read_iq
    >>> x = Composer(type="tone", freq=1e5, num_samples=512).compose()
    >>> p = os.path.join(tempfile.mkdtemp(), "cap.cf32")
    >>> with Writer(p, sample_type="cf32") as w:
    ...     _ = w.write(x)
    >>> import numpy as np
    >>> bool(np.allclose(read_iq(p, "cf32"), x))
    True
    """

    def __init__(
        self,
        path: str | os.PathLike,
        *,
        file_type: str = "raw",
        sample_type: str = "cf32",
        endian: str = "le",
        fs: float = 1e6,
        fc: float = 0.0,
        total: int = 0,
    ) -> None:
        self._cap = _c.writer_open(
            os.fspath(path),
            _idx(file_type, _FTYPES, "file_type"),
            _idx(sample_type, _STYPES, "sample_type"),
            _idx(endian, _ENDIANS, "endian"),
            float(fs),
            float(fc),
            int(total),
        )

    def write(self, iq: NDArray[np.complex64]) -> int:
        """Write a block of samples; returns the number written."""
        return _c.writer_write(self._cap, iq)

    def close(self) -> None:
        """Flush, patch any header, and close the file (idempotent)."""
        _c.writer_close(self._cap)

    def __enter__(self) -> "Writer":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


class Reader:
    """Read a capture back to ``complex64`` — the dual of :class:`Writer`.

    The container is **auto-detected** from the file (BLUE ``"BLUE"`` magic, a
    ``.sigmf-meta`` sidecar, the ``.csv`` extension, else raw), and
    self-describing containers (BLUE, SigMF) recover the sample type, byte
    order, sample rate and centre frequency from their metadata. Headerless raw
    / CSV take the ``sample_type`` / ``endian`` hints. All detection, header
    parsing and wire→unit conversion happen in C; this class is thin glue.

    Parameters
    ----------
    path : str or PathLike
        Capture to read. A BLUE ``.det`` or SigMF ``.sigmf-data`` data file
        resolves its ``.hdr`` / ``.sigmf-meta`` sidecar automatically.
    sample_type : {"cf32", "cf64", "ci32", "ci16", "ci8"}
        Wire type for headerless raw / CSV (ignored once BLUE/SigMF metadata is
        parsed).
    endian : {"le", "be"}
        Byte order for headerless raw.

    Examples
    --------
    >>> import tempfile, os, numpy as np
    >>> from doppler.wfmgen.compose import Composer, Writer, Reader
    >>> x = Composer(type="tone", freq=1e5, num_samples=512).compose()
    >>> p = os.path.join(tempfile.mkdtemp(), "cap.blue")
    >>> with Writer(p, file_type="blue", fs=1e6) as w:
    ...     _ = w.write(x)
    >>> with Reader(p) as r:            # BLUE self-describes — no hints needed
    ...     y = r.read_all()
    ...     print(r.file_type, int(r.fs), bool(np.allclose(y, x)))
    blue 1000000 True
    """

    def __init__(
        self,
        path: str | os.PathLike,
        *,
        sample_type: str = "cf32",
        endian: str = "le",
    ) -> None:
        self._cap = _c.reader_open(
            os.fspath(path),
            _idx(sample_type, _STYPES, "sample_type"),
            _idx(endian, _ENDIANS, "endian"),
        )
        # Metadata is fixed for the open capture; resolve it once.
        self._info = _c.reader_info(self._cap)

    @property
    def file_type(self) -> str:
        """Detected container: ``"raw"`` / ``"csv"`` / ``"blue"`` / ``"sigmf"``."""
        return _FTYPES[self._info[0]]

    @property
    def sample_type(self) -> str:
        """Resolved wire sample type."""
        return _STYPES[self._info[1]]

    @property
    def endian(self) -> str:
        """Resolved byte order."""
        return _ENDIANS[self._info[2]]

    @property
    def fs(self) -> float:
        """Sample rate (Hz); ``0.0`` if the container doesn't carry it."""
        return self._info[3]

    @property
    def fc(self) -> float:
        """Centre frequency (Hz); ``0.0`` if not recorded."""
        return self._info[4]

    @property
    def num_samples(self) -> int:
        """Total complex samples available; ``0`` if unknown (a stream)."""
        return self._info[5]

    def read(self, n: int) -> NDArray[np.complex64]:
        """Read up to ``n`` samples; a shorter (or empty) array marks EOF."""
        return _c.reader_read(self._cap, int(n))

    def read_all(self, block: int = 65536) -> NDArray[np.complex64]:
        """Drain the whole capture into one ``complex64`` array."""
        chunks = []
        while True:
            blk = self.read(block)
            if len(blk) == 0:
                break
            chunks.append(blk)
        if not chunks:
            return np.empty(0, dtype=np.complex64)
        return np.concatenate(chunks)

    def close(self) -> None:
        """Close the file (idempotent)."""
        _c.reader_close(self._cap)

    def __enter__(self) -> "Reader":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


class ZmqSink:
    """Publish ``complex64`` samples over a ZeroMQ PUB socket (POSIX only).

    Each :meth:`send` frames the block with its sample rate and centre frequency
    and the chosen ``sample_type`` wire format — the same framing the C CLI's
    ``--output zmq://…`` uses.

    Parameters
    ----------
    endpoint : str
        ZeroMQ endpoint, e.g. ``"tcp://0.0.0.0:5555"``.
    sample_type : {"cf32", "cf64", "ci32", "ci16", "ci8"}
        Wire element type.
    """

    def __init__(self, endpoint: str, *, sample_type: str = "cf32") -> None:
        if not hasattr(_c, "sink_open"):
            raise NotImplementedError(
                "ZmqSink is not available on this platform"
            )
        self._cap = _c.sink_open(
            endpoint, _idx(sample_type, _STYPES, "sample_type")
        )

    def send(
        self, iq: NDArray[np.complex64], fs: float, fc: float = 0.0
    ) -> None:
        """Publish a block tagged with its sample rate and centre frequency."""
        _c.sink_send(self._cap, iq, float(fs), float(fc))

    def close(self) -> None:
        """Close the publisher (idempotent)."""
        _c.sink_close(self._cap)

    def __enter__(self) -> "ZmqSink":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


class SampleClock:
    """Pace and timestamp a stream against an ideal ``fs``-Hz clock (POSIX).

    A :class:`SampleClock` mimics a hardware sample clock in software. Off one
    drift-free timeline anchored at construction it does two things:

    - :meth:`pace` — sleep so each block leaves at its real-time deadline
      ``epoch + n/fs``, throttling a producer (e.g. a :class:`Composer` feeding
      a :class:`ZmqSink`) to real time.
    - :meth:`stamp` — return the ideal UNIX-epoch-ns time of the next sample,
      for reproducible capture metadata (SigMF ``core:datetime``, records).

    The schedule is anchored, not incremental: every deadline is recomputed
    from the cumulative sample count against a fixed epoch, so an over- or
    under-sleep on one block is corrected on the next — the long-run rate is
    exactly ``fs``, with only bounded per-block jitter. Software on a
    non-realtime OS gives drift-free *average* rate, never true sample-clock
    fidelity; keep blocks large enough (period ``N/fs`` ≫ scheduler jitter,
    say ≥ 1 ms) and let the consumer's buffer absorb the jitter.

    When the producer can't keep up — a block takes longer than its ``N/fs``
    period — that's an *underrun*: :meth:`pace` doesn't sleep (it's already
    behind), counts it (:attr:`underruns`, :attr:`max_lateness`), and, if
    ``resync=True``, re-anchors the timeline to now instead of keeping the
    (now unreachable) absolute schedule.

    Parameters
    ----------
    fs : float
        Sample rate (Hz). Must be > 0.
    resync : bool, optional
        Re-anchor the timeline to "now" on each underrun (default: keep the
        absolute schedule and let the average rate self-heal if it catches up).

    Examples
    --------
    >>> from doppler.wfmgen.compose import SampleClock
    >>> clk = SampleClock(fs=1e6)
    >>> slack = clk.pace(1000)        # advance 1000 samples (~1 ms) and wait
    >>> clk.samples
    1000
    >>> isinstance(clk.stamp(), int)  # ideal ns timestamp of the next sample
    True
    """

    def __init__(self, fs: float, *, resync: bool = False) -> None:
        if not hasattr(_c, "clock_create"):
            raise NotImplementedError(
                "SampleClock is not available on this platform"
            )
        self._cap = _c.clock_create(float(fs), bool(resync))

    def pace(self, count: int) -> float:
        """Advance ``count`` samples and sleep to that block's deadline.

        Returns the slack in seconds measured before sleeping: ``>= 0`` means
        the block was early (and it slept that long); ``< 0`` means it arrived
        late — an underrun (no sleep, counted).
        """
        return _c.clock_pace(self._cap, int(count))

    def stamp(self) -> int:
        """Ideal UNIX-epoch-ns timestamp of the next sample (index ``n``).

        Call before :meth:`pace` to tag the block you're about to emit, or
        after to tag the following one.
        """
        return _c.clock_stamp(self._cap)

    def reset(self) -> None:
        """Re-anchor to now and zero the counters — a fresh clock at ``n=0``."""
        _c.clock_reset(self._cap)

    def resync(self) -> None:
        """Drop accumulated lateness; pace forward from now (keeps ``n``)."""
        _c.clock_resync(self._cap)

    @property
    def samples(self) -> int:
        """Cumulative samples advanced through :meth:`pace`."""
        return _c.clock_stats(self._cap)[0]

    @property
    def underruns(self) -> int:
        """Number of :meth:`pace` calls that arrived past their deadline."""
        return _c.clock_stats(self._cap)[1]

    @property
    def max_lateness(self) -> float:
        """Worst lateness observed (seconds); ``0.0`` if never behind."""
        return _c.clock_stats(self._cap)[2] / 1e9


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
        [s._tuple() for s in segments],
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
    >>> from doppler.wfmgen.compose import write_blue_header
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
    >>> from doppler.wfmgen.compose import rrc_taps
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
    >>> from doppler.wfmgen.compose import dsss_spread
    >>> syms = np.array([1+0j, -1+0j], dtype=np.complex64)
    >>> code = np.array([0, 1, 0, 1], dtype=np.uint8)
    >>> dsss_spread(syms, code, 4).shape
    (8,)
    """
    return _c.dsss_spread(syms, code, int(sf))


def mls_poly(n: int) -> int:
    """Maximal-length-sequence primitive polynomial for an LFSR of length ``n``.

    Mirrors the table the synth/PN engine uses for ``pn_poly=0``; valid for
    ``n`` in 2..64 (returns 0 otherwise).

    Examples
    --------
    >>> from doppler.wfmgen.compose import mls_poly
    >>> hex(mls_poly(7))
    '0x41'
    """
    return _c.mls_poly(int(n))
