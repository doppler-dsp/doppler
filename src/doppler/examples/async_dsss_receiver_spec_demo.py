"""async_dsss_receiver_spec_demo.py -- the packaged
:class:`~doppler.dsss.AsyncDsssReceiver` decoding SPEC's own continuous
asynchronous DSSS waveform end to end, through physically-coupled clock
Doppler, at a true 10 dB Es/N0.

This is the closing example of the continuous-DSSS story: where
``async_dsss_receiver_demo.py`` (Stage 3) hand-composes the receive
chain (``Acquisition`` -> ``Dll`` -> ``RateConverter`` -> ``MpskReceiver``)
to *show the mechanics*, this page drives the single packaged object that
wraps that whole chain -- search, carrier refine, and live tracking --
against the literal SPEC waveform (``prototypes/async_despreader/SPEC.md``):
CCSDS Gold-1023, 3.069 Mcps, asynchronous BPSK at 2700 bps, 2.5 GHz
carrier, +/-50 kHz frequency uncertainty, <500 Hz/s rate of change.

**The two SPEC Doppler regimes never coexist.** On a real pass the
carrier Doppler is an S-curve: the frequency *extremum* (+/-50 kHz) occurs
at the pass edges where the *rate is ~0*, and the maximum *rate*
(500 Hz/s) occurs at the closest-approach zero-crossing where the *offset
is ~0*. They are 90 degrees out of phase -- you never see max offset and
max rate at the same instant. So a single capture is stressed in exactly
one of two ways, and this example exercises both:

- **TCA crossing** (the figure): Doppler ramping through zero at
  +500 Hz/s -- the maximum-rate, ~zero-offset regime that stresses the
  *carrier dynamics*.
- **Offset extremum** (asserted, not plotted): +50 kHz static, ~zero rate
  -- stresses *acquisition + the coupled code-rate offset*.

**Physically-coupled Doppler.** Every prior page injected the residual
carrier as a phase multiply on a *nominally-clocked* code -- the carrier
moved but the chip clock did not. Real Doppler moves both coherently
(same v/c dilates the code rate and shifts the carrier). Here
:class:`~doppler.impairment.DopplerChannel` imposes exactly that: it
resamples (dilates the clock) and applies the coherent carrier, so this is
the first end-to-end test in the repo of the receiver's carrier->code
aiding (``carrier_freq_hz=``), which feeds the tracked carrier offset into
the code loop as a rate bias the code discriminator alone cannot pull in.

**Two honesty notes, both load-bearing:**

- **10 dB, not SPEC's 5 dB.** The AWGN-only decode floor is ~5 dB (the C
  test ``_test_awgn_esn0_floor`` proves 5 dB decodes, 4 dB fails), but the
  reliable point under the full coupled Doppler is ~10 dB. SPEC's 5 dB
  floor under Doppler is aspirational for the current receiver.
- **BER alone is not trusted.** At this SNR a lag search can report false
  floors or lucky alignments, so decode is corroborated by two truth-free
  validators that need no reference symbols and no lag: self-referenced
  EVM (each symbol vs its own hard decision) and blind M2M4 SNR
  (:func:`~doppler.snr.snr_m2m4_db`).

**Two carrier loops, and which one does the work.** The receiver runs a
*pre-despread* Costas (loop 1) that de-rotates every input sample before
the code loop, then a *post-despread* MpskReceiver carrier loop (loop 2)
that mops up a small residual. The architecture is
``coarse -> freeze -> refine -> unfreeze & track (loop 1) -> despread ->
loop 2 mop-up``: loop 1 owns the Doppler dynamics, so on a ramp it is
loop 1 -- not loop 2 -- whose loop-filter output rides the sweep. (When
loop 1 was inadvertently frozen, loop 2 silently absorbed the whole ramp
and held a constant Type-II ramp phase error that rotated the
constellation a fixed few degrees; putting the ramp back on loop 1 is what
un-rotates it.) The tracking panel therefore plots loop 1's
``car_nco_freq`` (its loop-filter output = NCO command, at the front-end
sample rate): its *mean* rides the Doppler ramp with no lag, its
*variance* IS the loop stress.

**Lock readout.** Lock is the new binary symbol-lock indicator ``locked``:
a hysteretic (up/down verify-counted) detector on ``lock_metric``, the
SNR-weighted running mean of the BPSK lock signal
``(I^2-Q^2)/(I^2+Q^2) = cos(2*phi)`` over the emitted symbols (locked ->
~+1). Both ``lock_metric`` and its declare threshold ``lock_threshold``
are exposed for engineering debug, and the panel shows the metric climbing
past the threshold and the binary flag latching.

Four panels, at the TCA-crossing operating point:

1. **Decoded BPSK constellation**, all symbols coloured by time: the early
   settling-transient symbols are visibly scattered and distinct from the
   tight converged cloud.
2. **Carrier tracking**: loop 1's ``car_nco_freq`` (loop-filter output)
   rides the Doppler ramping through zero; its variance is the loop stress.
3. **Windowed decode correctness** (50-symbol windows, full run).
4. **Symbol lock**: ``lock_metric`` climbs past ``lock_threshold`` and the
   binary ``locked`` flag latches once the constellation converges.

Run:  python -m doppler.examples.async_dsss_receiver_spec_demo  [out.png]
"""

