"""dsss_burst_pipeline_demo.py — a 5-burst DSSS link, generated and received
through every doppler face that touches it.

**Waveform**: 5 bursts, each ``[ 5x512-chip preamble | 1000-symbol BPSK
payload spread 50 chips/symbol ]`` at payload Es/N0 = 10 dB, separated by a
variable (burst-to-burst distinct) inter-burst gap. The preamble is
unmodulated (a pure repeated code, for coherent acquisition); the payload
frame is ``sync word | 1000 payload bits | CRC-16``, spread by a *second*,
short code distinct from the preamble code.

**Generation — all three faces of ``wfmgen`` on the *same* declarative
scene**, cross-checked byte-identical:

  1. the ``wfmgen`` C CLI, given the scene as ``--from-file scene.json``;
  2. :meth:`doppler.wfm.Composer.from_file` — the same JSON, loaded straight
     into the Python binding;
  3. :class:`doppler.wfm.Composer`/:class:`doppler.wfm.Segment` built as
     Python objects, no JSON at all (the ``pattern=`` list sugar instead of
     the JSON string form).

If any two diverge, that is a genuine engine bug — the assertion is the
test, not just documentation.

**Reception** — the same capture run through three doppler receiver objects,
each demonstrated on its own before they are chained:

  1. :class:`doppler.dsss.Acquisition` alone, on each burst's preamble
     window: Doppler bin + code phase + CFAR test statistic.
  2. :class:`doppler.dsss.BurstDespreader` alone, seeded from the
     acquisition hit: tracks the loops through the preamble (``set_acq``)
     then despreads the frame to soft symbols / hard bits.
  3. :class:`doppler.dsss.BurstDemod`, the one-shot feedforward path: seeded
     with ``set_preamble``/``set_sync``/``set_prior``, ``demod()`` recovers
     the payload and checks the CRC-16 trailer in one call.

Run::

    python -m doppler.examples.dsss_burst_pipeline_demo

Rough edges found while building this (see the gallery page for the full
write-up):

* ``wfmgen``'s ``snr_mode="esno"``/``"auto"`` treats the modulated *symbol*
  of a ``type="bits"`` segment as one output chip, not the outer DSSS data
  symbol — hitting a target *data-symbol* Es/N0 needs a hand conversion
  (``snr_db_fs = esn0_db - 10*log10(sf*sps)``) and an explicit
  ``snr_mode="fs"``. There is no dedicated "Es/N0 for a spread symbol" knob.
* ``type="pn"`` codes are capped to Mersenne periods (``2**n - 1`` chips);
  an exact chip count like 512 or 50 needs a hand-built ``bits`` pattern
  instead.
* There is no dedicated "N discrete bursts, jittered spacing" primitive —
  build it as N segments with distinct (or ranged, ``[lo, hi]``)
  ``off_samples``.
* :class:`~doppler.dsss.BurstDespreader` has no absolute phase reference —
  the Costas loop locks to a line, not a point — so its raw hard bits can
  come out globally inverted. Resolving that sign is exactly what the
  sync-word correlation inside :class:`~doppler.dsss.BurstDemod` (or a
  hand-rolled equivalent) is for; ``BurstDespreader`` alone cannot do it.
* Under realistic noise, ``BurstDespreader.bits()``/``.steps()`` do not
  always emit exactly ``len(x) // (sf*sps)`` symbols for an exact-multiple
  input: the DLL's code-tracking jitter can slip the integrate-and-dump
  boundary by one symbol over a long (1000+ symbol) frame. Genuine
  streaming behaviour, not a bug — but code consuming the output must not
  assume the count is exact.
* **Neither DSSS ``snr_est`` field is in dB, despite the demo's first draft
  printing both as "snr(dB)".**
  :attr:`Acquisition.push`'s 6th tuple element is documented as "estimated
  *per-sample amplitude* SNR" (``acq_core.h``) — a linear ratio, computed as
  ``test_stat / sqrt(2*pi) / sqrt(2*n)``. It is *supposed* to look small and
  flat (observed ~0.2 here) even when ``test_stat`` is large and healthy
  (observed ~24): it backs the coherent-integration gain back out of
  ``test_stat`` to recover the raw per-sample SNR, which is a different,
  correctly-related quantity, not a broken one.
* ``BurstDespreader.snr_est`` (EMA of ``Re(prompt)^2 / Im(prompt)^2``) is
  *also* not dB — a power-domain ratio — and is numerically unstable once
  the Costas loop is well locked on BPSK: a locked BPSK prompt has
  ``Im -> 0``, so the ratio can spike to absurd values (observed: single
  digits up to 6.9e6 across otherwise-healthy bursts in this demo). Treat it
  as a rough lock-quality signal, not a calibrated SNR.
* Neither of the two fields above is suffixed ``_db`` even though sibling
  fields elsewhere in the same module are (:attr:`BurstDemod.est_snr_db`) —
  worth a consistent naming convention upstream so "not dB" is visible from
  the name.
* **Found and fixed a real bug in** :class:`~doppler.dsss.BurstDemod`
  (``native/src/burst_demod/burst_demod_core.c``). It is a **one-shot
  feedforward** design (no tracking loop, by its own header docs): one
  static ``(f0, mu)`` dechirp is applied across the whole payload, so any
  residual estimation error accumulates uncorrected phase drift over the
  frame. At this demo's original scale (1000-symbol payload, 5x512-chip
  preamble, Es/N0=10dB) a residual few-Hz error from the preamble-only
  estimate broke the CRC on more than half of runs; raising
  ``est_segments`` from 10 to 200 barely moved the pass rate (1-2/10
  either way), because ``est_segments`` only changes the *time-sampling
  grid* within the fixed preamble span — it can't buy back precision a
  short coherent observation doesn't have.

  The actual bug: ``burst_demod_demod()`` already squared the despread
  payload symbols and ran :class:`~doppler.dsss.PolynomialPhaseEstimator`
  over them (a baseline ~20x longer than the preamble) to NDA-refine the
  chirp rate ``mu`` — but discarded the *frequency* term
  (``e2.freq_norm``) the same ``ppe_estimate()`` call also returns, and
  gated the whole refinement behind ``max_rate > 0.0``, skipping it
  entirely for the Doppler-only (``max_rate=0``) case this demo uses. The
  fix: apply the discarded frequency correction the same way the rate
  correction was already applied (``f0 += 0.5 * e2.freq_norm / tsym``,
  halved for the BPSK squaring — safe here because the preamble estimate
  already pins the residual to a small fraction of a cycle per symbol,
  nowhere near the squaring's half-cycle ambiguity zone), and stop gating
  the block on ``max_rate`` (a Doppler-only ``ppe_create(nsym, 0.0)``
  naturally returns ``rate_norm=0``, so the rate term is a no-op when
  ``max_rate=0`` — no branch needed). Verified: 10/10 valid in an isolated
  numpy repro (residual freq error dropped from a few-to-17 Hz down to
  sub-Hz) and 5/5 in this demo's actual wfmgen capture, with the existing
  ``test_burst_demod.py``/``test_realtime_file_demod.py``/``test_ppe.py``
  suites (89 tests) still green. ``BurstDespreader``'s continuous tracking
  loop never had this ceiling in the first place — it doesn't need a good
  feedforward estimate, it converges to one.
"""

