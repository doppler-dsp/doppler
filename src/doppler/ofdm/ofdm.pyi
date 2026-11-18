# ofdm/ofdm.pyi — type stubs for the ofdm C extension.
from typing import final

import numpy as np
import numpy.typing as npt
from numpy.typing import NDArray

@final
class Ofdm:
    """Create an empty modulator (no classes, no runs).

    Raises
    ------
    MemoryError
        If construction fails. The exception message is ``Ofdm: out of
        memory``.

    Examples
    --------
    Create with defaults:

    >>> from doppler.ofdm import Ofdm
    >>> obj = Ofdm()

    """

    def __init__(self, /, *args, **kwargs) -> None: ...
    def reset(self) -> None:
        """Return to the first OFDM symbol, keeping the layout.

        The next render starts from the post-seal state: the first run, no held
        tail. The layout, sealed or not, is unchanged.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
        >>> o = Ofdm()
        >>> o.add_class(8, m, cyclic_prefix=2)
        0
        >>> o.add_run(0, 2)
        0
        >>> x = o.modulate(np.ones(12, np.complex64))
        >>> o.reset()
        >>> o.points_remaining()
        12

        """

    def add_class(
        self,
        n_fft: int,
        subcarrier_map: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        useful: int = 0,
        cyclic_prefix: int = 0,
        window: int = 0,
        scale: float = 1.0,
        n_phase: int = 1,
    ) -> int:
        """Append one kind of OFDM symbol and return its index. `n_fft` is the
        transform length of this class; `subcarrier_map` has `n_fft` entries,
        index `k + n_fft/2`, 1 where the subcarrier may carry a constellation
        point and 0 where it carries nothing the caller supplies. `useful` is
        how many samples are read cyclically from the inverse transform (0
        reads as `n_fft`: a training field of ten short periods reads 160 of
        64). `cyclic_prefix` is the default prefix in samples, the last samples
        of the body copied ahead of it. `window` is the transition into and out
        of this class in samples. `scale` multiplies the transform output.
        `n_phase` is the period of the class's fixed pattern in OFDM symbols. A
        class's per-subcarrier gains and polarity sequence are added by
        `add_subcarrier_gain` and `add_polarity`.

        Copies subcarrier_map. The optional per-subcarrier gains and the
        polarity sequence are added to the class by
        dp_ofdm_add_subcarrier_gain() and dp_ofdm_add_polarity(). Refuses (a
        negative return, nothing changed) what cannot be honoured; a sealed
        layout is re-opened.

        Parameters
        ----------
        n_fft : int
            Transform length, 2 to OFDM_N_FFT_MAX.
        subcarrier_map : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            `n_fft` entries, index `k + n_fft/2`, each OFDM_DATA or OFDM_NULL.
        useful : int
            Body samples read cyclically from the transform; 0 reads as n_fft.
        cyclic_prefix : int
            Default prefix, samples; at most `useful`.
        window : int
            Window transition, samples; at most the symbol length.
        scale : float
            Multiplies the transform output.
        n_phase : int
            Period of the fixed pattern, at least 1.

        Returns
        -------
        int
            The class index, or a negative value if refused.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a class: n_fft is under 2, subcarrier_map is not
            n_fft long or holds a value other than 0 or 1, the cyclic prefix is
            longer than the useful length, the window is longer than the
            symbol, or n_phase is 0``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> o = Ofdm()
        >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
        >>> o.add_class(8, m, cyclic_prefix=2)
        0
        >>> o.add_class(8, m, cyclic_prefix=2, window=1)
        1

        """

    def add_subcarrier_gain(
        self,
        class_index: int,
        gain: npt.NDArray[np.complex64],
    ) -> None:
        """Give a class a complex multiplier per subcarrier: `n_fft` values,
        index `k + n_fft/2`, applied to the whole grid, data and fixed entries
        alike. It carries a per-subband phase rotation or any per-subcarrier
        amplitude. Without it the gain is 1 everywhere. Refuses if the class
        already has gains.

        Applied to the whole grid (data and fixed entries alike): a per-subband
        phase rotation, or any per-subcarrier amplitude. Without it the gain is
        1 everywhere.

        Parameters
        ----------
        class_index : int
            An existing class without gains yet.
        gain : npt.NDArray[np.complex64]
            `n_fft` multipliers, index `k + n_fft/2`.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``cannot add gains: class_index names no class, the class already
            has gains, or gain is not n_fft long``, with the return code
            appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
        >>> o = Ofdm()
        >>> o.add_class(8, m, cyclic_prefix=2)
        0
        >>> o.add_run(0, 1)
        0
        >>> pts = np.ones(6, np.complex64)
        >>> one = o.modulate(pts)
        >>> o.reset()
        >>> o.add_subcarrier_gain(0, 2 * np.ones(8, np.complex64))
        >>> bool(np.allclose(o.modulate(pts), 2 * one))
        True

        """

    def add_polarity(
        self,
        class_index: int,
        polarity: npt.NDArray[np.complex64],
    ) -> None:
        """Give a class a polarity sequence: a complex scalar per OFDM symbol
        that multiplies the class's fixed entries, never its data. OFDM symbol
        `n` of a run uses element `(polarity_offset + n) mod len`. Without it
        the scalar is 1. Refuses if the class already has a sequence.

        OFDM symbol `n` of a run multiplies its fixed entries (never its data)
        by element `(polarity_offset + n) mod polarity_len`. Without it the
        scalar is 1.

        Parameters
        ----------
        class_index : int
            An existing class without a sequence yet.
        polarity : npt.NDArray[np.complex64]
            The sequence.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``cannot add a polarity sequence: class_index names no class, the
            class already has one, or it is empty``, with the return code
            appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> z = np.zeros(8, np.uint8)               # no data subcarriers at all
        >>> p = Ofdm(); p.add_class(8, z, cyclic_prefix=2)
        0
        >>> p.add_fixed(0, 2**32 - 1, 2, 1.0)      # one pilot, every phase
        >>> p.add_polarity(0, np.array([1, -1], np.complex64))
        >>> p.add_run(0, 2)
        0
        >>> x = p.modulate(np.empty(0, np.complex64))
        >>> bool(np.allclose(x[2:10], -x[12:20]))   # the pilot flips sign
        True

        """

    def add_fixed(
        self,
        class_index: int,
        phase: int,
        k: int,
        value: complex,
    ) -> None:
        """Append one fixed entry to a class: a pilot, a training value, a
        boosted pilot. `phase` is the pattern phase it belongs to, or 2**32 - 1
        (`OFDM_ALL_PHASES` in C) for every phase. `k` is the subcarrier, from
        `-n_fft/2` to `n_fft/2 - 1`. A fixed entry displaces data on its
        subcarrier in its phase, and is multiplied by the class's polarity
        element for the OFDM symbol.

        A fixed entry displaces data on its subcarrier in its phase (when the
        map has data there) and is multiplied by the class's polarity element.

        Parameters
        ----------
        class_index : int
            An existing class.
        phase : int
            The pattern phase it belongs to, below the class's `n_phase`, or
            OFDM_ALL_PHASES.
        k : int
            Subcarrier, `-n_fft/2 .. n_fft/2 - 1`.
        value : complex
            The value placed.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            ``cannot append a fixed entry: class_index names no class, phase is
            not below n_phase, or k is outside -n_fft/2 .. n_fft/2 - 1``, with
            the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
        >>> o = Ofdm()
        >>> o.add_class(8, m, cyclic_prefix=2)
        0
        >>> o.add_run(0, 1)
        0
        >>> o.points_remaining()      # six data subcarriers
        6
        >>> o.add_fixed(0, 2**32 - 1, 3, 1j)   # a pilot on a data subcarrier
        >>> o.points_remaining()      # it displaces one
        5

        """

    def add_run(
        self,
        class_index: int,
        count: int,
        cyclic_prefix: int = 4294967295,
        polarity_offset: int = 0,
        first_phase: int = 0,
    ) -> int:
        """Append a run of `count` OFDM symbols of one class. `cyclic_prefix`
        overrides the class's prefix for this run (omit it, or give 4294967295
        = 2**32 - 1, `OFDM_CP_DEFAULT` in C, to keep the class's; a named
        default is just-buildit/just-makeit#2679); `polarity_offset` is the
        class's polarity element used by the run's first symbol; `first_phase`
        is the pattern phase of the run's first symbol. Returns the run's
        index.

        Parameters
        ----------
        class_index : int
            An existing class.
        count : int
            OFDM symbols, at least 1.
        cyclic_prefix : int
            Override of the class's prefix, or OFDM_CP_DEFAULT; at most the
            class's `useful`.
        polarity_offset : int
            Polarity element of the run's first symbol.
        first_phase : int
            Pattern phase of the run's first symbol.

        Returns
        -------
        int
            The run index, or a negative value if refused.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            ``cannot append a run: class_index names no class, count is 0,
            first_phase is not below n_phase, or the prefix does not fit the
            class's useful length``, with the return code appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
        >>> o = Ofdm()
        >>> o.add_class(8, m, cyclic_prefix=2)
        0
        >>> o.add_run(0, 2)
        0
        >>> o.add_run(0, 3)
        1

        """

    def check(self) -> None:
        """Check the whole layout and return 0 if it is usable; raise
        ValueError, with a sentence naming the condition, if not (no run, a
        window longer than an OFDM symbol of its class, two fixed entries on
        one subcarrier in one phase, or memory or a transform plan that could
        not be built). `modulate` checks on first use and raises `ValueError`,
        with the same sentence, for a layout that fails; call this first to
        learn it before any points exist. Adding a class, fixed entry or run to
        a checked layout re-opens it, keeping the position and the held window
        tail, so a signal can be extended while it is rendered; the next
        `modulate` checks the longer layout.

        Refuses a layout with no run, a window longer than an OFDM symbol of
        its class, or two fixed entries on one subcarrier in one phase. Derives
        what the render uses (the data count per phase, the window weights, one
        inverse transform per distinct length). Idempotent once it has passed.

        Raises
        ------
        ValueError
            If the C call returns a non-zero status. The exception message is
            the reason the C call gives, or ``the layout is not usable``, with
            the return code appended (gh-869), when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> o = Ofdm()
        >>> m = np.zeros(8, np.uint8); m[[1, 2, 3]] = 1
        >>> o.add_class(8, m)
        0
        >>> o.check()                # no run yet
        Traceback (most recent call last):
            ...
        ValueError: the layout has no run
        >>> o.add_run(0, 1)
        0
        >>> o.check()                # usable: returns None

        """

    def points_remaining(self) -> int:
        """Constellation points the rest of the layout consumes, from the
        current position. Pass exactly that many to `modulate` to render the
        rest in one call.

        Zero for a layout that does not seal.

        Returns
        -------
        int
            Points from the current position to the end of the layout.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
        >>> o = Ofdm()
        >>> o.add_class(8, m, cyclic_prefix=2)
        0
        >>> o.add_run(0, 3)
        0
        >>> o.points_remaining()
        18

        """

    def modulate(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Render OFDM symbols from constellation points, in layout order, and
        return the samples. Each OFDM symbol takes its data count from the
        points: its data subcarriers less those a fixed entry displaces in its
        pattern phase. Rendering stops when the next OFDM symbol needs more
        points than remain; OFDM symbols that need none (training fields)
        render. A refused call (an unusable layout, a point count that ends
        inside an OFDM symbol or runs past the layout, a buffer too short)
        raises `ValueError` saying which, the modulator untouched. The window
        tail of the last OFDM symbol is held for the next call; `finish`
        returns it.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Constellation points, consumed in layout order.
        out : npt.NDArray[np.complex64] | None
            Receives the samples.

        Returns
        -------
        NDArray[np.complex64]
            Samples written, which may be 0: no points at the end of the
            layout, or zero points mid-layout when the next OFDM symbol needs
            some. A refused call changes nothing and reports it by face: C
            returns a negative value and sets why; the Python binding raises
            ValueError with that sentence.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            the reason the C call gives, or ``modulate refused``, with the
            return code appended (gh-869), when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> o = Ofdm()
        >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
        >>> o.add_class(8, m, cyclic_prefix=2)
        0
        >>> o.add_run(0, 3)
        0
        >>> o.points_remaining()
        18
        >>> x = o.modulate(np.ones(12, np.complex64))   # two of the three
        >>> x.size
        20
        >>> o.points_remaining()
        6
        >>> o.modulate(np.ones(5, np.complex64))        # inside a symbol
        Traceback (most recent call last):
            ...
        ValueError: the points end inside an OFDM symbol: pass a count that ends where one does (see points_remaining and modulate_max_out)

        """

    def modulate_max_out(self, n_in: int, /) -> int:
        """Samples a render of n_in points writes.

        Simulates the render from the current position: OFDM symbols are taken
        in layout order until the next one needs more points than remain. The
        result is zero if n_in ends inside an OFDM symbol or runs past the
        layout (that call is refused), for a layout that does not seal, and for
        no points when the next OFDM symbol needs some. Use it to size the
        output before the call.

        Parameters
        ----------
        n_in : int
            Constellation points.

        Returns
        -------
        int
            Samples that call will write.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> o = Ofdm()
        >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
        >>> o.add_class(8, m, cyclic_prefix=2)
        0
        >>> o.add_run(0, 3)
        0
        >>> o.modulate_max_out(12)   # two OFDM symbols of 10 samples
        20
        >>> o.modulate_max_out(5)    # ends inside a symbol
        0

        """

    def finish(
        self,
        count: int = ...,
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Return the window tail still held after the last OFDM symbol (empty
        when the window is 0 or nothing has been rendered) and clear it.

        After the last OFDM symbol this is the final fall of the window; with a
        window of 0, or before anything is rendered, there is none.

        Parameters
        ----------
        count : int
            How many output samples to ask for. The call may return fewer; size
            an `out=` buffer with the matching `_max_out()` when you need the
            worst case.
        out : npt.NDArray[np.complex64] | None
            Receives the tail.

        Returns
        -------
        NDArray[np.complex64]
            Samples written.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.ofdm import Ofdm
        >>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
        >>> o = Ofdm()
        >>> o.add_class(8, m, cyclic_prefix=2, window=2)
        0
        >>> o.add_run(0, 2)
        0
        >>> x = o.modulate(np.ones(12, np.complex64))
        >>> o.finish().size          # the window's last fall
        2
        >>> o.finish().size          # and it is cleared
        0

        """

    def finish_max_out(self, n: int, /) -> int:
        """Capacity dp_ofdm_finish() needs for n samples.

        Parameters
        ----------
        n : int
            Samples asked for.

        Returns
        -------
        int
            The held tail's length, at most n.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Ofdm has already been destroyed.

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

        Raises ``RuntimeError`` if the Ofdm has already been destroyed.

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
        ``RuntimeError`` if the Ofdm has already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.
        """

    @property
    def run_index(self) -> int:
        """The run the next OFDM symbol belongs to; the number of runs once the
        layout is rendered.
        """

    @property
    def sym_in_run(self) -> int:
        """OFDM symbols already rendered of the run `run_index`."""

    @property
    def tail_len(self) -> int:
        """Samples of window tail held for the next OFDM symbol."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "Ofdm":
        """Enter a context manager, returning this object.

        Lets a Ofdm be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Ofdm
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the Ofdm.

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
