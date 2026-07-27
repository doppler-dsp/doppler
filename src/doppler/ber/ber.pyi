# ber/ber.pyi — type stubs for the ber C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class BerMeter:
    """Create a meter for constellation m stopping at target_errors.

    Parameters
    ----------
    m : int, default 4
        Constellation order (2, 4, 8).
    target_errors : int, default 200
        Inverse-binomial stop condition; 0 selects 200.
    conf : float, default 0.99
        Two-sided confidence level; 0 selects 0.99.

    Examples
    --------
    Create with defaults:

    >>> from doppler.ber import BerMeter
    >>> obj = BerMeter(m=4, target_errors=200, conf=0.99)

    """
    def __init__(self, m: int = ..., target_errors: int = ..., conf: float = ...) -> None: ...

    def reset(self) -> None:
        """Zero the counters; keeps the configuration and the truth.
        """

    def set_truth(self, truth: NDArray[np.uint8]) -> int:
        """Install the transmitted symbol INDICES (0..m-1, not Gray labels) this meter scores against. Copied, so the caller's buffer need not outlive the call, and reused across every burst. Raises ValueError if any index is outside 0..m-1.

        Copied, so the caller's buffer need not outlive the call, and reused
        across every burst. Values are symbol INDICES in `0..m-1` (not Gray
        labels).

        Parameters
        ----------
        truth : NDArray[np.uint8]
            Input.

        Returns
        -------
        int
            DP_OK, or DP_ERR_INVALID if any index is outside `0..m-1`.
        """

    def align(self, rx: NDArray[np.complex64], t0: int = 0, n_marker: int = 0, period: int = 0, lag_span: int = 0, pfa: float = 0.0) -> int:
        """Detect where the recovered symbols sit against truth, returning a BerAlign. The alignment is DETECTED by correlating a known marker -- truth[t0 : t0+n_marker], optionally repeating every `period` symbols -- and gated by a false-alarm probability, NOT searched by minimising the error count. That distinction is the whole point: a min-over-(lag, rotation) search is an optimisation over the answer, and it both false-passes on a lucky alignment and false-floors when the true lag falls outside the span. A marker too short to identify an alignment returns ok=False rather than a plausible wrong lag. Repeats are combined non-coherently, which raises the processing gain and exposes cycle slips.

        Parameters
        ----------
        rx : NDArray[np.complex64]
            Input.
        t0 : int
            Input.
        n_marker : int
            Input.
        period : int
            Input.
        lag_span : int
            Input.
        pfa : float
            Input.

        Returns
        -------
        int
            Output.
        """

    def score(self, rx: NDArray[np.complex64], lo: int = 0, hi: int = 0) -> int:
        """Score rx[lo:hi] against the truth and accumulate; returns the symbols scored. Uses the supplied alignment VERBATIM -- no lag search, no rotation search, no minimisation of any kind over the answer. Uses the alignment align() last detected, together with the marker geometry that found it, so a measurement cannot be handed an alignment belonging to a different burst. Symbols covered by a marker occurrence are excluded, as are any whose truth index falls outside the installed sequence; both land in `skipped`.

        Uses the supplied alignment verbatim — no lag search, no rotation
        search, no minimisation of any kind over the answer. Symbols covered by
        a marker occurrence are excluded, as are any whose truth index falls
        outside the installed sequence.

        Parameters
        ----------
        rx : NDArray[np.complex64]
            Input.
        lo : int
            Input.
        hi : int
            Input.

        Returns
        -------
        int
            Symbols scored.
        """

    def ser(self) -> tuple[float, float, float, float, float, int, int]:
        """Symbol error rate with its EXACT confidence interval, as a BerInterval. Assert on `lo`, never on `p_hat`: comparing the lower limit against a spec is the form that cannot flake on counting noise. The interval is the Gamma/chi-square one for inverse binomial sampling -- no normal approximation anywhere, so it stays honest at the small error counts where a Wald interval is worst -- and its quantiles come from doppler's own detection primitives rather than a second copy of an incomplete-gamma kernel.

        Returns
        -------
        tuple[float, float, float, float, float, int, int]
            Output.
        """

    def ber(self) -> tuple[float, float, float, float, float, int, int]:
        """Gray-coded bit error rate with its EXACT confidence interval, as a BerInterval. Same statistics as ser(), counted over bits.

        Returns
        -------
        tuple[float, float, float, float, float, int, int]
            Output.
        """

    def interval(self, errors: int, symbols: int) -> tuple[float, float, float, float, float, int, int]:
        """The exact confidence interval for error/trial counts gathered elsewhere, at this meter's confidence level. Same statistics as ser(): the Gamma/chi-square interval for inverse binomial sampling, with quantiles from doppler's own inverse regularized incomplete gamma rather than a normal approximation, so it stays honest at the small error counts where a Wald interval is worst. Assert on `lo`, never on `p_hat`.

        Parameters
        ----------
        errors : int
            Input.
        symbols : int
            Input.

        Returns
        -------
        tuple[float, float, float, float, float, int, int]
            Output.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def errors(self) -> int:
        """Symbol errors counted so far."""

    @property
    def symbols(self) -> int:
        """Symbols scored so far."""

    @property
    def bit_errors(self) -> int:
        """Gray-coded bit errors counted so far."""

    @property
    def bits(self) -> int:
        """Bits scored so far."""

    @property
    def skipped(self) -> int:
        """Symbols skipped: covered by a marker occurrence, or with a truth index outside the installed sequence. Marker symbols are excluded on purpose -- they are known, so scoring them would flatter the rate with symbols that had no chance of being wrong."""

    @property
    def m(self) -> int:
        """Constellation order."""

    @property
    def target_errors(self) -> int:
        """The inverse-binomial stop condition."""

    @property
    def conf(self) -> float:
        """Two-sided confidence level used by ser() and ber()."""

    @property
    def enough(self) -> int:
        """True once target_errors have been counted. THE stop condition for a measurement loop: fix the ERROR count and let the symbol count fall out (inverse binomial sampling), so the relative standard error is 1/sqrt(r) -- a function of the error count alone. Stopping on a fixed symbol count instead makes the precision depend on the very rate being measured: 20000 symbols at SER 1e-3 gives ~20 errors and ~22% relative error, which reads as real seed-to-seed variation in the receiver and is not."""

    @property
    def lag(self) -> int:
        """Alignment from the last align(): rx[i] carries truth[i + lag]."""

    @property
    def phase(self) -> float:
        """Absolute residual constellation rotation from the last align(), radians. The marker resolves this outright, so there is no M-fold ambiguity left to search over."""

    @property
    def align_stat(self) -> float:
        """Detection statistic at the correlation peak, in threshold units."""

    @property
    def align_margin_db(self) -> float:
        """20*log10(stat/threshold) -- headroom over the false-alarm gate. Negative means the alignment was NOT detected."""

    @property
    def align_runner_db(self) -> float:
        """Peak over the best runner-up lag, dB. Under ~3 dB the peak is ambiguous and the alignment is rejected."""

    @property
    def align_occurrences(self) -> int:
        """Marker occurrences combined non-coherently."""

    @property
    def align_slips(self) -> int:
        """Occurrences whose phase disagreed with the peak by more than half a decision sector -- cycle slips."""

    @property
    def align_saturated(self) -> int:
        """True when the peak sat on an edge of the lag search: lag_span is too small and the result is not trustworthy."""

    @property
    def align_ok(self) -> int:
        """True when the last align() was detected, unambiguous and unsaturated. score() is meaningless unless this is True."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "BerMeter": ...

    def __exit__(self, *args: object) -> None: ...

