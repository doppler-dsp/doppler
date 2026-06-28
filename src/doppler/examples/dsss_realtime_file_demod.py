"""Real-time feedforward DSSS burst demod from a wfmgen capture on disk.

End to end, using the project's own tools:

1. **Generate** — the ``wfmgen`` CLI engine writes a capture to disk: a
   BPSK-DSSS burst (unmodulated 5x500 preamble + a 50-chip-spread
   ``sync | payload | CRC`` frame) **repeating once every 500 ms**, with a
   Doppler offset (the segment ``freq``, an LO mix) and noise (``snr``) baked
   in. The burst is built from multi-segment ``bits`` patterns
   (``--from-file SPEC.json``); the trailing ``off_samples`` give the 500 ms
   PRI.

2. **Demod** — a single receive chain reads the file back **paced to
   wall-clock** with a :class:`~doppler.wfm.SampleClock` (so it runs in real
   time, as if off a live SDR), and per PRI window runs::

       DDC (tune the bulk Doppler out) -> Acquisition -> BurstDemod

   BurstDemod estimates the residual Doppler feedforward, despreads,
   frame-syncs, and checks the CRC — printing each burst's decode as it
   arrives.

All DSP lives in the C objects (wfmgen engine, DDC, Acquisition, BurstDemod);
this module is the demo orchestrator — it writes the spec, maps the detector
output to the demod's prior, and paces the playback.

Run it::

    python -m doppler.examples.dsss_realtime_file_demod
"""

from __future__ import annotations

import json
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np

from doppler.ddc import DDC
from doppler.dsss import Acquisition, BurstDemod
from doppler.wfm import SampleClock
from doppler.wfm.compose import Reader

# ── waveform geometry ────────────────────────────────────────────────────────
ACQ_SF, REPS, DATA_SF, SPC = 500, 5, 50, 4
CHIP_RATE = 1.0e6
FS = CHIP_RATE * SPC  # 4 MHz channel rate
PAYLOAD = 64
PRI_MS = 500.0  # one burst every 500 ms
SYNC = np.array(
    [0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], dtype=np.uint8
)  # Barker-13

DOPPLER_HZ = 11_500.0  # true Doppler baked into the capture
NOMINAL_HZ = 12_000.0  # the receiver's predicted (coarse) Doppler
SNR_DB = 22.0

_ACODE = ((np.arange(ACQ_SF) * 2654435761 >> 13) & 1).astype(np.uint8)
_DCODE = ((np.arange(DATA_SF) * 40503 >> 7) & 1).astype(np.uint8)


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


def generate(capture_path, payloads, *, doppler_hz=DOPPLER_HZ, snr_db=SNR_DB):
    """Write a multi-burst capture to ``capture_path`` via the wfmgen CLI.

    Each burst is two ``bits`` segments (preamble, then the framed payload with
    a trailing gap filling the 500 ms PRI); ``freq`` bakes in the Doppler and
    ``snr`` the noise. Returns the per-PRI window length in samples."""
    pre = "".join(map(str, np.tile(_ACODE, REPS)))
    period = round(PRI_MS * 1e-3 * FS)
    segments = []
    for payload in payloads:
        fr = _frame_chips(payload)
        gap = period - (len(pre) + len(fr)) * SPC
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
    spec = {"version": 1, "segments": segments}
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump(spec, f)
        spec_path = f.name
    subprocess.run(
        [_wfmgen_bin(), "--from-file", spec_path, "-o", str(capture_path)],
        check=True,
    )
    Path(spec_path).unlink()
    return period