from __future__ import annotations

import sys
import warnings

# --8<-- [start:signal]
import numpy as np

from doppler.impairment import DopplerChannel
from doppler.wfm import Gold, Synth

SF = 1023  # CCSDS 415.0-G-1 command-link Gold code period (2**10 - 1)
CHIP_RATE = 3.069e6  # Mcps -- SPEC's exact chip rate
SPC = 2  # samples/chip front-end oversample
FS = CHIP_RATE * SPC  # 6.138 MHz
SYM_RATE = 2700.0  # bps -- chips/symbol = 1136.67, non-integer (async)
CARRIER = 2.5e9  # 2.5 GHz nominal RF

# TCA crossing (the figure's operating point): the maximum-rate regime.
# Doppler ramps through zero at +500 Hz/s -- start below zero so the
# crossing lands mid-capture (offset ~0 where the rate is maximal, exactly
# the pass geometry). The offset extremum (+/-50 kHz, ~0 rate) is the OTHER
# regime, asserted separately -- the two never coexist physically.
TCA_START_HZ = -370.0  # ~ -N_SYM/2/SYM_RATE * 500, so it crosses 0 mid-run
DOPPLER_RATE_HZ_S = 500.0  # SPEC's maximum rate of change
OFFSET_EXTREMUM_HZ = 50e3  # the +/-50 kHz frequency-uncertainty edge

ESN0_DB = 10.0  # true Es/N0 at the receiver input (noise added after Doppler)
N_SYM = 4000
SEED = 1
PRE = SF * SPC * 20 + 737  # pre-signal silence (not a whole # of epochs)

# CCSDS Gold code: real, cross-checked reference (3-valued sidelobes).
CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)


def make_capture(start_hz: float, rate_hz_s: float, seed: int):
    """SPEC's continuous async DSSS waveform through coupled clock Doppler,
    with AWGN added *after* the channel at a true 10 dB Es/N0.

    The clean signal is synthesised by :class:`~doppler.wfm.Synth` in
    continuous DSSS mode (``symbol_rate>0``): the Gold code repeats forever
    and a known random payload rides on it at ``SYM_RATE`` with non-integer
    chips/symbol. :class:`~doppler.impairment.DopplerChannel` then dilates
    the clock and applies the coherent carrier for a Doppler that starts at
    ``start_hz`` and ramps at ``rate_hz_s`` (both expressed as ppm of the RF
    carrier); noise is added last at the Es/N0 referenced to the outer data
    symbol -- ``cn0 = Es/N0 + 10log10(symbol_rate)``.

    Returns
    -------
    x : NDArray[np.complex64]
        The full capture (pre-silence + coupled-Doppler signal + noise).
    data : NDArray[np.float64]
        The transmitted BPSK symbols (+/-1), ground truth for BER.
    """
    rng = np.random.default_rng(seed)
    payload = rng.integers(0, 2, N_SYM + 8).astype(np.uint8)
    n = int(N_SYM * FS / SYM_RATE) + 4 * SF * SPC
    syn = Synth(
        type="dsss",
        data_code=bytes(CODE.tolist()),
        symbol_rate=SYM_RATE,
        sps=SPC,
        snr=100.0,  # clean -- noise is added below, after Doppler
        fs=FS,
        bits=bytes(payload.tolist()),  # known payload = BER ground truth
        seed=seed,
    )
    clean = syn.steps(n).astype(np.complex64)

    channel = DopplerChannel(
        fs=FS,
        carrier_hz=CARRIER,
        doppler_ppm=start_hz / CARRIER * 1e6,
        doppler_rate_ppm_s=rate_hz_s / CARRIER * 1e6,
    )
    signal = channel.execute(clean)

    x = np.concatenate([np.zeros(PRE, np.complex64), signal])
    cn0 = ESN0_DB + 10.0 * np.log10(SYM_RATE)
    sigma = 1.0 / np.sqrt(10.0 ** (cn0 / 10.0) / FS)
    noise = (sigma / np.sqrt(2.0)) * (
        rng.standard_normal(len(x)) + 1j * rng.standard_normal(len(x))
    )
    x = (x + noise).astype(np.complex64)
    return x, np.where(payload > 0, 1.0, -1.0)


