# wfm/__init__.py — re-export all types from the C extension.
import os as _os
import sys as _sys

if _sys.platform == "win32" and hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_os.path.dirname(_os.path.abspath(__file__)))
del _os, _sys

from .wfm import PN, bpsk_map, qpsk_map, wfm_awgn_amplitude, wfm_ebno_to_snr_db, _SynthEngine  # noqa: E402
from .sample_clock import SampleClock  # noqa: E402
from .wfm_sink import ZmqSink  # noqa: E402
from .wfm_reader import Reader  # noqa: E402
from .wfm_writer import Writer  # noqa: E402
from .readback import read_iq  # noqa: E402
from .compose import Synth, tone, bpsk, qpsk, pn, noise, Segment, Timeline, Composer, paced, sigmf_meta, write_blue_header, bits, chirp, rrc_taps, dsss_spread, mls_poly  # noqa: E402

__all__ = ["PN", "bpsk_map", "qpsk_map", "wfm_awgn_amplitude", "wfm_ebno_to_snr_db", "_SynthEngine", "rrc_taps", "dsss_spread", "mls_poly", "Synth", "tone", "bpsk", "qpsk", "pn", "noise", "Segment", "Timeline", "Composer", "paced", "sigmf_meta", "write_blue_header", "bits", "chirp", "Writer", "Reader", "ZmqSink", "SampleClock", "read_iq"]
