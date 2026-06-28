"""Real-time feedforward DSSS demod that *tails* a wfmgen capture as written.

The writer and reader run **concurrently against one growing file** — the
reader does not wait for the writer to finish:

* **Writer** (paced, background) — the ``wfmgen`` CLI engine streams a capture
  to disk with ``--realtime``: a BPSK-DSSS burst (unmodulated 5x500 preamble +
  a 50-chip-spread ``sync | payload | CRC`` frame, Doppler + noise baked in)
  **once every 500 ms** (the trailing ``off_samples`` give the PRI). It flushes
  incrementally, so the file grows in real time.

* **Reader** (flat out) — *tails* the growing file: for each burst it waits
  only until that burst's samples have landed, then reads them and runs
  ``DDC`` (tune the bulk Doppler out) -> ``Acquisition`` -> ``BurstDemod``,
  decoding it the moment it arrives — while the writer is still producing
  later bursts.

All DSP lives in the C objects (wfmgen engine, DDC, Acquisition, BurstDemod);
the Python is orchestration only — it writes the spec, tails the file, and maps
the detector output to the demod's prior.

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

# ── waveform geometry ────────────────────────────────────────────────────────
ACQ_SF, REPS, DATA_SF, SPC = 500, 5, 50, 4
CHIP_RATE = 1.0e6
FS = CHIP_RATE * SPC  # 4 MHz channel rate
PAYLOAD = 64
PRI_MS = 500.0  # one burst every 500 ms
# Barker-13 frame-sync word.
SYNC = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], dtype=np.uint8)

DOPPLER_HZ = 11_500.0  # true Doppler baked into the capture
NOMINAL_HZ = 12_000.0  # the receiver's predicted (coarse) Doppler
SNR_DB = 22.0

_ACODE = ((np.arange(ACQ_SF) * 2654435761 >> 13) & 1).astype(np.uint8)
_DCODE = ((np.arange(DATA_SF) * 40503 >> 7) & 1).astype(np.uint8)
_PERIOD = round(PRI_MS * 1e-3 * FS)  # samples per PRI
_BURST = (ACQ_SF * REPS + (len(SYNC) + PAYLOAD + 16) * DATA_SF) * SPC
_MARGIN = 4000  # trailing samples handed to the chain (clean ACQ framing)


def _crc16(bits):
    c = 0xFFFF
    for b in bits:
        c ^= (int(b) & 1) << 15
        c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def _frame_chips(payload):
    """The data section's chip pattern: each frame bit XOR'd with the 50-chip
    data code (DSSS spreading), for sync | payload | CRC-16."""
    crc = _crc16(payload)
    crc_bits = np.array([(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
    frame = np.concatenate([SYNC, payload, crc_bits])
    return "".join("".join(map(str, _DCODE ^ b)) for b in frame)


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


def _wfmgen_bin():
    exe = wfmgen_available()
    if exe is None:
        raise FileNotFoundError(
            "wfmgen CLI not found (pip install doppler-dsp, "
            "or build the wfmgen_cli target)"
        )
    return exe


def write_spec(spec_path, payloads, *, doppler_hz=DOPPLER_HZ, snr_db=SNR_DB):
    """Write a multi-burst wfmgen JSON spec: each burst is two ``bits``
    segments (preamble, then the framed payload + a trailing gap = the PRI);
    ``freq`` bakes in the Doppler and ``snr`` the noise."""
    pre = "".join(map(str, np.tile(_ACODE, REPS)))
    segments = []
    for payload in payloads:
        fr = _frame_chips(payload)
        gap = _PERIOD - (len(pre) + len(fr)) * SPC
        common = {
            "type": "bits",
            "fs": FS,
            "freq": doppler_hz,
            "snr": snr_db,
            "snr_mode": "fs",
            "sps": SPC,
            "modulation": "bpsk",
        }
        segments.append(
            {
                **common,
                "pattern": pre,
                "num_samples": len(pre) * SPC,
                "off_samples": 0,
            }
        )
        segments.append(
            {
                **common,
                "pattern": fr,
                "num_samples": len(fr) * SPC,
                "off_samples": gap,
            }
        )
    Path(spec_path).write_text(
        json.dumps({"version": 1, "segments": segments})
    )


def start_writer(capture_path, spec_path, *, realtime=True):
    """Launch the wfmgen CLI as a background process streaming the capture to
    ``capture_path`` (``--realtime`` paces it to fs — one burst per PRI)."""
    cmd = [
        _wfmgen_bin(),
        "--from-file",
        str(spec_path),
        "-o",
        str(capture_path),
    ]
    if realtime:
        cmd.append("--realtime")
    return subprocess.Popen(
        cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )


def _read_samples(path, start, count):
    """Read ``count`` cf32 samples starting at sample ``start`` (or None if the
    file doesn't hold that many yet)."""
    need = (start + count) * 8  # cf32 = 2 x float32 = 8 bytes/sample
    if not path.exists() or path.stat().st_size < need:
        return None
    with open(path, "rb") as f:
        f.seek(start * 8)
        buf = f.read(count * 8)
    return np.frombuffer(buf, dtype="<c8")


def decode_chunk(chunk, *, nominal_hz=NOMINAL_HZ):
    """Decode one burst-sized chunk: DDC tunes the predicted bulk Doppler out,
    Acquisition detects (slow-time Doppler FFT over the reps), BurstDemod
    estimates the residual feedforward, despreads, frame-syncs, checks CRC."""
    base = DDC(norm_freq=-nominal_hz / FS, rate=1.0).execute(chunk)
    # cn0_dbhz sized so Acquisition runs its slow-time Doppler FFT over the 5
    # preamble reps (doppler_bins = reps) — a real Doppler search that resolves
    # the residual; too high collapses it to a single DC bin (misses bursts).
    acq = Acquisition(
        _ACODE, reps=REPS, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=40.0
    )
    hits = acq.push(base)
    if not hits:
        return {"detected": False, "frame_valid": False}
    dop, _cp, *_ = max(hits, key=lambda h: h[4])
    f0 = dop * acq.doppler_res_hz
    if dop >= acq.doppler_bins / 2:
        f0 -= acq.doppler_bins * acq.doppler_res_hz

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
            return {
                "detected": True,
                "frame_valid": True,
                "est_freq_hz": d.est_freq_hz + nominal_hz,
                "bits": bits,
            }
    return {"detected": True, "frame_valid": False}


def tail_decode(capture_path, n_bursts, *, proc=None, on_decode=None):
    """Tail the growing capture: for each burst, wait only until its samples
    have landed, then read + decode it. Concurrent with the writer."""
    capture_path = Path(capture_path)
    results = []
    for k in range(n_bursts):
        start = k * _PERIOD
        chunk = None
        while chunk is None:
            chunk = _read_samples(capture_path, start, _BURST + _MARGIN)
            if chunk is None:
                if proc is not None and proc.poll() is not None:
                    # writer finished without producing this burst
                    chunk = _read_samples(
                        capture_path, start, _BURST + _MARGIN
                    )
                    break
                time.sleep(0.002)
        if chunk is None:
            break
        r = decode_chunk(chunk)
        r["burst"] = k
        results.append(r)
        if on_decode is not None:
            on_decode(k, r)
    return results


def run_streaming(payloads, *, realtime=True, on_decode=None):
    """Stream a capture with wfmgen and tail-decode it concurrently; return the
    per-burst decode results."""
    with tempfile.TemporaryDirectory() as tmp:
        cap = Path(tmp) / "dsss_capture.cf32"
        spec = Path(tmp) / "spec.json"
        write_spec(spec, payloads)
        proc = start_writer(cap, spec, realtime=realtime)
        try:
            results = tail_decode(
                cap, len(payloads), proc=proc, on_decode=on_decode
            )
        finally:
            if proc.poll() is None:
                proc.terminate()
            proc.wait()
    return results


def main():
    rng = np.random.default_rng(0)
    payloads = [rng.integers(0, 2, PAYLOAD).astype(np.uint8) for _ in range(6)]
    print(
        f"\nwriter: wfmgen --realtime streams 1 burst / {PRI_MS:.0f} ms to a "
        f"growing file (Doppler {DOPPLER_HZ / 1e3:.1f} kHz, {SNR_DB:.0f} dB)"
    )
    print(
        f"reader: tails the file -> DDC(tune -{NOMINAL_HZ / 1e3:.0f} kHz) -> "
        f"Acquisition -> BurstDemod, decoding each burst as it lands\n"
    )
    t0 = time.monotonic()

    def report(k, r):
        dt = time.monotonic() - t0
        if r["frame_valid"]:
            errs = int(np.sum(r["bits"] != payloads[k]))
            print(
                f"  [t={dt:5.2f}s] burst {k}: CRC ok  {errs} bit errs  "
                f"Doppler {r['est_freq_hz']:.0f} Hz"
            )
        else:
            tag = "no detect" if not r["detected"] else "CRC FAIL"
            print(f"  [t={dt:5.2f}s] burst {k}: {tag}")

    run_streaming(payloads, realtime=True, on_decode=report)
    print()


if __name__ == "__main__":
    main()