def stream_decode(capture_path, *, nominal_hz=NOMINAL_HZ, realtime=True):
    """Stream the capture back from disk one PRI chunk at a time (the natural
    chunk: one burst interval), paced to wall-clock, running a single
    DDC -> Acquisition -> BurstDemod chain on each."""
    reader = Reader(str(capture_path), sample_type="cf32")
    period = round(PRI_MS * 1e-3 * FS)
    clock = SampleClock(fs=FS) if realtime else None

    results = []
    npre = ACQ_SF * REPS * SPC
    burst_samps = (ACQ_SF * REPS + (len(SYNC) + PAYLOAD + 16) * DATA_SF) * SPC
    k = -1
    while True:
        window = reader.read(period)  # one PRI chunk off the disk
        if window is None or len(window) < period:
            break
        k += 1
        if clock is not None:
            clock.pace(len(window))  # throttle to real time

        # The burst sits at the start of each PRI chunk; hand the chain just
        # the burst (+ margin), not the long idle gap.
        rx = window[: burst_samps + 4000]
        # DDC: a pure tuner (rate=1.0) that removes the predicted bulk Doppler,
        # leaving a small residual inside Acquisition's search span.
        base = DDC(norm_freq=-nominal_hz / FS, rate=1.0).execute(rx)

        # cn0_dbhz sized so Acquisition runs its slow-time Doppler FFT over the
        # 5 preamble reps (doppler_bins = reps) — a real Doppler search that
        # resolves the residual; too high collapses it to a single DC bin.
        acq = Acquisition(
            _ACODE, reps=REPS, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=40.0
        )
        hits = acq.push(base)
        if not hits:
            results.append({"burst": k, "detected": False})
            continue
        dop, _cp, *_ = max(hits, key=lambda h: h[4])
        f0 = dop * acq.doppler_res_hz
        if dop >= acq.doppler_bins / 2:
            f0 -= acq.doppler_bins * acq.doppler_res_hz

        d = BurstDemod(_DCODE, SPC, CHIP_RATE, 0.0, 0.0, PAYLOAD, 10)
        d.set_preamble(_ACODE, REPS)
        d.set_sync(SYNC)
        rec = {"burst": k, "detected": True, "frame_valid": False}
        # The burst sits at the window start; sweep a small start offset until
        # the CRC confirms the preamble alignment (coarse Doppler seeds it).
        for start in range(0, ACQ_SF * SPC, SPC):
            if start + npre > len(base):
                break
            d.set_prior(f0 / FS, start)
            bits = d.demod(base)
            if d.frame_valid:
                rec.update(
                    frame_valid=True,
                    est_freq_hz=d.est_freq_hz + nominal_hz,
                    bits=bits,
                )
                break
        results.append(rec)
    return results


def main():
    rng = np.random.default_rng(0)
    payloads = [rng.integers(0, 2, PAYLOAD).astype(np.uint8) for _ in range(4)]
    with tempfile.TemporaryDirectory() as tmp:
        cap = Path(tmp) / "dsss_capture.cf32"
        generate(cap, payloads)
        mb = cap.stat().st_size / 1e6
        print(
            f"\nwfmgen -> {cap.name}: {len(payloads)} bursts, one per "
            f"{PRI_MS:.0f} ms, Doppler {DOPPLER_HZ / 1e3:.1f} kHz, "
            f"{SNR_DB:.0f} dB SNR ({mb:.0f} MB on disk)"
        )
        print(
            f"single chain: DDC(tune -{NOMINAL_HZ / 1e3:.0f} kHz) -> "
            f"Acquisition -> BurstDemod, paced to {FS / 1e6:.0f} MS/s\n"
        )
        results = stream_decode(cap, realtime=True)

    print(
        f"  {'burst':>5} {'det':>4} {'CRC':>5} {'bit errs':>9} "
        f"{'est Doppler':>13}"
    )
    for r, payload in zip(results, payloads):
        if not r["detected"]:
            print(f"  {r['burst']:>5} {'no':>4}")
            continue
        ok = "ok" if r["frame_valid"] else "FAIL"
        errs = int(np.sum(r["bits"] != payload)) if r["frame_valid"] else "-"
        f = f"{r.get('est_freq_hz', 0):.0f} Hz" if r["frame_valid"] else "-"
        print(f"  {r['burst']:>5} {'yes':>4} {ok:>5} {errs:>9} {f:>13}")
    print()


if __name__ == "__main__":
    main()