def ber_theory_ser(m: int, esn0: float) -> float:
    """Coherent M-PSK symbol error rate at matched-filter Es/N0 (LINEAR, not dB). BPSK Q(sqrt(2 Es/N0)); QPSK 2Q(sqrt(Es/N0)); 8PSK 2Q(sqrt(2 Es/N0) sin(pi/8)). This is a COHERENT bound: a differentially-decoded rate is ~2x it, so pairing a differential measurement with this curve invents a factor of two of implementation loss.

    `BPSK: Q(sqrt(2 Es/N0))`, `QPSK: 2 Q(sqrt(Es/N0))`, `8PSK: 2 Q(sqrt(2
    Es/N0) sin(pi/8))` — the nearest-neighbour union bound, tight to well
    under a percent at any Es/N0 worth testing at.

    **This is a COHERENT bound.** A differentially-decoded rate is ~2x it,
    because a differential decision fails when either of its two symbols is
    wrong (measured 1.88-2.11 across M and both receiver paths). Pairing a
    differential measurement with this curve invents a factor of two of
    "implementation loss".

    Parameters
    ----------
    m : int
        Input.
    esn0 : float
        Input.

    Returns
    -------
    float
        Output.
    """

def ber_theory_ber(m: int, esn0: float) -> float:
    """Coherent GRAY-coded M-PSK bit error rate at Es/N0 (LINEAR). BPSK and Gray QPSK are exactly Q(sqrt(2 Eb/N0)); 8PSK uses SER/log2(M).

    Parameters
    ----------
    m : int
        Input.
    esn0 : float
        Input.

    Returns
    -------
    float
        Output.
    """

