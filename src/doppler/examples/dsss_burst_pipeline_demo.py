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
     Python objects, no JSON at all.

Each burst is one declarative ``Segment(type="dsss", acq_code=..., acq_reps=5,
data_code=..., sync=..., payload=...)`` — the engine assembles the repeated
preamble, XOR-spreads the ``sync | payload | CRC-16`` frame with the second
code, sizes the segment to exactly one burst, and interprets
``snr_mode="esno"`` as the payload DATA-symbol Es/N0. Each burst's gap uses an
explicit, distinct sample count (not ``repeats=N``'s randomized redraws) so
every burst's ground truth position stays exactly known for scoring below;
``gap_noise="off"`` pins the inter-burst floor to silence for the same
reason (a realistic capture leaves the AWGN floor running through the
gaps — see the ``guide/wfmgen/dsss-bursts.md`` page for that variant).

If any two diverge, that is a genuine engine bug — the assertion is the
test, not just documentation.

**Reception** — the same capture run through three doppler receiver objects,
each demonstrated on its own before they are chained. Every downstream stage
is seeded from what :class:`~doppler.dsss.Acquisition` actually *finds*, not
from ground truth:

  1. :class:`doppler.dsss.Acquisition` alone: ONE instance, blindly and
     continuously sweeping the *entire* capture (silence, noise, and all 5
     bursts) with overlapping dwells, no prior knowledge of burst timing.
     Reports Doppler bin + absolute sample position + CFAR test statistic
     for every threshold crossing, deduplicated to one detection per
     cluster.
  2. :class:`doppler.dsss.BurstDespreader` alone, seeded from each
     discovered hit: tracks the loops through the preamble (``set_acq``)
     then despreads the frame to soft symbols, scored by
     :func:`doppler.snr.snr_data_aided_db` — a data-aided Es/N0 (dB)
     estimator (scale- and polarity-invariant) from the standalone
     :mod:`doppler.snr` module, used here instead of the object's own
     ``snr_est`` (see 'API notes' below for why).
  3. :class:`doppler.dsss.BurstDemod`, the one-shot feedforward path: seeded
     with ``set_preamble``/``set_sync``/``set_prior`` (from the same
     discovered hit), ``demod()`` recovers the payload and checks the
     CRC-16 trailer in one call.

A blind sweep across real noise will occasionally false-alarm; the
downstream stages correctly reject those (garbage lock/Es/N0, failed CRC)
rather than the pipeline assuming every detection is real.

Run::

    python -m doppler.examples.dsss_burst_pipeline_demo

API notes (see the gallery page for the full write-up):

* **How** :meth:`~doppler.dsss.Acquisition.push` **buffers/frames
  samples** (see ``native/src/acq/acq_core.c:322-410``):

  - It's a ring-buffer FIFO, not an accumulate-then-process call. Each call
    writes as many input samples as currently fit, drains every complete
    ``n = doppler_bins * code_bins``-sample frame available (one 2-D FFT +
    PN correlate + CFAR test per frame), and loops write/drain until the
    input is consumed — so one call can hand it far more than the ring's
    own capacity.
  - Leftover samples short of a full frame stay buffered in the ring
    *across calls* — unless a single call already hit the **hardcoded,
    non-configurable 64-result cap** (``dsss_ext_acq.c:148``, not exposed
    as a parameter), in which case the remaining input for that call is
    genuinely dropped, not buffered. Not a concern at realistic CFAR
    settings, but a real edge case for a pathologically long or
    under-thresholded single call.
  - Framing is strictly sequential and **non-overlapping**: one dwell is
    always exactly one frame's worth of samples, in order, with no
    sliding/search built in. A real burst's start is essentially arbitrary
    relative to a fixed dwell grid, so non-overlapping dwells *will*
    occasionally split a preamble across two dwells and miss it entirely.
    Achieving overlap (the way any real acquisition system searches with a
    coherent-integration primitive that doesn't overlap on its own) is the
    caller's job: ``reset()`` between dwells at a hop smaller than one
    dwell (this demo uses 1/4, 75% overlap).
* Pfa stays correctly calibrated under a blind, overlapping-dwell sweep
  like this demo's — validated by a large Monte-Carlo sweep
  (``dsss_acq_characterization.py``'s ``measure_sweep_pfa``) across
  several overlap fractions; overlapping search does not inflate the
  false-alarm rate beyond the configured ``pfa``.
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
* :attr:`Acquisition.push`'s ``cn0_dbhz_est`` tracks true C/N0 while AWGN
  dominates the CFAR noise estimate, and saturates at the code's own
  autocorrelation-sidelobe floor once C/N0 exceeds what the code/geometry
  can resolve — a real ceiling, not a bug.
* ``BurstDespreader.snr_est`` (EMA of ``Re(prompt)^2 / Im(prompt)^2``) is
  *not* dB and is numerically unstable once the Costas loop is well
  locked on BPSK: a locked BPSK prompt has ``Im -> 0``, so the ratio can
  spike to absurd values. Treat it as a rough lock-quality signal, not a
  calibrated SNR — this demo reports :func:`doppler.snr.snr_data_aided_db`
  instead.
* :class:`~doppler.dsss.BurstDemod` is one-shot feedforward (no tracking
  loop): one static ``(f0, mu)`` dechirp covers the whole payload, so its
  chirp-rate refinement matters. A
  :class:`~doppler.dsss.PolynomialPhaseEstimator` pass over the despread
  payload (a baseline ~20x longer than the preamble) NDA-refines both the
  rate and the residual frequency, applying both corrections regardless of
  whether a nonzero rate hypothesis was requested. At this demo's scale
  (1000-symbol payload, Es/N0=10 dB) that residual-frequency correction is
  what keeps the CRC passing on every burst.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np

from doppler.dsss import Acquisition, BurstDemod, BurstDespreader
from doppler.snr import snr_data_aided_db, snr_data_aided_db_series
from doppler.wfm import Composer, Segment, crc16

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
PRE_LEN = REPS * ACQ_SF * SPC  # preamble span, in samples = one acq dwell
BURST_CHIPS = REPS * ACQ_SF + FRAME_LEN * DATA_SF
BURST_LEN = BURST_CHIPS * SPC  # one burst's active span, in samples
TSYM = DATA_SF * SPC  # samples per despread data symbol
ACQ_HOP = PRE_LEN // 4  # blind-sweep dwell hop -> 75% overlap between dwells

ESN0_WINDOW = 51  # sliding-window length (symbols) for the Es/N0(dB) trace


def _build_codes_and_frame():
    """Two independent codes (long acq preamble, short data code), and the
    RX ground-truth frame bit sequence (sync | payload | CRC-16) the
    despreader's output is scored against. The CRC comes from
    :func:`doppler.wfm.crc16` — the same C kernel the ``dsss`` source
    appends on transmit and BurstDemod validates on receive."""
    rng = np.random.default_rng(0)
    acq_code = rng.integers(0, 2, ACQ_SF).astype(np.uint8)
    data_code = rng.integers(0, 2, DATA_SF).astype(np.uint8)
    payload_bits = rng.integers(0, 2, PAYLOAD).astype(np.uint8)
    c = crc16(payload_bits)
    crc_bits = np.array([(c >> (15 - j)) & 1 for j in range(16)], np.uint8)
    frame_bits = np.concatenate([SYNC, payload_bits, crc_bits])
    return acq_code, data_code, payload_bits, frame_bits


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
    — the exact same geometry expressed two ways.

    Each burst is ONE declarative ``type="dsss"`` segment: the engine tiles
    the preamble, XOR-spreads ``sync | payload | CRC-16`` by the data code,
    sizes the on-time to exactly one burst, and — because a dsss source's
    ``snr_mode="esno"`` refers to the outer DATA symbol — hits the target
    payload Es/N0 with no hand conversion."""
    acq_code, data_code, payload_bits, frame_bits = _build_codes_and_frame()
    segment_kwargs = [
        {
            "type": "dsss",
            "fs": FS,
            "freq": 0.0,
            "snr": ESN0_DB,
            "snr_mode": "esno",  # Es/N0 of the DATA_SF-chip data symbol
            "seed": k + 1,
            "sps": SPC,  # samples per CHIP
            "acq_code": acq_code.tobytes(),
            "acq_reps": REPS,
            "data_code": data_code.tobytes(),
            "sync": SYNC.tobytes(),
            "payload": payload_bits.tobytes(),  # CRC-16 auto-appended
            "off_samples": GAPS[k],
            # Pinned silent gaps: this walkthrough's numbers (5/5 decoded,
            # every false alarm attributable) depend on exactly-known burst
            # positions. The engine's default since gh-409 is gap_noise=
            # "auto" — gaps carry the segment's noise floor, the honest
            # capture for threshold tuning — demonstrated in the guide's
            # DSSS bursts page.
            "gap_noise": "off",
        }
        for k in range(N_BURSTS)
    ]
    return segment_kwargs, acq_code, data_code, payload_bits, frame_bits


def _scene_json(segment_kwargs):
    """The JSON-wire form of segment_kwargs (bit arrays as "0/1" strings) —
    what ``--from-file``/``Composer.from_file`` expect."""
    segments = []
    for kw in segment_kwargs:
        d = dict(kw)
        for key in ("acq_code", "data_code", "sync", "payload"):
            d[key] = "".join(str(b) for b in d[key])
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


def demo_acquisition(rx, acq_code, *, cn0_dbhz=40.0):
    """Acquisition, run the way it's actually meant to run: ONE instance,
    completely blind to where -- or whether -- a burst is anywhere in the
    stream, with no prior knowledge of burst timing. See the module
    docstring's "API notes" for how push() actually buffers/frames samples
    and why this function drives its own overlapping sweep instead of one
    call per known burst."""
    print("\n== Acquisition (alone, continuous blind sweep) ==")
    acq = Acquisition(
        acq_code, reps=REPS, spc=SPC, chip_rate=CHIP_RATE, cn0_dbhz=cn0_dbhz
    )
    raw = []
    n_dwells = (len(rx) - PRE_LEN) // ACQ_HOP + 1
    for pos in range(0, len(rx) - PRE_LEN + 1, ACQ_HOP):
        # reset() clears the ring + coherent accumulator: each dwell must
        # start clean, since push()'s own framing has no "restart here"
        # concept (see the walkthrough in the module docstring).
        acq.reset()
        for dop, cp, _peak, _noise, test_stat, cn0 in acq.push(
            rx[pos : pos + PRE_LEN]
        ):
            raw.append((pos + cp, dop, test_stat, cn0))
    print(
        f"  swept {n_dwells} overlapping dwells ({ACQ_HOP}-sample hop, "
        f"{PRE_LEN}-sample dwell) over {len(rx)} samples: "
        f"{len(raw)} raw threshold crossings"
    )

    # A real burst gets caught by several overlapping dwells as the window
    # slides through it (test stat rises then falls) -- cluster raw
    # crossings within one dwell-width of each other and keep the
    # strongest per cluster, so the report is one line per actual burst.
    raw.sort()
    hits = []
    for abs_pos, dop, test_stat, cn0 in raw:
        if hits and abs_pos - hits[-1]["abs_pos"] <= PRE_LEN:
            if test_stat > hits[-1]["test_stat"]:
                hits[-1].update(
                    abs_pos=abs_pos, dop=dop, test_stat=test_stat, cn0=cn0
                )
        else:
            hits.append(
                {
                    "abs_pos": abs_pos,
                    "dop": dop,
                    "test_stat": test_stat,
                    "cn0": cn0,
                }
            )

    print(
        f"  {'#':<3} {'abs. sample':>12} {'dop bin':>7} {'test stat':>9} "
        f"{'threshold':>9} {'C/N0 (dB-Hz)':>12}"
    )
    for i, h in enumerate(hits):
        # cn0_dbhz_est is a bandwidth/integration-time-independent estimate
        # of the burst's carrier-to-noise density, directly comparable to
        # this engine's own cn0_dbhz sizing input (acq_core.h) -- unlike a
        # raw per-sample or coherently-integrated ratio (both scale with
        # spc/reps and so aren't portable across configurations).
        print(
            f"  {i:<3} {h['abs_pos']:>12} {h['dop']:>7d} "
            f"{h['test_stat']:>9.1f} {acq.threshold:>9.1f} "
            f"{h['cn0']:>12.1f}"
        )
    return hits, acq


def demo_despreader(rx, hits, acq, acq_code, data_code, frame_bits):
    """BurstDespreader alone: seeded from each Acquisition hit *discovered
    by the blind sweep above* (not ground truth), ran across the preamble
    (loop pull-in via set_acq) then the frame (soft symbols). Bit errors are
    scored against both polarity hypotheses, since a Costas loop alone
    cannot resolve the absolute sign — see module docstring — but the
    reported Es/N0 needs no such resolution: it's invariant to a global sign
    flip (see :func:`doppler.snr.snr_data_aided_db`), so it's reported once,
    not per polarity. This *replaces* the object's own ``snr_est`` (not dB,
    not reliable once locked — see 'API notes') with a calibrated,
    data-aided Es/N0 (dB) computed against the known frame bits."""
    print("\n== BurstDespreader (alone) ==")
    print(
        f"  {'#':<3} {'symbols':>7} {'errs(as-is)':>11} "
        f"{'errs(inv)':>9} {'lock':>5} {'Es/N0(dB)':>9}"
    )
    results = []
    for k, hit in enumerate(hits):
        start = hit["abs_pos"]
        dop = hit["dop"]
        f0 = dop * acq.doppler_res_hz
        if dop >= acq.doppler_bins / 2:
            f0 -= acq.doppler_bins * acq.doppler_res_hz
        norm_freq = f0 / FS
        # abs_pos already IS the discovered code-phase-zero sample (the
        # sweep resolved code phase into an absolute position, not a
        # residual) -- feeding a window that starts exactly there leaves
        # ~0 chip phase for the DLL to seed from.
        d = BurstDespreader(
            data_code,
            sf=DATA_SF,
            sps=SPC,
            init_norm_freq=norm_freq,
            init_chip_phase=0.0,
        )
        d.set_acq(acq_code, REPS)
        pre_win = rx[start : start + PRE_LEN]
        frame_win = rx[start + PRE_LEN : start + BURST_LEN]
        d.steps(pre_win)  # preamble: loops pull in, no symbols emitted
        soft = d.steps(frame_win)  # complex prompt symbols, one per period
        hard = (soft.real < 0).astype(np.uint8)
        # At Es/N0=10dB the DLL's code-tracking jitter can slip the
        # integrate-and-dump boundary by one symbol over a 1000+ symbol
        # frame, so len(soft) is not always exactly len(frame_bits) — an
        # authentic tracking-loop effect, not a bug. Compare/estimate over
        # the common prefix rather than assume exact alignment.
        n = min(len(hard), len(frame_bits))
        errs_as_is = int(np.sum(hard[:n] != frame_bits[:n]))
        errs_inv = int(np.sum(hard[:n] != (1 - frame_bits[:n])))
        esn0_db = snr_data_aided_db(soft[:n], frame_bits[:n])
        slip = (
            f" (slipped {len(frame_bits) - len(soft):+d})"
            if n != len(frame_bits)
            else ""
        )
        print(
            f"  {k:<3} {len(soft):>7} {errs_as_is:>11} {errs_inv:>9} "
            f"{d.lock_metric:>5.2f} {esn0_db:>9.2f}{slip}"
        )
        results.append(
            {
                "errs": min(errs_as_is, errs_inv),
                "lock_metric": d.lock_metric,
                "esn0_db": esn0_db,
                "symbols": soft,
                "esn0_series": snr_data_aided_db_series(
                    soft[:n], frame_bits[:n], ESN0_WINDOW
                ),
            }
        )
    return results


def demo_burst_demod(rx, hits, acq, acq_code, data_code, payload_bits):
    """BurstDemod, one-shot per burst: set_preamble/set_sync configure the
    frame once; set_prior (seeded from each *discovered* Acquisition hit,
    not ground truth) + demod() run the feedforward chain (dechirp,
    despread, frame-sync, CRC check) per burst."""
    print("\n== BurstDemod (full pipeline) ==")
    print(
        f"  {'#':<3} {'CRC':<5} {'errs':>4} {'est freq(Hz)':>12} "
        f"{'est snr(dB)':>11} {'frame off':>9}"
    )
    d = BurstDemod(data_code, SPC, CHIP_RATE, 0.0, 0.0, PAYLOAD, 10)
    d.set_preamble(acq_code, REPS)
    d.set_sync(SYNC)
    results = []
    for k, hit in enumerate(hits):
        start, dop = hit["abs_pos"], hit["dop"]
        window = rx[start : start + BURST_LEN]
        f0 = dop * acq.doppler_res_hz
        if dop >= acq.doppler_bins / 2:
            f0 -= acq.doppler_bins * acq.doppler_res_hz
        d.set_prior(f0 / FS, 0)  # abs_pos already IS the preamble start
        bits_hat = d.demod(window)
        valid = bool(d.frame_valid)
        errs = (
            int(np.sum(bits_hat != payload_bits))
            if len(bits_hat) == len(payload_bits)
            else PAYLOAD
        )
        print(
            f"  {k:<3} {'ok' if valid else 'FAIL':<5} {errs:>4} "
            f"{d.est_freq_hz:>12.1f} {d.est_snr_db:>11.1f} "
            f"{d.frame_offset:>9d}"
        )
        results.append((valid, errs))
        d.reset()
    return results


def plot_esn0_drift(
    despreader_results, out_path="dsss_burst_pipeline_demo.png"
):
    """Plot the sliding-window Es/N0 (dB) trace vs symbol time, one line per
    burst, against the configured payload Es/N0 -- the visualization the
    console table's single scalar can't show: tracking-loop settling right
    after the preamble hand-off, and any mid-frame dip (e.g. the DLL
    boundary slip noted in 'API notes')."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(9, 5))
    for k, r in enumerate(despreader_results):
        if r is None:
            continue
        series = r["esn0_series"]
        t_ms = np.arange(len(series)) * TSYM / FS * 1000.0
        ax.plot(t_ms, series, lw=1.1, label=f"burst {k}")
    ax.axhline(
        ESN0_DB,
        color="0.3",
        ls="--",
        lw=1.2,
        label=f"configured Es/N0 = {ESN0_DB:.0f} dB",
    )
    ax.set_xlabel("time into frame (ms)")
    ax.set_ylabel(f"Es/N0 (dB), {ESN0_WINDOW}-symbol sliding window")
    ax.set_title(
        "BurstDespreader payload Es/N0 vs time — 5 bursts, data-aided"
    )
    ax.set_ylim(-40, 20)
    ax.grid(True, lw=0.5, alpha=0.5)
    ax.legend(fontsize=8, ncol=3, loc="lower center")
    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    plt.close(fig)
    return out_path


def _label_hits(hits, starts, tol=SPC):
    """Ground-truth labels, for REPORTING ONLY -- never fed back into the
    pipeline (demo_despreader/demo_burst_demod treat every hit identically,
    real or false). True where a hit lands within tol samples of an actual
    burst start, the same way you'd score a real detector's output against
    a controlled test injection."""
    return [any(abs(h["abs_pos"] - s) <= tol for s in starts) for h in hits]


def main():
    print("generating a 5-burst DSSS capture through all 3 wfmgen faces...")
    with tempfile.TemporaryDirectory() as tmp:
        rx, acq_code, data_code, payload_bits, frame_bits = generate_waveform(
            tmp
        )
    starts = burst_starts()  # ground truth -- used only to label below
    print(
        f"  {N_BURSTS} bursts, {BURST_LEN} active samples/burst, "
        f"gaps={GAPS} samples"
    )

    hits, acq = demo_acquisition(rx, acq_code)
    is_real = _label_hits(hits, starts)
    n_dwells = (len(rx) - PRE_LEN) // ACQ_HOP + 1
    n_real_found, n_false = sum(is_real), len(hits) - sum(is_real)
    print(
        f"  ground-truth check (labels for reporting only, not fed back "
        f"into the pipeline): {n_real_found}/{N_BURSTS} true bursts found, "
        f"{n_false} false alarm(s) -- naive pfa*dwells expectation was "
        f"~{n_dwells * 1e-3:.2f} (single-run Poisson variance around that "
        "is normal -- see 'API notes')"
    )

    despreader_results = demo_despreader(
        rx, hits, acq, acq_code, data_code, frame_bits
    )
    demod_results = demo_burst_demod(
        rx, hits, acq, acq_code, data_code, payload_bits
    )

    real_ok = sum(
        valid for (valid, _), real in zip(demod_results, is_real) if real
    )
    false_rejected = sum(
        not valid
        for (valid, _), real in zip(demod_results, is_real)
        if not real
    )
    print(
        f"\nsummary: {real_ok}/{n_real_found} real bursts decoded with a "
        f"valid CRC; {false_rejected}/{n_false} false alarms correctly "
        "rejected (failed CRC, as they should)"
    )
    if real_ok < n_real_found:
        print(
            "  (a regression: BurstDemod's payload-domain frequency "
            "refinement — see 'API notes' in the module docstring —\n"
            "  should make every REAL burst decode reliably at this "
            "scale. If it isn't, check\n"
            "  native/src/burst_demod/burst_demod_core.c for a reverted "
            "or broken fix.)"
        )
    if false_rejected < n_false:
        print(
            "  (notable: a false alarm passed CRC-16 -- a 1/65536 "
            "coincidence per attempt, worth a second look if reproducible)"
        )

    out_path = plot_esn0_drift(despreader_results)
    print(
        f"\nwrote {out_path} (Es/N0 vs time, {len(despreader_results)} "
        "detections)"
    )


if __name__ == "__main__":
    main()
