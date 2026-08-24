# wfm/wfm.pyi — type stubs for the wfm C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class FrameLayout(tuple[int, int, int, int, int, int, int, int, int]):
    """Where each field lands, in bits from the start of the frame. The offsets
    a receiver needs to slice a capture -- computed once, by the same code the
    generator laid the frame out with.

    Attributes
    ----------
    crc_bits : int
        16, or 0 when crc is unset or the payload is empty — a CRC over nothing protects nothing
    """

    @property
    def preamble_off(self) -> int: ...

    @property
    def preamble_bits(self) -> int: ...

    @property
    def sync_off(self) -> int: ...

    @property
    def sync_bits(self) -> int: ...

    @property
    def payload_off(self) -> int: ...

    @property
    def payload_bits(self) -> int: ...

    @property
    def crc_off(self) -> int: ...

    @property
    def crc_bits(self) -> int:
        """16, or 0 when crc is unset or the payload is empty — a CRC over
        nothing protects nothing
        """

    @property
    def total_bits(self) -> int: ...

@final
class FrameCheck(tuple[int, int, int, int, int, int, int]):
    """What checking one received frame found. `ok == units` is the verdict;
    `symbols` is what it cost, which is margin being spent and is visible
    before it is lost.

    Attributes
    ----------
    passed : int
        Every check good: 1 yes, 0 no. Also 0 when nothing was checked -- see `checked`. Named `passed` rather than `pass` because the obvious name is a Python keyword and `r.pass` will not parse.
    stages : int
        Stages in the description.
    checked : int
        How many were reversed here. 0 means the description carries no reversible stage, which is why `pass` is 0: carrying no check is not the same answer as passing one.
    units : int
        Checks performed: one for a CRC, one per codeword for an interleaved outer code.
    ok : int
        How many came out good -- clean or repaired.
    corrected : int
        How many needed and received repair.
    symbols : int
        Symbol errors repaired across the frame.
    """

    @property
    def passed(self) -> int:
        """Every check good: 1 yes, 0 no. Also 0 when nothing was checked --
        see `checked`. Named `passed` rather than `pass` because the obvious
        name is a Python keyword and `r.pass` will not parse.
        """

    @property
    def stages(self) -> int:
        """Stages in the description."""

    @property
    def checked(self) -> int:
        """How many were reversed here. 0 means the description carries no
        reversible stage, which is why `pass` is 0: carrying no check is not
        the same answer as passing one.
        """

    @property
    def units(self) -> int:
        """Checks performed: one for a CRC, one per codeword for an interleaved
        outer code.
        """

    @property
    def ok(self) -> int:
        """How many came out good -- clean or repaired."""

    @property
    def corrected(self) -> int:
        """How many needed and received repair."""

    @property
    def symbols(self) -> int:
        """Symbol errors repaired across the frame."""

