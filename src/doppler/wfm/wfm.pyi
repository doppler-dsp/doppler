# wfm/wfm.pyi — type stubs for the wfm C extension.
from typing import Any, final, Literal
import numpy as np
import numpy.typing as npt
from numpy.typing import NDArray

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
    period; they differ only in chip ordering/phase. Leave ``poly`` at 0 and
    the maximal-length polynomial for ``length`` is used, so ``PN(seed=1,
    length=7)`` is the 127-chip m-sequence.

    Parameters
    ----------
    poly : int, default 0
        Galois feedback tap polynomial (right-shift convention). The LSB is the
        tap at position 0 (always 1 for a primitive poly); bit k=1 means tap at
        position k. 0 (the default) selects the maximal-length polynomial for
        ``length``. The Fibonacci taps are derived automatically so you only
        supply one value.
    seed : int, default 0
        Initial LFSR register state; must be non-zero WITHIN the register,
        `seed & pn_register_mask (length)` -- the all-zero state is a fixed
        point, and a seed that masks to it (128 on 7 bits) is refused like 0.
    length : int, default 0
        Register width in bits, 1..64. The sequence period is 2^length - 1 for
        a primitive polynomial.
    lfsr : Literal["galois", "fibonacci"], default "galois"
        One of ``"galois"``, ``"fibonacci"``.

    Examples
    --------
    >>> from doppler.wfm import PN
    >>> p = PN(poly=96, seed=1, length=7)
    >>> chips = p.generate(127)
    >>> chips.dtype
    dtype('uint8')
    >>> int(chips.sum())   # 64 ones per MLS period (2^(n-1))
    64
    >>> int(PN(seed=1, length=7).generate(127).sum())
    64

    """

    def __init__(
        self,
        poly: int = 0,
        seed: int = 0,
        length: int = 0,
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
        out: npt.NDArray[np.uint8] | None = None,
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
        out : npt.NDArray[np.uint8] | None
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
        """Output capacity hint for dp_pn_generate(): always 0.

        The chip count is the caller's n, not a function of the state, so there
        is no state-derived bound; size out to n.

        Returns
        -------
        int
            0.
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

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the PN has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

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
        /,
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
    single streaming source. One call to dp_wfm_synth_step() or
    dp_wfm_synth_steps() advances all sub-components in lock-step. SNR >=
    WFM_SYNTH_SNR_CLEAN (100 dB) skips AWGN entirely — clean waveforms pay no
    noise overhead. When ``snr_mode`` is "auto" the library picks the natural
    reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR for
    tone/noise/PN.

    Parameters
    ----------
    type : Literal["tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits", "symbols", "dsss"], default "tone"
        The waveform. For `bits` attach the pattern with set_bits(), for
        `symbols` the complex stream with set_symbols(), and for `dsss` the
        burst with set_dsss_chips(), after construction.
        One of ``"tone"``, ``"noise"``, ``"pn"``, ``"bpsk"``, ``"qpsk"``,
        ``"chirp"``, ``"bits"``, ``"symbols"``, ``"dsss"``.
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
        What `snr` is referenced to: `fs` the full band, `ebno` Eb/N0, `esno`
        Es/N0. `auto` is Es/N0 for bpsk/qpsk and full scale for the others,
        dsss included: its spreading factor is not known at create time, so the
        composer refers a dsss SNR itself.
        One of ``"auto"``, ``"fs"``, ``"ebno"``, ``"esno"``.
    seed : int, default 1
        PRNG seed shared by AWGN and the PN LFSR. Default 1.
    sps : int, default 8
        Samples per symbol for modulated types (BPSK, QPSK, PN; per chip for
        dsss). Ignored for tone/noise. Values < 1 are clamped to 1. Default 8.
    pn_length : int, default 7
        LFSR register length; period = 2^pn_length - 1. With pn_poly 0 it must
        be 2..64 (the MLS table's range). Default 7 (period 127).
    pn_poly : int, default 0
        Galois tap polynomial for the LFSR. 0 means "look up the canonical MLS
        polynomial for pn_length" from the wfm_synth_mls_poly table. Default 0.
    lfsr : Literal["galois", "fibonacci"], default "galois"
        One of ``"galois"``, ``"fibonacci"``.
    f_end : float, default 0.0
        Chirp end frequency in Hz (type=chirp only; ignored otherwise). With
        ``freq`` as the start, the instantaneous frequency sweeps linearly from
        ``freq`` to ``f_end`` over the span set by
        dp_wfm_synth_set_chirp_span(), then holds at ``f_end``. Until a span is
        pinned the slope is 0 (a CW tone at ``freq``). ``f_end < freq`` is a
        down-chirp. Default 0.0.

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
        fs: float = 1000000.0,
        freq: float = 0.0,
        snr: float = 100.0,
        snr_mode: Literal["auto", "fs", "ebno", "esno"] = "auto",
        seed: int = 1,
        sps: int = 8,
        pn_length: int = 7,
        pn_poly: int = 0,
        lfsr: Literal["galois", "fibonacci"] = "galois",
        f_end: float = 0.0,
    ) -> None: ...
    def reset(self) -> None:
        """Reset Synth to its post-create state. Resets the LO phase
        accumulator, AWGN internal state, and PN LFSR register to their initial
        values so the output sequence is perfectly reproducible from sample 0.
        Also rewinds the bit/symbol cursors, the chirp phase and the
        continuous-DSSS clocks, and clears the shaper's delay line.
        Configuration (patterns, codes, window, RRC taps, chirp span) is kept.
        With a refill attached the data-ended latch is NOT cleared and the
        rewound cursor replays the frame in the buffer (the last one pulled),
        since a pulled source cannot rewind what it consumed.

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
            Next output sample (float _Complex).

        Examples
        --------
        >>> from doppler.wfm import _SynthEngine
        >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
        >>> s.step()
        (1+0j)

        """

    def steps(self, n: int = 1) -> NDArray[np.complex64]:
        """Generate a block of output samples. Produces the same stream as n
        calls of dp_wfm_synth_step(), writing each cf32 sample into ``output``,
        but batched: the LO, AWGN and PN are generated a chunk at a time and
        the symbol map, carrier mix and noise add are separate passes. Because
        the mix and the add round separately, the result can differ from step()
        by an ULP under -ffast-math on FMA targets (arm64, QPSK); drive one
        path where byte-for-byte agreement matters. The Python binding returns
        a freshly allocated NumPy complex64 array; ownership is transferred to
        the caller.

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

    def set_chirp_span(self, span: int) -> None:
        """Pin a chirp's sweep to `span` samples (no-op for non-chirp). Only
        the first non-zero pin takes effect; until then a chirp holds its start
        frequency on step() and steps() alike.

        A linear chirp's slope is `(f_end − f_start) / span`, so the span — the
        number of samples the sweep occupies — must be known before generation.
        The composer calls this with the source's declared span or the segment
        length. A synth that is never pinned does not sweep: it holds the start
        frequency on dp_wfm_synth_step() and dp_wfm_synth_steps() alike, so the
        waveform never depends on how reads are chunked. Only the first pin
        (while the span is still 0) takes effect, so it is safe to call
        unconditionally after dp_wfm_synth_create(); span 0 is a no-op.

        The span is configuration, not running state: dp_wfm_synth_get_state()
        does not carry it, so pin a resumed instance exactly as the original
        was pinned.

        Parameters
        ----------
        span : int
            Sweep length in samples (> 0).
        """

    def prod(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Multiply a block by the synth's next output samples.

        ``out[i] = x[i] * y[i]``, where ``y`` is what ``steps(len(x))`` would
        emit next. The synth advances exactly as ``steps(len(x))`` does, so
        split calls leave the same synth state as one call and an empty ``x``
        does not advance it; the product itself agrees across such call
        sequences to rounding (a few ulp of ``|x| * |y|``), not to the bit,
        because the compiler may contract the multiply differently on different
        paths (arm64 under ``-ffast-math``). ``x`` is not modified.

        ``x`` must be a 1-D C-contiguous complex64 array: a strided,
        non-complex64 or non-1-D block is refused (``ValueError`` /
        ``TypeError``), never copied or cast.

        This is the general waveform-multiplication operator: with a tone synth
        it is a rotation, with a chirp synth a swept rotation, with noise a
        noise multiplier, with PN/BPSK a modulation.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input.
        out : npt.NDArray[np.complex64] | None
            Output block of ``n`` cf32 samples (may be the input block itself).

        Returns
        -------
        NDArray[np.complex64]
            Output.

        Examples
        --------
        >>> from doppler.wfm import Synth
        >>> import numpy as np
        >>> s = Synth(type="tone", fs=1.0, freq=0.25)
        >>> y = s.prod(np.ones(4, np.complex64))
        >>> bool(np.allclose(y, [1, 1j, -1, -1j], atol=1e-5))
        True

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

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the _SynthEngine has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

        Raises ``TypeError`` if *blob* is not bytes, ``ValueError`` if its
        length differs from `state_bytes()` or the core rejects it, and
        ``RuntimeError`` if the _SynthEngine has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    def set_rrc(self, taps: npt.ArrayLike, /) -> None:
        """Enable root-raised-cosine pulse shaping with real FIR taps.

        Replaces the rectangular sample-and-hold of a symbol synth (pn, bpsk,
        qpsk, bits, symbols, dsss) with the pulse ``taps``, typically
        ``doppler.wfm.rrc_taps(beta, sps, span)``. The taps are scaled by
        ``sqrt(sps)`` internally for unit transmit power, so pass them raw. A
        no-op for tone, noise and chirp. Replaces any existing shaper and
        clears its delay line.

        Parameters
        ----------
        taps : array_like
            Real FIR taps, coerced to float32 and copied.

        Raises
        ------
        ValueError
            ``taps`` is empty on a symbol synth, or the shaper could not be
            allocated.
        """

    def set_bits(
        self,
        pattern: npt.ArrayLike,
        modulation: int = ...,
        /,
    ) -> None:
        """Attach a user bit pattern to a type='bits' synth.

        The pattern is oversampled by the create-time ``sps`` and sent ONCE:
        one pass is ``len(pattern) * sps`` samples (divided by ``modulation``
        for a PSK), and the output is silent after it. Replaces any previous
        pattern and resets the read position. A no-op on any other waveform
        type.

        Parameters
        ----------
        pattern : array_like
            Bits, each 0 or 1, coerced to uint8 and copied.
        modulation : int, default 1
            Bits per symbol: 0 = none (0/1 amplitude), 1 = bpsk, 2 = qpsk,
            3 = 8psk. A PSK symbol's bits are read MSB-first as a Gray label
            into the library's ``mpsk_constellation`` mapping.

        Raises
        ------
        ValueError
            ``pattern`` is empty, or ``modulation`` is not 0 to 3, on a
            type='bits' synth.
        """

    def set_dsss_chips(self, chips: npt.ArrayLike, /) -> None:
        """Attach an already-assembled DSSS burst to a type='dsss' synth.

        One chip per element, BPSK-mapped by the synth and held for the
        create-time ``sps`` samples (samples per chip here). One pass is one
        burst, sent once, then silence. Assemble the burst from a frame
        description: the unspread preamble, then every bit of a ``Frame``'s
        ``bits()`` spread by the data code. Replaces any previous pattern and
        resets the read position. A no-op on any other waveform type.

        Parameters
        ----------
        chips : array_like
            Burst chips, each 0 or 1, coerced to uint8 and copied.

        Raises
        ------
        ValueError
            ``chips`` is empty, on a type='dsss' synth.
        """

    def set_dsss_cont(
        self,
        code: npt.ArrayLike,
        chips_per_symbol: float,
        data: str = ...,
        payload: npt.ArrayLike | None = ...,
    ) -> None:
        """Configure a type='dsss' synth for continuous asynchronous
        generation.

        The spreading ``code`` repeats endlessly, and data rides on it at
        ``chips_per_symbol`` chips a symbol. A non-integer value is the normal,
        asynchronous case: symbol edges land mid-epoch. Chips are generated per
        sample, so the stream has no length. A no-op on any other waveform
        type.

        Parameters
        ----------
        code : array_like
            Spreading code chips, each 0 or 1, coerced to uint8 and copied.
        chips_per_symbol : float
            ``chip_rate / symbol_rate``, at least 1.
        data : {'prbs', 'none', 'bits'}, default 'prbs'
            The data-symbol source: 'none' is the pure code, 'prbs' the synth's
            own seeded PN (a receiver regenerates it with ``doppler.wfm.PN``),
            and 'bits' the ``payload``, one bit a data symbol, sent once.
        payload : array_like, optional
            Payload bits, each 0 or 1. Supplying it forces ``data='bits'``.

        Raises
        ------
        ValueError
            ``data`` is not one of the three, or the geometry is invalid: no
            code, ``chips_per_symbol < 1``, no payload for 'bits', or no valid
            ``pn_length`` for 'prbs'.
        """

    def set_dsss_window(
        self,
        code_only_symbols: int,
        frame_symbols: int,
    ) -> None:
        """Give the continuous DSSS stream a frame with a pure-code window.

        The frame is on the data clock: of every ``frame_symbols`` symbols, the
        first ``code_only_symbols`` carry the pure code and no data, and the
        rest carry the payload, running on across frames. The symbol clock
        free-runs through the window, so a frame edge lands at no particular
        chip phase. Configuration, not running state: ``reset()`` keeps it. The
        order against ``set_dsss_cont`` does not matter. A no-op on any other
        waveform type.

        Parameters
        ----------
        code_only_symbols : int
            Pure-code symbols opening each frame, at most ``frame_symbols``.
        frame_symbols : int
            Frame length in symbols. 0 means no window: the stream exactly as
            without this call.

        Raises
        ------
        ValueError
            A count is negative, or ``code_only_symbols`` exceeds a non-zero
            ``frame_symbols``.
        """

    def set_symbols(self, symbols: npt.ArrayLike, /) -> None:
        """Attach a complex-symbol stream to a type='symbols' synth.

        Each element is the constellation point itself, with no bit-to-symbol
        map, so any modulation is "compute the symbols, pass them in". The
        stream is oversampled by the create-time ``sps``, cycled to fill every
        request, and RRC-shaped when ``set_rrc`` is active. Replaces any
        previous stream and resets the read position. A no-op on any other
        waveform type.

        Parameters
        ----------
        symbols : array_like
            Complex symbols, cast to complex64 and copied.

        Raises
        ------
        ValueError
            ``symbols`` is empty, on a type='symbols' synth.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import _SynthEngine
        >>> s = _SynthEngine(
        ...     type="symbols", fs=1.0, freq=0.0, snr=100.0, sps=4)
        >>> s.set_symbols(np.array([1+0j, 1j, -1+0j, -1j], np.complex64))
        >>> bool(np.array_equal(s.steps(16)[::4], [1, 1j, -1, -1j]))
        True

        """
    def get_wtype(self) -> int:
        """Return the active waveform type discriminant. Maps to the
        WFM_SYNTH_* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk, 5=chirp,
        6=bits, 7=symbols, 8=dsss. Use this to inspect which synthesis path is
        active at runtime.

        Returns
        -------
        int
            Integer waveform type index (WFM_SYNTH_TONE .. WFM_SYNTH_DSSS), the
            create-time `type` unless overridden.
        """

    def set_wtype(self, value: int, /) -> None:
        """Override the waveform type discriminant in-place. Changing wtype
        does not reinitialise sub-objects (no PN, LO, AWGN or pattern is
        created or freed, and the value is not validated); use with care.

        Parameters
        ----------
        value : int
            Input.
        """

    def get_nsps(self) -> int:
        """Return the samples-per-symbol count. For symbol types (PN, BPSK,
        QPSK, bits, symbols, dsss chips) each symbol is held for nsps
        consecutive output samples. For tone/noise/chirp this field is present
        but unused by the synthesis path.

        Returns
        -------
        int
            Samples per symbol: the create-time `sps` clamped to >= 1, or
            whatever dp_wfm_synth_set_nsps() last stored.
        """

    def set_nsps(self, value: int, /) -> None:
        """Override the samples-per-symbol count in-place. Does not flush the
        symbol-position counter (sym_pos); set sym_pos=0 as well when changing
        sps mid-stream. Not validated, and does not rebuild an attached RRC
        shaper (whose taps and polyphase split were fixed for the nsps at
        dp_wfm_synth_set_rrc() time) or rescale the noise.

        Parameters
        ----------
        value : int
            Input.
        """

    def get_sym_pos(self) -> int:
        """Return the current position within the current symbol (0..nsps-1).
        Reaches nsps and wraps to 0 each time a new symbol is latched from the
        active source (PN LFSR, bit pattern, symbol stream or DSSS chip).
        Useful for frame alignment: sym_pos==0 on a step boundary means the
        very next sample begins a fresh symbol. Not advanced on the
        polyphase-shaper path (power-of-two sps with RRC), nor for
        tone/noise/chirp.

        Returns
        -------
        int
            Symbol position counter (0 <= sym_pos < nsps); 0 after create.
        """

    def set_sym_pos(self, value: int, /) -> None:
        """Override the symbol-position counter in-place. Injecting 0 forces
        the next dp_wfm_synth_step() to latch a new symbol; any other value
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
        to 1.0 so that the held symbol is a clean unit-power carrier (chirp
        too); for noise it is 0.0 (noise has no held symbol), as it is for
        every symbol type before its first latch.

        Returns
        -------
        float
            Current symbol real (I) component.
        """

    def set_cur_re(self, value: float, /) -> None:
        """Override the held-symbol real (I) component in-place. Takes effect
        on the next dp_wfm_synth_step() within the current symbol hold.

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

    def set_cur_im(self, value: float, /) -> None:
        """Override the held-symbol imaginary (Q) component in-place. Takes
        effect on the next dp_wfm_synth_step() within the current symbol hold.

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
        /,
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
    the 2**length-1 XOR members of the Gold-code family while Register B stays
    fixed.

    Parameters
    ----------
    taps_a : int, default 934
        Register A feedback-tap mask; bit k set means stage k+1 is XORed into
        the feedback. Default 934 (stages 2,3,6,8,9,10 — the CCSDS-fixed
        Register A polynomial x^10+x^9+x^8+x^6+x^3+x^2+1).
    seed_a : int, default 350
        Register A initial value; must be non-zero within the register (`seed_a
        & pn_register_mask (length)`: a multiple of 2^length is refused like
        0). Per CCSDS this is "User dependent" — each of the 2^length-1 nonzero
        values selects a different member of the family (1023 distinct codes at
        length=10, verified in test_gold_core.c). Default 350 is the worked
        example from CCSDS 415.0-G-1 Figure 5-2 (PN Code Library Table 1, Code
        Number 365).
    taps_b : int, default 567
        Register B feedback-tap mask, same bit convention as ``taps_a``.
        Default 567 (stages 1,2,3,5,6,10 — the CCSDS-fixed Register B
        polynomial).
    seed_b : int, default 73
        Register B initial value; must be non-zero within the register, as
        seed_a. Default 73 (stages 1,4,7 — CCSDS's fixed Register B initial
        value 1001001000, unique per the standard, not user-selectable).
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
        taps_a: int = 934,
        seed_a: int = 350,
        taps_b: int = 567,
        seed_b: int = 73,
        length: int = 10,
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
        out: npt.NDArray[np.uint8] | None = None,
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
        out : npt.NDArray[np.uint8] | None
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
        """Output capacity hint for dp_gold_generate(): always 0.

        The chip count is the caller-requested n of dp_gold_generate(), so
        there is no fixed per-call maximum; size the output buffer to n.

        Returns
        -------
        int
            0, meaning "no fixed bound; the output length is n".
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

        Captures the state that evolves as the object runs, including any value
        a setter changed after construction, so a blob taken now and restored
        later resumes from this point. Construction parameters are not
        restored: restore into an object built the same way. A core may check
        some of them and refuse a blob built differently.

        The blob is opaque and always `state_bytes()` long. Its layout is an
        implementation detail of the C core and is not a stable format across
        builds.

        Raises ``RuntimeError`` if the Gold has already been destroyed.

        Returns
        -------
        bytes
            Opaque snapshot, `state_bytes()` bytes long.
        """

    def set_state(self, blob: bytes, /) -> None:
        """Restore mutable state from a `get_state()` blob.

        Overwrites the live state in place, including any value a setter
        changed after construction. Construction parameters are not restored:
        restore into an object built the same way. Length is validated against
        `state_bytes()` before the blob is handed to the C core, which may
        reject it too: a core may check some construction parameters and refuse
        a blob built differently.

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
        /,
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
    preamble : NDArray[np.uint8], default ...
        Preamble bits, one per element, each 0 or 1. Omitted, there is no
        preamble. A repeated preamble is repeated in its bits:
        `field_bits("pn:31:5*4")`.
    sync : NDArray[np.uint8], default ...
        Sync-word bits, one per element, each 0 or 1. Omitted, the frame is
        unsynced.
    payload : NDArray[np.uint8], default ...
        Payload bits, one per element, each 0 or 1. Omitted, the frame carries
        none.
    crc : Literal["none", "crc16"], default "none"
        `crc16` covers the payload with a CRC-16; `none` adds no check.
        One of ``"none"``, ``"crc16"``.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``frame geometry is
        empty, or a field holds an element that is not a bit (0 or 1)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.wfm import Frame
    >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)   # Barker-13
    >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
    >>> f = Frame(sync=sync, payload=payload, crc="crc16")
    >>> f.nbits                                          # 13 + 16 + 16
    45
    >>> f.field_off(f.field_index("payload"))
    13
    >>> f.crc_ok(f.bits())        # its own bits are its own truth
    1

    """

    def __init__(
        self,
        preamble: npt.NDArray[np.uint8] | bytes | bytearray | memoryview = ...,
        sync: npt.NDArray[np.uint8] | bytes | bytearray | memoryview = ...,
        payload: npt.NDArray[np.uint8] | bytes | bytearray | memoryview = ...,
        crc: Literal["none", "crc16"] = "none",
    ) -> None: ...
    def bits(
        self,
        count: int = 1,
        out: npt.NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Materialise n consecutive frames, one bit per byte.

        n counts FRAMES, not bits: a descriptor describes one frame, and a
        capture holds many. It is the truth for a transmitter that sends the
        same frame n times -- a data source of n copies of the payload, one
        chunk a frame -- so a stream compared against this lines up with the
        one that was transmitted.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[np.uint8] | None
            Output, one bit per byte.

        Returns
        -------
        NDArray[np.uint8]
            Bits written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> len(d.bits())        # one frame: 13 + 16 + 16
        45
        >>> len(d.bits(2))       # n counts FRAMES, tiled the way a capture is
        90

        """

    def bits_max_out(self, n: int, /) -> int:
        """Bits dp_frame_bits will write for n frames — `n * nbits`.

        Parameters
        ----------
        n : int
            Frame repetitions.

        Returns
        -------
        int
            `n * nbits`, the output capacity in bits (one per byte) that
            dp_frame_bits() needs; 0 for a NULL state or an unbuilt
            description.
        """

    def crc_ok(
        self,
        rx_bits: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
    ) -> int:
        """Check one received frame's CRC.

        **This is what makes a truth-free frame error rate possible.** It needs
        no payload truth at all, so it works on a real capture, and unlike a
        self-referenced EVM or a blind M2M4 it still catches a false lock — a
        rotated constellation fails the check rather than looking clean.

        Parameters
        ----------
        rx_bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> d.crc_ok(d.bits())           # its own bits are its own truth
        1
        >>> rx = np.asarray(d.bits()).copy()
        >>> rx[d.field_off(d.field_index("payload"))] ^= 1   # one payload bit
        >>> d.crc_ok(rx)
        0

        """

    def add_field(
        self,
        name: str,
        bits: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
    ) -> int:
        """Append one named field to a description (see `FrameDesc`): bits and
        nothing else, one per element, each 0 or 1. Text reaches it through
        `field_bits()`, hex and packed octets through `cvt`. A field a STAGE
        fills is `add_derived` instead, because the caller has no bits for it.
        `name` may be empty for an anonymous field; a name another field
        carries is refused. Returns the new field's index. Refuses once the
        frame is built.

        The field is bits and nothing else, copied here so the description
        outlives the call. A field a STAGE fills is appended with
        dp_frame_add_derived instead, because the caller has no bits for it.

        Parameters
        ----------
        name : str
            The field's name, or NULL/"" for anonymous; a name another field
            carries is refused.
        bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            The bits, one per element, each 0 or 1.

        Returns
        -------
        int
            The new field's index, or -1 if the description is full or already
            built, the name is taken, or an element is not a bit.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a field: the description is full or already built,
            the name is taken, the bits are empty, or an element is not a
            bit``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> from doppler.ccsds import asm_bits
        >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
        ...                   np.uint8)
        >>> d = FrameDesc()                      # begin from nothing
        >>> d.add_field("asm", asm_bits())       # the attached sync marker
        0
        >>> d.add_field("data", np.unpackbits(octets))   # the transfer frame
        1
        >>> d.field_index("data")
        1

        """

    def add_stage(
        self,
        kind: int = 0,
        first_field: int = 0,
        n_fields: int = 0,
        depth: int = 0,
        emit_num: int = 0,
        emit_den: int = 0,
        unit_bits: int = 0,
    ) -> int:
        """Append one transform and -- the load-bearing part -- the span of
        fields it covers. `kind` is a stage kind: `STAGE_CRC16`, `STAGE_RS`,
        `STAGE_RANDOMISE`, `STAGE_CONV`, `STAGE_INTERLEAVE` from `doppler.wfm`,
        or a caller's own from `STAGE_USER` up. It stays an INT rather than a
        name because the kind is an open `uint32_t` a caller extends -- the
        constants are generated from the C enum, so there is nothing to
        transcribe. `n_fields = 0` means the stage does not run. A stage that
        inherited whatever ran before it is the representation that cannot
        express a CCSDS CADU, where the marker is covered by the inner code and
        by neither the outer code nor the randomiser. `unit_bits` applies to
        `interleave` alone and is the bits per permuted unit (0 reads as 1);
        its ROW count is `depth` and its column count is derived from the span
        the stage covers.

        n_fields is the load-bearing part and 0 means the stage does not run. A
        stage that inherited "everything before me" instead of declaring its
        cover is the representation that cannot express a CCSDS CADU — see
        `wfm/wfm_frame.h`.

        Parameters
        ----------
        kind : int
            stage kind: a wfm_stage_kind_t value (0=crc16…4=interleave), or a
            caller's own from `WFM_STAGE_USER` (0x1000) up, whose kernel then
            has to reach the assembler through its ops table.
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
        unit_bits : int
            INTERLEAVE only: bits per interleaved unit; 0 reads as 1. Match it
            to the outer code's symbol — permuting octets is what spreads a
            burst across the codewords of a code over GF(256), and permuting
            bits inside one spreads a burst within a symbol that is already
            wrong.

        Returns
        -------
        int
            The new stage's index, or -1 if the description is full or already
            built. The Python binding raises `ValueError` rather than handing
            back the -1.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a stage: the description is full or already
            built``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> from doppler.ccsds import asm_bits
        >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
        ...                   np.uint8)
        >>> d = FrameDesc()
        >>> _ = d.add_field("asm", asm_bits())
        >>> _ = d.add_field("data", np.unpackbits(octets))
        >>> _ = d.add_derived("parity", 32 * 8)   # the outer code fills it
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

    def field_index(self, name: str) -> int:
        """Index of the field called `name`, or -1 -- the one verb whose
        sentinel survives into Python, because a name that matches nothing is
        an ANSWER rather than a refusal. The one lookup that resolves a name,
        so every index-taking method keeps working and a rename can only be
        wrong once. An unnamed field is ANONYMOUS rather than named "", so the
        empty name matches nothing.

        The one lookup that resolves a name, so every index-taking entry point
        keeps working unchanged and a rename can only be wrong once. An unnamed
        field is ANONYMOUS rather than named `""`, so the empty name matches
        nothing — including a field that has no name.

        Parameters
        ----------
        name : str
            the field name.

        Returns
        -------
        int
            the index, or -1 on NULL or a name no field carries. This is the
            one verb whose -1 survives into Python: a name that matches nothing
            is an ANSWER, not a refusal, so there is nothing to raise about.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> d = FrameDesc()
        >>> d.add_field("sync", np.array([1,0,1,0,1,0,1,1,1,1,0,0], np.uint8))
        0
        >>> d.field_index("sync")
        0
        >>> d.field_index("absent")
        -1

        """

    def name_field(self, index: int, name: str) -> None:
        """Give an already-appended field a name, or clear it with "". Refuses
        a name another field already carries, because `field_index` would then
        answer with whichever it reached first. Refuses once the frame is
        built.

        Parameters
        ----------
        index : int
            the field to name.
        name : str
            the new name; truncated at `WFM_FRAME_NAME_MAX - 1`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``cannot name a field: the index is out of range, another field
            already carries the name, or the frame is already built``, with the
            return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> d = FrameDesc()
        >>> d.add_field("", np.array([1, 0, 1, 0], np.uint8))   # anonymous
        0
        >>> d.name_field(0, "payload")
        >>> d.field_index("payload")
        0

        """

    def add_derived(self, name: str, bits: int) -> int:
        """Append a named field a STAGE will fill -- a CRC trailer, a block of
        check symbols. Its producer is not named here because no stage exists
        yet when the field it derives is appended; `add_stage_over` wires it.
        Returns the new field's index; a refusal raises `ValueError`.

        A field with a declared length and no source: a CRC trailer, a block of
        check symbols. Its producer is wired by dp_frame_add_stage_over rather
        than named here, because no stage exists yet when the field it derives
        is appended — fields are ordered by POSITION and stages by APPLICATION.

        Parameters
        ----------
        name : str
            the field's name, or NULL for anonymous.
        bits : int
            its length, which its stage decides and the caller states.

        Returns
        -------
        int
            The new field's index, or -1 on NULL state, a built frame, a zero
            bits, a full description (`WFM_FRAME_MAX_FIELDS`) or a name another
            field already carries.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a derived field: the description is full or already
            built``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> d = FrameDesc()
        >>> d.add_field("payload", np.array([1, 0, 1, 0], np.uint8))
        0
        >>> d.add_derived("crc", 16)          # a stage will fill it
        1

        """

    def add_data(self, name: str, len: int) -> int:
        """Append a named DATA field: len bits a data source fills, one chunk
        per frame. Returns its index; -1 in C, `ValueError` from Python.

        The object spelling of the Field text `data:LEN`, and the same field: a
        WFM_SEQ_DATA sequence of length len, which is exactly what
        `dp_wfm_field_parse("data:LEN")` produces for a scene's or the CLI's
        frame. The description knows the field's length and never its bits: a
        transmitter draws them from its data source at each frame (a source's
        `data=`), and a CRC or outer code covering the field covers that
        frame's chunk. It is a method rather than Field text in
        dp_frame_add_field because an object takes bits, and a data field has
        none (rx-frame-description.md, D5).

        A frame draws from one data source, so a description carries at most
        one data field; a second is refused where geometry is judged, by the
        layout, as it is for every other face.

        Parameters
        ----------
        name : str
            The field's name, or NULL/"" for anonymous; a name another field
            carries is refused.
        len : int
            Bits per frame, `LEN` in `data:LEN`: from 1 to WFM_FIELD_MAX_BITS,
            the bound the Field grammar puts on the text form.

        Returns
        -------
        int
            The new field's index, or -1 if len is out of range, the
            description is full or already built, or the name is taken.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a data field: len is 0 or past the Field grammar's
            bound (WFM_FIELD_MAX_BITS), or the description is full, already
            built, or already carries the name``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc, STAGE_CRC16, Segment, Composer
        >>> d = FrameDesc()
        >>> d.add_field("sync", np.array([1, 1, 1, 0, 0, 1, 0], np.uint8))
        0
        >>> d.add_data("payload", 8)          # 8 bits of the data source a frame
        1
        >>> d.add_derived("crc", 16)
        2
        >>> d.add_stage_over(STAGE_CRC16, "payload", "crc")
        0
        >>> seg = Segment(type="bits", sps=1, modulation="bpsk", frame=d,
        ...               data=np.unpackbits(np.array([0xA5, 0x3C], np.uint8)))
        >>> len(Composer([seg]).compose())    # two frames of 7 + 8 + 16 bits
        62

        """

    def add_stage_over(
        self,
        kind: int,
        first: str,
        last: str,
        depth: int = 0,
        unit_bits: int = 0,
    ) -> int:
        """Append a stage covering `[first .. last]` BY NAME --
        `add_stage_over(STAGE_CRC16, "payload", "crc")` says what three
        integers used to. It wires a derived field's producer for you, which
        applies the invariant the layout already enforces: a field with a
        declared length and no source sitting at the end of a cover has exactly
        one possible producer. `kind` is a stage kind, as for `add_stage`.
        Returns the new stage's index; a refusal raises `ValueError`.

        The cover is the load-bearing part of the representation and this is
        the form that reads. It wires a derived field's producer for you, which
        applies the invariant the layout already enforces rather than adding
        one.

        Parameters
        ----------
        kind : int
            a stage kind — `doppler.wfm.STAGE_CRC16` and its siblings, or a
            caller's own from `STAGE_USER` up.
        first : str
            name of the first field covered.
        last : str
            name of the last field covered; may equal first.
        depth : int
            RS / interleave depth; 0 when unused.
        unit_bits : int
            interleave unit; 0 reads as 1.

        Returns
        -------
        int
            the new stage's index, or -1 on NULL, a full description, a name
            neither field carries, last before first, or once built. The Python
            binding raises `ValueError` rather than handing back the -1.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a stage: the description is full, already built, or
            names a field the description does not carry (and `last` must not
            precede `first`)``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> d = FrameDesc()
        >>> d.add_field("payload", np.array([0, 1, 1, 0, 1, 0, 0, 1], np.uint8))
        0
        >>> d.add_derived("crc", 16)
        1
        >>> d.add_stage_over(0, "payload", "crc")   # 0 = crc16
        0
        >>> d.build()
        >>> d.crc_ok(d.bits())                # its own bits are its own truth
        1

        """

    def build(self) -> None:
        """Lay out and materialise a description. Where a description is
        checked: one that cannot produce its own bits is not a frame. Separate
        from the constructor only because the description arrives over several
        calls and there is no earlier moment at which it is complete. A
        description with a data field builds too, laid out from its lengths
        with its stages proved runnable over a discarded chunk: it has no
        single frame, so bits() returns none, while deframe() and check() work.
        Raises if it is empty, unbuildable, names a stage no kernel here
        covers, or was already built.

        The point at which a description is checked, which for dp_frame_create
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

        A description with a data field (dp_frame_add_data) builds too: the
        field has no bits until a source draws them, so the description is laid
        out from its lengths and its stages are proved runnable over a chunk
        that is thrown away. It then has no single frame, so dp_frame_bits
        writes none, while dp_frame_deframe and dp_frame_check work as for any
        other -- a receiver needs the data field's length and nothing else.

        The inner encoder starts from the all-zero register on every build: a
        description describes ONE frame. A stream of CADUs sharing one register
        is a transmitter's job and lives in `dp_ccsds_tm_frame_encode`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``cannot build: the description is empty, unbuildable, names a
            stage no kernel here covers, or was already built``, with the
            return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> d.nbits                     # 13 + 16 + 16, laid out by build()
        45

        A description that cannot produce bits is not a frame, and is refused
        rather than half-built:

        >>> FrameDesc().build()
        Traceback (most recent call last):
            ...
        ValueError: cannot build: the description is empty, unbuildable, ...

        """

    def deframe(
        self,
        rx_bits: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        out: npt.NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Undo the description's stages over a received frame and hand back
        the CORRECTED bits — the layer a receiver stops short of
        (doppler#1022).

        The receive counterpart of building one, and the layer a receiver stops
        short of: `DsssBurstReceiver` and friends hand back hard and soft
        decisions for a frame's symbols and make no claim about what they mean,
        because knowing that needs a description — this one (doppler#1022).

        Returns the frame with every reversible stage undone, in place order: a
        randomiser XORed back, an outer code's repairs APPLIED, a CRC checked.
        The payload is then a slice, at dp_frame_field_off of the payload field
        — which is the caller's arithmetic because a description does not
        privilege one field over another.

        The verdict comes back as read-backs (`ok`, `units`, `checked`,
        `symbols`), not as a return value, since the return is the bits. Read
        them exactly as frame_check_t's, including the distinction that matters
        most: `checked == 0` says the description carries no reversible stage
        at all, which is a different fact from a check that failed.

        A stage with no `undo` kernel — a convolutional inner code, which a
        receiver cannot even frame-sync through — is reported as not checked
        rather than as passed.

        Parameters
        ----------
        rx_bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            Received bits, `frame_bits` of them; treated as a capture and never
            modified.
        out : npt.NDArray[np.uint8] | None
            Receives the corrected frame.

        Returns
        -------
        NDArray[np.uint8]
            Bits written — the frame's length — or 0 if the description is
            empty or either buffer is too small.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import Frame
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], dtype=np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], dtype=np.uint8)
        >>> f = Frame(sync=sync, payload=payload, crc="crc16")
        >>> rx = np.asarray(f.bits())          # a clean capture of its own frame
        >>> got = np.asarray(f.deframe(rx))
        >>> f.rx_ok, f.rx_units, f.rx_checked  # one CRC, and it passed
        (1, 1, 1)
        >>> off = f.field_off(f.field_index("payload"))   # a SLICE
        >>> bool(np.array_equal(got[off:off + 16], payload))
        True
        >>> rx[off] ^= 1                       # one bit flipped in flight
        >>> _ = f.deframe(rx)
        >>> f.rx_ok, f.rx_units                # the check notices
        (0, 1)

        """

    def deframe_max_out(self, rx_bits_len: int, /) -> int:
        """Max bits dp_frame_deframe() writes: the frame's own length.

        Size a `deframe()` buffer with this. The bound is the DESCRIPTION's,
        not the input's: a frame is as long as its fields say, so how many bits
        were received does not change how many come back.

        Parameters
        ----------
        rx_bits_len : int
            How many bits are on offer. Ignored, for the reason above; it is in
            the signature because the binding's capacity call passes the
            input's length.

        Returns
        -------
        int
            The frame's length in bits, or 0 for an empty description.
        """

    def check(
        self,
        rx_bits: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
    ) -> FrameCheck:
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

        The receive mirror of dp_frame_bits, reading the same description — so
        a transmitter and a receiver holding the same `Frame` cannot disagree
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
        rx_bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> r = d.check(d.bits(1))
        >>> r.passed, r.ok, r.units
        (1, 1, 1)

        Flip a bit the CRC covers and the verdict turns over:

        >>> rx = np.asarray(d.bits(1)).copy()
        >>> rx[d.field_off(d.field_index("payload"))] ^= 1
        >>> d.check(rx).passed
        0

        Carrying no check is NOT passing one -- both are reported, separately:

        >>> n = FrameDesc(sync=sync, payload=payload, crc="none")
        >>> n.build()
        >>> c = n.check(n.bits(1))
        >>> c.passed, c.checked
        (0, 0)

        """

    def n_fields(self) -> int:
        """Fields in the description. A `Frame` counts only the fields it was
        given -- `Frame(sync=..., payload=..., crc="crc16")` is 3 -- so read a
        field by name with `field_index`, not by position.

        Returns
        -------
        int
            How many fields the description carries.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.n_fields()          # sync, payload, crc -- no preamble was given
        3

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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> d.field_off(0), d.field_off(1), d.field_off(2)
        (0, 13, 29)

        An absent field has no index: no preamble was given, so field 0 is the
        sync word. Ask for a field by name rather than by position, and an index
        past the end is 0.

        >>> d.field_off(d.field_index("crc")), d.field_off(7)
        (29, 0)

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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> d.field_bits(0), d.field_bits(1), d.field_bits(2)
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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> d.stage_bits(0)      # payload+CRC: what crc16 covered
        32

        """

    @property
    def rx_ok(self) -> int:
        """Checks that came out good in the last deframe() -- one per CRC, one
        per outer-code codeword. `rx_ok == rx_units` is the verdict.
        """

    @property
    def rx_units(self) -> int:
        """Checks the last deframe() performed across every stage it
        reversed.
        """

    @property
    def rx_checked(self) -> int:
        """Stages the last deframe() actually reversed. 0 means the description
        carries no reversible stage at all -- which is why `rx_ok` is 0 too,
        and is a different fact from a check that failed. An FER conflating
        them scores every unprotected frame as an error.
        """

    @property
    def rx_symbols(self) -> int:
        """Symbol errors the last deframe() repaired. Margin being spent,
        visible before it is lost -- what an outer code reports and a CRC
        cannot.
        """

    @property
    def nbits(self) -> int:
        """Bits in one frame as transmitted: the assembled frame's length for
        dp_frame_create(), and the last emitting stage's output length
        (`out_bits`) after dp_frame_build(). 0 until a description is built, or
        if building fails.
        """

    @property
    def _capsule(self) -> Any:
        """The description as a `doppler.wfm.frame_desc` capsule (a borrowed
        `wfm_frame_desc_t *`), the handle a composer source's `frame=` copies
        from. Valid while this object lives.
        """

    def destroy(self) -> None:
        """Close the object; the memory goes with its last user.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this, or use the object as a context manager,
        to end its use at a definite point.

        A consumer this object's ``_capsule`` was handed to may still hold its
        pointer, and keeps this object alive while it does: the memory is freed
        when the last of them, and this object, are gone, not here (gh-2346).
        Idempotent: calling it again does nothing. Every other method raises
        ``RuntimeError`` once it has run.
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
        /,
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
    """The same frame, DEFERRED — a description a caller can extend.

    Parameters
    ----------
    preamble : NDArray[np.uint8], default ...
        Preamble bits, one per element, each 0 or 1. Omitted, there is no
        preamble. A repeated preamble is repeated in its bits:
        `field_bits("pn:31:5*4")`.
    sync : NDArray[np.uint8], default ...
        Sync-word bits, one per element, each 0 or 1. Omitted, the frame is
        unsynced.
    payload : NDArray[np.uint8], default ...
        Payload bits, one per element, each 0 or 1. Omitted, the frame carries
        none.
    crc : Literal["none", "crc16"], default "none"
        `crc16` covers the payload with a CRC-16; `none` adds no check.
        One of ``"none"``, ``"crc16"``.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``frame geometry is
        empty, or a field holds an element that is not a bit (0 or 1)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.wfm import FrameDesc, STAGE_CRC16
    >>> d = FrameDesc()                             # begin from nothing
    >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)  # Barker-13
    >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
    >>> d.add_field("sync", sync)                   # returns its index
    0
    >>> d.add_field("payload", payload)
    1
    >>> d.add_derived("crc", 16)                    # a stage will fill it
    2
    >>> d.add_stage_over(STAGE_CRC16, "payload", "crc")
    0
    >>> d.build()
    >>> d.nbits                                     # 13 + 16 + 16
    45
    >>> d.crc_ok(d.bits())        # its own bits are its own truth
    1

    """

    def __init__(
        self,
        preamble: npt.NDArray[np.uint8] | bytes | bytearray | memoryview = ...,
        sync: npt.NDArray[np.uint8] | bytes | bytearray | memoryview = ...,
        payload: npt.NDArray[np.uint8] | bytes | bytearray | memoryview = ...,
        crc: Literal["none", "crc16"] = "none",
    ) -> None: ...
    def bits(
        self,
        count: int = 1,
        out: npt.NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Materialise n consecutive frames, one bit per byte.

        n counts FRAMES, not bits: a descriptor describes one frame, and a
        capture holds many. It is the truth for a transmitter that sends the
        same frame n times -- a data source of n copies of the payload, one
        chunk a frame -- so a stream compared against this lines up with the
        one that was transmitted.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[np.uint8] | None
            Output, one bit per byte.

        Returns
        -------
        NDArray[np.uint8]
            Bits written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> len(d.bits())        # one frame: 13 + 16 + 16
        45
        >>> len(d.bits(2))       # n counts FRAMES, tiled the way a capture is
        90

        """

    def bits_max_out(self, n: int, /) -> int:
        """Bits dp_frame_bits will write for n frames — `n * nbits`.

        Parameters
        ----------
        n : int
            Frame repetitions.

        Returns
        -------
        int
            `n * nbits`, the output capacity in bits (one per byte) that
            dp_frame_bits() needs; 0 for a NULL state or an unbuilt
            description.
        """

    def crc_ok(
        self,
        rx_bits: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
    ) -> int:
        """Check one received frame's CRC.

        **This is what makes a truth-free frame error rate possible.** It needs
        no payload truth at all, so it works on a real capture, and unlike a
        self-referenced EVM or a blind M2M4 it still catches a false lock — a
        rotated constellation fails the check rather than looking clean.

        Parameters
        ----------
        rx_bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> d.crc_ok(d.bits())           # its own bits are its own truth
        1
        >>> rx = np.asarray(d.bits()).copy()
        >>> rx[d.field_off(d.field_index("payload"))] ^= 1   # one payload bit
        >>> d.crc_ok(rx)
        0

        """

    def add_field(
        self,
        name: str,
        bits: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
    ) -> int:
        """Append one named field to a description (see `FrameDesc`): bits and
        nothing else, one per element, each 0 or 1. Text reaches it through
        `field_bits()`, hex and packed octets through `cvt`. A field a STAGE
        fills is `add_derived` instead, because the caller has no bits for it.
        `name` may be empty for an anonymous field; a name another field
        carries is refused. Returns the new field's index. Refuses once the
        frame is built.

        The field is bits and nothing else, copied here so the description
        outlives the call. A field a STAGE fills is appended with
        dp_frame_add_derived instead, because the caller has no bits for it.

        Parameters
        ----------
        name : str
            The field's name, or NULL/"" for anonymous; a name another field
            carries is refused.
        bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            The bits, one per element, each 0 or 1.

        Returns
        -------
        int
            The new field's index, or -1 if the description is full or already
            built, the name is taken, or an element is not a bit.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a field: the description is full or already built,
            the name is taken, the bits are empty, or an element is not a
            bit``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> from doppler.ccsds import asm_bits
        >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
        ...                   np.uint8)
        >>> d = FrameDesc()                      # begin from nothing
        >>> d.add_field("asm", asm_bits())       # the attached sync marker
        0
        >>> d.add_field("data", np.unpackbits(octets))   # the transfer frame
        1
        >>> d.field_index("data")
        1

        """

    def add_stage(
        self,
        kind: int = 0,
        first_field: int = 0,
        n_fields: int = 0,
        depth: int = 0,
        emit_num: int = 0,
        emit_den: int = 0,
        unit_bits: int = 0,
    ) -> int:
        """Append one transform and -- the load-bearing part -- the span of
        fields it covers. `kind` is a stage kind: `STAGE_CRC16`, `STAGE_RS`,
        `STAGE_RANDOMISE`, `STAGE_CONV`, `STAGE_INTERLEAVE` from `doppler.wfm`,
        or a caller's own from `STAGE_USER` up. It stays an INT rather than a
        name because the kind is an open `uint32_t` a caller extends -- the
        constants are generated from the C enum, so there is nothing to
        transcribe. `n_fields = 0` means the stage does not run. A stage that
        inherited whatever ran before it is the representation that cannot
        express a CCSDS CADU, where the marker is covered by the inner code and
        by neither the outer code nor the randomiser. `unit_bits` applies to
        `interleave` alone and is the bits per permuted unit (0 reads as 1);
        its ROW count is `depth` and its column count is derived from the span
        the stage covers.

        n_fields is the load-bearing part and 0 means the stage does not run. A
        stage that inherited "everything before me" instead of declaring its
        cover is the representation that cannot express a CCSDS CADU — see
        `wfm/wfm_frame.h`.

        Parameters
        ----------
        kind : int
            stage kind: a wfm_stage_kind_t value (0=crc16…4=interleave), or a
            caller's own from `WFM_STAGE_USER` (0x1000) up, whose kernel then
            has to reach the assembler through its ops table.
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
        unit_bits : int
            INTERLEAVE only: bits per interleaved unit; 0 reads as 1. Match it
            to the outer code's symbol — permuting octets is what spreads a
            burst across the codewords of a code over GF(256), and permuting
            bits inside one spreads a burst within a symbol that is already
            wrong.

        Returns
        -------
        int
            The new stage's index, or -1 if the description is full or already
            built. The Python binding raises `ValueError` rather than handing
            back the -1.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a stage: the description is full or already
            built``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> from doppler.ccsds import asm_bits
        >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
        ...                   np.uint8)
        >>> d = FrameDesc()
        >>> _ = d.add_field("asm", asm_bits())
        >>> _ = d.add_field("data", np.unpackbits(octets))
        >>> _ = d.add_derived("parity", 32 * 8)   # the outer code fills it
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

    def field_index(self, name: str) -> int:
        """Index of the field called `name`, or -1 -- the one verb whose
        sentinel survives into Python, because a name that matches nothing is
        an ANSWER rather than a refusal. The one lookup that resolves a name,
        so every index-taking method keeps working and a rename can only be
        wrong once. An unnamed field is ANONYMOUS rather than named "", so the
        empty name matches nothing.

        The one lookup that resolves a name, so every index-taking entry point
        keeps working unchanged and a rename can only be wrong once. An unnamed
        field is ANONYMOUS rather than named `""`, so the empty name matches
        nothing — including a field that has no name.

        Parameters
        ----------
        name : str
            the field name.

        Returns
        -------
        int
            the index, or -1 on NULL or a name no field carries. This is the
            one verb whose -1 survives into Python: a name that matches nothing
            is an ANSWER, not a refusal, so there is nothing to raise about.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> d = FrameDesc()
        >>> d.add_field("sync", np.array([1,0,1,0,1,0,1,1,1,1,0,0], np.uint8))
        0
        >>> d.field_index("sync")
        0
        >>> d.field_index("absent")
        -1

        """

    def name_field(self, index: int, name: str) -> None:
        """Give an already-appended field a name, or clear it with "". Refuses
        a name another field already carries, because `field_index` would then
        answer with whichever it reached first. Refuses once the frame is
        built.

        Parameters
        ----------
        index : int
            the field to name.
        name : str
            the new name; truncated at `WFM_FRAME_NAME_MAX - 1`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``cannot name a field: the index is out of range, another field
            already carries the name, or the frame is already built``, with the
            return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> d = FrameDesc()
        >>> d.add_field("", np.array([1, 0, 1, 0], np.uint8))   # anonymous
        0
        >>> d.name_field(0, "payload")
        >>> d.field_index("payload")
        0

        """

    def add_derived(self, name: str, bits: int) -> int:
        """Append a named field a STAGE will fill -- a CRC trailer, a block of
        check symbols. Its producer is not named here because no stage exists
        yet when the field it derives is appended; `add_stage_over` wires it.
        Returns the new field's index; a refusal raises `ValueError`.

        A field with a declared length and no source: a CRC trailer, a block of
        check symbols. Its producer is wired by dp_frame_add_stage_over rather
        than named here, because no stage exists yet when the field it derives
        is appended — fields are ordered by POSITION and stages by APPLICATION.

        Parameters
        ----------
        name : str
            the field's name, or NULL for anonymous.
        bits : int
            its length, which its stage decides and the caller states.

        Returns
        -------
        int
            The new field's index, or -1 on NULL state, a built frame, a zero
            bits, a full description (`WFM_FRAME_MAX_FIELDS`) or a name another
            field already carries.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a derived field: the description is full or already
            built``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> d = FrameDesc()
        >>> d.add_field("payload", np.array([1, 0, 1, 0], np.uint8))
        0
        >>> d.add_derived("crc", 16)          # a stage will fill it
        1

        """

    def add_data(self, name: str, len: int) -> int:
        """Append a named DATA field: len bits a data source fills, one chunk
        per frame. Returns its index; -1 in C, `ValueError` from Python.

        The object spelling of the Field text `data:LEN`, and the same field: a
        WFM_SEQ_DATA sequence of length len, which is exactly what
        `dp_wfm_field_parse("data:LEN")` produces for a scene's or the CLI's
        frame. The description knows the field's length and never its bits: a
        transmitter draws them from its data source at each frame (a source's
        `data=`), and a CRC or outer code covering the field covers that
        frame's chunk. It is a method rather than Field text in
        dp_frame_add_field because an object takes bits, and a data field has
        none (rx-frame-description.md, D5).

        A frame draws from one data source, so a description carries at most
        one data field; a second is refused where geometry is judged, by the
        layout, as it is for every other face.

        Parameters
        ----------
        name : str
            The field's name, or NULL/"" for anonymous; a name another field
            carries is refused.
        len : int
            Bits per frame, `LEN` in `data:LEN`: from 1 to WFM_FIELD_MAX_BITS,
            the bound the Field grammar puts on the text form.

        Returns
        -------
        int
            The new field's index, or -1 if len is out of range, the
            description is full or already built, or the name is taken.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a data field: len is 0 or past the Field grammar's
            bound (WFM_FIELD_MAX_BITS), or the description is full, already
            built, or already carries the name``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc, STAGE_CRC16, Segment, Composer
        >>> d = FrameDesc()
        >>> d.add_field("sync", np.array([1, 1, 1, 0, 0, 1, 0], np.uint8))
        0
        >>> d.add_data("payload", 8)          # 8 bits of the data source a frame
        1
        >>> d.add_derived("crc", 16)
        2
        >>> d.add_stage_over(STAGE_CRC16, "payload", "crc")
        0
        >>> seg = Segment(type="bits", sps=1, modulation="bpsk", frame=d,
        ...               data=np.unpackbits(np.array([0xA5, 0x3C], np.uint8)))
        >>> len(Composer([seg]).compose())    # two frames of 7 + 8 + 16 bits
        62

        """

    def add_stage_over(
        self,
        kind: int,
        first: str,
        last: str,
        depth: int = 0,
        unit_bits: int = 0,
    ) -> int:
        """Append a stage covering `[first .. last]` BY NAME --
        `add_stage_over(STAGE_CRC16, "payload", "crc")` says what three
        integers used to. It wires a derived field's producer for you, which
        applies the invariant the layout already enforces: a field with a
        declared length and no source sitting at the end of a cover has exactly
        one possible producer. `kind` is a stage kind, as for `add_stage`.
        Returns the new stage's index; a refusal raises `ValueError`.

        The cover is the load-bearing part of the representation and this is
        the form that reads. It wires a derived field's producer for you, which
        applies the invariant the layout already enforces rather than adding
        one.

        Parameters
        ----------
        kind : int
            a stage kind — `doppler.wfm.STAGE_CRC16` and its siblings, or a
            caller's own from `STAGE_USER` up.
        first : str
            name of the first field covered.
        last : str
            name of the last field covered; may equal first.
        depth : int
            RS / interleave depth; 0 when unused.
        unit_bits : int
            interleave unit; 0 reads as 1.

        Returns
        -------
        int
            the new stage's index, or -1 on NULL, a full description, a name
            neither field carries, last before first, or once built. The Python
            binding raises `ValueError` rather than handing back the -1.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a stage: the description is full, already built, or
            names a field the description does not carry (and `last` must not
            precede `first`)``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> d = FrameDesc()
        >>> d.add_field("payload", np.array([0, 1, 1, 0, 1, 0, 0, 1], np.uint8))
        0
        >>> d.add_derived("crc", 16)
        1
        >>> d.add_stage_over(0, "payload", "crc")   # 0 = crc16
        0
        >>> d.build()
        >>> d.crc_ok(d.bits())                # its own bits are its own truth
        1

        """

    def build(self) -> None:
        """Lay out and materialise a description. Where a description is
        checked: one that cannot produce its own bits is not a frame. Separate
        from the constructor only because the description arrives over several
        calls and there is no earlier moment at which it is complete. A
        description with a data field builds too, laid out from its lengths
        with its stages proved runnable over a discarded chunk: it has no
        single frame, so bits() returns none, while deframe() and check() work.
        Raises if it is empty, unbuildable, names a stage no kernel here
        covers, or was already built.

        The point at which a description is checked, which for dp_frame_create
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

        A description with a data field (dp_frame_add_data) builds too: the
        field has no bits until a source draws them, so the description is laid
        out from its lengths and its stages are proved runnable over a chunk
        that is thrown away. It then has no single frame, so dp_frame_bits
        writes none, while dp_frame_deframe and dp_frame_check work as for any
        other -- a receiver needs the data field's length and nothing else.

        The inner encoder starts from the all-zero register on every build: a
        description describes ONE frame. A stream of CADUs sharing one register
        is a transmitter's job and lives in `dp_ccsds_tm_frame_encode`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``cannot build: the description is empty, unbuildable, names a
            stage no kernel here covers, or was already built``, with the
            return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> d.nbits                     # 13 + 16 + 16, laid out by build()
        45

        A description that cannot produce bits is not a frame, and is refused
        rather than half-built:

        >>> FrameDesc().build()
        Traceback (most recent call last):
            ...
        ValueError: cannot build: the description is empty, unbuildable, ...

        """

    def deframe(
        self,
        rx_bits: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        out: npt.NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Undo the description's stages over a received frame and hand back
        the CORRECTED bits — the layer a receiver stops short of
        (doppler#1022).

        The receive counterpart of building one, and the layer a receiver stops
        short of: `DsssBurstReceiver` and friends hand back hard and soft
        decisions for a frame's symbols and make no claim about what they mean,
        because knowing that needs a description — this one (doppler#1022).

        Returns the frame with every reversible stage undone, in place order: a
        randomiser XORed back, an outer code's repairs APPLIED, a CRC checked.
        The payload is then a slice, at dp_frame_field_off of the payload field
        — which is the caller's arithmetic because a description does not
        privilege one field over another.

        The verdict comes back as read-backs (`ok`, `units`, `checked`,
        `symbols`), not as a return value, since the return is the bits. Read
        them exactly as frame_check_t's, including the distinction that matters
        most: `checked == 0` says the description carries no reversible stage
        at all, which is a different fact from a check that failed.

        A stage with no `undo` kernel — a convolutional inner code, which a
        receiver cannot even frame-sync through — is reported as not checked
        rather than as passed.

        Parameters
        ----------
        rx_bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            Received bits, `frame_bits` of them; treated as a capture and never
            modified.
        out : npt.NDArray[np.uint8] | None
            Receives the corrected frame.

        Returns
        -------
        NDArray[np.uint8]
            Bits written — the frame's length — or 0 if the description is
            empty or either buffer is too small.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import Frame
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], dtype=np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], dtype=np.uint8)
        >>> f = Frame(sync=sync, payload=payload, crc="crc16")
        >>> rx = np.asarray(f.bits())          # a clean capture of its own frame
        >>> got = np.asarray(f.deframe(rx))
        >>> f.rx_ok, f.rx_units, f.rx_checked  # one CRC, and it passed
        (1, 1, 1)
        >>> off = f.field_off(f.field_index("payload"))   # a SLICE
        >>> bool(np.array_equal(got[off:off + 16], payload))
        True
        >>> rx[off] ^= 1                       # one bit flipped in flight
        >>> _ = f.deframe(rx)
        >>> f.rx_ok, f.rx_units                # the check notices
        (0, 1)

        """

    def deframe_max_out(self, rx_bits_len: int, /) -> int:
        """Max bits dp_frame_deframe() writes: the frame's own length.

        Size a `deframe()` buffer with this. The bound is the DESCRIPTION's,
        not the input's: a frame is as long as its fields say, so how many bits
        were received does not change how many come back.

        Parameters
        ----------
        rx_bits_len : int
            How many bits are on offer. Ignored, for the reason above; it is in
            the signature because the binding's capacity call passes the
            input's length.

        Returns
        -------
        int
            The frame's length in bits, or 0 for an empty description.
        """

    def check(
        self,
        rx_bits: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
    ) -> FrameCheck:
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

        The receive mirror of dp_frame_bits, reading the same description — so
        a transmitter and a receiver holding the same `Frame` cannot disagree
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
        rx_bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> r = d.check(d.bits(1))
        >>> r.passed, r.ok, r.units
        (1, 1, 1)

        Flip a bit the CRC covers and the verdict turns over:

        >>> rx = np.asarray(d.bits(1)).copy()
        >>> rx[d.field_off(d.field_index("payload"))] ^= 1
        >>> d.check(rx).passed
        0

        Carrying no check is NOT passing one -- both are reported, separately:

        >>> n = FrameDesc(sync=sync, payload=payload, crc="none")
        >>> n.build()
        >>> c = n.check(n.bits(1))
        >>> c.passed, c.checked
        (0, 0)

        """

    def n_fields(self) -> int:
        """Fields in the description. A `Frame` counts only the fields it was
        given -- `Frame(sync=..., payload=..., crc="crc16")` is 3 -- so read a
        field by name with `field_index`, not by position.

        Returns
        -------
        int
            How many fields the description carries.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wfm import FrameDesc
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.n_fields()          # sync, payload, crc -- no preamble was given
        3

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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> d.field_off(0), d.field_off(1), d.field_off(2)
        (0, 13, 29)

        An absent field has no index: no preamble was given, so field 0 is the
        sync word. Ask for a field by name rather than by position, and an index
        past the end is 0.

        >>> d.field_off(d.field_index("crc")), d.field_off(7)
        (29, 0)

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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> d.field_bits(0), d.field_bits(1), d.field_bits(2)
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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
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
        >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
        >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
        >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
        >>> d.build()
        >>> d.stage_bits(0)      # payload+CRC: what crc16 covered
        32

        """

    @property
    def rx_ok(self) -> int:
        """Checks that came out good in the last deframe() -- one per CRC, one
        per outer-code codeword. `rx_ok == rx_units` is the verdict.
        """

    @property
    def rx_units(self) -> int:
        """Checks the last deframe() performed across every stage it
        reversed.
        """

    @property
    def rx_checked(self) -> int:
        """Stages the last deframe() actually reversed. 0 means the description
        carries no reversible stage at all -- which is why `rx_ok` is 0 too,
        and is a different fact from a check that failed. An FER conflating
        them scores every unprotected frame as an error.
        """

    @property
    def rx_symbols(self) -> int:
        """Symbol errors the last deframe() repaired. Margin being spent,
        visible before it is lost -- what an outer code reports and a CRC
        cannot.
        """

    @property
    def nbits(self) -> int:
        """Bits in one frame as transmitted: the assembled frame's length for
        dp_frame_create(), and the last emitting stage's output length
        (`out_bits`) after dp_frame_build(). 0 until a description is built, or
        if building fails.
        """

    @property
    def _capsule(self) -> Any:
        """The description as a `doppler.wfm.frame_desc` capsule (a borrowed
        `wfm_frame_desc_t *`), the handle a composer source's `frame=` copies
        from. Valid while this object lives.
        """

    def destroy(self) -> None:
        """Close the object; the memory goes with its last user.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this, or use the object as a context manager,
        to end its use at a definite point.

        A consumer this object's ``_capsule`` was handed to may still hold its
        pointer, and keeps this object alive while it does: the memory is freed
        when the last of them, and this object, are gone, not here (gh-2346).
        Idempotent: calling it again does nothing. Every other method raises
        ``RuntimeError`` once it has run.
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
        /,
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

def bpsk_map(bits: npt.NDArray[np.uint8] | bytes | bytearray | memoryview) -> NDArray[np.complex64]:
    """Map bits {0,1} to BPSK symbols {+1,-1} (cf32).

    Parameters
    ----------
    bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
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

