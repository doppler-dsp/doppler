"""Pin wfmgen's resolved behaviour, one case per flag.

Why this exists
---------------
``wfmgen`` accepts 51 flags and the CLI test covered 23 of them. gh-723
replaced the 49-arm ``else if`` chain that resolved them with a flag
table, which is a rewrite of how every one of those 51 flags reaches its
field -- exactly the change a 45%-covered suite cannot police. So the
coverage came first and the rewrite was measured against it.

What it pins
------------
Two things, because ``--record`` alone does not see the whole surface:

* the **resolved spec** ``--record`` writes -- type, fs, freq, snr,
  snr_mode, seed, sps, pn geometry, pulse shaping, gaps, repeats. That
  is the parse result serialised, which is precisely what a table-driven
  parser must reproduce.
* the **size** of the emitted bytes, which distinguishes the sample-type
  widths and the container formats that never appear in the record.

plus the exit code, so the usage-error paths are pinned too.

The golden deliberately does NOT hash the output. It used to, and that
made it machine-specific: rebuilding the identical source with ``-O0``
instead of the project's ``-O3 -march=x86-64-v2 -ffast-math`` leaves the
record byte-identical and changes every waveform hash, because float
rounding is a property of the toolchain. CI's flags are not this
machine's, so the hash failed there and passed here -- a golden that
encodes the builder rather than the behaviour.

What the hash was there for -- ``--endian`` and ``--sample-type``, whose
effect never reaches the record -- is covered by RELATIONAL checks
instead (``relational_checks``): big-endian output must be the
little-endian output with each element reversed, and a narrower sample
type must produce proportionally fewer bytes. Those hold whatever the
compiler does to the last mantissa bit.

Fail-closed
-----------
``--check`` fails if any flag in the dispatcher is missing from the case
table. A harness that silently stops covering a flag is worse than no
harness, because it reads as coverage. The flag list is derived from the
source rather than restated here, so a new flag fails this gate on the
commit that adds it.

Deriving it from source has its own failure mode, and it fired once: the
gh-723 rewrite changed the shape being scanned, and a discovery that
matches nothing would have reported full coverage of an empty set.
``dispatcher_flags`` therefore hard-fails unless it finds a handful of
anchor flags that will exist for as long as the tool does.

Usage
-----
``make wfmgen-flag-matrix`` regenerates the golden
(``native/tests/wfmgen_flag_matrix.json``) against ``build/``'s wfmgen,
through ``uv``. It is the only supported way to refresh it: the Makefile is
the one place that says how a tool runs (#1627). ctest runs the check as
``wfmgen_flag_matrix`` (``--check``), and its failure message names the
target.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_REL = "native/src/app/wfmgen.c"
# The flags that set a source or segment field are rows of this generated
# table rather than of wfmgen.c's own OPTS (doppler#853).
SURFACE_REL = "native/inc/doppler/wfm/wfm_surface.h"
SRC = ROOT / SRC_REL
GOLDEN = ROOT / "native" / "tests" / "wfmgen_flag_matrix.json"

# Flags the dispatcher accepts but that this matrix deliberately does not
# drive, each for a reason that would make the case meaningless rather
# than merely awkward.
SKIP = {
    # Aliases of a flag already covered; same arm, same field.
    "-o": "alias of --output, covered by out_file",
    # Writes a PAIR of files rather than one, so there is no single output
    # for this matrix to size. It does not spawn anything: --detached
    # selects BLUE's detached-header format, the HCB in <out>.hdr and the
    # samples in <out>.det. The reason here used to say "spawns a background
    # process", which is what the tool's own --help said too (gh-725); the
    # pair is covered file by file in wfmgen_cli_test.cmake.
    "--detached": "writes <out>.hdr + <out>.det; see wfmgen_cli_test.cmake",
    # Paces to wall-clock. Pinning it would make the suite sleep.
    "--realtime": "paces to wall clock; would make the suite sleep",
    "--realtime-resync": "only meaningful with --realtime",
    # These two do not terminate, and that is the whole point of them:
    # --continuous is "stream continuously (no defined end)" and --repeat
    # is "loop the spec indefinitely". An earlier version of this file
    # drove --continuous as an ordinary case; it wrote into a temp file
    # until it filled a 7.8 GB tmpfs, and because the file was unlinked
    # while the process still held it open, `du` then showed nothing while
    # `df` showed the disk gone. TIMEOUT below is the backstop, but a case
    # whose success criterion is "never finishes" has nothing to pin.
    "--continuous": "never terminates by design; nothing to pin",
    "--repeat": "loops indefinitely by design; nothing to pin",
    # Not under-covered but over-covered: run_case appends it to EVERY
    # case, because its output is what this matrix pins. Listed here so the
    # coverage check stays exhaustive rather than being loosened to ignore
    # flags the harness supplies itself.
    "--record": "driven on every case; its output IS the matrix",
}

# No case may run unbounded. Every wfmgen invocation here is a handful of
# samples, so this is 100x the honest worst case and only ever fires on a
# flag that does not terminate -- which is a finding, not a flake.
TIMEOUT_S = 20

#: Floor for `replay_checks()`. Set just under what the table replays today
#: so the count may grow and not silently collapse: every skip in that
#: function is legitimate on its own, and a change that turned them all on
#: at once would leave the check passing while looking at nothing.
MIN_REPLAY_CASES = 25

BITS_FILE = "bits.bin"
SYMS_FILE = "syms.cf32"
SCENE_FILE = "scene.json"

# The frame DESCRIPTIONS the `--frame` cases read, in the form a scene's
# "frame" key holds. Each is the frame a retired coding flag used to build,
# written out: fields in wire order, and stages that name the span each
# covers -- `first_field` and `n_fields` -- in application order. A derived
# field (a CRC trailer, the outer code's parity) is its length in bits and
# the stage that produces it, `derived_by` = that stage's index plus one.
# A 223-octet Transfer Frame, the RS(255,223) message: the frame's payload
# is its data:LEN field (#1718), and the case gives the bits with --data.
_TF_BITS = 223 * 8
_TF = f"data:{_TF_BITS}"
_TF_DATA = "0x" + "f" * (_TF_BITS // 4)  # the Transfer Frame: all ones
_RS_PARITY = {"name": "rs_parity", "bits": 32 * 8, "derived_by": 1}
FRAMES = {
    # payload + CRC-16, block-interleaved 8 deep over bits
    "interleave_bits.frame.json": {
        "fields": [
            {"name": "payload", "spec": _TF},
            {"name": "crc", "bits": 16, "derived_by": 1},
        ],
        "stages": [
            {"kind": "crc16", "first_field": 0, "n_fields": 2},
            {
                "kind": "interleave",
                "first_field": 0,
                "n_fields": 2,
                "depth": 8,
            },
        ],
    },
    # the outer code, then 5 deep over octets across its codewords
    "interleave_rs.frame.json": {
        "fields": [{"name": "payload", "spec": _TF}, _RS_PARITY],
        "stages": [
            {"kind": "rs", "first_field": 0, "n_fields": 2, "depth": 1},
            {
                "kind": "interleave",
                "first_field": 0,
                "n_fields": 2,
                "depth": 5,
                "unit_bits": 8,
            },
        ],
    },
    # a CCSDS CADU: the marker, the outer code, the randomiser over the
    # data group only, and the inner code over everything
    "cadu.frame.json": {
        "fields": [
            {"name": "asm", "spec": "0x1ACFFC1D"},
            {"name": "payload", "spec": _TF},
            _RS_PARITY,
        ],
        "stages": [
            {"kind": "rs", "first_field": 1, "n_fields": 2, "depth": 1},
            {"kind": "randomise", "first_field": 1, "n_fields": 2, "depth": 1},
            {
                "kind": "conv",
                "first_field": 0,
                "n_fields": 3,
                "emit_num": 2,
                "emit_den": 1,
            },
        ],
    },
    # a DSSS burst's SPREAD frame: marker, Barker-13 sync, payload, CRC,
    # randomised over the data group and inner-coded over all of it
    "dsss_coded.frame.json": {
        "fields": [
            {"name": "asm", "spec": "0x1ACFFC1D"},
            {"name": "sync", "spec": "1111100110101"},
            {"name": "payload", "spec": "data:11"},
            {"name": "crc", "bits": 16, "derived_by": 1},
        ],
        "stages": [
            {"kind": "crc16", "first_field": 2, "n_fields": 2},
            {"kind": "randomise", "first_field": 2, "n_fields": 2, "depth": 1},
            {
                "kind": "conv",
                "first_field": 0,
                "n_fields": 4,
                "emit_num": 2,
                "emit_den": 1,
            },
        ],
    },
    # an outer code over one octet: not 223*I octets, so refused
    "rs_short.frame.json": {
        "fields": [
            {"name": "payload", "spec": "data:8"},
            {"name": "crc", "bits": 16, "derived_by": 1},
            {"name": "rs_parity", "bits": 32 * 8, "derived_by": 2},
        ],
        "stages": [
            {"kind": "crc16", "first_field": 0, "n_fields": 2},
            {"kind": "rs", "first_field": 0, "n_fields": 3, "depth": 1},
        ],
    },
}
# The same CADU with 10.4.2's legacy randomiser: the generator is the stage's
# depth (1 = 10.4.1's, 2 = 10.4.2's), which a record has to carry.
FRAMES["cadu_legacy.frame.json"] = json.loads(
    json.dumps(FRAMES["cadu.frame.json"])
)
FRAMES["cadu_legacy.frame.json"]["stages"][1]["depth"] = 2
# ...and with a generator B-6 does not define: refused, never read as 10.4.1
# (doppler#1609 on the description face -- the only face that codes a frame).
FRAMES["cadu_rand3.frame.json"] = json.loads(
    json.dumps(FRAMES["cadu.frame.json"])
)
FRAMES["cadu_rand3.frame.json"]["stages"][1]["depth"] = 3

#: Every file a case READS, written by fixtures(). A case's outputs are the
#: other files in its directory, so this is what tells the two apart.
FIXTURES = frozenset({BITS_FILE, SYMS_FILE, SCENE_FILE, *FRAMES})


def cases() -> list[tuple[str, list[str]]]:
    """(name, argv) pairs. Every non-SKIP flag must appear at least once."""
    return [
        # ---- waveform types, and the signal params each one resolves ----
        (
            "tone",
            [
                "--type",
                "tone",
                "--freq",
                "0.1",
                "--fs",
                "48000",
                "--count",
                "32",
                "--seed",
                "3",
            ],
        ),
        (
            "noise",
            [
                "--type",
                "noise",
                "--snr",
                "10",
                "--snr-mode",
                "fs",
                "--count",
                "32",
            ],
        ),
        (
            "pn",
            [
                "--type",
                "pn",
                "--pn-length",
                "7",
                "--pn-poly",
                "0",
                "--lfsr",
                "fibonacci",
                "--sps",
                "2",
                "--count",
                "32",
            ],
        ),
        (
            "bpsk_rrc",
            [
                "--type",
                "bpsk",
                "--sps",
                "4",
                "--pulse",
                "rrc",
                "--rrc-beta",
                "0.5",
                "--rrc-span",
                "6",
                "--count",
                "32",
            ],
        ),
        (
            "qpsk_level",
            [
                "--type",
                "qpsk",
                "--sps",
                "2",
                "--level",
                "-3",
                "--headroom",
                "1",
                "--count",
                "32",
                "--snr-mode",
                "esno",
                "--snr",
                "12",
            ],
        ),
        (
            "chirp",
            [
                "--type",
                "chirp",
                "--freq",
                "0.1",
                "--f-end",
                "0.4",
                "--count",
                "32",
            ],
        ),
        # ---- clock Doppler: the ppm pair, its carrier, and the lifetime ----
        # Two cases rather than one, because the lifetime only means anything
        # over `repeats` -- a single-instance run resolves both lifetimes to
        # the same spec and would pin the flag without exercising it.
        (
            "doppler_scalar",
            [
                "--type",
                "qpsk",
                "--fs",
                "1e6",
                "--sps",
                "4",
                "--count",
                "64",
                "--doppler",
                "25",
                "--doppler-rate",
                "4",
                "--carrier-hz",
                "2.4e9",
            ],
        ),
        (
            "doppler_persist_ranged",
            [
                "--type",
                "bpsk",
                "--fs",
                "1e6",
                "--sps",
                "8",
                "--count",
                "32",
                "--off",
                "16",
                "--repeats",
                "3",
                "--doppler",
                "2:9",
                "--doppler-rate",
                "0.1:0.5",
                "--carrier-hz",
                "1.5e9",
                "--doppler-lifetime",
                "persist",
            ],
        ),
        # ---- bits input, all three sources and the modulation knob ----
        (
            "bits_literal",
            [
                "--type",
                "bits",
                "--data",
                "10110010",
                "--modulation",
                "qpsk",
                "--sps",
                "2",
            ],
        ),
        (
            "bits_hex",
            [
                "--type",
                "bits",
                "--data",
                "0xb2",
                "--modulation",
                "none",
            ],
        ),
        (
            "bits_file",
            [
                "--type",
                "bits",
                "--data-from-file",
                BITS_FILE,
            ],
        ),
        # ---- #1619 F6a: the payload as a data source ----
        # Code-only continuous dsss is its own switch; `--data` is the
        # Field grammar, so the old `--data prbs|none` is refused naming it.
        (
            "dsss_code_only",
            [
                "--type",
                "dsss",
                "--symbol-rate",
                "100",
                "--data-code",
                "1011",
                "--code-only",
                "--fs",
                "48000",
                "--count",
                "64",
            ],
        ),
        (
            "err_data_prbs",
            [
                "--type",
                "dsss",
                "--symbol-rate",
                "100",
                "--data-code",
                "1011",
                "--data",
                "prbs",
            ],
        ),
        # A finite source sets the run's length: 24 bits in 8-bit frames
        # with a CRC, no --count.
        (
            "data_field",
            ["--type", "bpsk", "--data", "0xABCDEF", "--data-len", "8"],
        ),
        # 12 bits do not divide into 8-bit frames: --fill pads the last.
        (
            "data_fill",
            [
                "--type",
                "bpsk",
                "--data",
                "0xABC",
                "--data-len",
                "8",
                "--fill",
                "01",
            ],
        ),
        (
            "data_from_file",
            [
                "--type",
                "bits",
                "--data-from-file",
                BITS_FILE,
                "--data-len",
                "8",
            ],
        ),
        (
            "err_data_and_data_from_file",
            [
                "--type",
                "bpsk",
                "--data",
                "0xAB",
                "--data-from-file",
                BITS_FILE,
            ],
        ),
        (
            "symbols",
            [
                "--type",
                "symbols",
                "--symbols-file",
                SYMS_FILE,
                "--sps",
                "1",
                "--count",
                "8",
            ],
        ),
        # ---- dsss: burst frame, hex spelling, and continuous mode ----
        # An UNSPREAD frame. The same five flags as dsss_burst on a `bits`
        # source: for a long time they were accepted here and applied only on
        # dsss, so the record below is the part that matters — it pins that
        # what was recorded is what was generated. See gh-755.
        (
            "bits_framed",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--data",
                "10110010",
                "--acq-code",
                "1010*2",
                "--sync",
                "1111100110101",
                "--crc",
                "crc16",
                "--sps",
                "2",
            ],
        ),
        # ---- generated sequences: the kinds a face could not spell ----
        # gh-762. `wfm_seq_t` has always had PN/Gold/Dotted and the frame
        # layer materialised all of them, but every route in flattened the
        # kind to LITERAL, so no command line could ask for one. These three
        # cases exist to pin the RECORD as much as the waveform: a generated
        # sequence has no bit string, so before it had a key of its own the
        # field left no trace in `--record` at all and `--from-file` rebuilt
        # an unframed waveform at exit 0.
        (
            "bits_sync_gen_pn",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--data",
                "10110010",
                "--sync",
                "pn:31:5:3",
                "--sps",
                "2",
            ],
        ),
        (
            "bits_sync_gen_gold",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--data",
                "10110010",
                "--sync",
                "gold:64:10:0x3a6:0x15e:0x237:0x49",
                "--sps",
                "2",
            ],
        ),
        (
            "dsss_codes_gen",
            [
                "--type",
                "dsss",
                "--data",
                "1011",
                "--acq-code",
                "dotted:8*2",
                "--data-code",
                "pn:7:3:1",
                "--sync",
                "gold:16:10:934:350:567:73",
                "--sps",
                "2",
            ],
        ),
        # ---- coded frames: a description, `--frame FILE` (#853 item 11) ----
        # The coding flags are gone; a coded frame is a DESCRIPTION whose
        # stages name the spans they cover. Each case below is the frame the
        # retired flags used to build, written as data (FRAMES, above), and
        # was proven byte-identical to the flag-spelled run it replaces
        # before those flags were deleted.
        #
        # The interleaver is two cases rather than one, because depth and
        # unit are not the same kind of thing: the depth selects the stage,
        # the unit selects WHAT it permutes, and only the second is easy to
        # get wrong in a way that still produces a waveform.
        (
            "bits_frame_interleave_bits",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--frame",
                "interleave_bits.frame.json",
                "--data",
                _TF_DATA,
                "--sps",
                "1",
            ],
        ),
        (
            # Depth 5 over octets, behind the outer code: one codeword per
            # row, so a burst spreads one symbol into each.
            "bits_frame_interleave_octets_with_outer_code",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--frame",
                "interleave_rs.frame.json",
                "--data",
                _TF_DATA,
                "--sps",
                "1",
            ],
        ),
        # A 223-octet Transfer Frame with the marker, the outer code, the
        # randomiser and the inner code IS a CCSDS CADU -- and the COVERAGE
        # asymmetry between the stages is the interesting property, which
        # only appears with all of them together.
        (
            "bits_frame_ccsds_cadu",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--frame",
                "cadu.frame.json",
                "--data",
                _TF_DATA,
                "--sps",
                "1",
            ],
        ),
        # The SAME CADU with the legacy randomiser: "which generator" is the
        # axis a record has to carry, because only the matching receiver
        # derandomises a given waveform.
        (
            "bits_frame_ccsds_cadu_legacy_rand",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--frame",
                "cadu_legacy.frame.json",
                "--data",
                _TF_DATA,
                "--sps",
                "1",
            ],
        ),
        # A randomise stage naming no generator (depth 3) is refused, exit 2
        # -- it used to build 10.4.1's waveform silently (doppler#1609).
        (
            "err_frame_randomise_depth3",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--frame",
                "cadu_rand3.frame.json",
                "--data",
                _TF_DATA,
                "--sps",
                "1",
            ],
        ),
        # A coded DSSS burst: the description is the SPREAD frame, and the
        # acquisition preamble stays on the source, unspread.
        (
            "dsss_frame_coded",
            [
                "--type",
                "dsss",
                "--acq-code",
                "1010*2",
                "--data-code",
                "1011",
                "--frame",
                "dsss_coded.frame.json",
                "--data",
                "10110010101",
                "--sps",
                "2",
            ],
        ),
        # The outer code refuses a payload off the 223*I grid rather than
        # padding it -- virtual fill is not implemented (gh-813). With the
        # flag gone the refusal is the kernel's, asked by assembling.
        (
            "err_frame_rs_short_payload",
            [
                "--type",
                "bits",
                "--frame",
                "rs_short.frame.json",
                "--data",
                "10110010",
            ],
        ),
        # A carried frame is the whole frame: the common-frame flags beside
        # it are refused rather than silently dropped.
        (
            "err_frame_with_sync",
            [
                "--type",
                "bits",
                "--frame",
                "cadu.frame.json",
                "--data",
                _TF_DATA,
                "--sync",
                "1111100110101",
            ],
        ),
        (
            "err_frame_with_crc",
            [
                "--type",
                "bits",
                "--frame",
                "cadu.frame.json",
                "--data",
                _TF_DATA,
                "--crc",
                "crc16",
            ],
        ),
        (
            "err_frame_with_from_file",
            ["--from-file", SCENE_FILE, "--frame", "cadu.frame.json"],
        ),
        (
            "err_frame_not_a_frame",
            [
                "--type",
                "bits",
                "--frame",
                BITS_FILE,
                "--count",
                "64",
            ],
        ),
        (
            "dsss_burst",
            [
                "--type",
                "dsss",
                "--acq-code",
                "1010*2",
                "--data-code",
                "1011",
                "--sync",
                "1111100110101",
                "--crc",
                "crc16",
                "--data",
                "10101010",
                "--sps",
                "2",
            ],
        ),
        (
            "dsss_burst_hex",
            [
                "--type",
                "dsss",
                "--acq-code",
                "0xa5",
                "--data-code",
                "0xb2",
                "--data",
                "0x0f",
                "--crc",
                "none",
                "--sps",
                "2",
            ],
        ),
        (
            "dsss_continuous",
            [
                "--type",
                "dsss",
                "--symbol-rate",
                "100",
                "--data-code",
                "1011",
                "--fs",
                "48000",
                "--count",
                "64",
            ],
        ),
        # ---- timeline: gaps, repeats, and the composition switches ----
        (
            "gaps",
            [
                "--type",
                "tone",
                "--count",
                "16",
                "--off",
                "8",
                "--delay",
                "4",
                "--gap-noise",
                "off",
                "--repeats",
                "2",
            ],
        ),
        (
            "seed_advance",
            ["--type", "tone", "--count", "8", "--seed-advance", "all"],
        ),
        ("fc_meta", ["--type", "tone", "--fc", "2400000", "--count", "8"]),
        # ---- clipping reporters ----
        (
            "clip_report",
            [
                "--type",
                "tone",
                "--level",
                "0",
                "--count",
                "8",
                "--clip-report",
            ],
        ),
        (
            "clip_error",
            ["--type", "tone", "--level", "0", "--count", "8", "--clip-error"],
        ),
        # ---- output side: only the emitted bytes witness these ----
        (
            "out_file",
            [
                "--type",
                "tone",
                "--count",
                "8",
                "--sample-type",
                "ci16",
                "--file-type",
                "raw",
                "--endian",
                "be",
                "--output",
                "out.bin",
            ],
        ),
        (
            "out_csv",
            [
                "--type",
                "tone",
                "--count",
                "4",
                "--file-type",
                "csv",
                "--output",
                "out.csv",
            ],
        ),
        (
            "out_blue",
            [
                "--type",
                "tone",
                "--count",
                "4",
                "--file-type",
                "blue",
                "--sample-type",
                "cf32",
                "--output",
                "out.blue",
            ],
        ),
        (
            "out_sigmf",
            [
                "--type",
                "tone",
                "--count",
                "4",
                "--file-type",
                "sigmf",
                "--output",
                "out_sigmf",
            ],
        ),
        # ---- scene replay ----
        ("from_file", ["--from-file", SCENE_FILE]),
        # ---- usage errors: the exit codes are behaviour too ----
        ("err_unknown", ["--nope"]),
        ("err_missing_value", ["--type", "tone", "--freq"]),
        ("err_bad_choice", ["--type", "tone", "--pulse", "nonsense"]),
        (
            "err_rrc_beta",
            [
                "--type",
                "bpsk",
                "--pulse",
                "rrc",
                "--rrc-beta",
                "5",
                "--count",
                "4",
            ],
        ),
        (
            "err_dsss_burst_flag_in_continuous",
            [
                "--type",
                "dsss",
                "--symbol-rate",
                "100",
                "--data-code",
                "1011",
                "--acq-code",
                "1010",
            ],
        ),
        # A GENERATED data code is a data code; the check read the
        # pointer (#1592).
        (
            "dsss_continuous_data_code_gen",
            [
                "--type",
                "dsss",
                "--symbol-rate",
                "100",
                "--data-code",
                "pn:15:4",
                "--fs",
                "48000",
                "--count",
                "64",
            ],
        ),
        # A generated sync is still a burst-frame field (#1592).
        (
            "err_dsss_sync_gen_in_continuous",
            [
                "--type",
                "dsss",
                "--symbol-rate",
                "100",
                "--data-code",
                "1011",
                "--sync",
                "pn:15:4",
            ],
        ),
        # --crc is a burst-frame flag too, as --help says (#1595).
        (
            "err_dsss_crc_in_continuous",
            [
                "--type",
                "dsss",
                "--symbol-rate",
                "100",
                "--data-code",
                "1011",
                "--crc",
                "crc16",
            ],
        ),
        # A PN length with no m-sequence was a silent 0-byte run, exit 0,
        # until the composer built each source at create (#1590).
        (
            "err_pn_length_no_msequence",
            [
                "--type",
                "pn",
                "--pn-length",
                "65",
                "--count",
                "64",
            ],
        ),
        # ---- the payload: a generated Field ----
        # A PN-sourced waveform (bpsk/qpsk/pn) was once refused a frame
        # outright (#755): its data is endless, and nothing said where the
        # payload stopped. A generated payload Field over the waveform's own
        # register is that bound -- `--bits pn:N:REG`, which replaced the
        # retired --payload-len bit for bit (#853) -- so the record is the
        # Field and the replay is the waveform that was transmitted.
        (
            "bpsk_framed_pn_payload",
            [
                "--type",
                "bpsk",
                "--sync",
                "1111100110101",
                "--data",
                "pn:64:7",
                "--pn-length",
                "7",
                "--sps",
                "2",
            ],
        ),
        # QPSK too: the frame's bits take the mapping the TYPE names, not a
        # --modulation, which would be a second way to spell the type.
        (
            "qpsk_framed_pn_payload",
            [
                "--type",
                "qpsk",
                "--sync",
                "1111100110101",
                "--data",
                "pn:128:9",
                "--pn-length",
                "9",
                "--sps",
                "2",
            ],
        ),
        (
            "bits_payload_gen_pn",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--data",
                "pn:64:7",
                "--sync",
                "1111100110101",
                "--sps",
                "2",
            ],
        ),
        # A spread frame whose PAYLOAD is generated -- the descriptor's
        # payload field was the last place gh-762's flattening survived.
        (
            "dsss_payload_gen",
            [
                "--type",
                "dsss",
                "--data-code",
                "0110",
                "--data",
                "pn:31:5",
                "--sync",
                "10",
                "--acq-code",
                "10101010*2",
                "--sps",
                "2",
            ],
        ),
        # A field takes ONE spelling. BOTH orders are cases, because they
        # fail in different places and used to fail differently: the literal
        # first is caught before the generated parse memsets it away, the
        # generated first is caught only once the whole line is read. Neither
        # was refused at all before gh-762 -- the pair produced a generated
        # kind wearing a stray literal array, whose `--record` emitted both
        # `sync` and `sync_gen` and could then never be read back.
        (
            "err_retired_sync_gen",
            [
                "--type",
                "bits",
                "--data",
                "10110010",
                "--sync-gen",
                "pn:31:5",
                "--count",
                "64",
            ],
        ),
        (
            "err_sync_field_repeats",
            [
                "--type",
                "bits",
                "--data",
                "10110010",
                "--sync",
                "0110*2",
                "--count",
                "64",
            ],
        ),
        # The hex spelling is the same field by another name, so it conflicts
        # too -- and on the preamble rather than the sync, so the pair table
        # is exercised on more than one of its rows.
        (
            "err_retired_acq_code_hex",
            [
                "--type",
                "dsss",
                "--data",
                "1011",
                "--data-code",
                "0110",
                "--acq-code-hex",
                "a5",
                "--count",
                "256",
            ],
        ),
        # A --pn-poly with a bit above --pn-length's register was masked to
        # a register with no feedback: a constant waveform, exit 0
        # (doppler#1636). Refused, exit 2. --count caps it either way.
        (
            "err_pn_poly_above_length",
            [
                "--type",
                "pn",
                "--pn-length",
                "5",
                "--pn-poly",
                "0x40",
                "--count",
                "32",
            ],
        ),
        # A Field past WFM_FIELD_MAX_BITS is refused at parse, exit 2
        # (doppler#1622). Before, it parsed and the source allocated the
        # caller's length with the abort-on-OOM helper: 2^64-1 aborted
        # (exit 134) on every machine, while 4e9 spent 7.8 GB and 40 s and
        # exited 0 where the memory existed -- so both are pinned.
        (
            "err_field_past_the_bound_2e64",
            [
                "--type",
                "bits",
                "--data",
                "pn:18446744073709551615:5",
                "--count",
                "16",
            ],
        ),
        (
            "err_field_past_the_bound",
            [
                "--type",
                "bits",
                "--data",
                "pn:4000000000:5",
                "--count",
                "16",
            ],
        ),
        # The generated parser's own refusals: `literal` is not a kind it
        # will spell, and a length is the one parameter no default supplies.
        (
            "err_seq_gen_kind_literal",
            [
                "--type",
                "bits",
                "--data",
                "1011",
                "--sync",
                "literal:8",
                "--count",
                "64",
            ],
        ),
        (
            "err_seq_gen_no_len",
            [
                "--type",
                "bits",
                "--data",
                "1011",
                "--sync",
                "pn",
                "--count",
                "64",
            ],
        ),
        (
            "err_seq_gen_pn_no_reg",
            [
                "--type",
                "bits",
                "--data",
                "1011",
                "--sync",
                "pn:31",
                "--count",
                "64",
            ],
        ),
        (
            "err_seq_gen_gold_short",
            [
                "--type",
                "bits",
                "--data",
                "1011",
                "--sync",
                "gold:16:10:934",
                "--count",
                "64",
            ],
        ),
        # The payload is one field however it is spelled: an array, a
        # generator, or a length the waveform's own PN fills.
        (
            "err_retired_payload_len",
            [
                "--type",
                "bits",
                "--data",
                "1011",
                "--payload-len",
                "64",
                "--count",
                "64",
            ],
        ),
        (
            "err_retired_payload_gen",
            ["--type", "bits", "--payload-gen", "pn:64:7", "--count", "64"],
        ),
        # A type carrying no bit stream still cannot be framed, payload or
        # not -- the refusal was lifted for bpsk/qpsk/pn only.
        (
            "err_framed_chirp",
            [
                "--type",
                "chirp",
                "--sync",
                "1111100110101",
                "--data",
                "pn:64:15",
            ],
        ),
        # Each retired spelling is REFUSED by name, pointing at the Field
        # flag that replaced it -- never read as an alias (#853, F.3).
        (
            "err_retired_bits_hex",
            ["--type", "bits", "--bits-hex", "b2", "--count", "32"],
        ),
        # The payload is a data source (#1718): --bits and --bits-file are
        # refused by name, pointing at --data and --data-from-file.
        (
            "err_retired_bits",
            ["--type", "bits", "--bits", "10110010", "--count", "32"],
        ),
        (
            "err_retired_bits_file",
            ["--type", "bits", "--bits-file", BITS_FILE],
        ),
        (
            "err_retired_acq_code_gen",
            [
                "--type",
                "dsss",
                "--data",
                "1011",
                "--data-code",
                "0110",
                "--acq-code-gen",
                "dotted:8",
                "--count",
                "256",
            ],
        ),
        (
            "err_retired_acq_reps",
            [
                "--type",
                "dsss",
                "--data",
                "1011",
                "--data-code",
                "0110",
                "--acq-code",
                "1010",
                "--acq-reps",
                "2",
                "--count",
                "256",
            ],
        ),
        (
            "err_retired_data_code_hex",
            [
                "--type",
                "dsss",
                "--data",
                "1011",
                "--data-code-hex",
                "b2",
                "--count",
                "256",
            ],
        ),
        (
            "err_retired_data_code_gen",
            [
                "--type",
                "dsss",
                "--data",
                "1011",
                "--data-code-gen",
                "pn:7:3:1",
                "--count",
                "256",
            ],
        ),
        # The coding flags, each refused naming `--frame FILE` (#853).
        (
            "err_retired_rs_depth",
            [
                "--type",
                "bits",
                "--data",
                "10110010",
                "--rs-depth",
                "1",
                "--count",
                "64",
            ],
        ),
        (
            "err_retired_randomise",
            [
                "--type",
                "bits",
                "--data",
                "10110010",
                "--randomise",
                "--count",
                "64",
            ],
        ),
        (
            "err_retired_randomize",
            [
                "--type",
                "bits",
                "--data",
                "10110010",
                "--randomize",
                "--count",
                "64",
            ],
        ),
        (
            "err_retired_asm",
            ["--type", "bits", "--data", "10110010", "--asm", "--count", "64"],
        ),
        (
            "err_retired_conv",
            [
                "--type",
                "bits",
                "--data",
                "10110010",
                "--conv",
                "--count",
                "64",
            ],
        ),
        (
            "err_retired_interleave",
            [
                "--type",
                "bits",
                "--data",
                "10110010",
                "--interleave",
                "4",
                "--count",
                "64",
            ],
        ),
        (
            "err_retired_interleave_unit",
            [
                "--type",
                "bits",
                "--data",
                "10110010",
                "--interleave-unit",
                "8",
                "--count",
                "64",
            ],
        ),
    ] + [
        (name, ["--type", "tone", "--count", "64", flag, value])
        for name, flag, value in NUMERIC
    ]


# doppler#1611: a numeric flag reads its WHOLE token or refuses it, exit 2,
# naming the flag and the value. They used to stop at the first character
# that was not a digit and exit 0: `--seed 0x10` recorded 0 and `--sps 4x`
# recorded 4. An integer is decimal or `0x` hex, a leading 0 is decimal --
# the Field grammar's rule, read by the same reader. One case per numeric
# option kind, and every row of the issue's table. Their stderr is pinned
# too (STDERR_PINNED), because the sentence is what the fix promises.
NUMERIC = [
    # accepted: hex, and a leading zero that is NOT octal
    ("num_seed_hex", "--seed", "0x10"),
    ("num_pn_poly_hex", "--pn-poly", "0x6000"),
    ("num_seed_leading_zero", "--seed", "010"),
    # refused: a trailing character, per kind (U32, U64, INT, SIZE,
    # RANGE_N, RANGE_D and its hi, DOUBLE)
    ("err_num_seed_trailing", "--seed", "16x"),
    ("err_num_pn_poly_trailing", "--pn-poly", "0x6000z"),
    ("err_num_sps_trailing", "--sps", "4x"),
    ("err_num_repeats_trailing", "--repeats", "2junk"),
    ("err_num_count_trailing", "--count", "64x"),
    ("err_num_freq_trailing", "--freq", "0.1abc"),
    ("err_num_freq_hi_trailing", "--freq", "0.1:0.2x"),
    ("err_num_fc_trailing", "--fc", "1e6x"),
    # refused: what the whole-token rule alone would not catch
    ("err_num_seed_bare_0x", "--seed", "0x"),
    ("err_num_seed_sign", "--seed", "-1"),
    ("err_num_seed_overflow", "--seed", "4294967296"),
    ("err_num_freq_empty", "--freq", ""),
    ("err_num_freq_space", "--freq", " 0.1"),
    # doppler#1629: a sample count (RANGE_N) is whole, non-negative and fits
    # a size_t, on both sides of LO:HI. A negative one wrapped to 2^64 and
    # the run wrote WITHOUT BOUND -- never run these against a binary that
    # predates the fix without `ulimit -f` (it filled a shared /tmp once).
    ("num_count_exponent", "--count", "1e3"),
    ("err_num_delay_negative", "--delay", "-1"),
    ("err_num_off_negative", "--off", "-1"),
    ("err_num_count_negative", "--count", "-5"),
    ("err_num_count_hi_negative", "--count", "4:-2"),
    ("err_num_count_fraction", "--count", "1.5"),
    ("err_num_off_fraction", "--off", "1.5"),
]
STDERR_PINNED = {name for name, _, _ in NUMERIC} | {
    "err_frame_randomise_depth3",
    # An unknown flag exits 2 as well, so a retirement's exit code alone
    # cannot tell it from a deleted RETIRED row: the sentence naming the
    # replacement is the claim (#1718).
    "err_retired_bits",
    "err_retired_bits_file",
}


# The option table's rows, e.g. `{ .name = "--freq", .alias = "-o", ... }`.
# Whitespace-tolerant because clang-format decides where a row wraps.
_ROW_RE = re.compile(r'\.(?:name|alias)\s*=\s*"(--?[A-Za-z0-9-]+)"')
_SURFACE_RE = re.compile(r'\.cli\s*=\s*"(--?[A-Za-z0-9-]+)"')

# Flags that must always be discovered. They are not a coverage requirement
# -- cases() already drives them -- they are a check on the DISCOVERY, which
# reads C source and can therefore go stale silently. It did: the gh-723
# rewrite replaced the `!strcmp (a, "--x")` chain this used to scan with a
# table, and had the SKIP cross-check below not fired, coverage would have
# passed over an empty set and reported "0 flags covered" as success.
_ANCHORS = {"--type", "--count", "--output", "--freq", "-o"}


def dispatcher_flags(root: Path = ROOT) -> set[str]:
    """Every flag the parser accepts, read from its two option tables.

    wfmgen's own flags are rows of `OPTS` in wfmgen.c; every flag that sets
    a source or segment field is a row of the generated surface table. A
    tree without the surface table contributes only OPTS -- and the anchors
    below include field flags, so a real tree that lost it fails here
    rather than reporting fewer flags.

    `root` exists so a second caller can ask the same question of a tree
    that is not this checkout -- `check_wfmgen_flag_docs.py` imports this
    function and its own tests seed a synthetic `wfmgen.c`. It is a
    parameter rather than a second regex on purpose: two implementations of
    "what flags does the parser accept" would drift, and the one that
    drifted would report a gap that is not there, or miss one that is.
    """
    src = root / SRC_REL
    flags = set(_ROW_RE.findall(src.read_text()))
    surface = root / SURFACE_REL
    if surface.is_file():
        flags |= set(_SURFACE_RE.findall(surface.read_text()))
    missing = _ANCHORS - flags
    if missing:
        raise SystemExit(
            f"wfmgen_flag_matrix: flag discovery is broken -- "
            f"{', '.join(sorted(missing))} not found in {src.name} or "
            f"{Path(SURFACE_REL).name}. The option-table format changed; "
            "fix _ROW_RE / _SURFACE_RE."
        )
    return flags


def run_case(
    exe: Path, argv: list[str], workdir: Path, pin_stderr: bool = False
) -> dict:
    """Run one case and capture everything that is behaviour.

    `pin_stderr` adds what the tool printed to stderr -- for a case whose
    claim IS the sentence (a refusal naming its flag), not for every case,
    since most print nothing worth freezing.
    """
    rec = workdir / "record.json"
    # Every case gets --record; a run that exits before building the spec
    # simply leaves no file, which is itself pinned (record: null).
    full = [str(exe), *argv]
    if "--record" not in argv:
        full += ["--record", str(rec)]
    # Always give the run a destination. wfmgen's --output defaults to `-`,
    # i.e. binary IQ on stdout, which capture_output then buffers in memory
    # and (with text=True) tries to decode as UTF-8. --from-file was
    # originally exempted here on the reasoning that a scene carries its own
    # settings; it does not carry a destination, so it streamed to stdout.
    if not any(a in argv for a in ("--output", "-o")):
        full += ["--output", str(workdir / "sink.bin")]

    try:
        # Bytes, not text: what the tool prints is not pinned here, and a
        # run that does emit binary must not crash the harness decoding it.
        proc = subprocess.run(
            full, cwd=workdir, capture_output=True, timeout=TIMEOUT_S
        )
        code = proc.returncode
    except subprocess.TimeoutExpired:
        # subprocess.run kills the child before re-raising, so nothing is
        # left holding an unlinked file open. Pinned as a distinct outcome
        # rather than swallowed: a case that starts timing out has either
        # gained a non-terminating flag or genuinely hung.
        code = "timeout"

    out: dict = {"argv": argv, "exit": code, "record": None, "outputs": {}}
    if pin_stderr and code != "timeout":
        out["stderr"] = proc.stderr.decode("utf-8", "replace")
    if rec.is_file():
        out["record"] = json.loads(rec.read_text())

    # Size only. A content hash lived here and made the golden
    # machine-specific -- see the module docstring. Size still separates
    # cf32 from ci16 and raw from csv, and it is the same number on every
    # toolchain; the content-sensitive part is relational_checks().
    for f in sorted(workdir.iterdir()):
        if f.name == "record.json" or f.name in FIXTURES:
            continue
        if f.is_file():
            out["outputs"][f.name] = {"bytes": f.stat().st_size}
    return out


def relational_checks(exe: Path, problems: list[str]) -> None:
    """Pin --endian and --sample-type without pinning float values.

    Both flags change the emitted bytes and neither reaches the record, so
    the golden cannot see them by size alone. What CAN be asserted
    portably is the relationship between two runs of the same waveform:
    byte order is a permutation, and sample width is a ratio. Neither
    depends on what the compiler did to the last mantissa bit.
    """
    with tempfile.TemporaryDirectory() as td:
        wd = Path(td)
        base = [
            "--type",
            "tone",
            "--freq",
            "0.1",
            "--count",
            "16",
            "--seed",
            "1",
        ]

        def emit(name: str, extra: list[str], head: list[str] = base) -> bytes:
            path = wd / name
            subprocess.run(
                [str(exe), *head, *extra, "--output", str(path)],
                cwd=wd,
                capture_output=True,
                timeout=TIMEOUT_S,
                check=True,
            )
            return path.read_bytes()

        le = emit("le.bin", ["--sample-type", "cf32", "--endian", "le"])
        be = emit("be.bin", ["--sample-type", "cf32", "--endian", "be"])
        if len(le) != len(be):
            problems.append(
                f"endian changed the output SIZE "
                f"({len(le)} vs {len(be)}); it must only "
                f"reorder bytes"
            )
        else:
            # cf32 is 4-byte elements; big-endian reverses each one.
            swapped = b"".join(
                le[i : i + 4][::-1] for i in range(0, len(le), 4)
            )
            if swapped != be:
                problems.append(
                    "--endian be is not the byte-reversed form of --endian le"
                )
            if le == be:
                problems.append(
                    "--endian le and be produced identical "
                    "bytes; the flag did nothing"
                )

        ci16 = emit("ci16.bin", ["--sample-type", "ci16"])
        if len(ci16) * 2 != len(le):
            problems.append(
                f"ci16 output is {len(ci16)} bytes against "
                f"cf32's {len(le)}; expected exactly half"
            )

        # A framed bpsk/qpsk/pn transmits its FRAME, so its bytes are the
        # framed `bits` source with the mapping the type names -- the same
        # run of the same build, so no mantissa bit can differ. The record
        # cannot see this: it was identical while the wire carried the
        # type's own PN stream instead (doppler#1616).
        frame = ["--sync", "1111100110101", "--data", "10110011"]
        frame += ["--crc", "crc16", "--sps", "2"]  # one frame: the data
        frame += ["--seed", "1"]
        for typ, mod in (("bpsk", "bpsk"), ("qpsk", "qpsk"), ("pn", "bpsk")):
            got = emit(f"{typ}.bin", ["--type", typ], frame)
            twin = emit(
                f"{typ}-bits.bin",
                ["--type", "bits", "--modulation", mod],
                frame,
            )
            if got != twin:
                problems.append(
                    f"a framed --type {typ} is not the framed --type bits "
                    f"--modulation {mod} waveform: the frame is not what "
                    f"reaches the wire"
                )


def fixtures(workdir: Path, exe: Path) -> None:
    """Inputs the cases read. Written per-case so runs stay independent."""
    # A real binary payload: --data-from-file consumes a file's BYTES,
    # MSB first, which is what a transfer frame on disk actually is.
    (workdir / BITS_FILE).write_bytes(bytes([0xB2, 0x5A, 0x0F, 0xFF]))
    # The --frame descriptions, as files.
    for name, frame in FRAMES.items():
        (workdir / name).write_text(json.dumps(frame), encoding="utf-8")
    # 4 constellation points as interleaved float32 I,Q.
    import struct

    pts = [(1.0, 0.0), (0.0, 1.0), (-1.0, 0.0), (0.0, -1.0)]
    (workdir / SYMS_FILE).write_bytes(
        b"".join(struct.pack("<ff", i, q) for i, q in pts)
    )
    # A scene for --from-file, produced by wfmgen itself so it stays valid
    # as the schema moves.
    subprocess.run(
        [
            str(exe),
            "--type",
            "tone",
            "--count",
            "8",
            "--record",
            str(workdir / SCENE_FILE),
            "--output",
            str(workdir / "seed.bin"),
        ],
        cwd=workdir,
        capture_output=True,
        check=True,
        timeout=TIMEOUT_S,
    )
    (workdir / "seed.bin").unlink(missing_ok=True)


def replay_checks(exe: Path, problems: list[str]) -> None:
    """Every recordable case must replay from its own record, byte for byte.

    `--record` writes the resolved run and `--from-file` reads it back; the
    schema states the contract outright -- *"a recorded run reproduces
    byte-for-byte when fed back"*. Nothing checked it. `check_coverage()`
    asks only that each flag be DRIVEN, and `run_case()` pins the record it
    produced, so a flag the serialiser forgets is pinned as correct: the
    golden records its absence and agrees with itself forever.

    That is not hypothetical twice over. `seed_advance` was dropped this way
    (doppler#978), and `--interleave` was dropped again on the flag's first
    release -- a replay came back the same LENGTH with different bytes and
    no error, which is the worst shape available: a capture that looks like
    the one you recorded and is a different waveform.

    So this compares the artifact rather than the record. Derived from the
    case table rather than a hand-written list, because a list is the thing
    that stops growing when someone adds a flag.

    Every container is compared, not just the single-file ones. BLUE, CSV
    and SigMF look like they could not be byte-identical across two runs --
    a BLUE header has a timecode field and SigMF has `core:datetime` -- and
    measurably they ARE: wfmgen writes a zero timecode and no wall-clock
    date, which is a deliberate reproducibility choice and worth having a
    check stand on. Two runs two seconds apart produce identical bytes in
    all four containers.

    The replay therefore lands in a SECOND directory under the case's own
    output name, so a container that writes a pair (SigMF's data + meta,
    detached BLUE's .det + .hdr) is compared file by file rather than
    skipped for having more than one.

    Skips are COUNTED and reported, never silent. A case that already reads
    a scene is its own replay, and a case that refuses pins an exit code
    rather than a waveform; both are legitimate, and a gate that stopped
    checking everything would otherwise still print OK.
    """
    checked = 0
    skipped: dict[str, int] = {}

    def skip(why: str) -> None:
        skipped[why] = skipped.get(why, 0) + 1

    for name, argv in cases():
        if "--from-file" in argv:
            skip("already a replay")
            continue
        with (
            tempfile.TemporaryDirectory() as td,
            tempfile.TemporaryDirectory() as td2,
        ):
            wd, wd2 = Path(td), Path(td2)
            fixtures(wd, exe)
            first = run_case(exe, argv, wd)
            if first["exit"] != 0 or first["record"] is None:
                skip("refusal: pins an exit code, not a waveform")
                continue
            produced = [n for n in first["outputs"] if n != "record.json"]
            if not produced or not any(
                (wd / n).stat().st_size for n in produced
            ):
                skip("no output to compare")
                continue

            # The record describes the SIGNAL; the container is the caller's.
            # --file-type/--sample-type/--endian deliberately do not reach the
            # spec (there is no output section in the schema), so the replay
            # supplies them exactly as the case did. Without this the check
            # would be asserting a contract nobody makes -- and it would fail
            # on out_csv/out_blue, which are correct.
            carried: list[str] = []
            for flag in ("--file-type", "--sample-type", "--endian"):
                if flag in argv:
                    carried += [flag, argv[argv.index(flag) + 1]]
            # Same output NAME, second directory: the container decides how
            # many files it writes and what it suffixes them with, so the
            # only way to compare a pair is to let it choose both names
            # twice.
            out_arg = "sink.bin"
            for flag in ("--output", "-o"):
                if flag in argv:
                    out_arg = Path(argv[argv.index(flag) + 1]).name
            proc = subprocess.run(
                [
                    str(exe),
                    "--from-file",
                    str(wd / "record.json"),
                    *carried,
                    "--output",
                    str(wd2 / out_arg),
                ],
                cwd=wd2,
                capture_output=True,
                timeout=TIMEOUT_S,
            )
            if proc.returncode != 0:
                problems.append(
                    f"{name}: --record wrote a spec its own --from-file "
                    f"refuses (exit {proc.returncode}): "
                    f"{proc.stderr.decode('utf-8', 'replace').strip()[:160]}"
                )
                continue

            checked += 1
            again = {p.name for p in wd2.iterdir() if p.is_file()}
            if again != set(produced):
                problems.append(
                    f"{name}: the replay wrote {sorted(again)} where the "
                    f"run wrote {sorted(produced)}"
                )
                continue
            for fname in sorted(produced):
                original, got = (
                    (wd / fname).read_bytes(),
                    (wd2 / fname).read_bytes(),
                )
                if got == original:
                    continue
                how = (
                    "same length, different bytes -- a flag reached the "
                    "waveform but not the record"
                    if len(got) == len(original)
                    else f"{len(original)} bytes -> {len(got)}"
                )
                problems.append(
                    f"{name}: replaying its own --record does not reproduce "
                    f"{fname} ({how}). Every flag that changes the waveform "
                    f"has to reach wfm_json.c's writer AND its reader, and "
                    f"the schema's source object."
                )

    # A floor, not a formality: without it, a change that made every case
    # skip would leave this function printing nothing and passing.
    if checked < MIN_REPLAY_CASES:
        problems.append(
            f"only {checked} case(s) were replayed, below the floor of "
            f"{MIN_REPLAY_CASES} -- the round-trip check has stopped "
            f"looking at most of the matrix"
        )
    note = ", ".join(f"{n} {why}" for why, n in sorted(skipped.items()))
    print(
        f"wfmgen_flag_matrix: {checked} case(s) replayed from their own "
        f"--record" + (f"; skipped {note}" if note else "")
    )


def build_matrix(exe: Path) -> dict:
    matrix: dict = {}
    for name, argv in cases():
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            fixtures(wd, exe)
            matrix[name] = run_case(
                exe, argv, wd, pin_stderr=name in STDERR_PINNED
            )
    return matrix


def check_coverage(problems: list[str]) -> None:
    """Every dispatcher flag must be driven, or named in SKIP with a why."""
    driven = {a for _, argv in cases() for a in argv if a.startswith("-")}
    for flag in sorted(dispatcher_flags()):
        if flag in driven or flag in SKIP:
            continue
        problems.append(
            f"flag {flag} is in the dispatcher but no case drives "
            f"it (add a case, or SKIP it with a reason)"
        )
    for flag in sorted(SKIP):
        if flag not in dispatcher_flags():
            problems.append(
                f"SKIP lists {flag}, which the dispatcher no "
                f"longer accepts -- drop it"
            )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--exe", required=True, help="path to the wfmgen binary")
    ap.add_argument(
        "--check",
        action="store_true",
        help="compare against the golden instead of writing it",
    )
    args = ap.parse_args()

    exe = Path(args.exe).resolve()
    if not exe.is_file():
        print(f"wfmgen_flag_matrix: no binary at {exe}", file=sys.stderr)
        return 1

    problems: list[str] = []
    check_coverage(problems)
    relational_checks(exe, problems)
    replay_checks(exe, problems)
    got = build_matrix(exe)

    if not args.check:
        GOLDEN.write_text(json.dumps(got, indent=2, sort_keys=True) + "\n")
        if problems:
            print("wfmgen_flag_matrix: wrote golden, but coverage is short:")
            for p in problems:
                print(f"  {p}")
            return 1
        print(
            f"wfmgen_flag_matrix: wrote {len(got)} case(s) covering "
            f"{len(dispatcher_flags()) - len(SKIP)} flag(s)"
        )
        return 0

    if not GOLDEN.is_file():
        print(
            f"wfmgen_flag_matrix: no golden at {GOLDEN}; run "
            f"`make wfmgen-flag-matrix` to create it",
            file=sys.stderr,
        )
        return 1

    want = json.loads(GOLDEN.read_text())
    for name in sorted(set(want) | set(got)):
        if name not in got:
            problems.append(f"{name}: in the golden, not produced now")
        elif name not in want:
            problems.append(f"{name}: produced now, not in the golden")
        elif want[name] != got[name]:
            problems.append(
                f"{name}: behaviour changed\n"
                f"    want {json.dumps(want[name], sort_keys=True)}\n"
                f"    got  {json.dumps(got[name], sort_keys=True)}"
            )

    if problems:
        print("wfmgen_flag_matrix: FAIL")
        for p in problems:
            print(f"  {p}")
        print(
            "\n  If the change is intended, regenerate the golden with "
            "`make wfmgen-flag-matrix` and commit it."
        )
        return 1

    print(
        f"wfmgen_flag_matrix: OK — {len(got)} case(s), "
        f"{len(dispatcher_flags()) - len(SKIP)} flag(s) covered"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
