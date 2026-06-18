# compose.pyi — type stubs for the hand-written composer wrapper.
#
# Hand-owned (no_generate): the C extension is doppler.wfm._wfmcompose and the
# ergonomic API lives in compose.py. The doctests here are the CI-gated surface
# (pytest --doctest-glob='*.pyi'); they run against the built extension.
from __future__ import annotations

import os
from typing import Sequence

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

# rrc_taps / dsss_spread are generated module functions — see wfm.pyi.