def qpsk_map(syms: npt.NDArray[np.uint8] | bytes | bytearray | memoryview) -> NDArray[np.complex64]:
    """Map QPSK symbol indices {0,1,2,3} to Gray-coded symbols (cf32).

    Parameters
    ----------
    syms : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
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
    """Maximal-length-sequence primitive polynomial for an LFSR of length n.

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

def crc16(bits: npt.NDArray[np.uint8] | bytes | bytearray | memoryview) -> int:
    """CRC-16-CCITT (poly 0x1021, init 0xFFFF) over an unpacked 0/1 bit array,
    MSB-first — the DSSS burst frame trailer wfmgen appends and BurstDemod
    validates.

    Parameters
    ----------
    bits : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
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

def rrc_h(t: npt.NDArray[np.float64], beta: float) -> NDArray[np.float64]:
    """Analytic root-raised-cosine pulse at arbitrary (non-grid) times `t`, in
    symbol periods. The transmit half of a matched-filter pair. Use this, not a
    transcription of the formula, whenever a stimulus needs the pulse off the
    integer sample grid — a non-integer samples-per-symbol or a fractional
    timing offset has no grid to sample. `rrc_taps` remains the right call for
    filter taps.

    Evaluates the header-inline wfm_rrc_h() at each `t[i]`: the
    unnormalized RRC impulse response (peak `1 - beta + 4*beta/pi` at t =
    0), with the t = 0 and t = +-1/(4*beta) singularities replaced by their
    closed-form limits. Use it when the pulse is needed off the integer
    sample grid; dp_rrc_taps() remains the call for filter taps. If beta is
    outside 0 to 1 (or NaN) the output is zeroed instead; no error is
    raised.

    Parameters
    ----------
    t : npt.NDArray[np.float64]
        Times in symbol periods, relative to the pulse center.
    beta : float
        Roll-off factor, 0 to 1.

    Returns
    -------
    NDArray[np.float64]
        Output.
    """

def rc_h(t: npt.NDArray[np.float64], beta: float) -> NDArray[np.float64]:
    """Analytic full raised-cosine pulse at arbitrary (non-grid) times `t`, in
    symbol periods. Already the Nyquist response a matched TX/RX pair produces,
    so this is what models the matched-filter OUTPUT directly — a
    timing-detector S-curve reference, or a receiver test with its front end
    collapsed away.

    Evaluates the header-inline wfm_rc_h() at each `t[i]`: the full raised
    cosine, normalized to g(0) = 1 and zero at every nonzero integer t,
    i.e. the matched-filter output a TX/RX RRC pair delivers at timing
    offset t. The t = +-1/(2*beta) singularity uses its closed-form limit.
    If beta is outside 0 to 1 (or NaN) the output is zeroed instead; no
    error is raised.

    Parameters
    ----------
    t : npt.NDArray[np.float64]
        Times in symbol periods, relative to the pulse center.
    beta : float
        Roll-off factor, 0 to 1.

    Returns
    -------
    NDArray[np.float64]
        Output.
    """

def field_bits(spec: str) -> NDArray[np.uint8]:
    """A Field's bits from its text form: `0101`, `0x1ACFFC1D`,
    `pn:LEN:REG[:SEED[:POLY]][:galois|fibonacci]`, `gold:LEN:REG:TA:SA:TB:SB`,
    `dotted:LEN`, each optionally `*REPS`. The one door from text to the bits
    every frame object takes, over the one C parser
    (docs/design/frame-description.md §F.1). Text outside the grammar raises.

    The grammar is docs/design/frame-description.md §F.1, read once by
    dp_wfm_field_parse and rendered once by dp_wfm_field_render, both
    inside dp_wfm_field_bits; this is that call with the output sized to
    fit. `0101` and `0x1ACFFC1D` are literals, `pn:LEN:REG[:SEED
    [:POLY]][:galois|fibonacci]`, `gold:LEN:REG:TA:SA:TB:SB` and
    `dotted:LEN` are generated, and any of them takes `*REPS`. Text outside
    the grammar is refused, never repaired: `pn::10`, `12abc` and `0102`
    all raise.

    Parameters
    ----------
    spec : str
        NUL-terminated Field text.

    Returns
    -------
    NDArray[np.uint8]
        the bits written, or 0 on refusal (the binding raises).

    Examples
    --------
    >>> from doppler.wfm import field_bits
    >>> field_bits("0x1A").tolist()          # hex, MSB first
    [0, 0, 0, 1, 1, 0, 1, 0]
    >>> field_bits("dotted:4*2").tolist()    # a repeat is the same bits again
    [1, 0, 1, 0, 1, 0, 1, 0]
    >>> len(field_bits("pn:1023:10"))        # an m-sequence, 10-bit register
    1023
    >>> field_bits("pn::10")
    Traceback (most recent call last):
        ...
    ValueError: LEN, the output length in bits, must be a number > 0

    """

def rrc_taps(beta: float, sps: int, span: int) -> NDArray[np.float32]:
    """Root-raised-cosine pulse-shaping taps (2*span*sps+1 unit-energy real
    float32 taps).

    The wfm-namespace alias of dp_wfm_rrc_taps(): fills out with
    `2*span*sps + 1` real taps, symmetric about the center tap and scaled
    so the sum of squares is 1 (cascading TX and RX gives a Nyquist raised
    cosine). If sps or span is below 1 the filter is refused and the
    `2*span*sps + 1` output slots are zeroed instead; no error is raised.

    Parameters
    ----------
    beta : float
        Roll-off factor, 0 to 1.
    sps : int
        Samples per symbol (>= 1).
    span : int
        One-sided filter span in symbols (>= 1).

    Returns
    -------
    NDArray[np.float32]
        Output.
    """

def _sweep_ends(
    fs: float,
    f_mid: float,
    rate: float,
    n: int,
    center: float,
) -> NDArray[np.float64]:
    """Start and end frequency of the chirp that sweeps about a center.

    A chirp from `f0` to `f_end` pinned over `n` samples has instantaneous
    frequency `f_mid + rate*u` Hz at sample `i`, `u = (i - center)/fs`
    seconds, so it multiplies a block by `exp(j*2*pi*(f_mid*u +
    rate*u**2/2))` up to one constant phase. This is the single copy of
    that arithmetic (dp_wfm_synth_sweep_ends()); `doppler.wfm.rotate` takes
    it from here. Private: bound as `doppler.wfm._sweep_ends`. Pure
    arithmetic: no state, nothing allocated.

    Parameters
    ----------
    fs : float
        Sample rate, Hz (> 0).
    f_mid : float
        Frequency at the center, Hz.
    rate : float
        Sweep rate, Hz per second.
    n : int
        Samples the sweep spans.
    center : float
        Sample index of the sweep's center (need not be an integer).

    Returns
    -------
    NDArray[np.float64]
        Output.

    Examples
    --------
    >>> from doppler.wfm import _sweep_ends
    >>> f0, f_end = _sweep_ends(1.0, 0.0, 1e-4, 1001, 500.0)
    >>> round(float(f0), 6), round(float(f_end), 6)
    (-0.04995, 0.05015)

    """

def dsss_spread(
    syms: npt.NDArray[np.complex64],
    code: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
    sf: int,
) -> NDArray[np.complex64]:
    """Direct-sequence spread syms by the ±1 chip code; yields len(syms)*sf
    chips.

    The wfm-namespace alias of dp_wfm_dsss_spread(): `out[i*sf + j] =
    syms[i] * (code[j] ? -1 : +1)`, so each symbol is repeated over sf
    chips with a per-chip sign flip. Only the first sf chips of code are
    used. If sf < 1 or code_len < sf the spread is refused and the output
    is zeroed instead (`syms_len * sf` slots, none when sf < 1); no error
    is raised.

    Parameters
    ----------
    syms : npt.NDArray[np.complex64]
        Complex data symbols.
    code : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
        Chip code, one chip per byte, 0 or 1.
    sf : int
        Spreading factor, chips per symbol (>= 1).

    Returns
    -------
    NDArray[np.complex64]
        Output.
    """