from __future__ import annotations

import json
import math
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np

from doppler.dsss import Acquisition, BurstDemod, BurstDespreader
from doppler.wfm import Composer, Segment

# ── waveform geometry ────────────────────────────────────────────────────────
ACQ_SF, REPS, DATA_SF, SPC = 512, 5, 50, 4
CHIP_RATE = 1.0e6
FS = CHIP_RATE * SPC  # 4 MHz channel rate
PAYLOAD = 1000
ESN0_DB = 10.0  # payload Es/N0, per DATA_SF-chip data symbol
N_BURSTS = 5
SYNC = np.array(
    [0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], dtype=np.uint8
)  # Barker-13 frame-sync word
# Distinct trailing gap (samples) after each of the 5 bursts — the
# "variable inter-burst spacing": each burst arrives at a different offset,
# so the receiver cannot assume a fixed grid. (A real link would draw these
# from a range, e.g. off_samples=[lo, hi] per segment, as in
# dsss_realtime_file_demod.py; fixed-but-distinct values keep this demo's
# ground truth exactly known so every stage below can be checked.)
GAPS = [20_000, 35_000, 15_000, 40_000, 25_000]

FRAME_LEN = len(SYNC) + PAYLOAD + 16  # sync + payload + CRC-16, in symbols
PRE_LEN = REPS * ACQ_SF * SPC  # preamble span, in samples
BURST_CHIPS = REPS * ACQ_SF + FRAME_LEN * DATA_SF
BURST_LEN = BURST_CHIPS * SPC  # one burst's active span, in samples


