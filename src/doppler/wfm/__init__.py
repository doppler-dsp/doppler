# wfm/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .wfm import PN, bpsk_map, qpsk_map, wfm_awgn_amplitude, wfm_ebno_to_snr_db, _SynthEngine, mls_poly, rrc_taps, dsss_spread, crc16, Gold  # noqa: E402
from .sample_clock import SampleClock  # noqa: E402
from .wfm_sink import StreamSink  # noqa: E402
from .wfm_reader import Reader  # noqa: E402
from .wfm_writer import Writer  # noqa: E402
from .readback import read_iq  # noqa: E402
from .compose import Synth, tone, bpsk, qpsk, pn, noise, Segment, Timeline, Composer, write_blue_header, bits, chirp, Plan, prepare  # noqa: E402

__all__ = ["PN", "bpsk_map", "qpsk_map", "wfm_awgn_amplitude", "wfm_ebno_to_snr_db", "_SynthEngine", "mls_poly", "rrc_taps", "dsss_spread", "crc16", "Gold", "Writer", "Reader", "StreamSink", "SampleClock", "Synth", "tone", "bpsk", "qpsk", "pn", "noise", "Segment", "Timeline", "Composer", "write_blue_header", "bits", "chirp", "Plan", "prepare", "read_iq"]
