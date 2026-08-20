# coding/coding.pyi — type stubs for the coding C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

@final
class ConvEncoder:
    """Build an encoder for the code the polynomials describe.

    Parameters
    ----------
    poly : NDArray[np.uint32]
        Generator polynomials, one per output. The array IS the code;
        `poly_len` gives `n`.
    k : int, default 7
        Constraint length, 2 to `CONV_K_MAX`.
    invert : int, default 0
        Bit `j` complements output `j`.

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``ConvEncoder: not a
        usable code (need 1 to 6 non-zero polynomials, each under 2**k, and 2
        <= k <= 9)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.coding import ConvEncoder
    >>> e = ConvEncoder([0o171, 0o133], k=7, invert=0x2)
    >>> e.encode(np.zeros(8, dtype=np.uint8)).size
    16

    """
    def __init__(
        self,
        poly: NDArray[np.uint32],
        k: int = ...,
        invert: int = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Return the register to all-zero, keeping the code.

        The boundary between two independent records, not a reconfiguration.
        The next encode starts from the same state a freshly created encoder is
        in, which is what makes a reset stream byte-identical to a fresh one.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.coding import ConvEncoder
        >>> e = ConvEncoder([0o171, 0o133], k=7)
        >>> e.reset()

        """

    def encode(
        self,
        x: NDArray[np.uint8],
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Encode information bits into channel symbols.

        The register carries across calls, so a long record may be fed in
        blocks and the symbol sequence is identical to one call — which is the
        property a standard fixes and a chunked encoder silently breaks.

        Outputs are emitted in polynomial order per input bit: for `[G1, G2]`,
        `out[2i]` is `G1`'s symbol for input bit `i` and `out[2i+1]` is `G2`'s.

        Parameters
        ----------
        x : NDArray[np.uint8]
            Input.
        out : NDArray[np.uint8] | None
            Receives `n_in * n` unpacked symbols, one per byte.

        Returns
        -------
        NDArray[np.uint8]
            Symbols written, or 0 if max_out is too small — in which case out
            is untouched.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.coding import ConvEncoder, Viterbi
        >>> bits = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 40, dtype=np.uint8)
        >>> sym = ConvEncoder([0o171, 0o133], k=7).encode(bits)
        >>> llr = np.where(sym, -8.0, 8.0).astype(np.float32)
        >>> out = Viterbi([0o171, 0o133], k=7, depth=35).decode(llr)
        >>> bool(np.array_equal(out, bits[: out.size]))
        True

        """

    def encode_max_out(self, n_in: int) -> int:
        """Symbols conv_enc_encode writes for n_in input bits.

        Exactly `n_in * n` — a convolutional code has no fill and no latency on

        the encode side, which is the asymmetry with viterbi_decode_max_out,

        where the traceback still owes bits at the start of a stream.

        Parameters
        ----------
        n_in : int
            Number of input bits.

        Returns
        -------
        int
            Symbols that call will write.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the ConvEncoder has already been destroyed.

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

        Raises ``RuntimeError`` if the ConvEncoder has already been destroyed.

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
        ``RuntimeError`` if the ConvEncoder has already been destroyed.

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


    def __enter__(self) -> "ConvEncoder":
        """Enter a context manager, returning this object.

        Lets a ConvEncoder be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        ConvEncoder
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the ConvEncoder.

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
class Viterbi:
    """Build a decoder for the code the polynomials describe.

    Parameters
    ----------
    poly : NDArray[np.uint32]
        Generator polynomials, one per output. The array IS the code;
        `poly_len` gives `n`.
    k : int, default 7
        k (default: 7).
    invert : int, default 0
        invert (default: 0).
    depth : int, default 35
        depth (default: 35).

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``Viterbi: not a usable
        code (need 1 to 6 non-zero polynomials, each under 2**k, 2 <= k <= 9,
        and depth >= 1)``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.coding import Viterbi
    >>> v = Viterbi([0o171, 0o133], k=7, depth=35)
    >>> v.decode(np.zeros(8, dtype=np.float32)).dtype
    dtype('uint8')

    """
    def __init__(
        self,
        poly: NDArray[np.uint32],
        k: int = ...,
        invert: int = ...,
        depth: int = ...,
    ) -> None: ...

    def reset(self) -> None:
        """Return to the all-zero start state, discarding the traceback.

        The code and the depth are unchanged — this is the boundary between two
        independent captures, not a reconfiguration. The next decode refills
        the traceback before it emits, exactly as after create, and the
        all-zero state is given the winning metric, matching an encoder that
        starts from a reset register.

        Examples
        --------
        >>> from doppler.coding import Viterbi
        >>> v = Viterbi([0o171, 0o133], k=7, depth=35)
        >>> v.reset()

        """

    def decode(
        self,
        x: NDArray[np.float32],
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Decode soft channel symbols into information bits.

        The input carries one value per channel symbol, in the convention
        `mpsk_soft_demap` produces: `L = log(P(0)/P(1))`, so **positive means
        symbol 0**. The branch metric for an expected symbol e is `+L` when `e
        == 0` and `-L` otherwise, and the survivor maximises the sum — which
        makes the decoder agree with `mpsk_demap` on hard decisions by
        construction rather than by a second convention.

        A maximum-likelihood path cannot move when every metric is scaled by a
        positive constant, so **the LLRs need no accurate scaling** — a caller
        with no SNR estimate may pass unscaled values.

        Streaming: state carries across calls, so a long capture may be fed in
        blocks and the bits come out continuously. The first `depth - 1`
        branches of a stream produce no output — the traceback walks `depth -
        1` steps back, so a decision needs that many branches BEHIND it — and
        thereafter one bit is emitted per `n` symbols consumed.
        viterbi_decode_max_out is the same statement as arithmetic, and is what
        a caller should size a buffer with rather than repeating this sentence:
        they disagreed by one until a test asserted the count against a
        literal.

        Parameters
        ----------
        x : NDArray[np.float32]
            Input.
        out : NDArray[np.uint8] | None
            Receives the decoded information bits, one per byte.

        Returns
        -------
        NDArray[np.uint8]
            Bits written, which may be 0 while the traceback fills.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.coding import Viterbi
        >>> v = Viterbi([0o171, 0o133], k=7, depth=35)
        >>> llr = np.array([2.0, -2.0] * 128, dtype=np.float32)
        >>> bits = v.decode(llr)
        >>> set(np.unique(bits)) <= {0, 1}
        True

        """

    def decode_max_out(self, n_in: int) -> int:
        """Bits viterbi_decode will emit for n_in soft symbols.

        Accounts for the fill still owed at the start of a stream, so a caller
        can

        size a buffer exactly rather than conservatively.

        Parameters
        ----------
        n_in : int
            Number of soft symbols the next call would be given.

        Returns
        -------
        int
            Bits that call would write.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Viterbi has already been destroyed.

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

        Raises ``RuntimeError`` if the Viterbi has already been destroyed.

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
        ``RuntimeError`` if the Viterbi has already been destroyed.

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


    def __enter__(self) -> "Viterbi":
        """Enter a context manager, returning this object.

        Lets a Viterbi be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Viterbi
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the Viterbi.

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
class ReedSolomon:
    """Create a codec for the code named by the five arguments.

    Parameters
    ----------
    nroots : int
        Parity symbols per codeword, `2E` — even, at least 2, and small enough
        to leave one information symbol. The code corrects `E = nroots / 2`
        symbol errors.
    symbol_bits : int, default 8
        `J`, the symbol width in bits, 2..8. A codeword is `n = 2**J - 1`
        symbols, one per byte, so `J = 8` gives the familiar 255.
    field_poly : int, default 29
        `F(x)`, low `J` bits, with `x**J` implicit. Must be PRIMITIVE — a
        polynomial that generates a subgroup instead of the field produces
        perfectly self-consistent arithmetic that interoperates with nothing,
        so the constructor checks rather than trusts. The default 29 is `x**8 +
        x**4 + x**3 + x**2 + 1`.
    first_root : int, default 1
        `j0`: the generator's first root is `a**(root_stride * j0)`.
    root_stride : int, default 1
        `s`: the roots are powers of `a**s`. Must be coprime with `n`, or the
        `nroots` roots are not distinct and the code corrects fewer errors than
        its parity count claims (CCSDS 4.3.4 states this as a note about
        `a**11`; for a general code it is a condition, and the constructor
        checks it).

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``ReedSolomon: not a
        usable code — need an even nroots in 2..64 leaving at least one
        information symbol, 2 <= symbol_bits <= 8, a PRIMITIVE field_poly, and
        a root_stride coprime with 2**symbol_bits - 1``.

    Examples
    --------
    >>> from doppler.coding import ReedSolomon
    >>> rs = ReedSolomon(nroots=32)      # RS(255,223) over the usual GF(256)
    >>> rs.n, rs.k, rs.e
    (255, 223, 16)
    >>> ReedSolomon(nroots=4, symbol_bits=4, field_poly=0b0011).n
    15

    """
    def __init__(
        self,
        nroots: int,
        symbol_bits: int = ...,
        field_poly: int = ...,
        first_root: int = ...,
        root_stride: int = ...,
    ) -> None: ...

    def encode(
        self,
        x: NDArray[np.uint8],
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """Encode `k` information symbols into a whole `n`-symbol codeword.

        Systematic: the information symbols are copied through untouched and
        the `nroots` parity symbols follow them, which is the order they are
        transmitted in. `rs_encode` computes the parity; this places it.

        The WHOLE codeword rather than the parity alone, because that is the
        unit every other method here takes — rs_codec_decode,
        rs_codec_syndromes and rs_codec_codeword_ok all read `n` symbols, and a
        caller who wants the parity by itself can take the last `nroots` of the
        answer. (`rs_encode` is the other split, and is still there for a frame
        assembler that has already placed the information.)

        out may alias in — `rs_codec_encode (rs, buf, k, buf, n)` appends the
        parity to a buffer that already holds the information, which is the
        call a frame assembler makes and the one `rs_encode` exists for.

        Parameters
        ----------
        x : NDArray[np.uint8]
            Input.
        out : NDArray[np.uint8] | None
            Receives `n` symbols; may be in.

        Returns
        -------
        NDArray[np.uint8]
            `n` on success, or 0 if n_in is not exactly `k` or out is too small
            — refusing rather than truncating, since a short codeword is not a
            codeword.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.coding import ReedSolomon
        >>> rs = ReedSolomon(nroots=32)
        >>> info = np.arange(rs.k, dtype=np.uint8)
        >>> word = rs.encode(info)
        >>> word.size, bool(np.array_equal(word[: rs.k], info))
        (255, True)
        >>> rs.codeword_ok(word)
        1

        """

    def encode_max_out(self, n_in: int) -> int:
        """Symbols rs_codec_encode writes for n_in information symbols: a whole
        codeword, `n`.

        Parameters
        ----------
        n_in : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def decode(self, codeword: NDArray[np.uint8]) -> int:
        """Correct up to `E` symbol errors, IN PLACE.

        `rs_decode`, over the caller's own buffer: the corrected symbols land
        in codeword itself, which is why the binding demands a writable array
        rather than quietly working on a copy the caller would then discard.

        **It either refuses or leaves a codeword.** On success the key equation
        has zeroed every syndrome by construction, so the result passes
        rs_codec_codeword_ok. On refusal codeword is untouched.

        A refusal is not the same claim as "more than `E` errors". Beyond `E` a
        bounded-distance decoder can land inside another codeword's sphere and
        miscorrect — a property of the code, not of this implementation — which
        is why this reports a COUNT rather than a verdict, and why frame-level
        accounting is the protection.

        Parameters
        ----------
        codeword : NDArray[np.uint8]
            `n` symbols, corrected in place.

        Returns
        -------
        int
            Symbols corrected, 0 for an already-valid codeword, **-1** when the
            word is too far from every codeword to name one, or **-2** when
            codeword_len is not `n`. Two negative codes rather than one because
            they are different kinds of fact: -1 is the channel's answer and -2
            is the caller's mistake.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.coding import ReedSolomon
        >>> rs = ReedSolomon(nroots=32)
        >>> word = rs.encode(np.arange(rs.k, dtype=np.uint8))
        >>> word[3] ^= 0xFF          # one symbol, however many bits it moved
        >>> word[40] ^= 0x01
        >>> rs.decode(word)          # corrected in place
        2
        >>> bool(np.array_equal(word[: rs.k], np.arange(rs.k, dtype=np.uint8)))
        True

        """

    def syndromes(
        self,
        x: NDArray[np.uint8],
        out: NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """The `nroots` syndromes of an `n`-symbol word.

        All zero is the DEFINING property of the code: it needs no encoder and
        no decoder to check, which is what makes it usable both as a test
        oracle and as a receiver's error detector. rs_codec_codeword_ok is this
        reduced to the one bit most callers want.

        Parameters
        ----------
        x : NDArray[np.uint8]
            Input.
        out : NDArray[np.uint8] | None
            Receives `nroots` syndromes.

        Returns
        -------
        NDArray[np.uint8]
            `nroots`, or 0 if n_in is not `n` or out is too small.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.coding import ReedSolomon
        >>> rs = ReedSolomon(nroots=32)
        >>> word = rs.encode(np.zeros(rs.k, dtype=np.uint8))
        >>> bool(rs.syndromes(word).any())      # a codeword has none
        False
        >>> word[7] ^= 0x20
        >>> bool(rs.syndromes(word).any())
        True

        """

    def syndromes_max_out(self, n_in: int) -> int:
        """Syndromes rs_codec_syndromes writes: `nroots`.

        Parameters
        ----------
        n_in : int
            Input.

        Returns
        -------
        int
            Output.
        """

    def codeword_ok(self, codeword: NDArray[np.uint8]) -> int:
        """Is this a valid codeword? — every syndrome zero.

        Parameters
        ----------
        codeword : NDArray[np.uint8]
            `n` symbols.

        Returns
        -------
        int
            1 when every syndrome is zero, 0 otherwise — including when
            codeword_len is not `n`, since a word of the wrong length is not a
            codeword of this code.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.coding import ReedSolomon
        >>> rs = ReedSolomon(nroots=32)
        >>> rs.codeword_ok(np.zeros(rs.n, np.uint8))   # all-zero IS a codeword
        1
        >>> rs.codeword_ok(np.zeros(rs.n - 1, np.uint8))   # at the right size
        0

        """

    def generator(self, out: NDArray[np.uint8]) -> int:
        """The `nroots + 1` coefficients of `g(x)`, `out[i]` for `x^i`.

        Exposed because standards PUBLISH them — CCSDS 131.0-B Annex G prints
        all 33 for `E = 16` — so a caller who has just configured a code from a
        document can check that they read the five numbers correctly, against
        the document rather than against this implementation.

        The caller supplies the buffer rather than being handed one, because
        the length is a property of the CODE and not of the call: `g(x)` has
        exactly `nroots + 1` coefficients and there is no other number a caller
        could ask for. A self-sizing method would carry a `count` parameter
        that means nothing, which is a worse trade than one line of allocation.

        Parameters
        ----------
        out : NDArray[np.uint8]
            Receives `nroots + 1` coefficients; `out[i]` is the coefficient of
            `x^i`, so `out[nroots]` is 1.

        Returns
        -------
        int
            `nroots + 1`, or 0 if out is too small.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.coding import ReedSolomon
        >>> rs = ReedSolomon(nroots=32, field_poly=0x87, first_root=112,
        ...                  root_stride=11)          # CCSDS 131.0-B 4.3
        >>> g = np.empty(rs.nroots + 1, np.uint8)
        >>> rs.generator(g)                  # Annex G prints all 33
        33
        >>> int(g[0]), int(g[-1])
        (1, 1)

        """

    @property
    def n(self) -> int:
        """Symbols per codeword, `2^J - 1`."""

    @property
    def k(self) -> int:
        """Information symbols per codeword, `n - nroots`."""

    @property
    def e(self) -> int:
        """Correctable symbols per codeword, `nroots / 2`."""

    @property
    def nroots(self) -> int:
        """Parity symbols per codeword, `2E`."""

    @property
    def symbol_bits(self) -> int:
        """Symbol width `J`, in bits."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """


    def __enter__(self) -> "ReedSolomon":
        """Enter a context manager, returning this object.

        Lets a ReedSolomon be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        ReedSolomon
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
    ) -> None:
        """Exit a context manager, releasing the ReedSolomon.

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