@final
class PN:
    """Allocate and initialise a maximal-length-sequence LFSR. The register is
    seeded from ``seed`` and will produce a pseudo-random binary sequence with
    period 2^length - 1 for any primitive ``poly``. Both Galois and Fibonacci
    realizations share the same primitive polynomial and therefore the same
    period; they differ only in chip ordering/phase.

    Parameters
    ----------
    poly : int, default 0
        Galois feedback tap polynomial (right-shift convention). The LSB is the
        tap at position 0 (always 1 for a primitive poly); bit k=1 means tap at
        position k. Default 96 (0x60) is primitive for length=7, giving period
        127. The Fibonacci taps are derived automatically so you only supply
        one value.
    seed : int, default 0
        Initial LFSR register state; must be non-zero (the all-zero state is a
        fixed point). Default 1.
    length : int, default 0
        Register width in bits, 1..64. The sequence period is 2^length - 1 for
        a primitive polynomial. Default 7.
    lfsr : Literal["galois", "fibonacci"], default "galois"
        Realization: PN_GALOIS (0, default) or PN_FIBONACCI (1).

    Examples
    --------
    >>> from doppler.wfm import PN
    >>> import numpy as np
    >>> p = PN(poly=96, seed=1, length=7)
    >>> chips = p.generate(127)
    >>> chips.dtype
    dtype('uint8')
    >>> int(chips.sum())   # 64 ones per MLS period (2^(n-1))
    64

    """
    def __init__(
        self,
        poly: int = ...,
        seed: int = ...,
        length: int = ...,
        lfsr: Literal["galois", "fibonacci"] = "galois",
    ) -> None: ...

    def reset(self) -> None:
        """Reset PN to its post-create state. Reloads the LFSR register from
        the original seed so the sequence restarts from chip 0. Useful for
        reproducible captures without re-allocating.

        Examples
        --------
        >>> from doppler.wfm import PN
        >>> import numpy as np
        >>> p = PN(poly=96, seed=1, length=7)
        >>> a = p.generate(8).copy()
        >>> p.reset()
        >>> np.array_equal(a, p.generate(8))
        True

        """

    def generate(
        self,
        count: int = 1,
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Generate ``n`` chips into ``out`` and advance the LFSR by ``n``
        positions. Each element of ``out`` is 0 or 1. Requesting more than one
        MLS period is valid — the sequence simply wraps around. The Python
        binding returns a zero-copy NumPy uint8 view over a pre-allocated
        buffer; copy the result before calling generate again if you need a
        snapshot.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : NDArray[np.uint8] | None
            Output buffer of at least ``n`` uint8 elements; each element
            receives 0 or 1.

        Returns
        -------
        NDArray[np.uint8]
            min(n, max_out) chips.

        Examples
        --------
        >>> from doppler.wfm import PN
        >>> import numpy as np
        >>> p = PN(poly=96, seed=1, length=7)
        >>> chips = p.generate(127)
        >>> chips[:8].tolist()
        [1, 0, 0, 0, 0, 0, 1, 1]
        >>> int(chips.sum())   # 64 ones per MLS period
        64

        """

    def generate_max_out(self) -> int:
        """Largest number of samples generate() can return in the current
        state.

        Size an `out=` buffer with this before calling generate(), or use it to
        allocate one up front. The bound is this object's own: what it depends
        on is a property of the algorithm, so a header block on
        generate_max_out() replaces this text.

        Returns
        -------
        int
            Upper bound on the output length; the actual call may return fewer.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the PN has already been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the PN has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the PN has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "PN":
        """Enter a context manager, returning this object.

        Lets a PN be used in a `with` statement so its C resources are released
        deterministically on exit rather than at collection time.

        Returns
        -------
        PN
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the PN.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

@final
class _SynthEngine:
    """Allocate and configure a waveform synthesiser. The synthesiser combines
    a local oscillator (LO), optional AWGN, and an optional PN LFSR into a
    single streaming source. One call to wfm_synth_step() or wfm_synth_steps()
    advances all sub-components in lock-step. SNR >= WFM_SYNTH_SNR_CLEAN (100
    dB) skips AWGN entirely — clean waveforms pay no noise overhead. When
    ``snr_mode`` is "auto" the library picks the natural reference: Es/No for
    modulated types (BPSK, QPSK), fs-band SNR for tone/noise/PN.

    Parameters
    ----------
    type : Literal["tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits", "symbols", "dsss"], default "tone"
        Waveform type: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk, 5=chirp, 6=bits,
        7=symbols, 8=dsss. The Python binding accepts strings
        "tone"|"noise"|"pn"|"bpsk"|"qpsk"|"chirp"|"bits"|"symbols"|"dsss". For
        "bits" attach the pattern with wfm_synth_set_bits(); for "symbols"
        attach the complex stream with wfm_synth_set_symbols(); for "dsss"
        attach the burst with wfm_synth_set_dsss() after create().
    fs : float, default 1000000.0
        Sample rate in Hz. Sets the carrier frequency normalisation and the
        noise bandwidth. Default 1 000 000.0.
    freq : float, default 0.0
        Carrier frequency offset in Hz (−fs/2 … fs/2). A complex LO is created
        only when freq != 0. For a chirp this is the start frequency f_start
        (the instantaneous frequency at t=0). Default 0.0.
    snr : float, default 100.0
        Target SNR in dB, interpreted per ``snr_mode``. Values >=
        WFM_SYNTH_SNR_CLEAN (100) disable AWGN. Default 100.0.
    snr_mode : Literal["auto", "fs", "ebno", "esno"], default "auto"
        SNR reference: 0=auto, 1=fs (full-band), 2=ebno, 3=esno. The Python
        binding accepts strings "auto"|"fs"|"ebno"|"esno". Default 0.
    seed : int, default 1
        PRNG seed shared by AWGN and the PN LFSR. Default 1.
    sps : int, default 8
        Samples per symbol for modulated types (BPSK, QPSK, PN). Ignored for
        tone/noise. Default 8.
    pn_length : int, default 7
        LFSR register length (1..64); period = 2^pn_length - 1. Default 7
        (period 127).
    pn_poly : int, default 0
        Galois tap polynomial for the LFSR. 0 means "look up the canonical MLS
        polynomial for pn_length" from the wfm_synth_mls_poly table. Default 0.
    lfsr : Literal["galois", "fibonacci"], default "galois"
        LFSR realization: PN_GALOIS (0) or PN_FIBONACCI (1).
    f_end : float, default 0.0
        Chirp end frequency in Hz (type=chirp only; ignored otherwise). With
        ``freq`` as the start, the instantaneous frequency sweeps linearly from
        ``freq`` to ``f_end`` over the span (set by wfm_synth_set_chirp_span()
        or the first wfm_synth_steps() call), then holds at ``f_end``. ``f_end
        < freq`` is a down-chirp. Default 0.0.

    Examples
    --------
    >>> from doppler.wfm import _SynthEngine
    >>> import numpy as np
    >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
    >>> x = s.steps(4)
    >>> x.dtype
    dtype('complex64')
    >>> x.tolist()
    [(1+0j), (1+0j), (1+0j), (1+0j)]

    """
    def __init__(
        self,
        type: Literal["tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits", "symbols", "dsss"] = "tone",
        fs: float = ...,
        freq: float = ...,
        snr: float = ...,
        snr_mode: Literal["auto", "fs", "ebno", "esno"] = "auto",
        seed: int = ...,
        sps: int = ...,
        pn_length: int = ...,
        pn_poly: int = ...,
        lfsr: Literal["galois", "fibonacci"] = "galois",
        f_end: float = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Reset Synth to its post-create state. Resets the LO phase
        accumulator, AWGN internal state, and PN LFSR register to their initial
        values so the output sequence is perfectly reproducible from sample 0.

        Examples
        --------
        >>> from doppler.wfm import _SynthEngine
        >>> import numpy as np
        >>> s = _SynthEngine(type="qpsk", sps=4, seed=1, snr=100.0)
        >>> a = s.steps(16).copy()
        >>> s.reset()
        >>> np.array_equal(a, s.steps(16))
        True

        """

    def step(self) -> complex:
        """Generate one output sample from internal state. Advances the PN LFSR
        (modulated types only, on symbol boundaries), the LO phase accumulator,
        and the AWGN engine, then returns the mixed result: ``sym * carrier +
        noise``. Inlined and hot-path annotated so tight per-sample loops pay
        no call overhead.

        Returns
        -------
        complex
            Next output sample (float complex).

        Examples
        --------
        >>> from doppler.wfm import _SynthEngine
        >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
        >>> s.step()
        (1+0j)

        """

    def steps(self, n: int = 1) -> NDArray[np.complex64]:
        """Generate a block of output samples. Calls wfm_synth_step() in a
        tight loop, writing each cf32 sample into ``output``. The Python
        binding returns a freshly allocated NumPy complex64 array; ownership is
        transferred to the caller.

        Parameters
        ----------
        n : int
            Number of samples to generate.

        Returns
        -------
        NDArray[np.complex64]
            Output.

        Examples
        --------
        >>> from doppler.wfm import _SynthEngine
        >>> import numpy as np
        >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
        >>> x = s.steps(4)
        >>> x.shape, x.dtype
        ((4,), dtype('complex64'))
        >>> x.tolist()
        [(1+0j), (1+0j), (1+0j), (1+0j)]

        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the _SynthEngine has already been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the _SynthEngine has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the _SynthEngine has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """
    def get_wtype(self) -> int:
        """Return the active waveform type discriminant. Maps to the
        WFM_SYNTH_* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk. Use this to
        inspect which synthesis path is active at runtime.

        Returns
        -------
        int
            Integer waveform type index (WFM_SYNTH_TONE .. WFM_SYNTH_QPSK).
        """

    def set_wtype(self, value: int) -> None:
        """Override the waveform type discriminant in-place. Changing wtype
        does not reinitialise sub-objects; use with care.

        Parameters
        ----------
        value : int
            Input.
        """

    def get_nsps(self) -> int:
        """Return the samples-per-symbol count. For modulated types (BPSK,
        QPSK, PN) each symbol is held for nsps consecutive output samples. For
        tone/noise this field is present but unused by the synthesis path.

        Returns
        -------
        int
            Samples per symbol (nsps >= 1).
        """

    def set_nsps(self, value: int) -> None:
        """Override the samples-per-symbol count in-place. Does not flush the
        symbol-position counter (sym_pos); set sym_pos=0 as well when changing
        sps mid-stream.

        Parameters
        ----------
        value : int
            Input.
        """

    def get_sym_pos(self) -> int:
        """Return the current position within the current symbol (0..nsps-1).
        Reaches nsps and wraps to 0 each time a new symbol is consumed from the
        PN LFSR. Useful for frame alignment: sym_pos==0 on a step boundary
        means the very next sample begins a fresh symbol.

        Returns
        -------
        int
            Symbol position counter (0 <= sym_pos < nsps).
        """

    def set_sym_pos(self, value: int) -> None:
        """Override the symbol-position counter in-place. Injecting 0 forces
        the next wfm_synth_step() to latch a new PN chip; any other value
        fast-forwards into the middle of the current symbol hold.

        Parameters
        ----------
        value : int
            Input.
        """

    def get_cur_re(self) -> float:
        """Return the real part of the current held symbol. For modulated types
        this is the I component latched at the last symbol boundary (±1 for
        BPSK/PN, ±1/√2 for QPSK). For tone the synthesiser initialises cur_re
        to 1.0 so that the held symbol is a clean unit-power carrier; for noise
        it is 0.0 (noise has no held symbol).

        Returns
        -------
        float
            Current symbol real (I) component.
        """

    def set_cur_re(self, value: float) -> None:
        """Override the held-symbol real (I) component in-place. Takes effect
        on the next wfm_synth_step() within the current symbol hold.

        Parameters
        ----------
        value : float
            Input.
        """

    def get_cur_im(self) -> float:
        """Return the imaginary part of the current held symbol. For QPSK this
        is the Q component (±1/√2); for BPSK/PN it is always 0; for tone/noise
        it is 0.

        Returns
        -------
        float
            Current symbol imaginary (Q) component.
        """

    def set_cur_im(self, value: float) -> None:
        """Override the held-symbol imaginary (Q) component in-place. Takes
        effect on the next wfm_synth_step() within the current symbol hold.

        Parameters
        ----------
        value : float
            Input.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "_SynthEngine":
        """Enter a context manager, returning this object.

        Lets a _SynthEngine be used in a `with` statement so its C resources
        are released deterministically on exit rather than at collection time.

        Returns
        -------
        _SynthEngine
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the _SynthEngine.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

@final
class Gold:
    """Allocate and initialise a CCSDS-style Gold code generator. Two
    independent Fibonacci LFSRs of the same ``length`` free-run in lock-step;
    each output chip is the XOR of both registers' current top-bit (stage
    ``length``, i.e. bit ``length-1``). Both registers shift left one bit per
    chip: the new bit (parity of the tapped stages, read *before* the shift)
    enters at stage 1 (bit 0), and the old stage-``length`` bit is discarded
    after being XORed into the output. The sequence period is ``2^length - 1``
    for primitive ``taps_a``/``taps_b``. With the CCSDS default polynomials the
    two m-sequences form a genuine "preferred pair" — their XOR family has a
    strict three-valued periodic autocorrelation/cross-correlation set ``{-1,
    -65, 63}`` — so varying ``seed_a`` (User dependent per the standard) walks
    the whole 2**length -member Gold-code family while Register B stays fixed.

    Parameters
    ----------
    taps_a : int, default 934
        Register A feedback-tap mask; bit k set means stage k+1 is XORed into
        the feedback. Default 934 (stages 2,3,6,8,9,10 — the CCSDS-fixed
        Register A polynomial x^10+x^9+x^8+x^6+x^3+x^2+1).
    seed_a : int, default 350
        Register A initial value; must be non-zero. Per CCSDS this is "User
        dependent" — any nonzero value selects a different member of the
        1024-code Gold family. Default 350 is the worked example from CCSDS
        415.0-G-1 Figure 5-2 (PN Code Library Table 1, Code Number 365).
    taps_b : int, default 567
        Register B feedback-tap mask, same bit convention as ``taps_a``.
        Default 567 (stages 1,2,3,5,6,10 — the CCSDS-fixed Register B
        polynomial).
    seed_b : int, default 73
        Register B initial value; must be non-zero. Default 73 (stages 1,4,7 —
        CCSDS's fixed Register B initial value 1001001000, unique per the
        standard, not user-selectable).
    length : int, default 10
        Register width in bits, 1..64. CCSDS command link uses 10 (period
        1023). Default 10.

    Examples
    --------
    >>> from doppler.wfm import Gold
    >>> import numpy as np
    >>> g = Gold()
    >>> chips = g.generate(1023)
    >>> chips.dtype
    dtype('uint8')
    >>> chips[:15].tolist()   # CCSDS Code #365 worked example
    [0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1]
    >>> int(chips.sum()), int((1 - chips).sum())   # 512 ones, 511 zeros
    (512, 511)

    """
    def __init__(
        self,
        taps_a: int = ...,
        seed_a: int = ...,
        taps_b: int = ...,
        seed_b: int = ...,
        length: int = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Reset Gold to its post-create state. Reloads both LFSR registers
        from their original seeds so the sequence restarts from chip 0. Useful
        for reproducible captures without re-allocating.

        Examples
        --------
        >>> from doppler.wfm import Gold
        >>> import numpy as np
        >>> g = Gold()
        >>> a = g.generate(8).copy()
        >>> g.reset()
        >>> np.array_equal(a, g.generate(8))
        True

        """

    def generate(
        self,
        count: int = 1,
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Generate ``n`` chips into ``out`` and advance both LFSRs by ``n``
        positions. Each element of ``out`` is 0 or 1. Requesting more than one
        period is valid — the sequence simply wraps around. The Python binding
        returns a zero-copy NumPy uint8 view over a pre-allocated buffer; copy
        the result before calling generate again if you need a snapshot.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : NDArray[np.uint8] | None
            Output buffer of at least ``n`` uint8 elements; each element
            receives 0 or 1.

        Returns
        -------
        NDArray[np.uint8]
            min(n, max_out) chips.

        Examples
        --------
        >>> from doppler.wfm import Gold
        >>> import numpy as np
        >>> g = Gold()
        >>> chips = g.generate(1023)
        >>> len(chips)
        1023

        """

    def generate_max_out(self) -> int:
        """Largest number of samples generate() can return in the current
        state.

        Size an `out=` buffer with this before calling generate(), or use it to
        allocate one up front. The bound is this object's own: what it depends
        on is a property of the algorithm, so a header block on
        generate_max_out() replaces this text.

        Returns
        -------
        int
            Upper bound on the output length; the actual call may return fewer.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Gold has already been destroyed.

        Returns
        -------
        int
            Byte length of one serialized state blob.
        """

    def get_state(self) -> bytes:
        """Serialize this object's mutable state to bytes.

        Captures exactly the state that evolves as the object runs, so a blob
        taken now and restored later resumes from this point. Construction
        parameters are not included: restore into an object built the same way.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the Gold has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place; the object keeps the parameters it
        was constructed with. Length is validated against `state_bytes()`
        before the blob is handed to the C core, and the core may reject it as
        well.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the Gold has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "Gold":
        """Enter a context manager, returning this object.

        Lets a Gold be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Gold
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Gold.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

@final
class Frame:
    """Create a frame instance.

    Parameters
    ----------
    preamble_kind : Literal["literal", "pn", "gold", "dotted"], default "literal"
        Enum index; 0=literal…3=dotted.
    preamble : NDArray[np.uint8]
        Literal preamble bits, one per element. Pass an EMPTY array when the
        field is absent or generated -- `wfm_seq_t` already spells absence as a
        zero length, so this is that convention reaching Python rather than a
        placeholder. (An omittable array init-param is a jm gap; see the module
        docs.)
    preamble_nbits : int, default 0
        Output bits for a GENERATED preamble kind. A literal takes its length
        from the `preamble` array instead; `wfm_seq_t` names these apart (len
        vs reg_bits) for the same reason.
    preamble_reps : int, default 0
        Repetitions of the preamble; 0 = no preamble (default: 0).
    preamble_poly : int, default 0
        PN feedback polynomial; 0 selects the maximal-length one (default: 0).
    preamble_seed : int, default 0
        PN seed; 0 selects 1, since an all-zero register is a fixed point
        (default: 0).
    preamble_reg_bits : int, default 0
        PN/Gold register width, 1..64 (default: 0).
    preamble_lfsr : Literal["galois", "fibonacci"], default "galois"
        Enum index; 0=galois…1=fibonacci.
    preamble_taps_a : int, default 0
        Gold: first register's taps (default: 0).
    preamble_seed_a : int, default 0
        Gold: first register's seed (default: 0).
    preamble_taps_b : int, default 0
        Gold: second register's taps (default: 0).
    preamble_seed_b : int, default 0
        Gold: second register's seed (default: 0).
    sync_kind : Literal["literal", "pn", "gold", "dotted"], default "literal"
        Enum index; 0=literal…3=dotted.
    sync : NDArray[np.uint8]
        Literal sync word bits, one per element. Pass an EMPTY array when the
        field is absent or generated -- `wfm_seq_t` already spells absence as a
        zero length, so this is that convention reaching Python rather than a
        placeholder. (An omittable array init-param is a jm gap; see the module
        docs.)
    sync_nbits : int, default 0
        Output bits for a GENERATED sync kind (default: 0).
    sync_poly : int, default 0
        PN feedback polynomial; 0 selects the maximal-length one (default: 0).
    sync_seed : int, default 0
        PN seed; 0 selects 1 (default: 0).
    sync_reg_bits : int, default 0
        PN/Gold register width, 1..64 (default: 0).
    sync_lfsr : Literal["galois", "fibonacci"], default "galois"
        Enum index; 0=galois…1=fibonacci.
    sync_taps_a : int, default 0
        Gold: first register's taps (default: 0).
    sync_seed_a : int, default 0
        Gold: first register's seed (default: 0).
    sync_taps_b : int, default 0
        Gold: second register's taps (default: 0).
    sync_seed_b : int, default 0
        Gold: second register's seed (default: 0).
    payload_kind : Literal["literal", "pn", "gold", "dotted"], default "literal"
        Enum index; 0=literal…3=dotted.
    payload : NDArray[np.uint8]
        Literal payload bits, one per element. Pass an EMPTY array when the
        field is absent or generated -- `wfm_seq_t` already spells absence as a
        zero length, so this is that convention reaching Python rather than a
        placeholder. (An omittable array init-param is a jm gap; see the module
        docs.)
    payload_nbits : int, default 0
        Output bits for a GENERATED payload kind (default: 0).
    payload_poly : int, default 0
        PN feedback polynomial; 0 selects the maximal-length one (default: 0).
    payload_seed : int, default 0
        PN seed; 0 selects 1 (default: 0).
    payload_reg_bits : int, default 0
        PN/Gold register width, 1..64 (default: 0).
    payload_lfsr : Literal["galois", "fibonacci"], default "galois"
        Enum index; 0=galois…1=fibonacci.
    payload_taps_a : int, default 0
        Gold: first register's taps (default: 0).
    payload_seed_a : int, default 0
        Gold: first register's seed (default: 0).
    payload_taps_b : int, default 0
        Gold: second register's taps (default: 0).
    payload_seed_b : int, default 0
        Gold: second register's seed (default: 0).
    crc : Literal["none", "crc16"], default "none"
        Enum index; 0=none…1=crc16.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``frame geometry is
        empty or a field is unbuildable (a literal with no array, or a
        generated field with no register width)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.wfm import Frame
    >>> empty = np.empty(0, np.uint8)                    # an absent field
    >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)   # Barker-13
    >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
    >>> f = Frame(empty, sync, payload, crc="crc16")
    >>> f.nbits                                          # 13 + 16 + 16
    45
    >>> f.layout().payload_off
    13
    >>> f.crc_ok(f.bits())        # its own bits are its own truth
    1

    A payload a receiver can REGENERATE, rather than one it must be handed:

    >>> g = Frame(empty, sync, empty, payload_kind="pn",
    ...           payload_nbits=1024, payload_reg_bits=10, crc="crc16")
    >>> g.nbits
    1053

    """
    def __init__(
        self,
        preamble: NDArray[np.uint8],
        sync: NDArray[np.uint8],
        payload: NDArray[np.uint8],
        preamble_kind: Literal["literal", "pn", "gold", "dotted"] = "literal",
        preamble_nbits: int = ...,
        preamble_reps: int = ...,
        preamble_poly: int = ...,
        preamble_seed: int = ...,
        preamble_reg_bits: int = ...,
        preamble_lfsr: Literal["galois", "fibonacci"] = "galois",
        preamble_taps_a: int = ...,
        preamble_seed_a: int = ...,
        preamble_taps_b: int = ...,
        preamble_seed_b: int = ...,
        sync_kind: Literal["literal", "pn", "gold", "dotted"] = "literal",
        sync_nbits: int = ...,
        sync_poly: int = ...,
        sync_seed: int = ...,
        sync_reg_bits: int = ...,
        sync_lfsr: Literal["galois", "fibonacci"] = "galois",
        sync_taps_a: int = ...,
        sync_seed_a: int = ...,
        sync_taps_b: int = ...,
        sync_seed_b: int = ...,
        payload_kind: Literal["literal", "pn", "gold", "dotted"] = "literal",
        payload_nbits: int = ...,
        payload_poly: int = ...,
        payload_seed: int = ...,
        payload_reg_bits: int = ...,
        payload_lfsr: Literal["galois", "fibonacci"] = "galois",
        payload_taps_a: int = ...,
        payload_seed_a: int = ...,
        payload_taps_b: int = ...,
        payload_seed_b: int = ...,
        crc: Literal["none", "crc16"] = "none",
    ) -> None: ...

    def bits(
        self,
        count: int = 1,
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Materialise n consecutive frames, one bit per byte.

        n counts FRAMES, not bits: a descriptor describes one frame, and a
        capture holds many. Repeating here rather than making the caller tile
        it is what matches the generator, whose framed source cycles the same
        frame to fill whatever length was asked for — so a stream compared
        against this lines up with the one that was transmitted.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : NDArray[np.uint8] | None
            Output, one bit per byte.

        Returns
        -------
        NDArray[np.uint8]
            Bits written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> len(d.bits())        # one frame: 13 + 16 + 16
        45
        >>> len(d.bits(2))       # n counts FRAMES, tiled the way a capture is
        90

        """

    def bits_max_out(self, n: int) -> int:
        """Bits frame_bits will write for n frames — `n * nbits`.

        Parameters
        ----------
        n : int
            Frame repetitions.

        Returns
        -------
        int
            Output.
        """

    def layout(self) -> FrameLayout:
        """Where each field lands, in bits from the start of the frame.

        The offsets a receiver needs to slice a capture, computed by the same
        code the generator laid the frame out with.

        Returns
        -------
        FrameLayout
            Where each named field lands.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import Frame
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> lay = Frame(empty, sync, payload, crc="crc16").layout()
        >>> lay.sync_off, lay.payload_off, lay.crc_off
        (0, 13, 29)
        >>> lay.total_bits
        45

        This is the NAMED view, so it reports the four fields a `Frame` is built
        from. A description assembled with `add_field` reports zeros here and is
        read with `field_off()` / `field_bits()` instead.

        """

    def crc_ok(self, rx_bits: NDArray[np.uint8]) -> int:
        """Check one received frame's CRC.

        **This is what makes a truth-free frame error rate possible.** It needs
        no payload truth at all, so it works on a real capture, and unlike a
        self-referenced EVM or a blind M2M4 it still catches a false lock — a
        rotated constellation fails the check rather than looking clean.

        Parameters
        ----------
        rx_bits : NDArray[np.uint8]
            Received bits, one per byte.

        Returns
        -------
        int
            1 pass, 0 fail, -1 if the frame carries no CRC or rx_bits is
            shorter than one frame.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.crc_ok(d.bits())           # its own bits are its own truth
        1
        >>> rx = np.asarray(d.bits()).copy()
        >>> rx[d.field_off(2)] ^= 1      # flip one payload bit
        >>> d.crc_ok(rx)
        0

        """

    def add_field(
        self,
        lit: NDArray[np.uint8],
        kind: int = 0,
        gen_len: int = 0,
        reps: int = 0,
        poly: int = 0,
        seed: int = 0,
        reg_bits: int = 0,
        lfsr: int = 0,
        taps_a: int = 0,
        seed_a: int = 0,
        taps_b: int = 0,
        seed_b: int = 0,
        derived_by: int = 0,
        derived_bits: int = 0,
    ) -> int:
        """Append one field to a description (see `FrameDesc`). `kind` is a
        `wfm_seq_kind_t` index -- 0 literal, 1 pn, 2 gold, 3 dotted -- and
        `lfsr` a `wfm_lfsr` one (0 galois, 1 fibonacci); they are ints rather
        than the strings the constructor takes because a method parameter
        cannot yet be a string enum (jm gh-1021), and the C enum is the SSOT
        either way. Either the caller supplies the bits (`lit`, or a generated
        `kind`) or a stage derives them (`derived_by` non-zero) -- both are
        fields, because both are on the wire. Returns the new field's index,
        which is what `derived_by` and a stage's `first_field` are counted in.
        Refuses once the frame is built.

        Either the caller supplies the bits (lit, or a generated kind) or a
        stage derives them (derived_by non-zero). Both are fields, because both
        are on the wire.

        Parameters
        ----------
        lit : NDArray[np.uint8]
            Literal bits, copied here so the description outlives the call; may
            be NULL.
        kind : int
            wfm_seq_kind_t index; 0=literal…3=dotted.
        gen_len : int
            Output bits for a GENERATED kind.
        reps : int
            Repetitions of the field, verbatim; 0 means one.
        poly : int
            PN feedback polynomial; 0 selects the maximal-length.
        seed : int
            PN seed; 0 selects 1.
        reg_bits : int
            PN/Gold register width.
        lfsr : int
            0=galois, 1=fibonacci.
        taps_a : int
            Gold: first register's taps.
        seed_a : int
            Gold: first register's seed.
        taps_b : int
            Gold: second register's taps.
        seed_b : int
            Gold: second register's seed.
        derived_by : int
            0 when the caller supplies this field; otherwise the index of the
            producing stage, PLUS ONE.
        derived_bits : int
            Length of a derived field, in bits.

        Returns
        -------
        int
            The new field's index, or -1 if the description is full, already
            built, or the literal could not be copied.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc, ccsds_asm_bits
        >>> empty = np.empty(0, np.uint8)
        >>> asm = ccsds_asm_bits()
        >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
        ...                   np.uint8)
        >>> data = np.unpackbits(octets).astype(np.uint8)
        >>> d = FrameDesc(empty, empty, empty)   # begin from nothing
        >>> d.add_field(asm)                     # the attached sync marker
        0
        >>> d.add_field(data)                    # the transfer frame
        1

        A field the CALLER does not supply is still a field, because it is still
        on the wire -- `derived_by` names the stage that fills it, PLUS ONE:

        >>> d.add_field(empty, derived_by=1, derived_bits=32 * 8)
        2

        """

    def add_stage(
        self,
        kind: int = 0,
        first_field: int = 0,
        n_fields: int = 0,
        depth: int = 0,
        emit_num: int = 0,
        emit_den: int = 0,
    ) -> int:
        """Append one transform and -- the load-bearing part -- the span of
        fields it covers. `kind` is a `wfm_stage_kind_t` index: 0 crc16, 1 rs,
        2 randomise, 3 conv (jm gh-1021 -- a method parameter cannot yet be a
        string enum). `n_fields = 0` means the stage does not run. A stage that
        inherited whatever ran before it is the representation that cannot
        express a CCSDS CADU, where the marker is covered by the inner code and
        by neither the outer code nor the randomiser.

        n_fields is the load-bearing part and 0 means the stage does not run. A
        stage that inherited "everything before me" instead of declaring its
        cover is the representation that cannot express a CCSDS CADU — see
        `wfm/wfm_frame.h`.

        Parameters
        ----------
        kind : int
            wfm_stage_kind_t index; 0=crc16…3=conv.
        first_field : int
            First field covered.
        n_fields : int
            Fields covered; 0 = the stage does not run.
        depth : int
            Interleaving depth, for an outer code.
        emit_num : int
            Expansion numerator for a stage that emits a NEW stream; 0 when the
            stage stays inside the frame.
        emit_den : int
            Expansion denominator.

        Returns
        -------
        int
            The new stage's index, or -1 if the description is full or already
            built.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc, ccsds_asm_bits
        >>> empty = np.empty(0, np.uint8)
        >>> asm = ccsds_asm_bits()
        >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
        ...                   np.uint8)
        >>> data = np.unpackbits(octets).astype(np.uint8)
        >>> d = FrameDesc(empty, empty, empty)
        >>> _ = d.add_field(asm), d.add_field(data)
        >>> _ = d.add_field(empty, derived_by=1, derived_bits=32 * 8)
        >>> d.add_stage(1, first_field=1, n_fields=2, depth=1)   # RS(255,223)
        0
        >>> d.add_stage(2, first_field=1, n_fields=2)            # randomiser
        1

        Both start at field 1, so both skip the marker -- the cover is DECLARED,
        which is the whole reason a CADU is describable here:

        >>> d.build()
        >>> d.stage_first(0), d.stage_bits(0)
        (32, 2040)

        """

    def build(self) -> None:
        """Lay out and materialise a description. Where a description is
        checked: one that cannot produce its own bits is not a frame. Separate
        from the constructor only because the description arrives over several
        calls and there is no earlier moment at which it is complete. Raises if
        it is empty, unbuildable, names a stage no kernel here covers, or was
        already built.

        The point at which a description is checked, which for frame_create
        happens inside the constructor: a description that cannot produce its
        own bits is not a frame. It is separate here only because the
        description arrives over several calls and there is no earlier moment
        at which it is complete.

        The CRC, the outer code, the randomiser and the inner code are all
        runnable: `ccsds_tm` has no Python binding and is not getting one, so
        this object is where a caller meets them. A stage naming a kernel
        nothing here carries is refused rather than skipped, because a stage
        that quietly did not run produces a frame that still assembles and
        syncs to nothing.

        The inner encoder starts from the all-zero register on every build: a
        description describes ONE frame. A stream of CADUs sharing one register
        is a transmitter's job and lives in `ccsds_tm_frame_encode`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``build failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.nbits                     # 13 + 16 + 16, laid out by build()
        45

        A description that cannot produce bits is not a frame, and is refused
        rather than half-built:

        >>> FrameDesc(empty, empty, empty).build()
        Traceback (most recent call last):
            ...
        ValueError: build failed (rc=-1)

        """

    def check(self, rx_bits: NDArray[np.uint8]) -> FrameCheck:
        """Undo the description's stages over a received frame and report what
        was found -- the receive mirror of `bits()`, reading the same
        description, so a transmitter and a receiver holding the same `Frame`
        cannot disagree about which stage covered what. This is the truth-free
        frame error rate on a CODED link: it needs no payload truth, so it
        works on a real capture, and an outer code is a strictly better
        detector than a CRC because it reports how much repair it took rather
        than one bit of right-or-wrong. `checked` is smaller than `stages` when
        the description names a stage the receiver does not reverse here -- the
        inner code is the case, being undone before frame synchronisation --
        and such a stage is reported as not checked, never as passed.

        The receive mirror of frame_bits, reading the same description — so a
        transmitter and a receiver holding the same `Frame` cannot disagree
        about which stage covered what.

        **This is the truth-free frame error rate on a coded link.** It needs
        the description and the received bits and no payload truth at all, so
        it works on a real capture, and unlike a self-referenced EVM it still
        catches a false lock.

        checked is smaller than stages when the description names a stage the
        receiver does not reverse here — the inner code is the case, since it
        is undone before frame synchronisation and a frame checker never sees
        channel symbols. Such a stage is reported as not checked, never as
        passed.

        Parameters
        ----------
        rx_bits : NDArray[np.uint8]
            Received bits, one per byte. Copied, not modified.

        Returns
        -------
        FrameCheck
            The outcome. passed is 0 and checked is 0 when the description
            carries no reversible stage at all — "carries no check" is not "the
            check passed", and an FER conflating them would score every
            unprotected frame as perfect.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> r = d.check(d.bits(1))
        >>> r.passed, r.ok, r.units
        (1, 1, 1)

        Flip a bit the CRC covers and the verdict turns over:

        >>> rx = np.asarray(d.bits(1)).copy()
        >>> rx[d.field_off(2)] ^= 1
        >>> d.check(rx).passed
        0

        Carrying no check is NOT passing one -- both are reported, separately:

        >>> n = FrameDesc(empty, sync, payload, crc="none")
        >>> n.build()
        >>> c = n.check(n.bits(1))
        >>> c.passed, c.checked
        (0, 0)

        """

    def n_fields(self) -> int:
        """Fields in the description. A `Frame` built the four-field way
        reports 4 -- `wfm_frame_t` IS a configuration of the general
        description, so the indexed view below reads it too.

        Returns
        -------
        int
            How many fields the description carries.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.n_fields()          # the four named fields, absent ones included
        4

        """

    def n_stages(self) -> int:
        """Stages in the description.

        Returns
        -------
        int
            How many stages the description carries.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.n_stages()         # the CRC is a stage like any other
        1

        """

    def field_off(self, i: int) -> int:
        """Bit offset of field `i`, or 0 if there is no such field.

        Parameters
        ----------
        i : int
            Field index.

        Returns
        -------
        int
            Bits from the start of the frame.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.field_off(1), d.field_off(2), d.field_off(3)
        (0, 13, 29)

        Field 0 is the absent preamble: an empty field still HAS an index, so the
        indices a caller passed to `add_field` keep meaning what they meant.

        >>> d.field_off(0), d.field_bits(0)
        (0, 0)

        """

    def field_bits(self, i: int) -> int:
        """Bits in field `i`, or 0 if there is no such field.

        Parameters
        ----------
        i : int
            Field index.

        Returns
        -------
        int
            The field's length in bits.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.field_bits(1), d.field_bits(2), d.field_bits(3)
        (13, 16, 16)

        """

    def stage_first(self, i: int) -> int:
        """First frame bit stage `i` covers; 0 for a stage that did not run.

        Parameters
        ----------
        i : int
            Stage index.

        Returns
        -------
        int
            Bits from the start of the frame.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.stage_first(0)     # the CRC starts at the payload, not at bit 0
        13

        """

    def stage_bits(self, i: int) -> int:
        """Bits stage `i` covers; 0 for a stage that did not run -- which is
        how an optional stage is spelled, and why `first` is 0 there too.

        Parameters
        ----------
        i : int
            Stage index.

        Returns
        -------
        int
            The covered span, in bits.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.stage_bits(0)      # payload+CRC: what crc16 covered
        32

        """

    @property
    def nbits(self) -> int:
        """Nbits."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "Frame":
        """Enter a context manager, returning this object.

        Lets a Frame be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Frame
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Frame.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

@final
class FrameDesc:
    """Create a frame instance.

    Parameters
    ----------
    preamble_kind : Literal["literal", "pn", "gold", "dotted"], default "literal"
        Enum index; 0=literal…3=dotted.
    preamble : NDArray[np.uint8]
        Literal preamble bits, one per element. Pass an EMPTY array when the
        field is absent or generated -- `wfm_seq_t` already spells absence as a
        zero length, so this is that convention reaching Python rather than a
        placeholder. (An omittable array init-param is a jm gap; see the module
        docs.)
    preamble_nbits : int, default 0
        Output bits for a GENERATED preamble kind. A literal takes its length
        from the `preamble` array instead; `wfm_seq_t` names these apart (len
        vs reg_bits) for the same reason.
    preamble_reps : int, default 0
        Repetitions of the preamble; 0 = no preamble (default: 0).
    preamble_poly : int, default 0
        PN feedback polynomial; 0 selects the maximal-length one (default: 0).
    preamble_seed : int, default 0
        PN seed; 0 selects 1, since an all-zero register is a fixed point
        (default: 0).
    preamble_reg_bits : int, default 0
        PN/Gold register width, 1..64 (default: 0).
    preamble_lfsr : Literal["galois", "fibonacci"], default "galois"
        Enum index; 0=galois…1=fibonacci.
    preamble_taps_a : int, default 0
        Gold: first register's taps (default: 0).
    preamble_seed_a : int, default 0
        Gold: first register's seed (default: 0).
    preamble_taps_b : int, default 0
        Gold: second register's taps (default: 0).
    preamble_seed_b : int, default 0
        Gold: second register's seed (default: 0).
    sync_kind : Literal["literal", "pn", "gold", "dotted"], default "literal"
        Enum index; 0=literal…3=dotted.
    sync : NDArray[np.uint8]
        Literal sync word bits, one per element. Pass an EMPTY array when the
        field is absent or generated -- `wfm_seq_t` already spells absence as a
        zero length, so this is that convention reaching Python rather than a
        placeholder. (An omittable array init-param is a jm gap; see the module
        docs.)
    sync_nbits : int, default 0
        Output bits for a GENERATED sync kind (default: 0).
    sync_poly : int, default 0
        PN feedback polynomial; 0 selects the maximal-length one (default: 0).
    sync_seed : int, default 0
        PN seed; 0 selects 1 (default: 0).
    sync_reg_bits : int, default 0
        PN/Gold register width, 1..64 (default: 0).
    sync_lfsr : Literal["galois", "fibonacci"], default "galois"
        Enum index; 0=galois…1=fibonacci.
    sync_taps_a : int, default 0
        Gold: first register's taps (default: 0).
    sync_seed_a : int, default 0
        Gold: first register's seed (default: 0).
    sync_taps_b : int, default 0
        Gold: second register's taps (default: 0).
    sync_seed_b : int, default 0
        Gold: second register's seed (default: 0).
    payload_kind : Literal["literal", "pn", "gold", "dotted"], default "literal"
        Enum index; 0=literal…3=dotted.
    payload : NDArray[np.uint8]
        Literal payload bits, one per element. Pass an EMPTY array when the
        field is absent or generated -- `wfm_seq_t` already spells absence as a
        zero length, so this is that convention reaching Python rather than a
        placeholder. (An omittable array init-param is a jm gap; see the module
        docs.)
    payload_nbits : int, default 0
        Output bits for a GENERATED payload kind (default: 0).
    payload_poly : int, default 0
        PN feedback polynomial; 0 selects the maximal-length one (default: 0).
    payload_seed : int, default 0
        PN seed; 0 selects 1 (default: 0).
    payload_reg_bits : int, default 0
        PN/Gold register width, 1..64 (default: 0).
    payload_lfsr : Literal["galois", "fibonacci"], default "galois"
        Enum index; 0=galois…1=fibonacci.
    payload_taps_a : int, default 0
        Gold: first register's taps (default: 0).
    payload_seed_a : int, default 0
        Gold: first register's seed (default: 0).
    payload_taps_b : int, default 0
        Gold: second register's taps (default: 0).
    payload_seed_b : int, default 0
        Gold: second register's seed (default: 0).
    crc : Literal["none", "crc16"], default "none"
        Enum index; 0=none…1=crc16.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``frame geometry is
        empty or a field is unbuildable (a literal with no array, or a
        generated field with no register width)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.wfm import Frame
    >>> empty = np.empty(0, np.uint8)                    # an absent field
    >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)   # Barker-13
    >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
    >>> f = Frame(empty, sync, payload, crc="crc16")
    >>> f.nbits                                          # 13 + 16 + 16
    45
    >>> f.layout().payload_off
    13
    >>> f.crc_ok(f.bits())        # its own bits are its own truth
    1

    A payload a receiver can REGENERATE, rather than one it must be handed:

    >>> g = Frame(empty, sync, empty, payload_kind="pn",
    ...           payload_nbits=1024, payload_reg_bits=10, crc="crc16")
    >>> g.nbits
    1053

    """
    def __init__(
        self,
        preamble: NDArray[np.uint8],
        sync: NDArray[np.uint8],
        payload: NDArray[np.uint8],
        preamble_kind: Literal["literal", "pn", "gold", "dotted"] = "literal",
        preamble_nbits: int = ...,
        preamble_reps: int = ...,
        preamble_poly: int = ...,
        preamble_seed: int = ...,
        preamble_reg_bits: int = ...,
        preamble_lfsr: Literal["galois", "fibonacci"] = "galois",
        preamble_taps_a: int = ...,
        preamble_seed_a: int = ...,
        preamble_taps_b: int = ...,
        preamble_seed_b: int = ...,
        sync_kind: Literal["literal", "pn", "gold", "dotted"] = "literal",
        sync_nbits: int = ...,
        sync_poly: int = ...,
        sync_seed: int = ...,
        sync_reg_bits: int = ...,
        sync_lfsr: Literal["galois", "fibonacci"] = "galois",
        sync_taps_a: int = ...,
        sync_seed_a: int = ...,
        sync_taps_b: int = ...,
        sync_seed_b: int = ...,
        payload_kind: Literal["literal", "pn", "gold", "dotted"] = "literal",
        payload_nbits: int = ...,
        payload_poly: int = ...,
        payload_seed: int = ...,
        payload_reg_bits: int = ...,
        payload_lfsr: Literal["galois", "fibonacci"] = "galois",
        payload_taps_a: int = ...,
        payload_seed_a: int = ...,
        payload_taps_b: int = ...,
        payload_seed_b: int = ...,
        crc: Literal["none", "crc16"] = "none",
    ) -> None: ...

    def bits(
        self,
        count: int = 1,
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Materialise n consecutive frames, one bit per byte.

        n counts FRAMES, not bits: a descriptor describes one frame, and a
        capture holds many. Repeating here rather than making the caller tile
        it is what matches the generator, whose framed source cycles the same
        frame to fill whatever length was asked for — so a stream compared
        against this lines up with the one that was transmitted.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : NDArray[np.uint8] | None
            Output, one bit per byte.

        Returns
        -------
        NDArray[np.uint8]
            Bits written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> len(d.bits())        # one frame: 13 + 16 + 16
        45
        >>> len(d.bits(2))       # n counts FRAMES, tiled the way a capture is
        90

        """

    def bits_max_out(self, n: int) -> int:
        """Bits frame_bits will write for n frames — `n * nbits`.

        Parameters
        ----------
        n : int
            Frame repetitions.

        Returns
        -------
        int
            Output.
        """

    def layout(self) -> FrameLayout:
        """Where each field lands, in bits from the start of the frame.

        The offsets a receiver needs to slice a capture, computed by the same
        code the generator laid the frame out with.

        Returns
        -------
        FrameLayout
            Where each named field lands.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import Frame
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> lay = Frame(empty, sync, payload, crc="crc16").layout()
        >>> lay.sync_off, lay.payload_off, lay.crc_off
        (0, 13, 29)
        >>> lay.total_bits
        45

        This is the NAMED view, so it reports the four fields a `Frame` is built
        from. A description assembled with `add_field` reports zeros here and is
        read with `field_off()` / `field_bits()` instead.

        """

    def crc_ok(self, rx_bits: NDArray[np.uint8]) -> int:
        """Check one received frame's CRC.

        **This is what makes a truth-free frame error rate possible.** It needs
        no payload truth at all, so it works on a real capture, and unlike a
        self-referenced EVM or a blind M2M4 it still catches a false lock — a
        rotated constellation fails the check rather than looking clean.

        Parameters
        ----------
        rx_bits : NDArray[np.uint8]
            Received bits, one per byte.

        Returns
        -------
        int
            1 pass, 0 fail, -1 if the frame carries no CRC or rx_bits is
            shorter than one frame.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.crc_ok(d.bits())           # its own bits are its own truth
        1
        >>> rx = np.asarray(d.bits()).copy()
        >>> rx[d.field_off(2)] ^= 1      # flip one payload bit
        >>> d.crc_ok(rx)
        0

        """

    def add_field(
        self,
        lit: NDArray[np.uint8],
        kind: int = 0,
        gen_len: int = 0,
        reps: int = 0,
        poly: int = 0,
        seed: int = 0,
        reg_bits: int = 0,
        lfsr: int = 0,
        taps_a: int = 0,
        seed_a: int = 0,
        taps_b: int = 0,
        seed_b: int = 0,
        derived_by: int = 0,
        derived_bits: int = 0,
    ) -> int:
        """Append one field to a description (see `FrameDesc`). `kind` is a
        `wfm_seq_kind_t` index -- 0 literal, 1 pn, 2 gold, 3 dotted -- and
        `lfsr` a `wfm_lfsr` one (0 galois, 1 fibonacci); they are ints rather
        than the strings the constructor takes because a method parameter
        cannot yet be a string enum (jm gh-1021), and the C enum is the SSOT
        either way. Either the caller supplies the bits (`lit`, or a generated
        `kind`) or a stage derives them (`derived_by` non-zero) -- both are
        fields, because both are on the wire. Returns the new field's index,
        which is what `derived_by` and a stage's `first_field` are counted in.
        Refuses once the frame is built.

        Either the caller supplies the bits (lit, or a generated kind) or a
        stage derives them (derived_by non-zero). Both are fields, because both
        are on the wire.

        Parameters
        ----------
        lit : NDArray[np.uint8]
            Literal bits, copied here so the description outlives the call; may
            be NULL.
        kind : int
            wfm_seq_kind_t index; 0=literal…3=dotted.
        gen_len : int
            Output bits for a GENERATED kind.
        reps : int
            Repetitions of the field, verbatim; 0 means one.
        poly : int
            PN feedback polynomial; 0 selects the maximal-length.
        seed : int
            PN seed; 0 selects 1.
        reg_bits : int
            PN/Gold register width.
        lfsr : int
            0=galois, 1=fibonacci.
        taps_a : int
            Gold: first register's taps.
        seed_a : int
            Gold: first register's seed.
        taps_b : int
            Gold: second register's taps.
        seed_b : int
            Gold: second register's seed.
        derived_by : int
            0 when the caller supplies this field; otherwise the index of the
            producing stage, PLUS ONE.
        derived_bits : int
            Length of a derived field, in bits.

        Returns
        -------
        int
            The new field's index, or -1 if the description is full, already
            built, or the literal could not be copied.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc, ccsds_asm_bits
        >>> empty = np.empty(0, np.uint8)
        >>> asm = ccsds_asm_bits()
        >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
        ...                   np.uint8)
        >>> data = np.unpackbits(octets).astype(np.uint8)
        >>> d = FrameDesc(empty, empty, empty)   # begin from nothing
        >>> d.add_field(asm)                     # the attached sync marker
        0
        >>> d.add_field(data)                    # the transfer frame
        1

        A field the CALLER does not supply is still a field, because it is still
        on the wire -- `derived_by` names the stage that fills it, PLUS ONE:

        >>> d.add_field(empty, derived_by=1, derived_bits=32 * 8)
        2

        """

    def add_stage(
        self,
        kind: int = 0,
        first_field: int = 0,
        n_fields: int = 0,
        depth: int = 0,
        emit_num: int = 0,
        emit_den: int = 0,
    ) -> int:
        """Append one transform and -- the load-bearing part -- the span of
        fields it covers. `kind` is a `wfm_stage_kind_t` index: 0 crc16, 1 rs,
        2 randomise, 3 conv (jm gh-1021 -- a method parameter cannot yet be a
        string enum). `n_fields = 0` means the stage does not run. A stage that
        inherited whatever ran before it is the representation that cannot
        express a CCSDS CADU, where the marker is covered by the inner code and
        by neither the outer code nor the randomiser.

        n_fields is the load-bearing part and 0 means the stage does not run. A
        stage that inherited "everything before me" instead of declaring its
        cover is the representation that cannot express a CCSDS CADU — see
        `wfm/wfm_frame.h`.

        Parameters
        ----------
        kind : int
            wfm_stage_kind_t index; 0=crc16…3=conv.
        first_field : int
            First field covered.
        n_fields : int
            Fields covered; 0 = the stage does not run.
        depth : int
            Interleaving depth, for an outer code.
        emit_num : int
            Expansion numerator for a stage that emits a NEW stream; 0 when the
            stage stays inside the frame.
        emit_den : int
            Expansion denominator.

        Returns
        -------
        int
            The new stage's index, or -1 if the description is full or already
            built.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc, ccsds_asm_bits
        >>> empty = np.empty(0, np.uint8)
        >>> asm = ccsds_asm_bits()
        >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
        ...                   np.uint8)
        >>> data = np.unpackbits(octets).astype(np.uint8)
        >>> d = FrameDesc(empty, empty, empty)
        >>> _ = d.add_field(asm), d.add_field(data)
        >>> _ = d.add_field(empty, derived_by=1, derived_bits=32 * 8)
        >>> d.add_stage(1, first_field=1, n_fields=2, depth=1)   # RS(255,223)
        0
        >>> d.add_stage(2, first_field=1, n_fields=2)            # randomiser
        1

        Both start at field 1, so both skip the marker -- the cover is DECLARED,
        which is the whole reason a CADU is describable here:

        >>> d.build()
        >>> d.stage_first(0), d.stage_bits(0)
        (32, 2040)

        """

    def build(self) -> None:
        """Lay out and materialise a description. Where a description is
        checked: one that cannot produce its own bits is not a frame. Separate
        from the constructor only because the description arrives over several
        calls and there is no earlier moment at which it is complete. Raises if
        it is empty, unbuildable, names a stage no kernel here covers, or was
        already built.

        The point at which a description is checked, which for frame_create
        happens inside the constructor: a description that cannot produce its
        own bits is not a frame. It is separate here only because the
        description arrives over several calls and there is no earlier moment
        at which it is complete.

        The CRC, the outer code, the randomiser and the inner code are all
        runnable: `ccsds_tm` has no Python binding and is not getting one, so
        this object is where a caller meets them. A stage naming a kernel
        nothing here carries is refused rather than skipped, because a stage
        that quietly did not run produces a frame that still assembles and
        syncs to nothing.

        The inner encoder starts from the all-zero register on every build: a
        description describes ONE frame. A stream of CADUs sharing one register
        is a transmitter's job and lives in `ccsds_tm_frame_encode`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``build failed``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.nbits                     # 13 + 16 + 16, laid out by build()
        45

        A description that cannot produce bits is not a frame, and is refused
        rather than half-built:

        >>> FrameDesc(empty, empty, empty).build()
        Traceback (most recent call last):
            ...
        ValueError: build failed (rc=-1)

        """

    def check(self, rx_bits: NDArray[np.uint8]) -> FrameCheck:
        """Undo the description's stages over a received frame and report what
        was found -- the receive mirror of `bits()`, reading the same
        description, so a transmitter and a receiver holding the same `Frame`
        cannot disagree about which stage covered what. This is the truth-free
        frame error rate on a CODED link: it needs no payload truth, so it
        works on a real capture, and an outer code is a strictly better
        detector than a CRC because it reports how much repair it took rather
        than one bit of right-or-wrong. `checked` is smaller than `stages` when
        the description names a stage the receiver does not reverse here -- the
        inner code is the case, being undone before frame synchronisation --
        and such a stage is reported as not checked, never as passed.

        The receive mirror of frame_bits, reading the same description — so a
        transmitter and a receiver holding the same `Frame` cannot disagree
        about which stage covered what.

        **This is the truth-free frame error rate on a coded link.** It needs
        the description and the received bits and no payload truth at all, so
        it works on a real capture, and unlike a self-referenced EVM it still
        catches a false lock.

        checked is smaller than stages when the description names a stage the
        receiver does not reverse here — the inner code is the case, since it
        is undone before frame synchronisation and a frame checker never sees
        channel symbols. Such a stage is reported as not checked, never as
        passed.

        Parameters
        ----------
        rx_bits : NDArray[np.uint8]
            Received bits, one per byte. Copied, not modified.

        Returns
        -------
        FrameCheck
            The outcome. passed is 0 and checked is 0 when the description
            carries no reversible stage at all — "carries no check" is not "the
            check passed", and an FER conflating them would score every
            unprotected frame as perfect.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> r = d.check(d.bits(1))
        >>> r.passed, r.ok, r.units
        (1, 1, 1)

        Flip a bit the CRC covers and the verdict turns over:

        >>> rx = np.asarray(d.bits(1)).copy()
        >>> rx[d.field_off(2)] ^= 1
        >>> d.check(rx).passed
        0

        Carrying no check is NOT passing one -- both are reported, separately:

        >>> n = FrameDesc(empty, sync, payload, crc="none")
        >>> n.build()
        >>> c = n.check(n.bits(1))
        >>> c.passed, c.checked
        (0, 0)

        """

    def n_fields(self) -> int:
        """Fields in the description. A `Frame` built the four-field way
        reports 4 -- `wfm_frame_t` IS a configuration of the general
        description, so the indexed view below reads it too.

        Returns
        -------
        int
            How many fields the description carries.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.n_fields()          # the four named fields, absent ones included
        4

        """

    def n_stages(self) -> int:
        """Stages in the description.

        Returns
        -------
        int
            How many stages the description carries.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.n_stages()         # the CRC is a stage like any other
        1

        """

    def field_off(self, i: int) -> int:
        """Bit offset of field `i`, or 0 if there is no such field.

        Parameters
        ----------
        i : int
            Field index.

        Returns
        -------
        int
            Bits from the start of the frame.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.field_off(1), d.field_off(2), d.field_off(3)
        (0, 13, 29)

        Field 0 is the absent preamble: an empty field still HAS an index, so the
        indices a caller passed to `add_field` keep meaning what they meant.

        >>> d.field_off(0), d.field_bits(0)
        (0, 0)

        """

    def field_bits(self, i: int) -> int:
        """Bits in field `i`, or 0 if there is no such field.

        Parameters
        ----------
        i : int
            Field index.

        Returns
        -------
        int
            The field's length in bits.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.field_bits(1), d.field_bits(2), d.field_bits(3)
        (13, 16, 16)

        """

    def stage_first(self, i: int) -> int:
        """First frame bit stage `i` covers; 0 for a stage that did not run.

        Parameters
        ----------
        i : int
            Stage index.

        Returns
        -------
        int
            Bits from the start of the frame.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.stage_first(0)     # the CRC starts at the payload, not at bit 0
        13

        """

    def stage_bits(self, i: int) -> int:
        """Bits stage `i` covers; 0 for a stage that did not run -- which is
        how an optional stage is spelled, and why `first` is 0 there too.

        Parameters
        ----------
        i : int
            Stage index.

        Returns
        -------
        int
            The covered span, in bits.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> empty = np.empty(0, np.uint8)
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(empty, sync, payload, crc="crc16")
        >>> d.build()
        >>> d.stage_bits(0)      # payload+CRC: what crc16 covered
        32

        """

    @property
    def nbits(self) -> int:
        """Nbits."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "FrameDesc":
        """Enter a context manager, returning this object.

        Lets a FrameDesc be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        FrameDesc
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the FrameDesc.

        Equivalent to calling `destroy()`. Returns ``None``, so an exception
        raised inside the `with` body propagates normally; this never
        suppresses one.

        Parameters
        ----------
        exc_type : object | None
            Exception class, or None. Ignored.
        exc : object | None
            Exception instance, or None. Ignored.
        tb : object | None
            Traceback object, or None. Ignored.
        """

def bpsk_map(bits: NDArray[np.uint8]) -> NDArray[np.complex64]:
    """Map bits {0,1} to BPSK symbols {+1,-1} (cf32).

    Parameters
    ----------
    bits : NDArray[np.uint8]
        Array of uint8 values; only the LSB of each byte is used.

    Returns
    -------
    NDArray[np.complex64]
        Output.

    Examples
    --------
    >>> from doppler.wfm import bpsk_map
    >>> import numpy as np
    >>> bits = np.array([0, 1, 0, 1], dtype=np.uint8)
    >>> bpsk_map(bits).tolist()
    [(1+0j), (-1+0j), (1+0j), (-1+0j)]

    """

def qpsk_map(syms: NDArray[np.uint8]) -> NDArray[np.complex64]:
    """Map QPSK symbol indices {0,1,2,3} to Gray-coded symbols (cf32).

    Parameters
    ----------
    syms : NDArray[np.uint8]
        Array of uint8 symbol indices; values must be in {0,1,2,3}. Bits
        above position 1 are ignored.

    Returns
    -------
    NDArray[np.complex64]
        Output.

    Examples
    --------
    >>> from doppler.wfm import qpsk_map
    >>> import numpy as np
    >>> idx = np.array([0, 1, 2, 3], dtype=np.uint8)
    >>> out = qpsk_map(idx)
    >>> [round(float(v.real), 4) for v in out]
    [0.7071, -0.7071, 0.7071, -0.7071]
    >>> [round(float(v.imag), 4) for v in out]
    [0.7071, 0.7071, -0.7071, -0.7071]

    """

def wfm_awgn_amplitude(snr_db: float, signal_power: float) -> float:
    """AWGN amplitude for a target SNR (dB, over fs) given signal power.

    Parameters
    ----------
    snr_db : float
        Target SNR in dB, referenced to the full sample rate.
    signal_power : float
        RMS power of the signal (e.g. 1.0 for unit-power complex tones or
        unit-energy BPSK/QPSK symbols).

    Returns
    -------
    float
        Per-component AWGN amplitude (sigma for one I or Q channel).

    Examples
    --------
    >>> from doppler.wfm import wfm_awgn_amplitude
    >>> round(float(wfm_awgn_amplitude(10.0, 1.0)), 6)
    0.223607
    >>> round(float(wfm_awgn_amplitude(0.0, 1.0)), 6)
    0.707107

    """

def wfm_ebno_to_snr_db(
    ebno_db: float,
    bits_per_symbol: int,
    samples_per_symbol: float,
) -> float:
    """Convert Eb/No (dB) to SNR (dB over fs).

    Parameters
    ----------
    ebno_db : float
        Eb/No in dB (energy per bit over noise spectral density).
    bits_per_symbol : int
        Bits carried per modulation symbol: 1 for BPSK, 2 for QPSK.
    samples_per_symbol : float
        Oversampling ratio (sps), e.g. 8.0.

    Returns
    -------
    float
        SNR in dB measured over the full sample-rate bandwidth.

    Examples
    --------
    >>> from doppler.wfm import wfm_ebno_to_snr_db
    >>> round(float(wfm_ebno_to_snr_db(10.0, 2, 8.0)), 4)
    3.9794
    >>> round(float(wfm_ebno_to_snr_db(10.0, 1, 8.0)), 4)
    0.9691

    """

def mls_poly(n: int) -> int:
    """Maximal-length-sequence primitive polynomial for an LFSR of length
    n.

    Parameters
    ----------
    n : int
        LFSR length in stages (2..64).

    Returns
    -------
    int
        Primitive-polynomial tap mask, or 0 if n is out of range.

    Examples
    --------
    >>> from doppler.wfm import mls_poly
    >>> hex(mls_poly(7))
    '0x41'

    """

def ccsds_asm_bits() -> NDArray[np.uint8]:
    """The CCSDS Attached Sync Marker, 0x1ACFFC1D, as 32 unpacked bits —
    `out[0]` is the first bit on the wire (the top of 0x1A). Pass it to
    `doppler.detection.SyncFinder` to acquire a CADU in a bit stream; it is
    NOT randomised, so it reads the same in every frame and in exactly one
    polarity, which is what makes it the thing that reports a 180-degree
    carrier ambiguity.

    `out[0]` is the first bit on the wire — figure 9-1 of 131.0-B numbers
    the marker's bit 0 as the most significant bit of 0x1A. One bit per
    byte, the convention every frame path here passes around.

    The thing a Python receiver ACQUIRES on: pair it with
    `doppler.detection.SyncFinder` to find where a CADU starts in a bit
    stream, then slice and `Frame.check()` it. The marker is deliberately
    NOT randomised (10.4's NOTE: "The ASM was not randomized and is not
    derandomized"), so it reads the same in every frame and in exactly one
    polarity — which is what makes it the only thing in a CADU that can
    report a 180-degree carrier ambiguity.

    A function rather than a constant a caller expands, because an
    MSB-first expansion written out twice is a transcription that can
    disagree with itself. This tree's own doctests were the second copy
    until doppler#900.

    Returns
    -------
    NDArray[np.uint8]
        Output.

    Examples
    --------
    >>> from doppler.wfm import ccsds_asm_bits
    >>> b = ccsds_asm_bits()
    >>> b.size, b[:8].tolist()          # 0x1A, first bit at the top
    (32, [0, 0, 0, 1, 1, 0, 1, 0])
    >>> int("".join(map(str, b.tolist())), 2) == 0x1ACFFC1D
    True

    """

def crc16(bits: NDArray[np.uint8]) -> int:
    """CRC-16-CCITT (poly 0x1021, init 0xFFFF) over an unpacked 0/1 bit
    array, MSB-first — the DSSS burst frame trailer wfmgen appends and
    BurstDemod validates.

    Parameters
    ----------
    bits : NDArray[np.uint8]
        Array of 0/1 bit values (one per byte).

    Returns
    -------
    int
        The 16-bit CRC.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.wfm import crc16
    >>> ascii_bits = np.unpackbits(np.frombuffer(b"123456789", np.uint8))
    >>> hex(crc16(ascii_bits))   # the standard CCITT check vector
    '0x29b1'

    """

def rrc_h(t: NDArray[np.float64], beta: float) -> NDArray[np.float64]:
    """Analytic root-raised-cosine pulse at arbitrary (non-grid) times `t`, in
    symbol periods. The transmit half of a matched-filter pair. Use this, not a
    transcription of the formula, whenever a stimulus needs the pulse off the
    integer sample grid — a non-integer samples-per-symbol or a fractional
    timing offset has no grid to sample. `rrc_taps` remains the right call for
    filter taps.
    """

def rc_h(t: NDArray[np.float64], beta: float) -> NDArray[np.float64]:
    """Analytic full raised-cosine pulse at arbitrary (non-grid) times `t`, in
    symbol periods. Already the Nyquist response a matched TX/RX pair produces,
    so this is what models the matched-filter OUTPUT directly — a
    timing-detector S-curve reference, or a receiver test with its front end
    collapsed away.
    """

def rrc_taps(beta: float, sps: int, span: int) -> NDArray[np.float32]:
    """Root-raised-cosine pulse-shaping taps (2*span*sps+1 unit-energy cf32
    taps).
    """

def dsss_spread(
    syms: NDArray[np.complex64],
    code: NDArray[np.uint8],
    sf: int,
) -> NDArray[np.complex64]:
    """Direct-sequence spread syms by the ±1 chip code; yields len(syms)*sf
    chips.
    """
