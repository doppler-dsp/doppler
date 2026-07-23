# compose.pyi — type stubs for the hand-written composer wrapper.
#
# Hand-owned: the ergonomic composer + Plan API lives in compose.py, wrapping
# the generated wfm extension types. The doctests here are the CI-gated surface
# (pytest --doctest-glob='*.pyi'); they run against the built extension.
from __future__ import annotations

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
from .wfm_sink import StreamSink as StreamSink
from .wfm_writer import Writer as Writer

# sigmf_meta is now the generated Composer.to_sigmf() method (see wfm_compose.pyi).
# write_blue_header is now a generated wfm_writer module function (see
# wfm_writer.pyi) — no longer a hand binding here.

# rrc_taps / dsss_spread are generated module functions — see wfm.pyi.