def _crc16(bits):
    """CRC-16-CCITT (poly 0x1021, init 0xFFFF), MSB-first — matches the C
    kernel in burst_demod_core.c bit-for-bit."""
    c = 0xFFFF
    for b in bits:
        c ^= (int(b) & 1) << 15
        c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def _build_codes_and_frame():
    """Two independent codes (long acq preamble, short data code) and the
    frame bit sequence (sync | payload | CRC-16) they will spread."""
    rng = np.random.default_rng(0)
    acq_code = rng.integers(0, 2, ACQ_SF).astype(np.uint8)
    data_code = rng.integers(0, 2, DATA_SF).astype(np.uint8)
    payload_bits = rng.integers(0, 2, PAYLOAD).astype(np.uint8)
    crc = _crc16(payload_bits)
    crc_bits = np.array([(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
    frame_bits = np.concatenate([SYNC, payload_bits, crc_bits])
    return acq_code, data_code, payload_bits, frame_bits


def _chip_pattern(acq_code, data_code, frame_bits):
    """Unmodulated preamble (REPS periods of acq_code) followed by the frame,
    each symbol XOR-spread by data_code -> one flat 0/1 chip pattern, fed to
    wfmgen as a single ``type=bits`` / ``modulation=bpsk`` segment."""
    preamble = np.tile(acq_code, REPS)
    frame_chips = np.bitwise_xor(
        data_code[None, :], frame_bits[:, None]
    ).astype(np.uint8)
    return np.concatenate([preamble, frame_chips.reshape(-1)])


def burst_starts():
    """Sample offset of each burst's preamble start (ground truth, since the
    gaps are fixed rather than drawn by the engine)."""
    starts, pos = [], 0
    for k in range(N_BURSTS):
        starts.append(pos)
        pos += BURST_LEN + GAPS[k]
    return starts


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


def _wfmgen_exe():
    """wfmgen_available(), or raise if the CLI isn't built."""
    exe = wfmgen_available()
    if exe is None:
        raise FileNotFoundError(
            "wfmgen CLI not found (build it: cmake --build build --target "
            "wfmgen_cli, or pip install .)"
        )
    return exe


def build_scene():
    """The 5-burst scene as both the ``Segment`` kwarg dicts (face 3, object
    composition) and the equivalent JSON scene dict (faces 1-2, CLI/from_file)
    — the exact same geometry expressed two ways."""
    acq_code, data_code, payload_bits, frame_bits = _build_codes_and_frame()
    chips = _chip_pattern(acq_code, data_code, frame_bits)
    assert len(chips) == BURST_CHIPS
    snr_fs_db = ESN0_DB - 10.0 * math.log10(DATA_SF * SPC)
    segment_kwargs = [
        {
            "type": "bits",
            "fs": FS,
            "freq": 0.0,
            "snr": snr_fs_db,
            "snr_mode": "fs",  # NOT "auto"/"esno" — see module docstring
            "seed": k + 1,
            "sps": SPC,
            "modulation": "bpsk",
            "pattern": chips.tolist(),
            "num_samples": BURST_LEN,
            "off_samples": GAPS[k],
        }
        for k in range(N_BURSTS)
    ]
    return segment_kwargs, acq_code, data_code, payload_bits, frame_bits


def _scene_json(segment_kwargs):
    """The JSON-wire form of segment_kwargs (``pattern`` as a char string,
    not a Python list) — what ``--from-file``/``Composer.from_file`` expect."""
    segments = []
    for kw in segment_kwargs:
        d = dict(kw)
        d["pattern"] = "".join(str(b) for b in d["pattern"])
        segments.append(d)
    return {
        "version": 1,
        "repeat": False,
        "continuous": False,
        "segments": segments,
    }


def generate_waveform(tmp_dir):
    """Generate the 5-burst capture through all three wfmgen faces and check
    they agree bit-for-bit; return the (agreed) samples plus the codes/bits
    the receiver side needs."""
    segment_kwargs, acq_code, data_code, payload_bits, frame_bits = (
        build_scene()
    )
    scene_path = Path(tmp_dir) / "scene.json"
    scene_path.write_text(json.dumps(_scene_json(segment_kwargs), indent=2))

    # Face 1 — the wfmgen C CLI.
    cli_out = Path(tmp_dir) / "capture_cli.cf32"
    subprocess.run(
        [
            _wfmgen_exe(),
            "--from-file",
            str(scene_path),
            "--output",
            str(cli_out),
        ],
        check=True,
        capture_output=True,
    )
    rx_cli = np.fromfile(cli_out, dtype=np.complex64)

    # Face 2 — Composer.from_file: the same JSON, loaded by the Python
    # binding instead of the C argv/file-reading path.
    rx_json = Composer.from_file(str(scene_path)).compose()

    # Face 3 — Segment/Composer built as Python objects, no JSON round trip.
    rx_obj = Composer([Segment(**kw) for kw in segment_kwargs]).compose()

    if not np.array_equal(rx_cli, rx_json):
        raise AssertionError("wfmgen CLI vs Composer.from_file diverged")
    if not np.array_equal(rx_cli, rx_obj):
        raise AssertionError(
            "wfmgen CLI vs Composer(Segment(...)) object API diverged"
        )
    print(
        f"  all 3 faces agree: {len(rx_cli)} samples, byte-identical "
        "(CLI == Composer.from_file == Composer(Segment(...)))"
    )
    return rx_cli, acq_code, data_code, payload_bits, frame_bits


def demo_acquisition(rx, starts, acq_code):
    """Acquisition alone: one fresh instance per burst, fed exactly that
    burst's preamble window."""
    print("\n== Acquisition (alone) ==")
    print(
        f"  {'burst':<5} {'dop bin':>7} {'code phase':>10} {'test stat':>9} "
        f"{'threshold':>9} {'snr_est(lin)':>12}"
    )
    results = []
    for k, start in enumerate(starts):
        window = rx[start : start + PRE_LEN]
        acq = Acquisition(
            acq_code, reps=REPS, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=40.0
        )
        hits = acq.push(window)
        if not hits:
            print(f"  {k:<5} no hit")
            results.append(None)
            continue
        dop, cp, _peak, _noise, test_stat, snr = max(hits, key=lambda h: h[4])
        # snr_est is a *linear per-sample amplitude* ratio (acq_core.h),
        # NOT dB, and NOT the post-integration detection stat — it's
        # expected to look small/flat for a spread-spectrum signal since
        # it backs the coherent gain back out of test_stat. See module
        # docstring's "Rough edges found".
        print(
            f"  {k:<5} {dop:>7d} {cp:>10d} {test_stat:>9.1f} "
            f"{acq.threshold:>9.1f} {snr:>12.3f}"
        )
        results.append((dop, cp, acq))
    return results


def demo_despreader(rx, starts, acq_results, acq_code, data_code, frame_bits):
    """BurstDespreader alone: seeded from the matching Acquisition hit, ran
    across the preamble (loop pull-in via set_acq) then the frame (soft
    symbols / hard bits). Reports both bit-polarity hypotheses, since a
    Costas loop alone cannot resolve the absolute sign — see module
    docstring."""
    print("\n== BurstDespreader (alone) ==")
    print(
        f"  {'burst':<5} {'symbols':>7} {'errs(as-is)':>11} "
        f"{'errs(inv)':>9} {'lock':>5} {'snr_est(pwr)':>12}"
    )
    results = []
    for k, (start, hit) in enumerate(zip(starts, acq_results)):
        if hit is None:
            print(f"  {k:<5} skipped (no acquisition hit)")
            results.append(None)
            continue
        dop, cp, acq = hit
        f0 = dop * acq.doppler_res_hz
        if dop >= acq.doppler_bins / 2:
            f0 -= acq.doppler_bins * acq.doppler_res_hz
        norm_freq = f0 / FS
        chip_phase = cp / SPC

        d = BurstDespreader(
            data_code,
            sf=DATA_SF,
            sps=SPC,
            init_norm_freq=norm_freq,
            init_chip_phase=chip_phase,
        )
        d.set_acq(acq_code, REPS)
        pre_win = rx[start : start + PRE_LEN]
        frame_win = rx[start + PRE_LEN : start + BURST_LEN]
        d.steps(pre_win)  # preamble: loops pull in, no symbols emitted
        hard = d.bits(frame_win)
        # At Es/N0=10dB the DLL's code-tracking jitter can slip the
        # integrate-and-dump boundary by one symbol over a 1000+ symbol
        # frame, so len(hard) is not always exactly len(frame_bits) — an
        # authentic tracking-loop effect, not a bug. Compare over the
        # common prefix and flag it rather than assume exact alignment.
        n = min(len(hard), len(frame_bits))
        errs_as_is = int(np.sum(hard[:n] != frame_bits[:n]))
        errs_inv = int(np.sum(hard[:n] != (1 - frame_bits[:n])))
        slip = (
            f" (slipped {len(frame_bits) - len(hard):+d})"
            if n != len(frame_bits)
            else ""
        )
        print(
            f"  {k:<5} {len(hard):>7} {errs_as_is:>11} {errs_inv:>9} "
            f"{d.lock_metric:>5.2f} {d.snr_est:>12.3g}{slip}"
        )
        results.append((min(errs_as_is, errs_inv), d.lock_metric, d.snr_est))
    return results


def demo_burst_demod(rx, starts, acq_code, data_code, payload_bits):
    """BurstDemod, one-shot per burst: set_preamble/set_sync configure the
    frame once; set_prior + demod() run the feedforward chain (dechirp,
    despread, frame-sync, CRC check) per burst."""
    print("\n== BurstDemod (full pipeline) ==")
    print(
        f"  {'burst':<5} {'CRC':<5} {'errs':>4} {'est freq(Hz)':>12} "
        f"{'est snr(dB)':>11} {'frame off':>9}"
    )
    d = BurstDemod(data_code, SPC, CHIP_RATE, 0.0, 0.0, PAYLOAD, 10)
    d.set_preamble(acq_code, REPS)
    d.set_sync(SYNC)
    results = []
    for k, start in enumerate(starts):
        window = rx[start : start + BURST_LEN]
        d.set_prior(0.0, 0)  # no injected Doppler in this scene
        bits_hat = d.demod(window)
        valid = bool(d.frame_valid)
        errs = (
            int(np.sum(bits_hat != payload_bits))
            if len(bits_hat) == len(payload_bits)
            else PAYLOAD
        )
        print(
            f"  {k:<5} {'ok' if valid else 'FAIL':<5} {errs:>4} "
            f"{d.est_freq_hz:>12.1f} {d.est_snr_db:>11.1f} "
            f"{d.frame_offset:>9d}"
        )
        results.append((valid, errs))
        d.reset()
    return results


def main():
    print("generating a 5-burst DSSS capture through all 3 wfmgen faces...")
    with tempfile.TemporaryDirectory() as tmp:
        rx, acq_code, data_code, payload_bits, frame_bits = generate_waveform(
            tmp
        )
    starts = burst_starts()
    print(
        f"  {N_BURSTS} bursts, {BURST_LEN} active samples/burst, "
        f"gaps={GAPS} samples"
    )

    acq_results = demo_acquisition(rx, starts, acq_code)
    demo_despreader(rx, starts, acq_results, acq_code, data_code, frame_bits)
    demod_results = demo_burst_demod(
        rx, starts, acq_code, data_code, payload_bits
    )

    n_ok = sum(1 for valid, _ in demod_results if valid)
    print(f"\nsummary: {n_ok}/{N_BURSTS} bursts decoded with a valid CRC")
    if n_ok < N_BURSTS:
        print(
            "  (a regression: BurstDemod's payload-domain frequency "
            "refinement — see 'Rough edges' in the module docstring —\n"
            "  should make this 5/5 reliably at this scale. If it isn't, "
            "check native/src/burst_demod/burst_demod_core.c for a\n"
            "  reverted or broken fix.)"
        )


if __name__ == "__main__":
    main()
