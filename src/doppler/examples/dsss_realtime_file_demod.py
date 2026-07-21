"""Real-time feedforward DSSS demod that tails a continuous wfmgen capture.

The whole pipeline through one growing file, with the project's own tools:

* **Writer** — the ``wfmgen`` CLI streams ``scene.json`` ``--continuous
  --realtime`` in the background: a BPSK-DSSS burst (PN preamble + a spread
  ``sync | payload | CRC`` frame) **every ~PRI ms**. The scene uses wfmgen's
  *ranged-field* interface — a numeric field given as ``[low, high]`` is drawn
  uniformly each repeat — so **every burst gets a fresh Doppler** (``freq:
  [lo, hi]``) **and a fresh arrival jitter** (the trailing gap ``off_samples:
  [lo, hi]``, which shifts the next burst's code phase), on top of a fresh
  noise realization (``seed_advance="noise"``). Code and payload stay fixed
  (they're explicit ``bits`` patterns), and the draws are reproducible — keyed
  off the source seed and the repeat index — so the same scene replays
  byte-for-byte.

* **Reader** — a streaming acquirer that *follows* the bursts rather than
  assuming a fixed grid: it seeks to each burst's expected position (one PRI on
  from the last), reads a short window that absorbs the sub-code-period arrival
  jitter, then runs ``DDC`` (tune the predicted bulk Doppler out) ->
  ``Acquisition`` (residual Doppler + code-phase search) -> ``BurstDemod``,
  decoding it the moment it lands — while the writer is still producing later
  bursts. The detected code phase and Doppler therefore change burst-to-burst.

All DSP lives in the C objects (wfmgen engine, DDC, Acquisition, BurstDemod);
this module is the orchestrator — it writes the scene spec, follows the file,
and maps the detector output to the demod's prior.

Run it::

    python -m doppler.examples.dsss_realtime_file_demod
"""

from __future__ import annotations

import json
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np

from doppler.ddc import DDC
from doppler.dsss import BurstAcquisition, BurstDemod
from doppler.wfm import PN

# ── waveform geometry ────────────────────────────────────────────────────────
# Spreading codes are real maximal-length sequences from the PN source
# (poly=0 auto-picks the primitive polynomial) — a clean thumbtack
# autocorrelation, so acquisition has genuine processing gain. They are carried
# in the scene as explicit `bits` patterns, which stay fixed as wfmgen advances
# the seed each repeat (only the random draws vary). Lengths are MLS periods.
ACQ_BITS, DATA_BITS = 9, 6
ACQ_SF, REPS, DATA_SF, SPC = (1 << ACQ_BITS) - 1, 5, (1 << DATA_BITS) - 1, 4
CHIP_RATE = 1.0e6
FS = CHIP_RATE * SPC  # 4 MHz channel rate
PAYLOAD = 64
PRI_MS = 250.0  # nominal burst spacing (the writer paces to this in realtime)
SYNC = np.array(
    [0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], dtype=np.uint8
)  # Barker

# Doppler is drawn uniformly per burst in [DOPPLER_LO, DOPPLER_HI]; the
# receiver DDCs by the band centre (NOMINAL_HZ) so the residual the Acquisition
# searches stays inside its ±span (5 bins × ~391 Hz ≈ ±978 Hz here).
DOPPLER_LO = 11_200.0
DOPPLER_HI = 12_800.0
NOMINAL_HZ = 0.5 * (DOPPLER_LO + DOPPLER_HI)  # 12.0 kHz: the receiver's guess
SNR_DB = 10.0

# Per-burst arrival jitter (samples): the trailing gap varies over this span,
# so the next burst's preamble lands at a different code phase. Kept below one
# code period (ACQ_SF * SPC) so the observed offset is the code phase exactly.
JITTER_MAX = 1_600
assert JITTER_MAX < ACQ_SF * SPC  # one code period = 2044 samples

SCENE = Path(__file__).resolve().parent / "scene.json"


def _pn(bits, n, seed):
    """One period of a maximal-length sequence (0/1) from the PN source."""
    return (
        np.asarray(PN(poly=0, seed=seed, length=bits).generate(n)) & 1
    ).astype(np.uint8)


_ACODE = _pn(ACQ_BITS, ACQ_SF, 1)  # preamble code (511-chip MLS)
_DCODE = _pn(DATA_BITS, DATA_SF, 1)  # data code (63-chip MLS)
_PAYLOAD_BITS = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)
# Active burst = preamble (REPS × 511 chips) + spread frame, in samples.
_BURST = (ACQ_SF * REPS + (len(SYNC) + PAYLOAD + 16) * DATA_SF) * SPC
_PERIOD = round(PRI_MS * 1e-3 * FS)  # nominal samples between burst starts
_NOMINAL_GAP = _PERIOD - _BURST  # trailing zeros for the nominal PRI
# Reader window: the burst plus the full jitter span plus search margin.
_WINDOW = _BURST + JITTER_MAX + 4000


