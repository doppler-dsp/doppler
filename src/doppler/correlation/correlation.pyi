# correlation/correlation.pyi — type stubs for the correlation C extension.
from typing import Any, Literal, final

import numpy as np
import numpy.typing as npt
from numpy.typing import NDArray

CORRELATOR_FLAVOR: dict[int, str]

@final
class Correlator:
    """Build the flavor the knobs name: the one constructor the Python class
    `Correlator` calls.

    Parameters
    ----------
    ref : NDArray[np.complex64]
        Row-major ny x nx reference, CF32; borrowed.
        A 1-D array is read as one row, shape (1, n).
    framing : Literal["none", "continuous", "burst"], default "none"
        'none' for maps or detections from whole frames; 'continuous' or
        'burst' for a search over a stream.
        One of ``"none"``, ``"continuous"``, ``"burst"``.
    doppler_rows : int, default 0
        Doppler rows of a delay-Doppler map: the code periods one frame holds.
        0 for no map.
    detect : int, default 0
        1 for detections from push() (framing='none'); a search always detects.
    dwell : int, default 1
        Frames integrated a dump; default 1.
    ny_out : int, default 0
        Padded Doppler rows; 0 (default) for native.
    nx_out : int, default 0
        Padded lags; 0 (default) for native.
    col_out : int, default -1
        The one lag to emit; -1 (default) for every lag.
    threshold : float, default 0.0
        Detection threshold on peak / noise; default 0.
    noise_lo : int, default 0
        First noise bin; default 0.
    noise_hi : int
        Last noise bin; default SIZE_MAX, the last bin.
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        How the noise bins are aggregated into the floor the test statistic
        divides by (a detector), or the CFAR reference's statistic (a search).
        One of ``"mean"``, ``"median"``, ``"min"``, ``"max"``.
    sample_rate : float, default 0.0
        Samples a second; a search needs it (0 is not given). 1.0 for
        normalized units.
    chip_rate : float, default 0.0
        Chips a second; the continuous search needs it (0 is not given).
    symbol_rate : float, default 0.0
        Data symbols a second, for the continuous search's diagnostic; 0 is no
        known data clock.
    reps : int, default 1
        Preamble repetitions a burst holds (the burst search).
    cn0_dbhz : float
        Design-point C/N0, dB-Hz; default DP_CORRELATOR_CN0_NONE (NaN, none).
        The continuous search needs a finite one.
    doppler_uncertainty : float, default 0.0
        Half-width of the Doppler search, Hz; default 0.
    pfa : float, default 1e-3
        False-alarm probability a search; default 1e-3.
    pd : float, default 0.9
        Detection probability at the design point; default 0.9.
    doppler_rate : float, default 0.0
        Doppler rate bound, Hz/s; default 0.
    rate_hypotheses : int, default 1
        Rate hypotheses of the burst search; default 1.
    code_only_epochs : int, default 1
        Code-only epochs of the continuous search; default 1.
    nthreads : int, default 1
        Worker threads of a map or detector; default 1. A search sizes its own
        pool and refuses any other value here: its count is set after create
        with dp_correlator_set_threads().

    Raises
    ------
    ValueError
        If construction fails. The exception message is the reason the
        constructor gives, or ``Correlator: refused, or out of memory
        (docs/design/correlator-python.md section 17: a value the flavor would
        not honor is refused)`` when it gives none.

    Warns
    -----
    UserWarning
        Emitted after construction when ``self->handle->search &&
        self->handle->search->underpowered`` holds: ``Correlator: the search
        cannot meet pd at this cn0_dbhz and geometry (pd_predicted < pd, or
        pd_burst < pd for a burst). It still builds a best-effort grid. Raise
        cn0_dbhz, narrow doppler_uncertainty, lower pd, or give a burst more
        repetitions.``.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.correlation import Correlator
    >>> ref = np.zeros(8, np.complex64); ref[0] = 1.0
    >>> Correlator(ref).flavor                      # a 1-D map
    '1d'
    >>> Correlator(ref, detect=1).flavor            # a streaming detector
    'detector_1d'
    >>> Correlator(np.ones((4, 8), np.complex64)).flavor
    '2d'
    >>> Correlator(ref, doppler_rows=4).flavor      # a delay-Doppler map
    'delay_doppler'
    >>> Correlator(ref).execute(ref).shape          # one dump of nx lags
    (8,)
    >>> from doppler.wfm import PN, mls_poly
    >>> code = np.asarray(PN(poly=mls_poly(5), seed=1, length=5).generate(31),
    ...                   np.uint8)
    >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(np.complex64)
    >>> b = Correlator(s0, framing="burst", reps=8, sample_rate=4e6,
    ...                cn0_dbhz=60.0)       # a burst search: 8 repetitions
    >>> b.flavor
    'burst'
    >>> h = b.push(np.tile(np.roll(s0, 17), 24))[0]
    >>> int(h["row"]), int(h["col"])     # (Doppler bin, delay in samples)
    (0, 17)

    """

    def __init__(
        self,
        ref: npt.NDArray[np.complex64],
        framing: Literal["none", "continuous", "burst"] = "none",
        doppler_rows: int = 0,
        detect: int = 0,
        dwell: int = 1,
        ny_out: int = 0,
        nx_out: int = 0,
        col_out: int = -1,
        threshold: float = 0.0,
        noise_lo: int = 0,
        noise_hi: int = ...,
        noise_mode: Literal["mean", "median", "min", "max"] = "mean",
        sample_rate: float = 0.0,
        chip_rate: float = 0.0,
        symbol_rate: float = 0.0,
        reps: int = 1,
        cn0_dbhz: float = ...,
        doppler_uncertainty: float = 0.0,
        pfa: float = 1e-3,
        pd: float = 0.9,
        doppler_rate: float = 0.0,
        rate_hypotheses: int = 1,
        code_only_epochs: int = 1,
        nthreads: int = 1,
    ) -> None: ...
    def reset(self) -> None:
        """Restart the correlator's running state, keeping how it was built.

        What each flavor drops; the reference, its spectrum, the FFT plans and
        every knob stay:

        - a map: zeroes the accumulator and the integration counter, a fresh
          dwell cycle;
        - a detector: that, plus the carry (the partial frame held between
          pushes) and the last dump's fields, so it matches a fresh detector
          built with the same arguments; last_corr reads None until the next
          dump;
        - a search: drains the input ring and clears the non-coherent power
          accumulator and the dwell bookkeeping, so the next push() begins a
          fresh search; the grid and thresholds are untouched.

        Examples
        --------
        >>> from doppler.correlation import Correlator
        >>> import numpy as np
        >>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
        >>> c = Correlator(ref=ref, dwell=3)
        >>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))
        >>> c.count
        1
        >>> c.reset()
        >>> c.count
        0

        """

    def execute(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64] | None:
        """Correlate one 2-D frame and optionally dump the coherent
        accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with
        ref_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷
        ny*nx). Accumulating in the frequency domain and inverting once is
        exactly the per-frame inverse summed, by linearity of the IFFT — valid
        because the dwell is **coherent** (a complex sum); a non-coherent
        (magnitude) integration could not defer the inverse.

        **Layout.** A frame is ny*nx complex samples in row-major order: row i
        is samples i*nx .. (i+1)*nx - 1. For the 2-D map that is a (ny, nx)
        image; for the delay-Doppler map it is the raw stream, ny code periods
        of nx = code_len samples back to back. The map a dump produces is flat,
        n_out = ny_out * nx_out values (ny values with col_out), also
        row-major: reshape it to (ny_out, nx_out). On the delay-Doppler map,
        axis 0 is Doppler (centered, zero Doppler on row ny_out / 2) and axis 1
        is delay (the lag, in samples).

        **Ownership.** In Python, a call that dumps returns a new array when no
        `out=` is given, and a view of `out=` when one is (a 1-D array holding
        at least n_out values; it is written on a dump only, and the next dump
        overwrites it). A call that does not dump returns None and touches
        neither.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input frame, flat row-major CF32, length ny*nx.
        out : npt.NDArray[np.complex64] | None
            Output buffer for the correlation map (CF32, length n_out:
            ny_out*nx_out, or ny with col_out); written only on a dump call.

        Returns
        -------
        NDArray[np.complex64] | None
            Python: the map on a dump, None otherwise; a frame that is not
            ny*nx samples is REFUSED with nothing read, written or counted, as
            a ValueError (#1925). C: n_out on a dump (or max_out if smaller), 0
            otherwise, and SIZE_MAX for the refused frame. None when the C call
            writes no output.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is the reason the C call gives, or
            ``execute: only a map (framing='none', detect=False) executes;
            Correlator.flavor says which this is`` when it gives none.

        Examples
        --------
        >>> from doppler.correlation import Correlator
        >>> import numpy as np
        >>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
        >>> c = Correlator(ref=ref, dwell=2)
        >>> x = np.ones((2, 2), dtype=np.complex64)
        >>> c.execute(x) is None   # frame 1 — no dump
        True
        >>> c.execute(x).tolist()  # frame 2 — dump
        [(2+0j), (2+0j), (2+0j), (2+0j)]

        """

    def execute_max_out(self) -> int:
        """Maximum output samples per execute call
        (dp_correlator_state_t::n_out -- ny_out*nx_out normally, or ny when a
        single dp_correlator_state_t::col_out was selected).

        Returns
        -------
        int
            n_out, fixed at create().
        """

    def execute_all(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Every map of a batch, unreduced: row h of the (H, n_out) result is
        the map of frame h (in C, map h is at `out + h * n_out`).

        Python: `x` is the H frames back to back, a flat array of H * ny * nx
        samples (H >= 1): frame h is the samples from h * ny * nx up to, not
        including, (h + 1) * ny * nx. With no `out=` each call returns a new
        array; with `out=` it returns a view of it, and the next call into the
        same `out=` overwrites that view. `out=` is a 2-D complex64 array of
        shape (rows, n_out) with at least H rows (a shorter one is refused);
        the result is its first H rows. The object's dwell is untouched.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            H frames back to back (CF32).
        out : npt.NDArray[np.complex64] | None
            H * n_out values (CF32).

        Returns
        -------
        NDArray[np.complex64]
            Python: the (H, n_out) array; a refused batch (empty, not a whole
            number of frames, or an `out=` with fewer than H rows) is a
            ValueError with nothing written. C: H * n_out, or SIZE_MAX for the
            refused batch.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is the reason the C call gives, or
            ``execute_all: only a map (framing='none', detect=False) runs a
            batch; Correlator.flavor says which this is`` when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> rng = np.random.default_rng(0)
        >>> ny, nx = 8, 32
        >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
        >>> dd = Correlator(code, doppler_rows=ny)
        >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
        ...                          for d in (3, 11)])   # delays 3 and 11
        >>> maps = dd.execute_all(frames)
        >>> maps.shape
        (2, 256)
        >>> [int(np.argmax(np.abs(m))) % nx for m in maps]   # each map's delay
        [3, 11]
        >>> dd.count                    # the dwell never moved
        0

        """

    def execute_all_max_out(self, n_in: int, /) -> int:
        """Samples dp_correlator_execute_all() writes for n_in input samples: H
        * n_out, H = n_in / (ny * nx).

        Parameters
        ----------
        n_in : int
            Input samples about to be passed. 0 on a detector or search flavor,
            which execute_all() refuses.

        Returns
        -------
        int
            The capacity `execute_all` needs for n_in samples: H * n_out values
            (C: SIZE_MAX if that overflows).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> rng = np.random.default_rng(0)
        >>> ny, nx = 8, 32
        >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
        >>> dd = Correlator(code, doppler_rows=ny)
        >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
        ...                          for d in (3, 11)])   # delays 3 and 11
        >>> dd.execute_all_max_out(len(frames))   # 2 maps of 8 x 32
        512

        """

    def execute_coherent(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """The coherent sum of a batch's maps, inverted once.

        Python: `x` is the H frames back to back, a flat array of H * ny * nx
        samples (H >= 1).

        The frames' product spectra are summed and the sum is inverted, as a
        dwell does: the result is bit-identical to what a fresh object with
        dwell = H dumps for the same frames, and equal to the sum of the H maps
        up to rounding. The object's own dwell is untouched.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            H frames back to back (CF32).
        out : npt.NDArray[np.complex64] | None
            n_out values (CF32).

        Returns
        -------
        NDArray[np.complex64]
            Python: the one map, a new (n_out,) array; a refused batch is a
            ValueError with nothing written. C: n_out, or SIZE_MAX for the
            refused batch.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is the reason the C call gives, or
            ``execute_coherent: only a map (framing='none', detect=False) runs
            a batch; Correlator.flavor says which this is`` when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> rng = np.random.default_rng(0)
        >>> ny, nx = 8, 32
        >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
        >>> dd = Correlator(code, doppler_rows=ny)
        >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
        ...                          for d in (3, 11)])   # delays 3 and 11
        >>> x = np.tile(code, ny)
        >>> total = dd.execute_coherent(np.concatenate([x, -x]))
        >>> bool(np.abs(total).max() < 1e-3)          # x and -x cancel
        True

        """

    def execute_coherent_max_out(self) -> int:
        """Samples dp_correlator_execute_coherent() writes: n_out.

        Returns
        -------
        int
            n_out, fixed at create().

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> rng = np.random.default_rng(0)
        >>> ny, nx = 8, 32
        >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
        >>> dd = Correlator(code, doppler_rows=ny)
        >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
        ...                          for d in (3, 11)])   # delays 3 and 11
        >>> dd.execute_coherent_max_out()
        256

        """

    def execute_power_sum(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[np.float32] | None = None,
    ) -> NDArray[np.float32]:
        """The power of a batch's maps, summed cell by cell: each output cell
        is the sum over h of that cell's `|map_h|^2`, summed in h order in
        float.

        Python: `x` is the H frames back to back, a flat array of H * ny * nx
        samples (H >= 1).

        Unlike the coherent sum, phase does not cancel: frames x and -x sum to
        twice one map's power, where their coherent sum is zero.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            H frames back to back (CF32).
        out : npt.NDArray[np.float32] | None
            n_out powers (float).

        Returns
        -------
        NDArray[np.float32]
            Python: the (n_out,) float32 powers, a new array; a refused batch
            is a ValueError with nothing written. C: n_out, or SIZE_MAX for the
            refused batch.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is the reason the C call gives, or
            ``execute_power_sum: only a map (framing='none', detect=False) runs
            a batch; Correlator.flavor says which this is`` when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> rng = np.random.default_rng(0)
        >>> ny, nx = 8, 32
        >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
        >>> dd = Correlator(code, doppler_rows=ny)
        >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
        ...                          for d in (3, 11)])   # delays 3 and 11
        >>> x = np.tile(code, ny)
        >>> power = dd.execute_power_sum(np.concatenate([x, -x]))
        >>> one = np.abs(dd.execute_all(x)[0]) ** 2
        >>> bool(np.allclose(power, 2 * one))        # x and -x do not cancel
        True

        """

    def execute_power_sum_max_out(self) -> int:
        """Powers dp_correlator_execute_power_sum() writes: n_out.

        Returns
        -------
        int
            n_out, fixed at create().

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> rng = np.random.default_rng(0)
        >>> ny, nx = 8, 32
        >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
        >>> dd = Correlator(code, doppler_rows=ny)
        >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
        ...                          for d in (3, 11)])   # delays 3 and 11
        >>> dd.execute_power_sum_max_out()
        256

        """

    def execute_power_max(
        self,
        x: npt.NDArray[np.complex64],
        /,
    ) -> tuple[NDArray[np.float32], NDArray[np.uint32]]:
        """The largest power over a batch's maps, cell by cell, and the map it
        came from.

        Python: `x` is the H frames back to back, a flat array of H * ny * nx
        samples (H >= 1).

        Each cell of out is the largest `|map_h|^2` that cell takes over the
        maps, and the same cell of out1 is the h it came from. A tie goes to
        the lowest h: a later map replaces the running maximum only when it is
        strictly greater.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            H frames back to back (CF32); H at most 2^32.

        Returns
        -------
        tuple[NDArray[np.float32], NDArray[np.uint32]]
            Python: the tuple (power, index), each (n_out,): float32 powers and
            uint32 map indices, always new arrays (this call takes no `out=`);
            a refused batch, or H above 2**32, is a ValueError. C: n_out, or
            SIZE_MAX with nothing written.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is the reason the C call gives, or
            ``execute_power_max: only a map (framing='none', detect=False) runs
            a batch; Correlator.flavor says which this is`` when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> rng = np.random.default_rng(0)
        >>> ny, nx = 8, 32
        >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
        >>> dd = Correlator(code, doppler_rows=ny)
        >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
        ...                          for d in (3, 11)])   # delays 3 and 11
        >>> power, index = dd.execute_power_max(frames)
        >>> int(index[4 * nx + 3]), int(index[4 * nx + 11])  # which map peaks
        (0, 1)

        """

    def execute_peaks(
        self,
        x: npt.NDArray[np.complex64],
        /,
    ) -> tuple[NDArray[np.float32], NDArray[np.uint32], NDArray[np.uint32], NDArray[np.float32]]:
        """One record per map of a batch: its peak power, where the peak is,
        and its mean power.

        Python: `x` is the H frames back to back, a flat array of H * ny * nx
        samples (H >= 1).

        Entry h of each output describes map h: out its largest `|map|^2`, out1
        and out2 the row and column of that cell (the first in row-major order
        on a tie), and out3 the mean `|map|^2` over its n_out cells. A map is
        ny_out x nx_out; with col_out it is ny x 1, and the column reported is
        col_out. The peak-to-mean of each map is out over out3, entry by entry:
        a statistic of each map, which the same statistic of a pointwise
        maximum over maps does not give.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            H frames back to back (CF32).

        Returns
        -------
        tuple[NDArray[np.float32], NDArray[np.uint32], NDArray[np.uint32], NDArray[np.float32]]
            Python: the tuple (peak, row, col, mean), each (H,): float32 peak
            power, uint32 row and column, float32 mean power, always new arrays
            (this call takes no `out=`); a refused batch is a ValueError. C: H,
            or SIZE_MAX with nothing written.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is the reason the C call gives, or
            ``execute_peaks: only a map (framing='none', detect=False) runs a
            batch; Correlator.flavor says which this is`` when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> rng = np.random.default_rng(0)
        >>> ny, nx = 8, 32
        >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
        >>> dd = Correlator(code, doppler_rows=ny)
        >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
        ...                          for d in (3, 11)])   # delays 3 and 11
        >>> peak, row, col, mean = dd.execute_peaks(frames)
        >>> row.tolist(), col.tolist()     # zero Doppler is row ny / 2
        ([4, 4], [3, 11])
        >>> bool(np.all(peak / mean > 100))   # each map's own peak-to-mean
        True

        """

    def bank_power_max(
        self,
        x: npt.NDArray[np.complex64],
        L: int,
        g_min: int,
        g_max: int,
        rates: npt.NDArray[np.complex64],
    ) -> tuple[NDArray[np.float32], NDArray[np.uint32]]:
        """The strongest power at every cell over a bank, and which hypothesis
        it came from.

        dp_correlator_bank_fold() with the `POWER_MAX` reducer: at each cell,
        out holds the largest |map_h|^2 over every hypothesis h, and out1 that
        h, `(g - g_min) * R + r`, the lowest h on a tie. Every argument and
        refusal is dp_correlator_bank_fold()'s; in addition max_out below
        `n_out` is refused with nothing written.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            One frame, x_len samples.
        L : int
            Fractions per whole bin; at least 1.
        g_min : int
            Lowest carrier, in units of `1 / (L * nx)` cycles/sample.
        g_max : int
            Highest carrier; at least g_min.
        rates : npt.NDArray[np.complex64]
            Per-period weights, R vectors of ny, or empty (NULL with rates_len
            0 in C) for one unit vector.

        Returns
        -------
        tuple[NDArray[np.float32], NDArray[np.uint32]]
            Python: the tuple (power, index), each (n_out,): float32 powers and
            uint32 hypothesis indices, always new arrays (this call takes no
            `out=`); out of memory is a MemoryError, any other refusal a
            ValueError. C: `n_out`, or SIZE_MAX with nothing written.

        Raises
        ------
        MemoryError
            If the C call refuses and reports ``DP_ERR_MEMORY`` in ``*status``.
            The exception message is the reason the C call gives, or
            ``bank_power_max: out of memory`` when it gives none.
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is the reason the C call gives, or
            ``bank_power_max: only a map (framing='none', detect=False) has a
            bank; Correlator.flavor says which this is`` when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> rng = np.random.default_rng(0)
        >>> ny, nx, d = 8, 32, 5                  # code delayed by 5 samples
        >>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)
        >>> m = np.arange(ny * nx)
        >>> x = (np.tile(np.roll(code, d), ny)    # on a carrier 1.5 bins off
        ...      * np.exp(2j * np.pi * 1.5 / nx * m)).astype(np.complex64)
        >>> dd = Correlator(code, doppler_rows=ny)
        >>> no_rates = np.empty(0, np.complex64)
        >>> pw, h = dd.bank_power_max(x, 2, -4, 4, no_rates)  # g / (2 nx), -4..4
        >>> cell = int(np.argmax(pw))
        >>> cell % nx, int(h[cell]) - 4           # (delay, g = h + g_min)
        (5, 3)

        """

    def bank_detect(
        self,
        x: npt.NDArray[np.complex64],
        L_d: int,
        L_r: int,
        g_min: int,
        g_max: int,
        t_candidate: float,
        t_confirm: float,
        rates: npt.NDArray[np.complex64],
    ) -> tuple[NDArray[np.float32], NDArray[np.int32], NDArray[np.uint32], NDArray[np.uint32], NDArray[np.uint32]]:
        """Detect on a coarse carrier grid, refine each candidate at its own
        cell on a fine one, and keep what confirms.

        The bank searches on L_d and resolves on L_r
        (docs/design/corr2d-hypotheses.md section 6):

        1. **Detect.** dp_correlator_bank_power_max() over carriers g_min ..
           g_max on the `1 / (L_d nx)` grid, with every rate.
        2. **Candidates.** Every cell whose power exceeds t_candidate and is at
           least each of its 8 neighbors, both axes circular (a delay lag and a
           Doppler bin both wrap).
        3. **Refine.** For a candidate found by coarse carrier g_d and rate r,
           the map value at its cell under every fine carrier g_r from `m g_d -
           (m - 1)` to `m g_d + (m - 1)`, m = `L_r / L_d`: the carriers between
           its coarse neighbors, on the `1 / (L_r nx)` grid. On the fast path
           and the delay-Doppler map with a native delay axis each is a
           time-domain correlation at the cell's lag and a slow-time DFT at its
           Doppler row, in double, with no FFT: `2 ny nx` multiply-adds a
           carrier. Any other object makes that hypothesis's map and reads the
           cell. The best g_r wins, the lowest on a tie.
        4. **Confirm.** A candidate whose refined power exceeds t_confirm is a
           record.

        Records come out in cell order (row-major), one entry of each output
        per record: out the refined power, out1 g_r on the fine grid, out2 the
        rate r, out3 the row and out4 the column, the cell as execute() lays
        out the map. Measured at 64 x 512 against an oracle that corrects the
        exact offset, L_d = 2 with L_r = 4 holds L = 4's sensitivity within
        0.16 dB, for about half the bank (the design page). L_d = L_r is the
        one-stage bank. Thresholds are powers in the map's own units; the
        caller sets them for its noise, as for any detector.

        As dp_correlator_bank_fold(); g_min and g_max are on the coarse grid.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            Input.
        L_d : int
            The search grid; at least 1.
        L_r : int
            The resolution grid; a multiple of L_d.
        g_min : int
            Input.
        g_max : int
            Input.
        t_candidate : float
            Candidate threshold, on the coarse power.
        t_confirm : float
            Confirm threshold, on the refined power.
        rates : npt.NDArray[np.complex64]
            Input.

        Returns
        -------
        tuple[NDArray[np.float32], NDArray[np.int32], NDArray[np.uint32], NDArray[np.uint32], NDArray[np.uint32]]
            Python: the tuple (power, g, rate, row, col), one entry per
            confirmed record (possibly none): float32 refined power, int32
            refined carrier, uint32 rate, row and column, always new arrays
            (this call takes no `out=`); out of memory is a MemoryError, and
            anything else below a ValueError. C: the record count (0 included),
            or SIZE_MAX with nothing written for anything
            dp_correlator_bank_fold() refuses, `L_d == 0`, an L_r that L_d does
            not divide, a refined carrier past int32, max_out below `n_out`, or
            memory for the detector's work or the refine's scratch.

        Raises
        ------
        MemoryError
            If the C call refuses and reports ``DP_ERR_MEMORY`` in ``*status``.
            The exception message is the reason the C call gives, or
            ``bank_detect: out of memory`` when it gives none.
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is the reason the C call gives, or
            ``bank_detect: only a map (framing='none', detect=False) has a
            bank; Correlator.flavor says which this is`` when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> rng = np.random.default_rng(0)
        >>> ny, nx, d = 8, 32, 5
        >>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)
        >>> m = np.arange(ny * nx)
        >>> x = (np.tile(np.roll(code, d), ny)    # 1.25 bins: between L_d = 2
        ...      * np.exp(2j * np.pi * 1.25 / nx * m)).astype(np.complex64)
        >>> dd = Correlator(code, doppler_rows=ny)
        >>> no_rates = np.empty(0, np.complex64)
        >>> coarse, _ = dd.bank_power_max(x, 2, -4, 4, no_rates)
        >>> t = 0.5 * coarse.max()
        >>> power, g, rate, row, col = dd.bank_detect(x, 2, 4, -4, 4, t, t,
        ...                                           no_rates)
        >>> len(power), int(g[0]), int(col[0])    # g on the 1/(4 nx) grid
        (1, 5, 5)
        >>> round(float(power[0] / coarse.max()), 3)  # the straddle recovered
        1.233

        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the search grid directly, bypassing both auto-sizing searches —
        the advanced escape hatch (mirrors Dll's/Costas's
        configure_lock_raw()).

        Resizes every buffer/plan that depends on the grid (the slow-time FFT,
        the code correlator, the reference, and every per-frame scratch
        buffer), re-derives the threshold ladder for the pinned grid from the
        same physics
        dp_correlator_create_burst()/dp_correlator_create_continuous() used,
        and clears in-flight accumulation (ring contents, the non-coherent
        power accumulator, dwell bookkeeping) — call between push() calls,
        never a substitute for one.

        Parameters
        ----------
        doppler_bins : int
            Coherent depth to pin, in `[1, reps]`.
        n_noncoh : int
            Non-coherent look count to pin, in `[1,
            DP_CORRELATOR_N_NONCOH_SAFETY_CEILING]`.

        Raises
        ------
        ValueError
            If the C call returns ``DP_ERR_UNSUPPORTED``. The exception message
            is the reason the C call gives, or ``configure_search_raw: only a
            search (framing='continuous' or 'burst') has a grid to pin;
            Correlator.flavor says which this is`` when it gives none.
        ValueError
            If the C call returns ``DP_ERR_INVALID``. The exception message is
            the reason the C call gives, or ``configure_search_raw:
            doppler_bins must lie in 1..reps and n_noncoh in
            1..DP_CORRELATOR_N_NONCOH_SAFETY_CEILING, and non-coherent looks
            need rate_hypotheses 1`` when it gives none.
        MemoryError
            If the C call returns ``DP_ERR_MEMORY``. The exception message is
            the reason the C call gives, or ``configure_search_raw: rebuilding
            the grid could not allocate; the engine keeps its prior grid`` when
            it gives none.
        ValueError
            If the C call returns any other non-zero status. The exception
            message is the reason the C call gives, or ``configure_search_raw
            failed``, with the return code appended (gh-869), when it gives
            none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> a = Correlator(ref, framing="continuous", sample_rate=4e6, chip_rate=1e6, cn0_dbhz=50.0)
        >>> a.configure_search_raw(doppler_bins=1, n_noncoh=4)  # pin the grid
        >>> a.doppler_bins, a.n_noncoh
        (1, 4)
        >>> burst = np.tile(np.roll(s0, 17), 4).astype(np.complex64)
        >>> h = a.push(burst)
        >>> int(h["row"][0]), int(h["col"][0])      # detects at the pinned grid
        (0, 18)

        """

    def set_max_peaks(self, n: int) -> None:
        """How many peaks a dwell may report: the peak list's capacity.

        One (the default) is the classic detector -- the maximum of the
        surface, gated. More is the list of docs/design/async-dsss-receiver.md
        §7.1: every peak above the same gate, strongest first, each with an
        exclusion zone of one Doppler bin by the reference's first
        autocorrelation null around it (one chip for a PN code; one emitter's
        main lobe, so its own shoulders are not the next peak), and the
        two-epoch rule for a peak at an already-listed code phase -- a data
        transition inside the epoch splits one emitter into twins at its own
        code phase on other tiles, so such a peak is held for one dwell and
        listed only if it was there, at the same tile, on the previous one.
        Each listed peak is one dp_correlator_hit_t from dp_correlator_push(),
        all of a dwell's sharing its `samples_consumed` and `noise_est`. A held
        twin takes a slot of the `n` for that dwell but is not reported. The
        threshold does not change: a second peak is another draw from the same
        cells against the same union bound. Clears the held candidates.

        Parameters
        ----------
        n : int
            1 … DP_CORRELATOR_MAX_PEAKS.

        Raises
        ------
        ValueError
            If the C call returns ``DP_ERR_UNSUPPORTED``. The exception message
            is the reason the C call gives, or ``set_max_peaks: only a search
            (framing='continuous' or 'burst') lists peaks; Correlator.flavor
            says which this is`` when it gives none.
        ValueError
            If the C call returns ``DP_ERR_INVALID``. The exception message is
            the reason the C call gives, or ``set_max_peaks: n must lie in
            1..DP_CORRELATOR_MAX_PEAKS`` when it gives none.
        ValueError
            If the C call returns any other non-zero status. The exception
            message is the reason the C call gives, or ``set_max_peaks
            failed``, with the return code appended (gh-869), when it gives
            none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> code = (np.arange(31) * 5 % 2).astype(np.uint8)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, symbol_rate=1e3,
        ...                 cn0_dbhz=50.0, doppler_uncertainty=50e3)
        >>> a.max_peaks
        1
        >>> a.set_max_peaks(8)
        >>> a.max_peaks
        8

        """

    def set_carrier_freq_hz(self, carrier_freq_hz: float) -> None:
        """Couple the code clock to the carrier: the chip rate dilates by
        `doppler_hz / carrier_freq_hz`, and the engine accounts for it.

        A physically-coupled Doppler moves the code as well as the carrier --
        100 chips/s at 20 ppm of 5 Mcps -- and the engine's two long
        integrations both smear over it (doppler#1256, #1254):

        - **Inside a coherent block** of D epochs every tile's epoch
          correlations are shifted along the code axis by the drift the tile's
          own frequency implies, `f_tile / carrier` chips per chip, aligned to
          the block's middle (a linear phase on each epoch's product). The one
          delay-Doppler map applies it at the block's end, read on a slow-time
          grid 4 times finer and rounded. The bound is 0.09 dB, the worst over
          33 tones and three dilations (docs/design/acq-surface.md §18.1); the
          engine measures 0.020-0.026 dB on its equivalence test's carrier
          scenes. dp_correlator_block_prompt() applies it exactly. A change of
          carrier mid-block applies to the whole block. Measured at the
          specification's 20 ppm with D = 154 (3.1 chips of drift across the
          block): without it the block's peak is 13 dB down and 3 chips wide
          and the depth detects nothing at 34 dB-Hz; with it the block reads as
          a still one.
        - **The hand-off** (dp_correlator_build_handoff()) advances the hit's
          code phase by the drift over half the dwell -- the non-coherent sum's
          peak is the phase at the dwell's middle, the seed is wanted at its
          end: 0.9 chip at the 40 dB-Hz floor, past a refine loop's pull-in.

        A mutator's value (#2022): it rides in the state blob (v8), and a
        restore takes it through this setter's rebuild, so a resumed engine
        carries it on. Setting it rebuilds the map's fine grid and every tile's
        ramp tables when they move (the regrid that re-applies the thread
        request). Default 0.0 (uncoupled) is the engine exactly as it ran
        without it.

        Parameters
        ----------
        carrier_freq_hz : float
            RF carrier, Hz; 0.0 = uncoupled. Refused (DP_ERR_UNSUPPORTED) on
            every flavor but the continuous search
            (docs/design/correlator-python.md section 18).

        Raises
        ------
        ValueError
            If the C call returns ``DP_ERR_UNSUPPORTED``. The exception message
            is ``set_carrier_freq_hz: only the continuous search
            (framing='continuous') takes a carrier; Correlator.flavor says
            which this is``.
        ValueError
            If the C call returns ``DP_ERR_INVALID``. The exception message is
            ``set_carrier_freq_hz: must be 0 (no carrier) or finite and above
            sample_rate / 2``.
        MemoryError
            If the C call returns ``DP_ERR_MEMORY``. The exception message is
            ``set_carrier_freq_hz: rebuilding the delay-Doppler map for the new
            carrier could not allocate; the engine is unchanged``.
        ValueError
            If the C call returns any other non-zero status. The exception
            message is ``set_carrier_freq_hz failed``, with the return code
            appended (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> from doppler.wfm import Gold
        >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> a = Correlator(ref, framing="continuous", sample_rate=1e7, chip_rate=5e6, symbol_rate=2700.0,
        ...                 cn0_dbhz=45.0, doppler_uncertainty=50e3)
        >>> a.carrier_freq_hz
        0.0
        >>> a.set_carrier_freq_hz(2.5e9)
        >>> a.carrier_freq_hz
        2500000000.0

        """

    def set_threads(self, n: int) -> None:
        """Set how many threads the searcher fans its tiles across
        (docs/design/acq-surface.md §13: a roll per thread on persistent
        workers).

        A continuous engine is created with a pool of the machine's online
        cores when it has more than one tile; a burst engine, and a single-tile
        one, run serially. This sets the count: 0 auto-selects the online core
        count, 1 runs everything on the calling thread, n runs on n workers
        (the caller included) -- but never more than `window_bins`: every fan
        hands out one tile a task, so a worker past that only wakes and waits.
        The request is kept, and every regrid re-applies it, so an engine
        narrowed to one tile and widened again runs on min(n, tiles), not on
        the 1 the narrow grid ran on. The workers are created here, once, and
        parked between pushes; nothing is created per push. The surface is
        bit-identical at every count -- the tiles are independent after the one
        forward transform and each writes its own rows -- so this changes the
        cost of a push and nothing about its result. Setup path, never hot; not
        while another thread is inside push().

        Parameters
        ----------
        n : int
            Thread count; 0 = online cores, 1 = serial. Refused
            (DP_ERR_UNSUPPORTED) on a map or detector flavor (no search
            record).

        Raises
        ------
        ValueError
            If the C call returns ``DP_ERR_UNSUPPORTED``. The exception message
            is ``set_threads: only a search (framing='continuous' or 'burst')
            sizes its pool after create; a map or detector takes nthreads at
            create``.
        ValueError
            If the C call returns any other non-zero status. The exception
            message is ``set_threads failed``, with the return code appended
            (gh-869).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(
        ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0,
        ...                 doppler_uncertainty=4000.0)
        >>> a.threads >= 1               # at most one worker a tile
        True
        >>> a.set_threads(1)
        >>> a.threads
        1

        """

    def set_telemetry(
        self,
        tlm: object | None,
        prefix: str,
        decim: int = 1,
    ) -> None:
        """Attach (or detach) a telemetry context and register the engine's
        probes on it (docs/design/async-dsss-receiver.md §2.4).

        Registers ten probes, emitted once per DECIDED dwell (a coherent dump,
        or the dwell that completes `n_noncoh` looks) and further thinned by
        decim: "<prefix>.stat" (the dwell's test statistic — the strongest cell
        against the CFAR reference, in the units the gate is set in),
        "<prefix>.gate" (that gate: `threshold` on the coherent path, `eta_nc`
        on the non-coherent one — plotted together they show exactly where a
        hit fired), "<prefix>.noise" (the CFAR reference `noise_est`),
        "<prefix>.peak" (the strongest cell's raw value), "<prefix>.row" and
        "<prefix>.col" (its native Doppler row and code-phase column — a
        surface coordinate, not a physical unit;
        dp_correlator_surface_doppler_hz() and
        dp_correlator_surface_chip_phase() convert), "<prefix>.n_peaks" (picks
        in the dwell, held twins included), "<prefix>.n_held" (picks held as
        same-code-phase twins rather than listed, §7.1), "<prefix>.conc" (the
        strongest pick's concentration — see `peak_conc`: its main lobe's power
        over its whole column's, near 1 for one clean emitter even when it
        straddles two tiles, about 0.5 when a data transition splits it into
        twins two or more tiles away, lower still when a coherent block
        straddles data — the discriminator between one emitter's splatter and a
        second emitter) and "<prefix>.hit" (1 when the gate fired). Passing
        NULL detaches. Setup path, never hot; the context is borrowed and must
        outlive the attachment (SPSC rules in dp_tlm/dp_tlm_core.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "acq" or "ch0.acq".
        decim : int
            Emit every decim-th decided dwell; >= 1. Refused
            (DP_ERR_UNSUPPORTED) on every flavor but the continuous search
            (docs/design/correlator-python.md section 18).

        Raises
        ------
        ValueError
            If the C call returns ``DP_ERR_UNSUPPORTED``. The exception message
            is the reason the C call gives, or ``set_telemetry: only the
            continuous search (framing='continuous') is instrumented;
            Correlator.flavor says which this is`` when it gives none.
        ValueError
            If the C call returns ``DP_ERR_INVALID``. The exception message is
            the reason the C call gives, or ``set_telemetry: refused, nothing
            attached: the prefix joined to each probe name must fit
            DP_TLM_NAME_MAX, decim must be at least 1, and the context must
            have room for all ten probes`` when it gives none.
        MemoryError
            If the C call returns ``DP_ERR_MEMORY``. The exception message is
            the reason the C call gives, or ``set_telemetry: the attach's
            scratch context could not be allocated; nothing is attached`` when
            it gives none.
        ValueError
            If the C call returns any other non-zero status. The exception
            message is the reason the C call gives, or ``set_telemetry
            failed``, with the return code appended (gh-869), when it gives
            none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> from doppler.telemetry import Telemetry
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(
        ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0)
        >>> tlm = Telemetry(1 << 12)
        >>> a.set_telemetry(tlm, "acq")
        >>> sorted(tlm.probe_names)[:3]
        ['acq.col', 'acq.conc', 'acq.gate']
        >>> x = np.zeros(a.n_noncoh * 511 * 2 * 3, dtype=np.complex64)
        >>> _ = a.push(x)
        >>> len(tlm.read()) % 10      # ten records per decided dwell
        0

        """

    def surface(self, out: npt.NDArray[np.float32]) -> int:
        """The last decided dwell's surface, in the gate's own units.

        Copies the surface the last dwell was decided on into out, row-major
        `surface_rows` (Doppler: tiles, or interpolated slow-time rows) by
        `lag_bins` (code phase), every cell divided by the same CFAR reference
        the gate used — so a cell reads as its own test statistic, to a float
        rounding (the SIMD build's fast-math may take a reciprocal in this loop
        and a divide in the gate's), and the gate (`threshold`, or `eta_nc` on
        the non-coherent path) is a flat plane on a plot.

        **A surface is kept while `keep_surface` is set OR a sink is attached**
        (dp_correlator_set_surface_sink()); this is the one statement of that
        rule. Set either, push, then read. `surface_at` says which dwell it is;
        a time-decimated record is the caller reading every k-th dwell, or a
        sink with `decim`.

        Parameters
        ----------
        out : npt.NDArray[np.float32]
            At least `surface_rows * lag_bins` floats.

        Returns
        -------
        int
            Cells written (`surface_rows * lag_bins`), or 0 when no dwell has
            been decided while a surface was kept, or out is too small.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is ``surface: only the continuous
            search (framing='continuous') has it; Correlator.flavor says which
            this is``.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(
        ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0)
        >>> a.keep_surface = 1
        >>> x = np.zeros(a.n_noncoh * 511 * 2, dtype=np.complex64)
        >>> _ = a.push(x)
        >>> s = np.empty(a.surface_rows * a.lag_bins, dtype=np.float32)
        >>> a.surface(s) == s.size
        True
        >>> a.code_bins, a.lag_bins  # 1022 is 2 x 7 x 73: read on 1024 lags
        (1022, 1024)
        >>> s.reshape(a.surface_rows, a.lag_bins).shape == (a.surface_rows, 1024)
        True

        """

    def surface_doppler_hz(self, out: npt.NDArray[np.float64]) -> int:
        """The surface's Doppler axis: the frequency of each row, in Hz.

        One value per surface row, the fold and scale a hit's `doppler_hz_est`
        uses (dp_fftfreq_index() times `doppler_res_hz`, on the interpolated
        grid where the slow-time axis is interpolated), so a plot of
        dp_correlator_surface() carries the same axis a DetectionEvent reports
        on.

        Parameters
        ----------
        out : npt.NDArray[np.float64]
            At least `surface_rows` doubles.

        Returns
        -------
        int
            Values written (`surface_rows`), or 0 if out is too small.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is ``surface_doppler_hz: only the
            continuous search (framing='continuous') has it; Correlator.flavor
            says which this is``.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(
        ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0,
        ...                 doppler_uncertainty=4000.0)
        >>> f = np.empty(a.surface_rows, dtype=np.float64)
        >>> a.surface_doppler_hz(f) == a.surface_rows
        True
        >>> bool(f[0] == 0.0 and f.min() < 0.0 < f.max())
        True

        """

    def surface_chip_phase(self, out: npt.NDArray[np.float64]) -> int:
        """The surface's code-phase axis: the chip phase of each column.

        One value per surface column, in chips, the same mapping
        dp_correlator_build_handoff() applies to a hit's `col` — so a plotted
        peak sits at the chip phase the DetectionEvent would carry.

        Parameters
        ----------
        out : npt.NDArray[np.float64]
            At least `lag_bins` doubles.

        Returns
        -------
        int
            Values written (`lag_bins`), or 0 if out is too small.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is ``surface_chip_phase: only the
            continuous search (framing='continuous') has it; Correlator.flavor
            says which this is``.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(
        ...     PN(poly=mls_poly(9), seed=1, length=9).generate(511), np.uint8)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> a = Correlator(ref, framing="continuous", sample_rate=2e6, chip_rate=1e6, cn0_dbhz=50.0)
        >>> c = np.empty(a.lag_bins, dtype=np.float64)
        >>> a.surface_chip_phase(c) == a.lag_bins
        True
        >>> bool(c[0] == 0.0 and c[1] == 511 - 511 / 1024)
        True

        """

    def surface_complex(self, out: npt.NDArray[np.complex64]) -> int:
        """The last decided dwell's surface, complex: amplitude and carrier
        phase per cell, before the magnitude the gate reads.

        Copies the coherent sum the last dwell was decided on into out,
        row-major `surface_rows` x `lag_bins` like dp_correlator_surface(), in
        the correlation's own units rather than the gate's. A cell's phase is
        the carrier at the block's middle, relative to its tile's centre; its
        neighbours along the code axis are complex early and late arms, so a
        tracker can form the coherent discriminator `Re(conj(P) (L - E)) /
        |P|^2`, which the magnitude surface cannot
        (docs/design/async-dsss-receiver-measurements.md §12.21). Coherent path
        only: a non-coherent dwell (`n_noncoh > 1`) is a power sum with no
        phase, and reads 0.

        Parameters
        ----------
        out : npt.NDArray[np.complex64]
            At least `surface_rows * lag_bins` complex floats.

        Returns
        -------
        int
            Cells written (`surface_rows * lag_bins`), or 0 when no dwell has
            been decided, the path is non-coherent, or out is too small.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is ``surface_complex: only a search
            (framing='continuous' or 'burst') has it; Correlator.flavor says
            which this is``.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> a = Correlator(ref, framing="continuous", sample_rate=4e6, chip_rate=1e6, cn0_dbhz=70.0)
        >>> a.n_noncoh                 # one look: the dwell is the coherent dump
        1
        >>> _ = a.push(np.roll(s0, 17).astype(np.complex64))
        >>> sc = np.empty(a.surface_rows * a.lag_bins, dtype=np.complex64)
        >>> a.surface_complex(sc) == sc.size
        True
        >>> int(np.argmax(np.abs(sc)) % a.lag_bins)   # the peak's lag column
        18

        """

    def block_prompt(
        self,
        tile: int,
        col: int,
        out: npt.NDArray[np.complex64],
    ) -> int:
        """One cell's column of the last whole block: the per-epoch complex
        correlations at a code phase, the despread stream at epoch rate.

        The block-coherent engine gathers every tile's correlation row for
        `coherent_bins` epochs before its slow-time transform (file doc,
        docs/design/acq-surface.md §4). This copies the `coherent_bins` values
        at column col of tile tile into out, in epoch order: each epoch's
        complex prompt at that code phase, rolled to the tile's centre
        frequency and shifted to the block's middle by the tile's code-rate
        hypothesis, so the phase is continuous along the column for an emitter
        at the tile's centre and rotates at its offset from it. At an emitter's
        cell this is what a despreader produces, one value per epoch, phase
        included (docs/design/async-dsss-receiver-measurements.md §12.21).
        Valid once a block is whole, until the next epoch is pushed.

        Parameters
        ----------
        tile : int
            Tile index, `0 … window_bins-1` (native FFT order, the order the
            surface's rows are cut in).
        col : int
            Code-phase column, `0 … lag_bins-1`.
        out : npt.NDArray[np.complex64]
            At least `coherent_bins` complex floats.

        Returns
        -------
        int
            Values written (`coherent_bins`), or 0 at `coherent_bins == 1` (no
            block is gathered), while a block is partial, for an index out of
            range, or when out is too small.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is ``block_prompt: only the
            continuous search (framing='continuous') has it; Correlator.flavor
            says which this is``.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> b = Correlator(ref, framing="continuous", sample_rate=4e6, chip_rate=1e6, cn0_dbhz=60.0,
        ...                 doppler_uncertainty=40e3, code_only_epochs=7)
        >>> b.coherent_bins                    # (7 + 1) // 2
        4
        >>> blk = np.tile(np.roll(s0, 17), b.coherent_bins).astype(np.complex64)
        >>> _ = b.push(blk)
        >>> p = np.empty(b.coherent_bins, dtype=np.complex64)
        >>> b.block_prompt(0, 17, p) == b.coherent_bins
        True
        >>> bool(np.abs(p).min() > 0.99 * np.abs(p).max())  # every epoch's prompt
        True
        >>> b.block_prompt(0, 17, np.empty(1, dtype=np.complex64))  # too small
        0

        """

    def block_raw(self, out: npt.NDArray[np.complex64]) -> int:
        """The last whole block's raw samples, as pushed.

        Copies the `coherent_bins * code_bins` samples the block-coherent
        engine gathered for its last whole block into out, epoch by epoch in
        stream order — the samples dp_correlator_push() consumed, untouched.
        Kept for the tile-edge re-ask (search_resolve_tile_alias()); exposed so
        a tracker can re-correlate them at any code phase, rate or symbol
        boundary the engine's own grid does not have — a symbol-rate despreader
        at the tracked timing runs on exactly this
        (docs/design/async-dsss-receiver-measurements.md §12.21). Valid once a
        block is whole, until the next epoch is pushed.

        Parameters
        ----------
        out : npt.NDArray[np.complex64]
            At least `coherent_bins * code_bins` complex floats.

        Returns
        -------
        int
            Samples written (`coherent_bins * code_bins`), or 0 at
            `coherent_bins == 1`, while a block is partial, or when out is too
            small.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is ``block_raw: only the continuous
            search (framing='continuous') has it; Correlator.flavor says which
            this is``.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.correlation import Correlator
        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
        ...                      length=5).generate(31)).astype(np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
        ...     np.complex64)
        >>> ref = np.where(code & 1, -1, 1).astype(np.complex64)
        >>> b = Correlator(ref, framing="continuous", sample_rate=4e6, chip_rate=1e6, cn0_dbhz=60.0,
        ...                 doppler_uncertainty=40e3, code_only_epochs=7)
        >>> blk = np.tile(np.roll(s0, 17), b.coherent_bins).astype(np.complex64)
        >>> _ = b.push(blk)
        >>> raw = np.empty(b.coherent_bins * b.code_bins, dtype=np.complex64)
        >>> b.block_raw(raw) == raw.size
        True
        >>> bool(np.array_equal(raw, blk))      # the samples as pushed
        True

        """

    def push(
        self,
        x: npt.NDArray[np.complex64],
        out: npt.NDArray[Any] | None = None,
    ) -> NDArray[Any]:
        """Stream an arbitrary-length CF32 chunk through a detector or a search
        flavor; return its detections.

        Takes the input in order and frames it. A detector runs each complete
        n-sample frame (n = ny*nx) through the correlator and, on every dump,
        computes the test statistic peak_mag / noise_est: each dump that passes
        the threshold is one hit, row and col its cell (on one row, col is the
        lag). A search gates each decided dwell on its CFAR threshold and lists
        up to max_peaks hits a dwell, each with its Doppler bin (row), code
        phase (col), C/N0 estimate and hand-off fields (dp_correlator_hit_t). A
        chunk that ends mid-frame keeps the rest as the carry, so the hits do
        not depend on how the stream is split into calls.

        In Python the hits are one structured array of dp_correlator_hit_t
        records, a field per member, sized from dp_correlator_push_max_out(): a
        push never loses input to a full list. A map flavor refuses it.

        Parameters
        ----------
        x : npt.NDArray[np.complex64]
            CF32 input chunk of arbitrary length.
        out : npt.NDArray[Any] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[Any]
            Python: the hits, a structured array, empty when no dump or dwell
            passed the gate. C: the number of hits written to result, or
            SIZE_MAX (nothing taken, nothing changed) when state is a map
            flavor.

            One row per `dp_correlator_hit_t` record, with fields:

            row : int
                Row of the peak: 0 at one row; a search flavor's Doppler bin (0 … coherent_bins-1), or, in wideband mode, a bin of the uniform grid of window_bins*coherent_bins bins (window_bins tiles of coherent_bins each) — see the search flavors' section.
            col : int
                Column of the peak: the lag, at one row; a search flavor's code phase (0 … lag_bins-1), a lag of col * code_bins / lag_bins samples.
            peak_mag : float
                max |R| (linear magnitude, not power).
            noise_est : float
                Noise estimate aggregated over noise_lo..noise_hi.
            test_stat : float
                peak_mag / noise_est; 0 if noise_est is not > 0. A search flavor's is scaled by sqrt(2*n_noncoh) once non-coherent looks are combined.
            cn0_dbhz_est : float
                Estimated carrier-to-noise density (dB-Hz), backed out of test_stat via the same C/N0 <-> per-sample-amplitude-SNR relationship used to size the engine (see dp_correlator_create_burst()/ dp_correlator_create_continuous()). Tracks the true C/N0 while receiver AWGN dominates the CFAR noise estimate; saturates at the code's own autocorrelation-sidelobe floor once the true C/N0 exceeds what this code/geometry can resolve — a real ceiling, not a fault.
            samples_consumed : int
                st->samples_consumed at the exact moment this hit was appended — the raw sample offset (since this engine's own stream start) this detection's epoch ended at. The per-hit anchor a caller needs to derive a precise timestamp (dp_sample_clock_stamp_at(), timing/timing_core.h) instead of reusing one message-level timestamp for every hit — a single push() call spanning multiple epochs can emit several hits at different sample offsets. Acquisition itself stays wall-clock-agnostic; this is the raw material a caller with a real clock converts.
            chip_phase : float
                The code's own instantaneous phase at the hand-off instant, in chips, Dll's convention -- the seed a code loop wants for this hit. The same value dp_correlator_build_handoff() reports as dp_correlator_handoff_t::chip_phase: the inverted lag advanced by the dwell's dilation when a carrier is set (dp_correlator_set_carrier_freq_hz()). For an engine built by dp_correlator_create_burst(), one chip is one template sample, so chip_phase is in template samples over code_bins: a caller at s samples per chip divides by s. The burst search refuses a carrier setter, so its hits carry no dwell advance. Appended last so positional initialisers do not shift.
            doppler_rate_hz_s : float
                The Doppler rate, Hz/s, of the rate hypothesis that won this hit's dwell (dp_correlator_create_burst()'s rate_hypotheses); 0 without hypotheses, and 0 when the dwell's block is one or two epochs, which cannot see a rate (every hypothesis is then the same surface). A hypothesis, not a fit, and a reliable one only from a block WHOLLY inside the preamble and well above the design point. Then it names the hypothesis nearest the carrier's rate -- within half a spacing, `doppler_rate/(n - 1)` -- every time (validate_acq_rate_pd's reported-rate rows, N = 9, D = 10, design -13 dB, 23 dB over it: 119/119 at true rate 0, 121/121 at +R). Below that it is not: at the design point only 30% (rate 0) and 66% (+R) of the full-block hits name it, adjacent hypotheses differing by at most pi/4 of phase at the block's edges so that noise picks among neighbours; and about three hits in four come from dwells STRADDLING the preamble's edges, which often name +-R (39% of all hits at +10 dB-Hz name the nearest at true rate 0, 51% at +R). DETECTION is unaffected -- the surface that wins is a good one whichever neighbour it is. Treat the value as a seed, not a measurement, at the C/N0 the engine was sized for. With `max_peaks > 1` one hypothesis's surface serves the whole dwell's list, so every hit of a dwell wears the winner's rate. The Doppler the hit reports is the carrier's at the middle of the dwell's block. Appended last so positional initialisers do not shift.
            test_stat_peak : float
                test_stat as the emitter would read on its own lag: the straddle a delay between two lag columns costs, put back. Estimated from the larger of the hit's two neighbouring columns under the mainlobe the engine models (a triangle `shape.zone` samples wide, as for a chip code): a centre c and neighbour n one column, s samples, apart give (c + n) / (2 - s / zone), exact on that triangle, clamped between c and c / (1 - s / (2 zone)) -- never below what was read, never past the worst straddle. Noise lifts the neighbour, so at low statistics it reads high, within that clamp. For a bound on what the emitter's sidelobes can reach (a sidelobe does not lose its straddle with the peak); detection reads test_stat. Appended last, like the field above.

        Raises
        ------
        ValueError
            If the C call returns ``SIZE_MAX``, its refusal value, in place of
            a count. The exception message is ``push: only a detector or a
            search pushes; a map does not detect (build one with detect=True,
            or framing='continuous' or 'burst')``.

        Examples
        --------
        >>> from doppler.correlation import Correlator
        >>> import numpy as np
        >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
        >>> det = Correlator(ref=ref, detect=1, dwell=1, noise_lo=1, noise_hi=15,
        ...                  noise_mode="mean", threshold=0.0)
        >>> h = det.push(np.ones((4, 4), dtype=np.complex64))[0]
        >>> row, col = int(h["row"]), int(h["col"])
        >>> peak, noise, stat = (float(h[f]) for f in ("peak_mag", "noise_est",
        ...                                            "test_stat"))
        >>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)
        (0, 0, 1.0, 1.0, 1.0)
        >>> det1 = Correlator(ref=ref[0], detect=1, dwell=1, noise_lo=1, noise_hi=3,
        ...                 noise_mode="mean", threshold=0.0)
        >>> h = det1.push(np.ones(4, dtype=np.complex64))[0]
        >>> lag = int(h["col"])               # one row: the lag is the column
        >>> peak, noise, stat = (float(h[f]) for f in ("peak_mag", "noise_est",
        ...                                            "test_stat"))
        >>> lag, round(peak, 4), round(noise, 4), round(stat, 4)
        (0, 1.0, 1.0, 1.0)

        A burst search over a PN preamble, 4 samples a chip, finds the burst's
        delay; any repeated preamble works, a Zadoff-Chu sequence included:

        >>> from doppler.wfm import PN, mls_poly
        >>> code = np.asarray(PN(poly=mls_poly(5), seed=1, length=5).generate(31),
        ...                   np.uint8)
        >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(np.complex64)
        >>> b = Correlator(s0, framing="burst", reps=8, sample_rate=4e6,
        ...                cn0_dbhz=60.0)
        >>> h = b.push(np.tile(np.roll(s0, 17), 24))[0]
        >>> int(h["row"]), int(h["col"])     # (Doppler bin, delay in samples)
        (0, 17)
        >>> k = np.arange(127)
        >>> zc = np.exp(-1j * np.pi * 5 * k * (k + 1) / 127).astype(np.complex64)
        >>> z = Correlator(zc, framing="burst", reps=8, sample_rate=1.0)
        >>> z.code_bins                      # samples a repetition
        127
        >>> h = z.push(np.tile(np.roll(zc, 40), 10))[0]
        >>> int(h["row"]), int(h["col"])
        (0, 40)

        """

    def push_max_out(self, n_in: int, /) -> int:
        """The most hits dp_correlator_push() can return for n_in more samples:
        a result array this long is never short.

        The frames n_in completes, counting the carry the last push left, times
        what one frame can make: one hit for a detector (one per dump),
        `max_peaks` for a search (a frame that ends a dwell lists up to that
        many). A bound, not a count: most frames end no dump. 0 on a map
        flavor, which push() refuses. SIZE_MAX if the product would overflow.

        Parameters
        ----------
        n_in : int
            Samples the next push() will be given.

        Returns
        -------
        int
            The bound, in hits.
        """

    def state_bytes(self) -> int:
        """Size in bytes of this object's serialized state.

        The exact length `get_state` returns and `set_state` requires. It
        depends on how the object was constructed (state arrays are sized at
        construction), so read it from the instance rather than assuming a
        constant.

        Raises ``RuntimeError`` if the Correlator has already been destroyed.

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

        Raises ``RuntimeError`` if the Correlator has already been destroyed.

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
        length differs from `state_bytes()`, the exception listed under Raises
        if the core rejects it, and ``RuntimeError`` if the Correlator has
        already been destroyed.

        Parameters
        ----------
        blob : bytes
            A `get_state()` blob from this type, exactly `state_bytes()` long.

        Raises
        ------
        MemoryError
            If the C call returns ``DP_ERR_MEMORY``. The exception message is
            ``set_state: the blob's carrier needs the delay-Doppler map
            rebuilt, and the rebuild could not allocate; the engine is
            unchanged``.
        ValueError
            If the C core rejects the blob with any other non-zero status. The
            exception message is ``set_state rejected the blob``.
        """

    @property
    def flavor(self) -> Literal["1d", "2d", "delay_doppler", "detector_1d", "detector_2d", "continuous", "burst"]:
        """Which flavor the knobs built: '1d', '2d' or 'delay_doppler' (maps),
        'detector_1d' or 'detector_2d', 'continuous' or 'burst' (searches). Ask
        it before reading a member only some flavors serve.
        """

    @property
    def last_corr(self) -> NDArray[np.complex64] | None:
        """A detector's correlation vector from the most recent push() that
        produced a result (None before that, and after reset() or set_state());
        AttributeError on any flavor but a detector. A zero-copy view into a
        buffer the detector reuses every push(): copy it before the next push()
        to keep it.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_last_corr_status`` returns non-zero
            before the view is built. The exception message is ``last_corr:
            only a detector (framing='none', detect=True) has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def keep_surface(self) -> int:
        """1 keeps every decided dwell's surface for surface() (normalized into
        the gate's units); 0 (the default) costs nothing. The continuous search
        only: elsewhere a read or a write raises AttributeError.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_keep_surface`` returns non-zero
            instead of writing a value. The exception message is
            ``keep_surface: only the continuous search (framing='continuous')
            keeps a surface``, with the return code appended.
        AttributeError
            On assignment, if ``dp_correlator_set_keep_surface`` refuses the
            value by returning non-zero. The exception message is
            ``keep_surface: only the continuous search (framing='continuous')
            keeps a surface``, with the return code appended.
        """
    @keep_surface.setter
    def keep_surface(self, value: int) -> None: ...
    @property
    def ny(self) -> int:
        """Rows of the reference: 1 for a 1-D one; the delay-Doppler map's
        Doppler rows (doppler_rows).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_ny`` returns non-zero instead of
            writing a value. The exception message is ``ny: only a map or a
            detector (framing='none') has it; Correlator.flavor says which this
            is``, with the return code appended.
        """

    @property
    def nx(self) -> int:
        """Samples a reference row: the correlation's native lag count.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_nx`` returns non-zero instead of
            writing a value. The exception message is ``nx: only a map or a
            detector (framing='none') has it; Correlator.flavor says which this
            is``, with the return code appended.
        """

    @property
    def ny_out(self) -> int:
        """Rows a dump writes: ny unless padded (ny_out), which interpolates
        the Doppler or row axis.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_ny_out`` returns non-zero instead
            of writing a value. The exception message is ``ny_out: only a map
            (framing='none', detect=False) has it; Correlator.flavor says which
            this is``, with the return code appended.
        """

    @property
    def nx_out(self) -> int:
        """Lags a dump writes per row: nx unless padded (nx_out), which
        interpolates the lag axis. The one padding name at any row count.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_nx_out`` returns non-zero instead
            of writing a value. The exception message is ``nx_out: only the 1-D
            or 2-D map (framing='none', detect=False, doppler_rows=0) has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def col_out(self) -> int:
        """The one lag a dump emits per row, or -1 for every lag.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_col_out`` returns non-zero instead
            of writing a value. The exception message is ``col_out: only the
            1-D or 2-D map (framing='none', detect=False, doppler_rows=0) has
            it; Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def doppler_step(self) -> float:
        """Doppler bin width of the delay-Doppler map, cycles per sample.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_doppler_step`` returns non-zero
            instead of writing a value. The exception message is
            ``doppler_step: only the delay-Doppler map (doppler_rows > 0) has
            it; Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def n(self) -> int:
        """Frame length in samples: the reference's length, ny * nx.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_n`` returns non-zero instead of
            writing a value. The exception message is ``n: only the 1-D map or
            a detector (detect=True) has it; Correlator.flavor says which this
            is``, with the return code appended.
        """

    @property
    def dwell(self) -> int:
        """Integration depth: a dump every `dwell` frames.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_dwell`` returns non-zero instead of
            writing a value. The exception message is ``dwell: only a map or a
            detector (framing='none') has it; Correlator.flavor says which this
            is``, with the return code appended.
        """

    @property
    def count(self) -> int:
        """Frames accumulated toward the next dump (0 ... dwell - 1).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_count`` returns non-zero instead of
            writing a value. The exception message is ``count: only a map or a
            detector (framing='none') has it; Correlator.flavor says which this
            is``, with the return code appended.
        """

    @property
    def ring_cap(self) -> int:
        """Input ring capacity in complex samples.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_ring_cap`` returns non-zero instead
            of writing a value. The exception message is ``ring_cap: only a
            detector (detect=True) or a search (framing='continuous' or
            'burst') has it; Correlator.flavor says which this is``, with the
            return code appended.
        """

    @property
    def noise_lo(self) -> int:
        """First noise (CFAR reference) bin, inclusive.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_noise_lo`` returns non-zero instead
            of writing a value. The exception message is ``noise_lo: only a
            detector (detect=True) or a search (framing='continuous' or
            'burst') has it; Correlator.flavor says which this is``, with the
            return code appended.
        """

    @property
    def noise_hi(self) -> int:
        """Last noise (CFAR reference) bin, inclusive.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_noise_hi`` returns non-zero instead
            of writing a value. The exception message is ``noise_hi: only a
            detector (detect=True) or a search (framing='continuous' or
            'burst') has it; Correlator.flavor says which this is``, with the
            return code appended.
        """

    @property
    def threshold(self) -> float:
        """The gate on the test statistic: a detector's as given (0 fires on
        every dump); a search's derived from pfa (the coherent path's gate).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_threshold`` returns non-zero
            instead of writing a value. The exception message is ``threshold:
            only a detector (detect=True) or a search (framing='continuous' or
            'burst') has it; Correlator.flavor says which this is``, with the
            return code appended.
        """

    @property
    def max_peaks(self) -> int:
        """The peak list's capacity per dwell (1 = the classic gated maximum);
        set with set_max_peaks().

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_max_peaks`` returns non-zero
            instead of writing a value. The exception message is ``max_peaks:
            only a search (framing='continuous' or 'burst') has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def code_bins(self) -> int:
        """One period of the reference in samples: the length of every forward
        transform and of a raw epoch.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_code_bins`` returns non-zero
            instead of writing a value. The exception message is ``code_bins:
            only a search (framing='continuous' or 'burst') has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def doppler_bins(self) -> int:
        """Doppler hypotheses searched, window tiles times the coherent depth:
        the count to fold a hit's row over (bin_to_signed(row, doppler_bins) *
        doppler_res_hz is the signed Doppler).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_doppler_bins`` returns non-zero
            instead of writing a value. The exception message is
            ``doppler_bins: only a search (framing='continuous' or 'burst') has
            it; Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks per detection (1 = pure coherent).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_n_noncoh`` returns non-zero instead
            of writing a value. The exception message is ``n_noncoh: only a
            search (framing='continuous' or 'burst') has it; Correlator.flavor
            says which this is``, with the return code appended.
        """

    @property
    def eta(self) -> float:
        """Raw per-cell Rayleigh amplitude threshold.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_eta`` returns non-zero instead of
            writing a value. The exception message is ``eta: only a search
            (framing='continuous' or 'burst') has it; Correlator.flavor says
            which this is``, with the return code appended.
        """

    @property
    def eta_nc(self) -> float:
        """Non-coherent CFAR threshold (order-N_nc Marcum).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_eta_nc`` returns non-zero instead
            of writing a value. The exception message is ``eta_nc: only a
            search (framing='continuous' or 'burst') has it; Correlator.flavor
            says which this is``, with the return code appended.
        """

    @property
    def pfa_cell(self) -> float:
        """Sidak per-cell false-alarm probability over the searched cells.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_pfa_cell`` returns non-zero instead
            of writing a value. The exception message is ``pfa_cell: only a
            search (framing='continuous' or 'burst') has it; Correlator.flavor
            says which this is``, with the return code appended.
        """

    @property
    def pd_predicted(self) -> float:
        """Predicted Pd at cn0_dbhz and the chosen grid, averaged over the
        straddle priors (slow-time scalloping, intra-segment rotation,
        code-phase sample offset): what the Monte-Carlo characterization
        measures, not the on-grid best case.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_pd_predicted`` returns non-zero
            instead of writing a value. The exception message is
            ``pd_predicted: only a search (framing='continuous' or 'burst') has
            it; Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def straddle_loss(self) -> float:
        """Mean amplitude derating of the correlation peak from grid straddle,
        a diagnostic; 20*log10(straddle_loss) is the loss in dB.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_straddle_loss`` returns non-zero
            instead of writing a value. The exception message is
            ``straddle_loss: only a search (framing='continuous' or 'burst')
            has it; Correlator.flavor says which this is``, with the return
            code appended.
        """

    @property
    def sample_rate(self) -> float:
        """Sample rate the search runs at, Hz: the constructor's sample_rate.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_sample_rate`` returns non-zero
            instead of writing a value. The exception message is ``sample_rate:
            only a search (framing='continuous' or 'burst') has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def cn0_dbhz(self) -> float:
        """Carrier-to-noise density the search was sized for, dB-Hz; NaN for no
        design point (the burst search only).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_cn0_dbhz`` returns non-zero instead
            of writing a value. The exception message is ``cn0_dbhz: only a
            search (framing='continuous' or 'burst') has it; Correlator.flavor
            says which this is``, with the return code appended.
        """

    @property
    def doppler_span_hz(self) -> float:
        """Native unambiguous Doppler half-range, Hz: sample_rate / (2 *
        code_bins).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_doppler_span_hz`` returns non-zero
            instead of writing a value. The exception message is
            ``doppler_span_hz: only a search (framing='continuous' or 'burst')
            has it; Correlator.flavor says which this is``, with the return
            code appended.
        """

    @property
    def doppler_res_hz(self) -> float:
        """Doppler bin width, Hz.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_doppler_res_hz`` returns non-zero
            instead of writing a value. The exception message is
            ``doppler_res_hz: only a search (framing='continuous' or 'burst')
            has it; Correlator.flavor says which this is``, with the return
            code appended.
        """

    @property
    def pd(self) -> float:
        """Target detection probability.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_pd`` returns non-zero instead of
            writing a value. The exception message is ``pd: only a search
            (framing='continuous' or 'burst') has it; Correlator.flavor says
            which this is``, with the return code appended.
        """

    @property
    def underpowered(self) -> bool:
        """True when the search cannot meet pd at cn0_dbhz and its geometry:
        pd_predicted < pd for the continuous search, pd_burst < pd for the
        burst search. It still builds a best-effort grid, and construction
        warns (UserWarning).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_underpowered`` returns non-zero
            instead of writing a value. The exception message is
            ``underpowered: only a search (framing='continuous' or 'burst') has
            it; Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def threads(self) -> int:
        """Workers the search fans its tiles across, the calling thread
        included; 1 = serial. Set with set_threads().

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_threads`` returns non-zero instead
            of writing a value. The exception message is ``threads: only a
            search (framing='continuous' or 'burst') has it; Correlator.flavor
            says which this is``, with the return code appended.
        """

    @property
    def lag_bins(self) -> int:
        """Code-phase columns of the surface: code_bins unless code_bins is not
        7-smooth, then the next power of two (an exact interpolation).
        surface_chip_phase() gives each column in chips.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_lag_bins`` returns non-zero instead
            of writing a value. The exception message is ``lag_bins: only the
            continuous search (framing='continuous') has it; Correlator.flavor
            says which this is``, with the return code appended.
        """

    @property
    def coherent_bins(self) -> int:
        """The block-coherent depth D inside every window tile: epochs per
        block and Doppler rows per tile. 1 without a code-only window.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_coherent_bins`` returns non-zero
            instead of writing a value. The exception message is
            ``coherent_bins: only the continuous search (framing='continuous')
            has it; Correlator.flavor says which this is``, with the return
            code appended.
        """

    @property
    def sf(self) -> int:
        """Chips per code period: len(ref).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_sf`` returns non-zero instead of
            writing a value. The exception message is ``sf: only the continuous
            search (framing='continuous') has it; Correlator.flavor says which
            this is``, with the return code appended.
        """

    @property
    def chip_rate(self) -> float:
        """Chip rate, Hz.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_chip_rate`` returns non-zero
            instead of writing a value. The exception message is ``chip_rate:
            only the continuous search (framing='continuous') has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def symbol_rate(self) -> float:
        """Data-symbol rate the search was built with, Hz (0 = no known data
        clock): a diagnostic, never sizing.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_symbol_rate`` returns non-zero
            instead of writing a value. The exception message is ``symbol_rate:
            only the continuous search (framing='continuous') has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def epochs_per_symbol(self) -> float:
        """(chip_rate / sf) / symbol_rate: code epochs per data symbol; 0 when
        symbol_rate is 0.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_epochs_per_symbol`` returns
            non-zero instead of writing a value. The exception message is
            ``epochs_per_symbol: only the continuous search
            (framing='continuous') has it; Correlator.flavor says which this
            is``, with the return code appended.
        """

    @property
    def n_peaks(self) -> int:
        """Picks in the last decided dwell, held twins included.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_n_peaks`` returns non-zero instead
            of writing a value. The exception message is ``n_peaks: only the
            continuous search (framing='continuous') has it; Correlator.flavor
            says which this is``, with the return code appended.
        """

    @property
    def n_held(self) -> int:
        """Picks of the last decided dwell held as same-code-phase twins rather
        than listed (docs/design/async-dsss-receiver.md section 7.1).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_n_held`` returns non-zero instead
            of writing a value. The exception message is ``n_held: only the
            continuous search (framing='continuous') has it; Correlator.flavor
            says which this is``, with the return code appended.
        """

    @property
    def peak_conc(self) -> float:
        """Concentration of the last dwell's strongest peak: its main lobe's
        power over its code-phase column's, across every Doppler row and tile.
        Near 1 for a clean single emitter; about 0.5 when a data transition
        splits it into twins (docs/design/async-dsss-receiver.md section 2.4).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_peak_conc`` returns non-zero
            instead of writing a value. The exception message is ``peak_conc:
            only the continuous search (framing='continuous') has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def carrier_freq_hz(self) -> float:
        """RF carrier the Doppler is coupled to, Hz (0.0 = uncoupled); set with
        set_carrier_freq_hz().

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_carrier_freq_hz`` returns non-zero
            instead of writing a value. The exception message is
            ``carrier_freq_hz: only the continuous search
            (framing='continuous') has it; Correlator.flavor says which this
            is``, with the return code appended.
        """

    @property
    def surface_rows(self) -> int:
        """Rows of the surface surface() returns: the Doppler axis in surface
        units; its columns are lag_bins.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_surface_rows`` returns non-zero
            instead of writing a value. The exception message is
            ``surface_rows: only the continuous search (framing='continuous')
            has it; Correlator.flavor says which this is``, with the return
            code appended.
        """

    @property
    def surface_at(self) -> int:
        """samples_consumed of the dwell whose surface surface() returns (0
        until one has been captured).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_surface_at`` returns non-zero
            instead of writing a value. The exception message is ``surface_at:
            only the continuous search (framing='continuous') has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def reps(self) -> int:
        """Preamble repetitions a burst holds: the coherence ceiling.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_reps`` returns non-zero instead of
            writing a value. The exception message is ``reps: only the burst
            search (framing='burst') has it; Correlator.flavor says which this
            is``, with the return code appended.
        """

    @property
    def pd_burst(self) -> float:
        """Predicted Pd of one burst of reps repetitions at cn0_dbhz: the
        number the sizer meets pd with. NaN with no design cn0_dbhz or with
        n_noncoh > 1.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_pd_burst`` returns non-zero instead
            of writing a value. The exception message is ``pd_burst: only the
            burst search (framing='burst') has it; Correlator.flavor says which
            this is``, with the return code appended.
        """

    @property
    def psl_db(self) -> float:
        """The preamble's peak sidelobe level, dB: its periodic
        autocorrelation's largest lag outside the mainlobe, relative to the
        peak (-inf for a perfect sequence such as Zadoff-Chu).

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_psl_db`` returns non-zero instead
            of writing a value. The exception message is ``psl_db: only the
            burst search (framing='burst') has it; Correlator.flavor says which
            this is``, with the return code appended.
        """

    @property
    def doppler_rate(self) -> float:
        """Doppler rate the coherent depth is bounded against, Hz/s (0 = no
        bound); with rate_hypotheses > 1, the span of the hypotheses.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_doppler_rate`` returns non-zero
            instead of writing a value. The exception message is
            ``doppler_rate: only the burst search (framing='burst') has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    @property
    def rate_hypotheses(self) -> int:
        """Doppler-rate hypotheses searched: as given, or the engine's choice
        when built with 0; 1 is none.

        Raises
        ------
        AttributeError
            On read, if ``dp_correlator_get_rate_hypotheses`` returns non-zero
            instead of writing a value. The exception message is
            ``rate_hypotheses: only the burst search (framing='burst') has it;
            Correlator.flavor says which this is``, with the return code
            appended.
        """

    def destroy(self) -> None:
        """Close the object; the memory goes with its last user.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this, or use the object as a context manager,
        to end its use at a definite point.

        Views this object handed out keep the memory they point into, as does a
        call still running on another thread: it is freed when the last of
        them, and this object, are gone, not here (gh-2187). Idempotent:
        calling it again does nothing. Every other method raises
        ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "Correlator":
        """Enter a context manager, returning this object.

        Lets a Correlator be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Correlator
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the Correlator.

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
