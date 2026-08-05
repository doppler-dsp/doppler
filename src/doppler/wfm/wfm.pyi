# wfm/wfm.pyi — type stubs for the wfm C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

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

    def steps(self, n: int) -> NDArray[np.complex64]:
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
