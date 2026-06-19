# compose.pyi — type stubs for compose.py re-exports.
#
# compose.py is now pure re-exports: all binding surface is jm-generated.
# This stub preserves the doctests that CI gates on.
from __future__ import annotations

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
# sample_clock.pyi.
from .sample_clock import SampleClock as SampleClock
from .wfm_reader import Reader as Reader
from .wfm_sink import ZmqSink as ZmqSink
from .wfm_writer import Writer as Writer

# write_blue_header is a generated module function (wfm.pyi).
# Re-exported here so doppler.wfm.compose.write_blue_header still resolves.
# The doctest exercises the build end-to-end.
def write_blue_header(
    path: str,
    total: int,
    sample_type: str = "cf32",
    endian: str = "le",
    fs: float = 1e6,
    fc: float = 0.0,
    data_start: float = 0.0,
    detached: int = 1,
) -> None:
    """Write a standalone BLUE type-1000 HCB header (the detached ``.hdr``).

    Examples
    --------
    >>> import os, tempfile
    >>> from doppler.wfm.compose import write_blue_header
    >>> p = os.path.join(tempfile.mkdtemp(), "cap.hdr")
    >>> write_blue_header(p, 512, sample_type="cf32", fs=1e6)
    >>> with open(p, "rb") as f:
    ...     head = f.read()
    >>> head[:4], len(head)
    (b'BLUE', 512)

    """
    ...

# rrc_taps / dsss_spread / mls_poly are generated module functions — see wfm.pyi.