def _crc16(bits):
    c = 0xFFFF
    for b in bits:
        c ^= (int(b) & 1) << 15
        c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def _frame_chips():
    """Spread the frame (sync | payload | CRC-16) by the data code -> chips."""
    crc = _crc16(_PAYLOAD_BITS)
    crcb = np.array([(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
    frame = np.concatenate([SYNC, _PAYLOAD_BITS, crcb])
    return "".join("".join(map(str, _DCODE ^ b)) for b in frame)


def write_scene(path, *, snr_db=SNR_DB):
    """Write the wfmgen scene: ONE bits segment per burst (preamble chips then
    the spread frame, concatenated so the burst shares one carrier and one
    Doppler draw), streamed `continuous`. The ranged fields make each repeat
    distinct: `freq` is a uniform Doppler draw, `off_samples` a uniform
    trailing gap (→ varying code phase), `seed_advance="noise"` for AWGN."""
    pattern = "".join(map(str, np.tile(_ACODE, REPS))) + _frame_chips()
    scene = {
        "version": 1,
        "continuous": True,
        "seed_advance": "noise",  # fresh noise each burst; code/payload fixed
        "segments": [
            {
                "type": "bits",
                "fs": FS,
                "freq": [DOPPLER_LO, DOPPLER_HI],  # per-burst Doppler draw
                "snr": snr_db,
                "snr_mode": "fs",
                "seed": 1,
                "sps": SPC,
                "modulation": "bpsk",
                "pattern": pattern,
                "num_samples": len(pattern) * SPC,
                # nominal PRI gap + uniform arrival jitter → varying code phase
                "off_samples": [_NOMINAL_GAP, _NOMINAL_GAP + JITTER_MAX],
            }
        ],
    }
    Path(path).write_text(json.dumps(scene, indent=2))
    return Path(path)


def wfmgen_available():
    """Path to the wfmgen CLI (PATH, else the CMake build tree), or None."""
    exe = shutil.which("wfmgen")
    if exe:
        return exe
    root = Path(__file__).resolve().parents[3]  # repo root
    for cand in root.glob("build*/**/wfmgen"):
        if cand.is_file():
            return str(cand)
    return None


def start_writer(capture_path, scene_path, *, realtime=True):
    """Launch wfmgen streaming the scene to disk in the background."""
    exe = wfmgen_available()
    if exe is None:
        raise FileNotFoundError("wfmgen CLI not found (build wfmgen_cli)")
    cmd = [
        exe,
        "--from-file",
        str(scene_path),
        "--continuous",
        "-o",
        str(capture_path),
    ]
    if realtime:
        cmd.append("--realtime")
    return subprocess.Popen(
        cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )


def _read_samples(path, start, count):
    """Read `count` cf32 samples at `start`, or None if not landed yet."""
    need = (start + count) * 8  # cf32 = 8 bytes/sample
    if not path.exists() or path.stat().st_size < need:
        return None
    with open(path, "rb") as f:
        f.seek(start * 8)
        return np.frombuffer(f.read(count * 8), dtype="<c8")


def decode_chunk(chunk, *, nominal_hz=NOMINAL_HZ):
    """DDC tunes the predicted bulk Doppler out; Acquisition detects (residual
    Doppler search over the reps + a code-phase search); BurstDemod estimates
    the residual feedforward, despreads, frame-syncs, and checks the CRC. The
    returned ``code_phase`` is where in the window the preamble landed — the
    per-burst arrival jitter — and ``est_freq_hz`` is the recovered Doppler."""
    base = DDC(norm_freq=-nominal_hz / FS, rate=1.0).execute(chunk)
    acq = BurstAcquisition(
        _ACODE, reps=REPS, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=40.0
    )
    # Pin n_noncoh=1: a single push() below must decide immediately: with
    # no caller-facing max_noncoh knob left to default to "coherent-only,"
    # the auto-sizer would otherwise silently pick n_noncoh > 1, requiring
    # several accumulated frames within this one push before a hit could
    # ever fire.
    acq.configure_search_raw(REPS, 1)
    hits = acq.push(base)
    if not hits:
        return {"detected": False, "frame_valid": False, "code_phase": 0}
    dop, cp, _peak, _noise, test_stat, _snr, *_rest = max(
        hits, key=lambda h: h[4]
    )
    f0 = dop * acq.doppler_res_hz
    if dop >= acq.doppler_bins / 2:
        f0 -= acq.doppler_bins * acq.doppler_res_hz
    rec = {
        "detected": True,
        "frame_valid": False,
        "code_phase": int(cp),
        "test_stat": float(test_stat),
    }

    d = BurstDemod(_DCODE, SPC, CHIP_RATE, 0.0, 0.0, PAYLOAD, 10)
    d.set_preamble(_ACODE, REPS)
    d.set_sync(SYNC)
    npre = ACQ_SF * REPS * SPC
    # Try the acquired code phase first (sample-precise), then fall back to a
    # coarse grid scan over one code period in case the peak was a chip off.
    starts = [int(cp), *list(range(0, ACQ_SF * SPC, SPC))]
    for start in starts:
        if start < 0 or start + npre > len(base):
            continue
        d.set_prior(f0 / FS, start)
        bits = d.demod(base)
        if d.frame_valid:
            rec.update(
                frame_valid=True,
                code_phase=int(start),
                bits=bits,
                est_freq_hz=d.est_freq_hz + nominal_hz,
                est_snr_db=d.est_snr_db,
            )
            break
    return rec


def tail_decode(capture_path, n_bursts, *, proc=None, on_decode=None):
    """Follow the growing capture: seek to each burst's expected position (one
    PRI on from the last decode), wait until that window lands, then read +
    decode it. Advancing by the *detected* code phase keeps the reader locked
    to the true burst positions even as the per-burst arrival jitter
    accumulates — no fixed grid assumption."""
    capture_path = Path(capture_path)
    results = []
    abs_prev = 0  # absolute sample index of the last burst's preamble
    for k in range(n_bursts):
        seek = 0 if k == 0 else abs_prev + _PERIOD
        chunk = None
        while chunk is None:
            chunk = _read_samples(capture_path, seek, _WINDOW)
            if chunk is None:
                if proc is not None and proc.poll() is not None:
                    break
                time.sleep(0.002)
        if chunk is None:
            break
        rec = decode_chunk(chunk)
        rec["burst"] = k
        abs_prev = seek + rec.get("code_phase", 0)
        results.append(rec)
        if on_decode is not None:
            on_decode(k, rec)
    return results


def run_streaming(
    n_bursts=6, *, realtime=True, scene_path=None, on_decode=None
):
    """Stream the scene with wfmgen and follow-decode `n_bursts` live."""
    with tempfile.TemporaryDirectory() as tmp:
        scene = scene_path or write_scene(Path(tmp) / "scene.json")
        cap = Path(tmp) / "capture.cf32"
        proc = start_writer(cap, scene, realtime=realtime)
        try:
            return tail_decode(cap, n_bursts, proc=proc, on_decode=on_decode)
        finally:
            if proc.poll() is None:
                proc.terminate()
            proc.wait()


def main():
    # The writer is launched by start_writer() with these flags (one per line
    # so each is legible); scene.json / capture.cf32 live in a temp dir.
    print("\nwriter: wfmgen \\")
    print("          --from-file scene.json \\")
    print("          --continuous \\")
    print("          --realtime \\")
    print("          -o capture.cf32")
    print(
        f"        ({PRI_MS:.0f} ms PRI, Doppler ∈ [{DOPPLER_LO / 1e3:.1f}, "
        f"{DOPPLER_HI / 1e3:.1f}] kHz, code φ jitter ≤ {JITTER_MAX} samp, "
        f"{SNR_DB:.0f} dB chip SNR, fresh noise each burst)"
    )
    print(
        f"reader: follow -> DDC(-{NOMINAL_HZ / 1e3:.1f} kHz) -> Acquisition "
        f"-> BurstDemod, decoding each burst as it lands"
    )
    print(
        "        (SNR(dB) below is the recovered per-symbol SNR — the "
        f"{SNR_DB:.0f} dB chip SNR lifted by the DSSS despread gain)\n"
    )
    # Doppler and code phase now shift burst-to-burst (ranged scene fields);
    # test_stat and SNR shift too because each repeat is a fresh noise draw.
    # Units live in the headers so the cells stay bare numbers, easy to scan.
    print(
        f"  {'t(s)':>5} {'burst':<5} {'CRC':<4} {'errs':>4} {'test':>5} "
        f"{'code φ':>6} {'Doppler(Hz)':>11} {'SNR(dB)':>8}"
    )
    # Anchor the clock to the first decode, so burst 0 reads 0.0 and the rest
    # show the PRI cadence relative to it (not the writer's start-up latency).
    t0 = None

    def report(k, r):
        nonlocal t0
        if t0 is None:
            t0 = time.monotonic()
        dt = time.monotonic() - t0
        ts = f"{r['test_stat']:.0f}" if "test_stat" in r else "-"
        cp = r.get("code_phase", "-")
        if r["frame_valid"]:
            errs = int(np.sum(r["bits"] != _PAYLOAD_BITS))
            print(
                f"  {dt:5.1f} {k:<5} {'ok':<4} {errs:>4} {ts:>5} {cp:>6} "
                f"{r['est_freq_hz']:>11.0f} {r['est_snr_db']:>8.1f}"
            )
        else:
            tag = "no-det" if not r["detected"] else "FAIL"
            print(f"  {dt:5.1f} {k:<5} {tag:<4}")

    results = run_streaming(6, realtime=True, on_decode=report)
    decoded = [r for r in results if r["frame_valid"]]
    if decoded:
        bit_errs = sum(
            int(np.sum(r["bits"] != _PAYLOAD_BITS)) for r in decoded
        )
        dopps = [r["est_freq_hz"] for r in decoded]
        snrs = [r["est_snr_db"] for r in decoded]
        print(
            f"\n  summary: {len(decoded)}/{len(results)} bursts decoded, "
            f"{bit_errs} bit error(s) | "
            f"Doppler {min(dopps):.0f}–{max(dopps):.0f} Hz, "
            f"mean SNR {sum(snrs) / len(snrs):.1f} dB"
        )
    print()


if __name__ == "__main__":
    main()