# --8<-- [end:signal]

from doppler.dsss import AsyncDsssReceiver  # noqa: E402
from doppler.snr import snr_m2m4_db  # noqa: E402

TE = SF * SPC  # samples per code epoch (the streaming chunk)


# --8<-- [start:receiver]
def receive(x: np.ndarray):
    """Stream the whole capture through one packaged
    :class:`~doppler.dsss.AsyncDsssReceiver`, epoch by epoch, collecting the
    demodulated symbols and the per-epoch carrier telemetry in one pass.

    ``carrier_freq_hz=CARRIER`` turns on carrier->code aiding: the tracked
    carrier offset is fed to the code loop as a rate bias, without which the
    coupled code-rate error outruns the code discriminator's pull-in at this
    SNR. ``steps()`` accepts any block size (state carries across calls), so
    one epoch per call is equivalent to one big call -- it just lets us
    sample the telemetry.

    The telemetry, indexed by INPUT sample position (the receiver's own
    sample clock -- the correct time base; the output-symbol index lags it by
    the acquisition/refine settling delay), is:

    - ``doppler_hz``    : the frozen post-refine coarse estimate (a DDC offset)
    - ``car_nco_freq``  : loop 1's (pre-despread Costas) loop-filter OUTPUT =
      NCO command at the front-end rate -- its mean rides the Doppler ramp,
      its variance is the loop stress
    - ``lock_metric``   : SNR-weighted cos(2*phi) symbol-lock metric (locked
      -> ~+1), with ``lock_threshold`` the declare level
    - ``locked``        : the binary hysteretic symbol-lock flag
    """
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        rx = AsyncDsssReceiver(
            code=CODE,
            chip_rate=CHIP_RATE,
            symbol_rate=SYM_RATE,
            spc=SPC,
            cn0_dbhz=ESN0_DB + 10.0 * np.log10(SYM_RATE),
            doppler_uncertainty=60e3,  # cover the +/-50 kHz uncertainty
            carrier_freq_hz=CARRIER,  # enable carrier->code aiding
        )
    syms: list[np.ndarray] = []
    trace: list[tuple[int, float, float, float, float]] = []
    for pos in range(0, len(x) - TE, TE):
        block = rx.steps(x[pos : pos + TE])
        if len(block):
            syms.append(block)
        if rx.tracking:
            trace.append(
                (
                    pos,
                    rx.doppler_hz,
                    rx.car_nco_freq,
                    rx.lock_metric,
                    float(rx.locked),
                )
            )
    return (
        rx,
        np.concatenate(syms) if syms else np.empty(0, np.complex64),
        (np.array(trace) if trace else np.zeros((0, 5))),
    )


# --8<-- [end:receiver]


