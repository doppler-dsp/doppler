"""Real-time feedforward DSSS demod that tails a continuous wfmgen capture.

The whole pipeline through one growing file, with the project's own tools:

* **Writer** — the ``wfmgen`` CLI streams ``scene.json`` ``--continuous
  --realtime`` in the background: a BPSK-DSSS burst (PN preamble + a
  spread ``sync | payload | CRC`` frame, Doppler + noise baked in) **once every
  500 ms**. wfmgen advances the seed each repeat, so every burst is a **fresh
  noise realization** (the code/payload stay fixed — the scene carries them as
  explicit ``bits`` patterns).

* **Reader** — *tails* the growing file: for each burst it waits only until
  that burst's samples have landed, then runs ``DDC`` (tune the bulk Doppler
  out) -> ``Acquisition`` -> ``BurstDemod``, decoding it the moment it arrives
  — while the writer is still producing later bursts.

All DSP lives in the C objects (wfmgen engine, DDC, Acquisition, BurstDemod);
this module is the orchestrator — it writes the scene spec, tails the file, and
maps the detector output to the demod's prior.

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
from doppler.dsss import Acquisition, BurstDemod
from doppler.wfm import PN

# ── waveform geometry ────────────────────────────────────────────────────────
# Spreading codes are real maximal-length sequences from the PN source
# (poly=0 auto-picks the primitive polynomial) — a clean thumbtack
# autocorrelation, so acquisition has genuine processing gain. They are carried
# in the scene as explicit `bits` patterns, which stay fixed as wfmgen advances
# the seed each repeat (only noise varies). Lengths are MLS periods (2^n-1).
ACQ_BITS, DATA_BITS = 9, 6
ACQ_SF, REPS, DATA_SF, SPC = (1 << ACQ_BITS) - 1, 5, (1 << DATA_BITS) - 1, 4
CHIP_RATE = 1.0e6
FS = CHIP_RATE * SPC  # 4 MHz channel rate
PAYLOAD = 64
PRI_MS = 500.0  # one burst every 500 ms
SYNC = np.array(
    [0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], dtype=np.uint8
)  # Barker

DOPPLER_HZ = 11_500.0  # true Doppler baked into the scene
NOMINAL_HZ = 12_000.0  # the receiver's predicted (coarse) Doppler
SNR_DB = 10.0

SCENE = Path(__file__).resolve().parent / "scene.json"


def _pn(bits, n, seed):
    """One period of a maximal-length sequence (0/1) from the PN source."""
    return (
        np.asarray(PN(poly=0, seed=seed, length=bits).generate(n)) & 1
    ).astype(np.uint8)


_ACODE = _pn(ACQ_BITS, ACQ_SF, 1)  # preamble code (511-chip MLS)
_DCODE = _pn(DATA_BITS, DATA_SF, 1)  # data code (63-chip MLS)
_PAYLOAD_BITS = ((np.arange(PAYLOAD) * 7 + 3) & 1).astype(np.uint8)
_PERIOD = round(PRI_MS * 1e-3 * FS)
_BURST = (ACQ_SF * REPS + (len(SYNC) + PAYLOAD + 16) * DATA_SF) * SPC


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


def write_scene(path, *, doppler_hz=DOPPLER_HZ, snr_db=SNR_DB):
    """Write the wfmgen scene spec: a PN-preamble `bits` segment + a
    spread-frame `bits` segment + the 500 ms gap; `continuous` streams it
    forever with a fresh noise realization each burst (seed advances)."""
    pre = "".join(map(str, np.tile(_ACODE, REPS)))
    fr = _frame_chips()
    gap = _PERIOD - (len(pre) + len(fr)) * SPC
    common = {
        "type": "bits",
        "fs": FS,
        "freq": doppler_hz,
        "snr": snr_db,
        "snr_mode": "fs",
        "seed": 1,
        "sps": SPC,
        "modulation": "bpsk",
    }
    scene = {
        "version": 1,
        "continuous": True,
        "seed_advance": "noise",  # fresh noise each burst; code/payload fixed
        "segments": [
            {
                **common,
                "pattern": pre,
                "num_samples": len(pre) * SPC,
                "off_samples": 0,
            },
            {
                **common,
                "pattern": fr,
                "num_samples": len(fr) * SPC,
                "off_samples": gap,
            },
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
    """DDC tunes the predicted bulk Doppler out; Acquisition detects (slow-time
    Doppler FFT over the reps); BurstDemod estimates the residual feedforward,
    despreads, frame-syncs, and checks the CRC."""
    base = DDC(norm_freq=-nominal_hz / FS, rate=1.0).execute(chunk)
    acq = Acquisition(
        _ACODE, reps=REPS, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=40.0
    )
    hits = acq.push(base)
    if not hits:
        return {"detected": False, "frame_valid": False}
    dop, cp, _peak, _noise, test_stat, _snr = max(hits, key=lambda h: h[4])
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
    for start in range(0, ACQ_SF * SPC, SPC):
        if start + npre > len(base):
            break
        d.set_prior(f0 / FS, start)
        bits = d.demod(base)
        if d.frame_valid:
            rec.update(
                frame_valid=True,
                bits=bits,
                est_freq_hz=d.est_freq_hz + nominal_hz,
            )
            break
    return rec


def tail_decode(capture_path, n_bursts, *, proc=None, on_decode=None):
    """Tail the growing capture: for each burst wait until its samples land,
    then read + decode it — concurrent with the writer."""
    capture_path = Path(capture_path)
    results = []
    for k in range(n_bursts):
        chunk = None
        while chunk is None:
            chunk = _read_samples(capture_path, k * _PERIOD, _BURST + 4000)
            if chunk is None:
                if proc is not None and proc.poll() is not None:
                    break
                time.sleep(0.002)
        if chunk is None:
            break
        rec = decode_chunk(chunk)
        rec["burst"] = k
        results.append(rec)
        if on_decode is not None:
            on_decode(k, rec)
    return results


def run_streaming(
    n_bursts=6, *, realtime=True, scene_path=None, on_decode=None
):
    """Stream the scene with wfmgen and tail-decode `n_bursts` concurrently."""
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
    print(
        f"\nwriter: wfmgen --continuous --realtime scene.json -> growing "
        f"file ({PRI_MS:.0f} ms PRI, {DOPPLER_HZ / 1e3:.1f} kHz Doppler, "
        f"{SNR_DB:.0f} dB, fresh noise each repeat)"
    )
    print(
        f"reader: tail -> DDC(-{NOMINAL_HZ / 1e3:.0f} kHz) -> Acquisition -> "
        f"BurstDemod, decoding each burst as it lands\n"
    )
    # test_stat shifts burst-to-burst because each repeat is a fresh noise
    # realization (wfmgen advances the seed) — the code/payload stay fixed.
    print(
        f"  {'t':>6} {'burst':<5} {'CRC':<4} {'errs':>4} {'test':>5} "
        f"{'code φ':>6} {'Doppler':>9}"
    )
    t0 = time.monotonic()

    def report(k, r):
        dt = time.monotonic() - t0
        ts = f"{r['test_stat']:.0f}" if "test_stat" in r else "-"
        cp = r.get("code_phase", "-")
        if r["frame_valid"]:
            errs = int(np.sum(r["bits"] != _PAYLOAD_BITS))
            print(
                f"  {dt:5.2f}s {k:<5} {'ok':<4} {errs:>4} {ts:>5} {cp:>6} "
                f"{r['est_freq_hz']:>7.0f}Hz"
            )
        else:
            tag = "no-det" if not r["detected"] else "FAIL"
            print(f"  {dt:5.2f}s {k:<5} {tag:<4} {'':>4} {ts:>5} {cp:>6}")

    run_streaming(6, realtime=True, on_decode=report)
    print()


if __name__ == "__main__":
    main()
