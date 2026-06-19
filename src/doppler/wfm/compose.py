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

The samples come back as ``complex64`` arrays; use :class:`Reader` or
:class:`IqFile` to round-trip a file.

The ``Synth`` / ``Segment`` / ``Timeline`` / ``Composer`` ergonomic types, the
transport handles (``Writer`` / ``Reader`` / ``ZmqSink`` / ``SampleClock``), and
the module-level helpers (``write_blue_header``, ``rrc_taps``, ``dsss_spread``,
``mls_poly``) are all **jm-generated** CPython bindings — this file is pure
re-exports, no hand-written binding.

Examples
--------
>>> from doppler.wfm.compose import Composer, Segment
>>> spec = [Segment("pn", num_samples=127, pn_length=7),
...         Segment("tone", freq=1e5, num_samples=256, off_samples=64)]
>>> x = Composer(spec).compose()
>>> x.dtype, len(x)  # 127 (pn) + 256 (tone) + 64 (gap)
(dtype('complex64'), 447)
"""

from __future__ import annotations

# The transport surface is now the generated kind="handle" types — re-export
# them through compose so `doppler.wfm.compose.Writer` (etc.) stays the import
# path.
from .sample_clock import SampleClock  # noqa: F401  (re-export)
from .wfm_reader import Reader  # noqa: F401  (re-export)
from .wfm_sink import ZmqSink  # noqa: F401  (re-export)
from .wfm_writer import Writer  # noqa: F401  (re-export)

# The composer OO surface IS the generated .so type — re-export it verbatim.
# Synth/Segment/Timeline/Composer carry standalone generation, the
# pattern/f_start aliases, the flat Segment view, stream() and to_dict().
from .wfm_compose import (  # noqa: F401  (re-export)
    Composer,
    Segment,
    Synth,
    Timeline,
    bits,
    bpsk,
    chirp,
    noise,
    pn,
    qpsk,
    tone,
)

# write_blue_header is now a jm-generated module function (doppler.wfm).
# Re-export it here so doppler.wfm.compose.write_blue_header still resolves.
from .wfm import write_blue_header  # noqa: F401  (re-export)
