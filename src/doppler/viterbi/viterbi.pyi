# viterbi/viterbi.pyi — type stubs for the viterbi C extension.
from typing import final
import numpy as np
from numpy.typing import NDArray

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
    >>> from doppler.viterbi import Viterbi
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
        >>> from doppler.viterbi import Viterbi
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

        Returns
        -------
        NDArray[np.uint8]
            Bits written, which may be 0 while the traceback fills.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.viterbi import Viterbi
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
