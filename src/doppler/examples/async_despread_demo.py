"""async_despread_demo.py — the streaming DSSS despreader at the DLL layer.

Drives :class:`doppler.track.Dll` in **segments** mode — the streaming
despreader whose one job is to remove the PN code and emit oversampled data
samples. Two things make this the *asynchronous* case, and both are why
segments mode exists:

  * The data symbols ride a clock **asynchronous** to the code epoch, so the
    DLL despreads in ``K`` **coherent** sub-epoch integrate-and-dump partials.
    A partial window that straddles a mid-symbol data flip merely loses
    amplitude, not phase — a single full-epoch I&D would be corrupted by the
    flip. (Coherent within each partial; the early-minus-late *combining* is
    what's non-coherent.)
  * The code Doppler dilates the chip clock. The power discriminator
    ``0.5*(|E|^2 - |L|^2)/|P|^2`` cannot pull that rate in on its own at low
    SNR, so the code rate is supplied by **carrier→code aiding**
    (:meth:`Dll.set_rate_aid`): the upstream carrier loop's offset scaled by
    ``carrier_offset/carrier_freq``. The DLL's own ``code_rate`` observable
    then stays ~1.0 — the loop only mops up the residual; the aid carries the
    Doppler.

The DLL runs on a **carrier-wiped** stream: the carrier loop is *upstream*
(here a genie de-rotate stands in for the Costas loop that
:class:`~doppler.dsss.AsyncDsssReceiver` runs before its DLL). With the carrier
removed and the rate aided, the despread output settles to clean BPSK and the
code stays locked through the Doppler.

It settles; it does not start there. The code NCO's increment is truncated
to an integer phase step (``nco_core.h``, 15f2a366): each update runs up to
1 LSB slow, 1.9e-6 of the code rate here, and half that on average. So the
loop starts against a frequency step of about 9.5e-7, which its integrator
has to absorb. That step sets only the SIZE of the disturbance. Its LENGTH
is set by the code loop's bandwidth: a type-2 loop pulls a frequency step in
with time constant ``1/(zeta*wn)``, ``wn = 8*zeta*bn/(1 + 4*zeta**2)``, about
375 epochs at the ``bn = 0.002``, ``zeta = 0.707`` used here. While the
integrator ramps up, the code phase lags, the despread envelope ripples and
the lock statistic dips. The run is long enough to show both the dip and the
recovery.

Writes TWO figures. The despread story is noiseless so the envelope is clean;
the lock detector needs noise to be meaningful, so it gets its own figure on
its own noisy signal — the two are never conflated.

``<out>.png`` — the despreader, three noiseless views:
  * **Oversampled async BPSK out** — the despread partial stream at ``K``
    samples/symbol; the symbol edges (dashed) slide through the code epochs
    because the symbol clock is independent of the code clock.
  * **Clean despread constellation** — the settled partials in the complex
    plane: two tight BPSK clusters at ±1 (carrier wiped upstream, rate
    aided), not the smeared ring an uncorrected residual carrier would leave.
  * **Code rate from aiding** — ``code_rate`` (the loop's own observable) sits
    at ~1.0 while the aid supplies the true ``1 + DCODE`` code-rate dilation:
    the DLL isn't rate-tracking the Doppler, it's being *handed* the rate.

``<out>_lock.png`` — the always-on lock detector on a SEPARATE noisy run: the
  non-coherent lock statistic ``R = sqrt(2*sum|P|^2 / E|O|^2)`` (acquisition's
  test, with a random off-peak EMA noise reference) climbing past the CFAR
  threshold at several SNRs, dipping while the code loop pulls in the NCO's
  frequency step and recovering, beside the noisy despread output it ran on.

Run:  python -m doppler.examples.async_despread_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.track import Dll
from doppler.wfm import Gold

# CCSDS Gold-1023 at the SPEC geometry. K = 11 coherent segments per epoch is
# dp_dll_lookback_segments(1023, 0.5 dB) -- the transition-free coherent
# windows
# the 1023 code splits into at the SPEC's tolerable correlation-power loss, and
# what AsyncDsssReceiver's refine/track stages use.
SF, SPS, K = 1023, 8, 11  # Gold-1023 chips, samples/chip, coherent segments
TE = SF * SPS  # code-epoch length, samples
F0 = 3e-4  # residual carrier, cyc/sample (removed upstream before the DLL)
DCODE = 2e-4  # code Doppler (chip-rate offset), supplied to the DLL as aid
DSYM = 4e-3  # symbol-vs-code rate offset (async)
PHI = 0.37 * TE  # symbol-clock phase, samples

# The code loop, and how long it needs. bn is normalised to the code-epoch
# rate (the loop updates once per epoch). The truncating code NCO starts the
# loop with a frequency step of up to 1 LSB: that step sets the SIZE of the
# transient. Its LENGTH comes from the loop bandwidth alone. A type-2 loop's
# response to a frequency step decays as exp(-zeta*wn*t), so its time
# constant is 1/(zeta*wn) epochs. This is dp_loop_filter_wn's formula (the
# check in main() pins it to LoopFilter's own gains). After 3 time constants
# the code-phase error is down to e^-3 = 5% of its peak, which costs R about
# 1.5% against the ~30% at the trough, so that is where the settled TAIL
# starts.
BN, ZETA = 0.002, 0.707  # code-loop noise bandwidth (per epoch), damping
WN = 8.0 * ZETA * BN / (1.0 + 4.0 * ZETA**2)  # rad/epoch
TAU = 1.0 / (ZETA * WN)  # pull-in time constant, epochs (~375)
SETTLE = int(np.ceil(3.0 * TAU))  # first settled epoch (~1126)
TAIL = 300  # settled epochs the tail asserts read
NSYM = SETTLE + TAIL  # one symbol spans ~1.004 epochs, so nep >= this


def make_signal(code, seed=9):
    """Async-data PN-spread BPSK with a residual carrier left on it."""
    rng = np.random.default_rng(seed)
    csign = np.where(code & 1, -1.0, 1.0)
    tsym = TE * (1.0 + DSYM)
    n = int(NSYM * tsym) + 2 * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, NSYM + 6) * 2 - 1).astype(float)
    si = np.clip(np.floor((idx - PHI) / tsym).astype(int), 0, len(data) - 1)
    cph = (idx * (1.0 + DCODE) / SPS).astype(int) % SF
    rx = data[si] * csign[cph] * np.exp(2j * np.pi * F0 * idx)
    return rx.astype(np.complex64), tsym


def carrier_wipe(rx):
    """Stand-in for the upstream carrier loop: de-rotate the residual carrier
    off the stream so the DLL sees a carrier-wiped signal (its documented
    input contract — the carrier loop wipes the carrier, the DLL wipes the
    code). AsyncDsssReceiver does this with a live Costas loop; here we
    de-rotate by the known F0."""
    idx = np.arange(len(rx))
    return (rx * np.exp(-2j * np.pi * F0 * idx)).astype(np.complex64)


# --8<-- [end:signal]


def main(
    out_path="async_despread_demo.png",
    lock_path="async_despread_demo_lock.png",
):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    code = Gold().generate(SF)
    rxw = carrier_wipe(make_signal(code)[0])  # carrier removed upstream
    _, tsym = make_signal(code)

    # The run length above restates dp_loop_filter_wn. Pin it to the loop
    # the library actually builds: for its bilinear PI gains
    # ki/kp = wn*t/(2*zeta), at t = 1 update per epoch.
    from doppler.track import LoopFilter

    lf = LoopFilter(BN, ZETA, 1.0)
    assert abs(2.0 * ZETA * lf.ki / lf.kp - WN) < 1e-12 * WN, "wn drifted"

    d = Dll(code, SPS, 0.0, BN, ZETA, 0.5, segments=K)
    assert abs(d.bn - BN) < 1e-15, "Dll does not run the loop SETTLE assumes"
    # Carrier→code aiding: the upstream carrier loop's offset, as a code-rate
    # ratio, hands the DLL the Doppler dilation it can't pull in on its own.
    d.set_rate_aid(DCODE)

    nep = len(rxw) // TE
    assert nep >= SETTLE + TAIL, f"run of {nep} epochs ends before the tail"
    rate = np.empty(nep)
    chunks = []
    for e in range(nep):
        chunks.append(d.steps(rxw[e * TE : (e + 1) * TE]))
        rate[e] = d.code_rate
    part = np.concatenate(chunks)  # K partials per epoch ~ K samples/symbol

    # Aided + carrier-wiped: the despread envelope sits at ~1 (except the
    # partial straddling each async symbol edge) and the code holds lock.
    env_med = float(np.median(np.abs(part[SETTLE * K :])))
    print(f"settled despread envelope median = {env_med:.3f}")
    assert env_med > 0.9, "despread envelope collapsed"
    print(f"code locked = {bool(d.locked)}  lock_stat = {d.lock_stat:.1f}")
    assert d.locked, "code loop did not hold lock"

    # code_rate is the loop's OWN observable (1 + integrator); with the aid
    # carrying the DCODE dilation, the loop only mops up residual, so it stays
    # near 1.0 — far below the true 1 + DCODE the aid supplies.
    settled_rate = float(rate[SETTLE:].mean())
    print(f"loop code_rate (aid off the books) = {settled_rate:.6f}")
    assert abs(settled_rate - 1.0) < DCODE, "loop residual larger than the aid"

    fig, (a, b, c) = plt.subplots(1, 3, figsize=(13.5, 4.2))

    # --- 1. oversampled asynchronous BPSK out (settled window) ---
    off = (SETTLE + TAIL // 2) * K  # inside the settled tail
    nshow = 16
    span = nshow * K
    pp = np.arange(off, off + span)
    sl = slice(off, off + span)
    # BPSK already on the real axis (carrier wiped); align residual phase.
    win = part[sl] * np.exp(-0.5j * np.angle(np.mean(part[sl] ** 2)))
    a.plot(
        pp,
        win.real,
        "-o",
        color="#1f77b4",
        ms=3.5,
        lw=1.0,
        label="despread (real)",
    )
    a.plot(pp, np.abs(part[sl]), color="#999999", lw=0.9, label="|despread|")
    a.plot(pp, -np.abs(part[sl]), color="#999999", lw=0.9)
    a.axhline(0, color="k", lw=0.6)
    m = 0  # async symbol edges (partials come out at the tracked code rate)
    while True:
        b_pp = (PHI + m * tsym) * K * (1.0 + DCODE) / TE
        if b_pp > off + span:
            break
        if b_pp >= off:
            a.axvline(b_pp, color="#d62728", ls="--", lw=1.0, alpha=0.7)
        m += 1
    a.set_xlim(off, off + span)
    a.set_title(
        f"Oversampled async BPSK out — {K}/symbol\n"
        "envelope dips: the 1/K straddle at each async symbol edge (red)",
        fontsize=9,
    )
    a.set_xlabel("partial index (= output sample)")
    a.set_ylabel("despread output (real)")
    a.legend(fontsize=7, loc="lower right")
    a.grid(alpha=0.25)

    # --- 2. clean despread constellation (carrier wiped + rate aided) ---
    w = part[SETTLE * K : (SETTLE + 80) * K]  # settled
    # align the BPSK axis to real (a downstream Costas job; genie here)
    w = w * np.exp(-0.5j * np.angle(np.mean(w**2)))
    b.scatter(w.real, w.imag, s=12, color="#1f77b4", alpha=0.5)
    lim = 1.4
    b.set_xlim(-lim, lim)
    b.set_ylim(-lim, lim)
    b.set_aspect("equal")
    b.axhline(0, color="k", lw=0.5)
    b.axvline(0, color="k", lw=0.5)
    b.set_title(
        "Clean despread BPSK, settled\n(carrier wiped upstream, rate aided)",
        fontsize=9,
    )
    b.set_xlabel("I")
    b.set_ylabel("Q")
    b.grid(alpha=0.25)

    # --- 3. code rate comes from aiding, not the DLL's own loop ---
    c.axhline(
        1.0 + DCODE,
        color="k",
        ls="--",
        lw=1.3,
        label="true rate (aid supplies)",
    )
    c.axhline(1.0, color="0.6", lw=0.8)
    c.plot(
        np.arange(nep),
        rate,
        color="#2ca02c",
        lw=1.1,
        label="loop code_rate (residual)",
    )
    c.set_ylim(1.0 - DCODE, 1.0 + 2 * DCODE)
    c.set_title(
        "Code rate from carrier→code aiding\n"
        "(loop observable stays ~1.0; the aid carries the Doppler)",
        fontsize=9,
    )
    c.set_xlabel("code epoch")
    c.set_ylabel("code rate (chips/chip)")
    c.legend(fontsize=8, loc="upper right")
    c.grid(alpha=0.25)

    fig.suptitle(
        f"track.Dll(segments={K}) streaming despreader — carrier-wiped, "
        f"rate-aided (SF={SF}, sps={SPS}, code Doppler {DCODE:.0e}, "
        f"async data δ={DSYM:.0e})",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")

    # The lock detector is a SEPARATE experiment — it needs noise to be
    # meaningful, whereas the panels above are noiseless so the envelope story
    # stays clean. Draw it in its own figure (on its own noisy signal) so the
    # two are never conflated.
    _lock_figure(code, plt, lock_path)


def _lock_figure(code, plt, out_path):
    """Always-on lock detector on its OWN noisy run (distinct from main()).

    Two panels: the lock statistic ``R`` climbing past the CFAR threshold at a
    few per-sample SNRs (with a noise-only trace that stays below), and the
    noisy despread output that produced the middle trace — so it is clearly a
    *different, noisy* signal from the noiseless despread figure. Same
    carrier-wiped + rate-aided DLL as main().
    """
    from doppler.detection import det_threshold_noncoherent

    rxw = carrier_wipe(make_signal(code)[0])
    nep = len(rxw) // TE
    thr = det_threshold_noncoherent(1e-3, 20)  # default lock config
    assert nep >= SETTLE + TAIL, f"run of {nep} epochs ends before the tail"
    rng = np.random.default_rng(5)

    def add_noise(namp):
        z = rng.standard_normal(len(rxw)) + 1j * rng.standard_normal(len(rxw))
        return (rxw + namp * z / np.sqrt(2)).astype(np.complex64)

    def noise_only(namp):
        z = rng.standard_normal(len(rxw)) + 1j * rng.standard_normal(len(rxw))
        return (namp * z / np.sqrt(2)).astype(np.complex64)

    fig, (a, b) = plt.subplots(1, 2, figsize=(11.5, 4.4))

    # --- left: R vs epoch at several SNRs, + noise-only, vs threshold ---
    runs = [
        ("strong (namp 3)", add_noise(3.0), "#1f77b4", "strong"),
        ("weak (namp 9)", add_noise(9.0), "#2ca02c", "mid"),
        ("very weak (namp 16)", add_noise(16.0), "#ff7f0e", None),
        ("noise only", noise_only(9.0), "#d62728", None),
    ]
    mid_part = None
    for label, rxn, col, tag in runs:
        dl = Dll(code, SPS, 0.0, BN, ZETA, 0.5, segments=K)
        dl.set_rate_aid(DCODE)
        R = np.empty(nep)
        chunks = []
        for e in range(nep):
            out = dl.steps(rxn[e * TE : (e + 1) * TE])
            R[e] = dl.lock_stat
            if tag == "mid":
                chunks.append(out)
        a.plot(np.arange(nep), R, color=col, lw=1.0, label=label)
        if tag == "mid":
            mid_part = np.concatenate(chunks)
        # Asserts read the settled TAIL (epochs >= SETTLE), not a median
        # over half the run: a median cannot tell a pull-in dip from a
        # loop walking off, which is how a decaying R went unnoticed (#1670).
        #  * CFAR: the strong and weak runs' tail sits at least 1.5x above
        #    eta; the noise-only tail stays below it. The very weak run is
        #    deliberately marginal (~1.4x) and gets no eta margin.
        #  * Recovery: every signal run's tail comes back to within 10% of
        #    where it started (epochs 2-39, before the dip has grown). The
        #    trough is ~30% down, so a run cut short or a loop that never
        #    absorbs the step fails here.
        r_tail = float(np.median(R[SETTLE:]))
        print(
            f"lock stat tail median ({label}) = {r_tail:.1f} vs eta {thr:.1f}"
        )
        if label == "noise only":
            assert r_tail < thr, "noise-only lock stat crossed the gate"
            continue
        if not label.startswith("very weak"):
            assert r_tail > 1.5 * thr, f"{label}: tail R within 1.5x of eta"
        r_start = float(np.median(R[2:40]))
        assert r_tail > 0.9 * r_start, (
            f"{label}: R did not recover ({r_tail:.1f} vs {r_start:.1f})"
        )
        if tag == "strong":
            # The dip, measured on a 50-epoch running median (R itself is
            # noisy epoch to epoch): its trough, and the first epoch after
            # it where R is back within 5% of the start level.
            win = np.lib.stride_tricks.sliding_window_view(R, 50)
            sm = np.median(win, axis=1)  # sm[e] = median of R[e : e + 50]
            trough = 2 + int(np.argmin(sm[2:SETTLE]))
            back = trough + int(np.argmax(sm[trough:] > 0.95 * r_start))
            print(
                f"strong run: start R {r_start:.1f}, trough "
                f"{sm[trough]:.1f} at epochs {trough}-{trough + 49}, back "
                f"within 5% from epoch {back} (tau = {TAU:.0f} epochs)"
            )
    a.axhline(thr, color="k", ls="--", lw=1.3, label=f"η={thr:.1f} (pfa=1e-3)")
    a.axvline(SETTLE, color="0.5", ls=":", lw=1.2, label="3τ: settled tail")
    a.set_ylim(0, None)
    a.set_title(
        "Lock statistic R vs epoch (noisy)\n"
        f"dip: loop pulls in the NCO's 1-LSB step, τ = 1/(ζωn) ≈ {TAU:.0f}",
        fontsize=9,
    )
    a.set_xlabel("code epoch")
    a.set_ylabel("lock statistic R")
    a.legend(fontsize=7, loc="center right")
    a.grid(alpha=0.25)

    # --- right: the noisy despread output the middle trace ran on ---
    if mid_part is not None:
        seg = mid_part[SETTLE * K : (SETTLE + 80) * K]  # settled
        seg = seg * np.exp(-0.5j * np.angle(np.mean(seg**2)))
        b.scatter(seg.real, seg.imag, s=10, color="#2ca02c", alpha=0.4)
        lim = 1.3 * float(np.median(np.abs(seg))) + 1e-9
        b.set_xlim(-4 * lim, 4 * lim)
        b.set_ylim(-4 * lim, 4 * lim)
    b.set_aspect("equal")
    b.axhline(0, color="k", lw=0.5)
    b.axvline(0, color="k", lw=0.5)
    b.set_title(
        "Noisy despread output (weak run)\n(the signal the R trace saw)",
        fontsize=9,
    )
    b.set_xlabel("I")
    b.set_ylabel("Q")
    b.grid(alpha=0.25)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    # Both default names are literals in main()'s signature on purpose:
    # `make gallery` moves exactly the quoted "<name>.png" literals a script
    # names (scripts/check_gallery_scripts.py). A lock path DERIVED from
    # out_path was invisible to it, so the committed _lock.png was never
    # refreshed. A custom out.png still gets its sibling out_lock.png.
    if len(sys.argv) > 1:
        out = sys.argv[1]
        stem = out[:-4] if out.endswith(".png") else out
        main(out, stem + "_lock.png")
    else:
        main()
