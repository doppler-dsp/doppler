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
SRC = ROOT / "native" / "src" / "app" / "wfmgen.c"
GOLDEN = ROOT / "native" / "tests" / "wfmgen_flag_matrix.json"

# Flags the dispatcher accepts but that this matrix deliberately does not
# drive, each for a reason that would make the case meaningless rather
# than merely awkward.
SKIP = {
    # Aliases of a flag already covered; same arm, same field.
    "-o": "alias of --output, covered by out_file",
    "--randomize": "alias of --randomise, covered by bits_ccsds_cadu",
    # Needs a reachable broker; the stream sink is an optional component
    # and its absence is its own tested path in wfmgen_cli_test.cmake.
    "--detached": "spawns a background process; see wfmgen_cli_test.cmake",
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

BITS_FILE = "bits.bin"
SYMS_FILE = "syms.cf32"
SCENE_FILE = "scene.json"


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
        # ---- bits input, all three sources and the modulation knob ----
        (
            "bits_literal",
            [
                "--type",
                "bits",
                "--bits",
                "10110010",
                "--modulation",
                "qpsk",
                "--sps",
                "2",
                "--count",
                "32",
            ],
        ),
        (
            "bits_hex",
            [
                "--type",
                "bits",
                "--bits-hex",
                "b2",
                "--modulation",
                "none",
                "--count",
                "32",
            ],
        ),
        (
            "bits_file",
            ["--type", "bits", "--bits-file", BITS_FILE, "--count", "32"],
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
                "--bits",
                "10110010",
                "--acq-code",
                "1010",
                "--acq-reps",
                "2",
                "--sync",
                "1111100110101",
                "--crc",
                "crc16",
                "--sps",
                "2",
                "--count",
                "64",
            ],
        ),
        # ---- channel coding: the stages, and the CADU they configure ----
        # A 223-octet Transfer Frame with all four stages on and neither a
        # preamble nor a sync word IS a CCSDS CADU. Pinned as one case rather
        # than four because the flags are not independent -- the outer code
        # fixes the payload length, and the interesting property is the
        # COVERAGE asymmetry between them, which only appears together.
        (
            "bits_ccsds_cadu",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--bits",
                "1" * (223 * 8),
                "--rs-depth",
                "1",
                "--randomise",
                "--asm",
                "--conv",
                "--crc",
                "none",
                "--sps",
                "1",
                "--count",
                "4144",
            ],
        ),
        # The SAME CADU with the legacy randomiser. Its own case because
        # "which generator" is the axis --record has to carry: B-6 specifies
        # two, and only the matching receiver derandomises a given waveform,
        # so a record that recorded a bare `true` could not rebuild either.
        (
            "bits_ccsds_cadu_legacy_rand",
            [
                "--type",
                "bits",
                "--modulation",
                "bpsk",
                "--bits",
                "1" * (223 * 8),
                "--rs-depth",
                "1",
                "--randomise",
                "legacy",
                "--asm",
                "--conv",
                "--crc",
                "none",
                "--sps",
                "1",
                "--count",
                "4144",
            ],
        ),
        # The outer code refuses a payload off the 223*I grid rather than
        # padding it -- virtual fill is not implemented (gh-813), and a
        # silently padded codeblock is the wrong length for the receiver it
        # was aimed at.
        (
            "err_rs_depth_short_payload",
            [
                "--type",
                "bits",
                "--bits",
                "10110010",
                "--rs-depth",
                "1",
                "--count",
                "64",
            ],
        ),
        (
            "err_rs_depth_not_allowed",
            [
                "--type",
                "bits",
                "--bits",
                "1" * (223 * 8),
                "--rs-depth",
                "7",
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
                "1010",
                "--acq-reps",
                "2",
                "--data-code",
                "1011",
                "--sync",
                "1111100110101",
                "--crc",
                "crc16",
                "--bits",
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
                "--acq-code-hex",
                "a5",
                "--data-code-hex",
                "b2",
                "--bits-hex",
                "0f",
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
                "--data",
                "prbs",
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
    ]


# The option table's rows, e.g. `{ .name = "--freq", .alias = "-o", ... }`.
# Whitespace-tolerant because clang-format decides where a row wraps.
_ROW_RE = re.compile(r'\.(?:name|alias)\s*=\s*"(--?[A-Za-z0-9-]+)"')

# Flags that must always be discovered. They are not a coverage requirement
# -- cases() already drives them -- they are a check on the DISCOVERY, which
# reads C source and can therefore go stale silently. It did: the gh-723
# rewrite replaced the `!strcmp (a, "--x")` chain this used to scan with a
# table, and had the SKIP cross-check below not fired, coverage would have
# passed over an empty set and reported "0 flags covered" as success.
_ANCHORS = {"--type", "--count", "--output", "--freq", "-o"}


def dispatcher_flags() -> set[str]:
    """Every flag the parser accepts, read from its option table."""
    flags = set(_ROW_RE.findall(SRC.read_text()))
    missing = _ANCHORS - flags
    if missing:
        raise SystemExit(
            f"wfmgen_flag_matrix: flag discovery is broken -- "
            f"{', '.join(sorted(missing))} not found in {SRC.name}. "
            f"The option-table format changed; fix _ROW_RE."
        )
    return flags


def run_case(exe: Path, argv: list[str], workdir: Path) -> dict:
    """Run one case and capture everything that is behaviour."""
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
    if rec.is_file():
        out["record"] = json.loads(rec.read_text())

    # Size only. A content hash lived here and made the golden
    # machine-specific -- see the module docstring. Size still separates
    # cf32 from ci16 and raw from csv, and it is the same number on every
    # toolchain; the content-sensitive part is relational_checks().
    for f in sorted(workdir.iterdir()):
        if f.name in {"record.json", BITS_FILE, SYMS_FILE, SCENE_FILE}:
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

        def emit(name: str, extra: list[str]) -> bytes:
            path = wd / name
            subprocess.run(
                [str(exe), *base, *extra, "--output", str(path)],
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


def fixtures(workdir: Path, exe: Path) -> None:
    """Inputs the cases read. Written per-case so runs stay independent."""
    (workdir / BITS_FILE).write_bytes(bytes([0xB2, 0x5A, 0x0F, 0xFF]))
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


def build_matrix(exe: Path) -> dict:
    matrix: dict = {}
    for name, argv in cases():
        with tempfile.TemporaryDirectory() as td:
            wd = Path(td)
            fixtures(wd, exe)
            matrix[name] = run_case(exe, argv, wd)
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
            f"wfmgen_flag_matrix: no golden at {GOLDEN}; run without "
            f"--check to create it",
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
            "\n  If the change is intended, re-run without --check and "
            "commit the golden."
        )
        return 1

    print(
        f"wfmgen_flag_matrix: OK — {len(got)} case(s), "
        f"{len(dispatcher_flags()) - len(SKIP)} flag(s) covered"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