def ber_esn0_db_for_ser(m: int, ser: float) -> float:
    """Es/N0 (dB) at which the coherent bound equals `ser`. How an implementation loss is quoted honestly: convert the MEASURED rate to the Es/N0 theory would need to produce it, and subtract. A loss in dB is comparable across M and across operating points; a ratio of rates is not.

    How an implementation loss is quoted honestly: convert the MEASURED rate
    to the Es/N0 theory would need to produce it, and subtract. A loss in dB
    is comparable across M and across operating points; a ratio of rates is
    not.

    Parameters
    ----------
    m : int
        Input.
    ser : float
        Input.

    Returns
    -------
    float
        Output.
    """

def ber_evm_scatter_floor_db(m: int) -> float:
    """EVM (dB) of an M-PSK constellation at a UNIFORMLY RANDOM rotation -- the FLOOR of a self-referenced EVM, i.e. what a completely destroyed constellation reads: -1.4 dB at BPSK, -7.0 at QPSK, -12.9 at 8PSK. ANY fixed EVM threshold must be stated against this, never against 0 dB: 'scattered reads ~0 dB' is the BPSK limit only, and at 8PSK a stream with no carrier recovery reads the same -12.9 dB a healthy 13 dB link does, so a `< -12.0` assertion is satisfied by pure noise. The room between 'on the bound at the SER=1e-3 anchor' and 'completely broken' collapses as M grows (5.4 / 3.3 / 2.8 dB), so at high M the EVM cannot carry a verdict alone. Not to be confused with the NOISE floor -(Es/N0).

    The FLOOR of a self-referenced EVM: what a completely destroyed
    constant-modulus constellation reads. Slicing a unit-modulus point at a
    uniformly random phase to its nearest of M neighbours leaves `E|e|^2 = 2
    - 2 sin(pi/M)/(pi/M)`: **-1.4 dB at BPSK, -7.0 at QPSK, -12.9 at 8PSK**.

    **Any fixed EVM threshold must be stated against this, never against 0
    dB.** "Scattered reads ~0 dB" is the BPSK limit only. At 8PSK a stream
    with no carrier recovery at all reads -12.9 dB — which is also what a
    perfectly healthy 13 dB link reads — so a `< -12.0` assertion is
    satisfied by pure noise. That was live in this repo's own receiver tests
    until 2026-07-27. The room between "on the bound at the SER=1e-3 anchor"
    and "completely broken" collapses as M grows: 5.4 dB at BPSK, 3.3 at
    QPSK, 2.8 at 8PSK, so at high M the EVM cannot carry a verdict by
    itself.

    Parameters
    ----------
    m : int
        Input.

    Returns
    -------
    float
        Output.
    """

def ber_settle_syms(bn_timing: float, bn_carrier: float) -> int:
    """Symbols to discard before a steady-state measurement means anything: 2*(5/bn_timing + 5/bn_carrier). Three factors, and skipping any produces a confident wrong number -- 5/Bn per loop is the standard second-order settling time (in SYMBOLS, since both bn are symbol-rate normalised); the two budgets ADD because the loops are cascaded (the carrier discriminator reads the on-time strobe, so it cannot converge until timing has); and the sum DOUBLES for joint tracking. This is a FLOOR, not the answer: take the max of it and every lock indicator the receiver publishes, plus the handover instant again if one is enabled. Pass a loop's bn as 0 if it is not running.

    `2 * (5/bn_timing + 5/bn_carrier)`. Three factors, and skipping any of
    them produces a confident wrong number: 5/Bn per loop is the standard
    second-order settling time (in symbols, because both `bn` are normalised
    to the SYMBOL rate); the two budgets ADD because the loops are CASCADED
    (the carrier discriminator reads the on-time strobe, so it cannot
    converge until timing has); and the sum DOUBLES for joint tracking,
    where each loop sees the other's transient as a disturbance.

    This is a floor, not the answer — take the max of it and every lock
    indicator the receiver publishes, plus the handover instant again if one
    is enabled. Pass a loop's `bn` as 0 if it is not running.

    Parameters
    ----------
    bn_timing : float
        Input.
    bn_carrier : float
        Input.

    Returns
    -------
    int
        Output.
    """

def ber_lock_symbol(flags: NDArray[np.uint8], sustain: int = 200, min_frac: float = 0.9) -> int:
    """First symbol from which a verify-counted lock flag is SUSTAINED, or -1 for 'never locked'. Sustained means `sustain` consecutive symbols high AND at least `min_frac` of everything after that point high too: the run rejects a single lucky decision, the fraction rejects a detector that declares early then flaps. Dating the lock by the FINAL contiguous run instead is right with no noise and badly wrong with it -- one late dip once moved a reported lock from 415 to 2286 and left no measurement window at all. The -1 is deliberate: it forces the caller to say 'never locked' rather than quietly measure a transient.

    "Sustained" is sustain consecutive symbols high AND at least min_frac of
    everything after that point high too. Both halves carry weight: the run
    rejects a single lucky decision, the fraction rejects a detector that
    declares early then flaps. Dating the lock by the FINAL contiguous run
    instead is right with no noise and badly wrong with it — one late dip
    once moved a reported lock from 415 to 2286 and left no measurement
    window.

    Parameters
    ----------
    flags : NDArray[np.uint8]
        Input.
    sustain : int
        Input.
    min_frac : float
        Input.

    Returns
    -------
    int
        The symbol index, or -1 for "never locked" — the honest answer, which forces the caller to say so rather than measure a transient.
    """