# --8<-- [start:validate]
def best_ber(syms: np.ndarray, data: np.ndarray):
    """Wide lag + polarity search of the back-half hard decisions against the
    known payload. The lag absorbs the acquisition/refine settling delay
    (which grows as Es/N0 drops); over a several-hundred-symbol back half a
    spurious sub-0.05 alignment is statistically impossible, so a wide search
    stays honest. Returns ``(bits, ber, lag, inverted)``."""
    bits = np.where(syms.real > 0, 1.0, -1.0)
    lo, hi = len(bits) // 2, len(bits)
    best = (1.0, 0, False)
    for lag in range(-300, 301):
        idx = np.arange(lo, hi) + lag
        ok = (idx >= 0) & (idx < len(data))
        if ok.sum() < 40:
            continue
        truth = data[idx[ok]]
        e = float(np.mean(bits[lo:hi][ok] != truth))
        if e < best[0]:
            best = (e, lag, False)
        if 1.0 - e < best[0]:
            best = (1.0 - e, lag, True)
    return bits, best[0], best[1], best[2]


def self_evm_db(z: np.ndarray) -> float:
    """Self-referenced EVM (dB): de-rotate a symbol block by the BPSK
    squaring angle, unit-power normalise, hard-decide +/-1, RMS error to the
    nearest. No truth symbols, no lag -- a locked constellation sits near
    -Es/N0, a scattered one near 0 dB."""
    if len(z) < 16:
        return 0.0
    z = z.astype(np.complex128)
    z = z * np.exp(-1j * 0.5 * np.angle(np.sum(z * z)))
    z = z / np.sqrt(np.mean(np.abs(z) ** 2))
    dec = np.where(z.real > 0, 1.0, -1.0)
    return 20.0 * np.log10(np.sqrt(np.mean(np.abs(z - dec) ** 2)) + 1e-12)


