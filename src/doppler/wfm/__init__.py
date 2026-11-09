"""Waveform generation: PN and CCSDS Gold code generators (PN, Gold) and a configurable modulated-symbol synthesizer, with BLUE/SigMF writers and readers re-exported.

Examples
--------
>>> from doppler.wfm import Gold
>>> Gold().generate(1023)[:15].tolist()   # CCSDS Code #365
[0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1]

"""

# wfm/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .compose import (  # noqa: E402
    Composer,
    Plan,
    PlanFromBlob,
    PlanFromFile,
    Segment,
    Synth,
    Timeline,
    bits,
    bpsk,
    chirp,
    draws,
    noise,
    pn,
    prepare,
    qpsk,
    rotate,
    tone,
)
from .sample_clock import SampleClock  # noqa: E402
from .stage_kinds import (  # noqa: E402
    STAGE_CONV,
    STAGE_CRC16,
    STAGE_INTERLEAVE,
    STAGE_RANDOMISE,
    STAGE_RS,
    STAGE_USER,
)
from .wfm import (  # noqa: E402
    PN,
    Frame,
    FrameCheck,
    FrameDesc,
    Gold,
    _sweep_ends,
    _SynthEngine,
    bpsk_map,
    crc16,
    dsss_spread,
    field_bits,
    mls_poly,
    qpsk_map,
    rc_h,
    rrc_h,
    rrc_taps,
    wfm_awgn_amplitude,
    wfm_ebno_to_snr_db,
)
from .wfm_reader import (  # noqa: E402
    ENDIAN,
    FC_SOURCE,
    FOLLOW_END,
    FS_SOURCE,
    FTYPE,
    SAMPLE_MODE,
    STYPE,
    T0_SOURCE,
    Reader,
)
from .wfm_sink import StreamSink  # noqa: E402
from .wfm_writer import Writer, write_blue_header  # noqa: E402

__all__ = [
    "PN",
    "bpsk_map",
    "qpsk_map",
    "wfm_awgn_amplitude",
    "wfm_ebno_to_snr_db",
    "_SynthEngine",
    "mls_poly",
    "rrc_taps",
    "dsss_spread",
    "crc16",
    "Gold",
    "Writer",
    "Reader",
    "SampleClock",
    "write_blue_header",
    "rrc_h",
    "rc_h",
    "Frame",
    "FrameDesc",
    "FrameCheck",
    "StreamSink",
    "field_bits",
    "_sweep_ends",
    "FOLLOW_END",
    "FTYPE",
    "STYPE",
    "SAMPLE_MODE",
    "ENDIAN",
    "FS_SOURCE",
    "T0_SOURCE",
    "FC_SOURCE",
    "Synth",
    "tone",
    "bpsk",
    "qpsk",
    "pn",
    "noise",
    "Segment",
    "Timeline",
    "Composer",
    "bits",
    "chirp",
    "Plan",
    "prepare",
    "PlanFromBlob",
    "PlanFromFile",
    "draws",
    "rotate",
    "STAGE_CRC16",
    "STAGE_RS",
    "STAGE_RANDOMISE",
    "STAGE_CONV",
    "STAGE_INTERLEAVE",
    "STAGE_USER",
]