def ber_evm_db(rx: NDArray[np.complex64], lo: int = 0, hi: int = 0, m: int = 4) -> float:
    """Self-referenced EVM (dB) over an EXPLICIT window [lo, hi): each symbol against the stream's OWN hard decision, with the constellation rotation estimated from the data. References neither the transmitted symbols nor a lag, so it cannot be fooled by an alignment search. A locked matched-filter output reads EVM_dB ~ -(Es/N0)_dB (an I/Q-plane quantity -- no factor of two; quoting one flatters the result by 3 dB). Read it against ber_evm_scatter_floor_db(m), NEVER against 0 dB. The window is explicit because BER and EVM must be measured on the SAME one: a convenience back-half default scores a different window than the error rate did, and the two eventually disagree in a way that reads as a receiver defect rather than the harness bug it is. Returns 0.0 for a window under 20 symbols.

    Scores each symbol against the stream's OWN hard decision, with the
    constellation rotation estimated from the data — so it references
    neither the transmitted symbols nor a lag, and cannot be fooled by an
    alignment search. At a matched-filter output the error vector IS the
    complex noise, so a locked stream reads `EVM[dB] ~ -(Es/N0)[dB]`. EVM is
    an I/Q-plane quantity: there is no factor of two — that belongs to an
    I-only measurement, and quoting it flatters the result by 3 dB.

    **Pass the real `m`**, and read the result against
    ber_evm_scatter_floor_db(m), never against 0 dB.

    The window is EXPLICIT because BER and EVM must be measured on the SAME
    one. A convenience "back half" default silently scores a different
    window than the error rate did, and the two eventually disagree in a way
    that reads as a receiver defect rather than the harness bug it is.

    Parameters
    ----------
    rx : NDArray[np.complex64]
        Input.
    lo : int
        Input.
    hi : int
        Input.
    m : int
        Input.

    Returns
    -------
    float
        EVM in dB, or 0.0 ("no lock") for a window under 20 symbols.
    """

def ber_settle_from(budget: int, timing_lock: int = -1, carrier_lock: int = -1, handover: int = -1) -> int:
    """Where a steady-state measurement may start: max(budget, timing lock, carrier lock, handover + budget). The analytic budget and the receiver's own indicators are both fallible in the SAME direction, so whichever settles last decides. A handover settles last of all -- it fires on carrier lock plus a warmup, strictly after every other term, and the decision-directed loop then has its own transient, so it contributes its instant PLUS the budget again (measured on 8PSK: handover at symbol 2525 against a 2000-symbol budget, SER 5.95x the coherent bound from 2000 versus 1.68x from 4525). Pass -1 for an indicator the receiver does not publish, which is what ber_lock_symbol() returns for 'never locked'. A -1 timing or carrier lock means there is NO valid steady-state window -- check that yourself before trusting the return; a -1 handover is not a failure, since a pure-NDA receiver never publishes one.

    The POLICY for where a steady-state window may start, in one place:
    `max(budget, timing lock, carrier lock, handover + budget)`. The
    analytic budget and the receiver's own indicators are both fallible in
    the SAME direction, so whichever settles last decides.

    **A handover settles last of all.** With `acq_to_track` on it fires on
    carrier lock plus a warmup — strictly after the budget and after every
    lock indicator — and the decision-directed loop then has its own
    transient, so it contributes `its instant + the budget again`. Measured
    on 8PSK at its SER=1e-3 anchor: handover at symbol 2525 against a
    2000-symbol budget, SER 5.95x the coherent bound measured from 2000 and
    1.68x from 4525.

    Pass -1 for any indicator the receiver does not publish (which is what
    ber_lock_symbol() returns for "never locked"). **A -1 timing or carrier
    lock means there is NO valid steady-state window** — check that yourself
    before trusting the return; a -1 handover is not a failure, because a
    pure-NDA receiver never publishes one.

    Parameters
    ----------
    budget : int
        ber_settle_syms() of the loops in use.
    timing_lock : int
        ber_lock_symbol() of the timing flag, or -1.
    carrier_lock : int
        ber_lock_symbol() of the carrier flag, or -1.
    handover : int
        ber_lock_symbol() of the tracking flag, or -1.

    Returns
    -------
    int
        First symbol of the measurement window.
    """