def decode_and_check(x: np.ndarray, data: np.ndarray, label: str):
    """Receive, decode, and validate one capture three ways -- a wide-lag BER
    for the headline, plus the two truth-free metrics (self-EVM, blind M2M4
    SNR) that make the lock claim trustworthy. Returns everything the plot
    needs; asserts a clean, corroborated decode."""
    rx, syms, trace = receive(x)
    assert len(syms) > 1000, f"[{label}] receiver never reached tracking"
    bits, ber, lag, inv = best_ber(syms, data)
    settled = syms[len(syms) // 2 :]
    evm = self_evm_db(settled)
    m2m4 = float(snr_m2m4_db(settled.astype(np.complex64)))
    print(
        f"[{label}] {len(syms)} syms  ber={ber:.4f}  evm={evm:.1f} dB  "
        f"m2m4={m2m4:.1f} dB  locked={rx.locked}  "
        f"lock_metric={rx.lock_metric:.3f}"
    )
    assert ber < 0.02, f"[{label}] failed to decode cleanly (ber={ber:.3f})"
    assert evm < -6.0, f"[{label}] constellation scattered (evm={evm:.1f})"
    assert m2m4 > 7.0, f"[{label}] not locked (m2m4={m2m4:.1f})"
    return rx, syms, trace, bits, ber, lag, inv


# --8<-- [end:validate]


def _windowed_ber(bits, data, lag, inv, window=50):
    aligned = bits if not inv else -bits
    idx = lag + np.arange(len(bits))
    valid = (idx >= 0) & (idx < len(data))
    correct = np.full(len(bits), np.nan)
    correct[valid] = (aligned[valid] == data[idx[valid]]).astype(float)
    n_win = len(correct) // window
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", RuntimeWarning)
        return np.array(
            [
                1.0 - np.nanmean(correct[i * window : (i + 1) * window])
                for i in range(n_win)
            ]
        )


def main(out_path: str = "async_dsss_receiver_spec_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    # The figure's case: the TCA crossing (max rate, Doppler through zero).
    x, data = make_capture(TCA_START_HZ, DOPPLER_RATE_HZ_S, SEED)
    rx, syms, trace, bits, ber, lag, inv = decode_and_check(x, data, "TCA")

    # The OTHER physical regime -- offset extremum (+/-50 kHz, ~0 rate) --
    # decodes too; assert it so the example self-validates both, without
    # claiming the two ever coexist.
    xe, de = make_capture(OFFSET_EXTREMUM_HZ, 0.0, SEED)
    decode_and_check(xe, de, "+50kHz static")

    wber = _windowed_ber(bits, data, lag, inv)
    assert np.nanmean(wber[-5:]) < 0.05, "decode did not stay converged"

    # Loop 1's NCO command is the ABSOLUTE carrier estimate (seeded from the
    # refine value, then tracking on its own), in cycles/sample of the
    # front-end rate -- so car_nco_freq * FS is the full tracked offset in Hz,
    # no need to add the frozen seed dh (that would double-count). It rides
    # the ramp; its variance is the loop stress.
    pos, _dh, car_nco, lock_metric, locked = (trace[:, i] for i in range(5))
    t = (pos - PRE) / FS
    true_off = TCA_START_HZ + DOPPLER_RATE_HZ_S * t
    tracked_car = car_nco * FS
    lock_thresh = float(rx.lock_threshold)

    fig, ((a, b), (c, d)) = plt.subplots(2, 2, figsize=(11, 8.5))

    # All recovered symbols, coloured by time: the early (settling-transient)
    # symbols are visibly scattered and distinct from the tight converged
    # cloud -- exactly what the running BER and lock detector quantify.
    ci = np.arange(len(syms))
    sc = a.scatter(syms.real, syms.imag, s=7, c=ci, cmap="viridis", alpha=0.6)
    a.axhline(0, color="k", lw=0.5)
    a.axvline(0, color="k", lw=0.5)
    lim = 1.3 * np.percentile(np.abs(syms[len(syms) // 3 :]), 99)
    a.set_xlim(-lim, lim)
    a.set_ylim(-lim, lim)
    a.set_aspect("equal")
    a.set_title(f"Decoded BPSK (all symbols)\nBER={ber:.4f}", fontsize=9)
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.grid(alpha=0.25)
    cb = fig.colorbar(sc, ax=a, fraction=0.046, pad=0.04)
    cb.set_label("symbol index (time)", fontsize=7)
    cb.ax.tick_params(labelsize=7)

    b.plot(
        t,
        tracked_car,
        color="#1f77b4",
        lw=0.8,
        alpha=0.8,
        label="loop 1 NCO cmd (car_nco_freq)",
    )
    b.plot(t, true_off, color="k", lw=1.4, ls="--", label="true offset")
    b.axhline(0, color="k", lw=0.4)
    b.set_title(
        "Carrier tracking: TCA crossing, Doppler through 0 at 500 Hz/s\n"
        "(loop 1 owns the ramp; its variance is the loop stress)",
        fontsize=9,
    )
    b.set_xlabel("time (s)")
    b.set_ylabel("carrier offset (Hz)")
    b.legend(fontsize=7)
    b.grid(alpha=0.25)

    c.plot(np.arange(len(wber)) * 50, wber, lw=1.1, color="#1f77b4")
    c.axhline(0.5, color="k", lw=0.8, ls="--", label="chance")
    c.set_ylim(-0.05, 0.65)
    c.set_title(
        "Windowed decode correctness, full run\n(50-symbol windows)",
        fontsize=9,
    )
    c.set_xlabel("symbol index")
    c.set_ylabel("windowed BER")
    c.legend(fontsize=7)
    c.grid(alpha=0.25)

    d.plot(
        t,
        lock_metric,
        lw=1.1,
        color="#1f77b4",
        label="lock_metric = cos(2$\\phi$)",
    )
    d.axhline(
        lock_thresh,
        color="#d62728",
        lw=1.0,
        ls="--",
        label=f"lock_threshold ({lock_thresh:.2f})",
    )
    d.fill_between(
        t,
        -0.1,
        1.1,
        where=locked > 0.5,
        color="#2ca02c",
        alpha=0.12,
        step="post",
        label="locked",
    )
    d.set_title(
        "Symbol lock: metric climbs past threshold, binary flag latches",
        fontsize=9,
    )
    d.set_xlabel("time (s)")
    d.set_ylabel("lock metric")
    d.set_ylim(-0.1, 1.1)
    d.legend(fontsize=7, loc="lower right")
    d.grid(alpha=0.25)

    fig.suptitle(
        f"AsyncDsssReceiver -- SPEC continuous async DSSS "
        f"(CCSDS Gold-{SF}, {CHIP_RATE / 1e6:.3f} Mcps, {SYM_RATE:.0f} bps), "
        f"TCA crossing: Doppler through 0 at 500 Hz/s, Es/N0={ESN0_DB:.0f} dB",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.subplots_adjust(hspace=0.55, wspace=0.3)
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(
        sys.argv[1]
        if len(sys.argv) > 1
        else "async_dsss_receiver_spec_demo.png"
    )
