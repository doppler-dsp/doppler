"""readback.py — load interleaved-I/Q captures written by the wfmgen CLI.

The generators write **interleaved** I/Q (``I Q I Q …``) in the chosen
``--sample-type``, so a naive ``np.fromfile`` gets the layout wrong — and, for
the integer types, the scale too. :func:`read_iq` does the right thing:

- ``cf32`` / ``cf64`` — the interleaved floats *are* the memory layout of a
  complex array, so the complex result is a **zero-copy** reinterpretation.
- ``ci8`` / ``ci16`` / ``ci32`` — full-scale integers with no complex-integer
  dtype, so a complex result needs a copy: a SIMD rescale to ±1.0 via the
  ``doppler.cvt`` converters (at the writer's exact full-scale), then combine.

Pass ``raw=True`` to skip all of that and get the **zero-copy** on-disk samples
as an ``(N, 2)`` array (column 0 = I, column 1 = Q) in the file's own dtype.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Literal

import numpy as np

from ..cvt import I8ToF32, I16ToF32, I32ToF32

if TYPE_CHECKING:
    from numpy.typing import NDArray

SampleType = Literal["cf32", "cf64", "ci32", "ci16", "ci8"]

# Float wire types: (complex dtype char, real dtype char). The complex form
# is a zero-copy reinterpretation of the interleaved reals.
_CPLX: dict[str, tuple[str, str]] = {
    "cf32": ("c8", "f4"),
    "cf64": ("c16", "f8"),
}

# Integer wire types: (int dtype char, writer full-scale, cvt converter). The
# scale matches wfmgen's quantizer (max-positive), so generate -> read_iq is
# bit-faithful.
_INT: dict[str, tuple[str, float, type]] = {
    "ci32": ("i4", 2147483647.0, I32ToF32),
    "ci16": ("i2", 32767.0, I16ToF32),
    "ci8": ("i1", 127.0, I8ToF32),
}


def read_iq(
    path: str,
    sample_type: SampleType = "cf32",
    endian: Literal["le", "be"] = "le",
    *,
    raw: bool = False,
) -> NDArray:
    """Read an interleaved-I/Q capture into a NumPy array.

    Parameters
    ----------
    path : str
        File written by the ``wfmgen`` CLI with ``--file-type raw`` (or a
        BLUE ``.det`` data file).
    sample_type : {"cf32", "cf64", "ci32", "ci16", "ci8"}
        The ``--sample-type`` the file was written with.
    endian : {"le", "be"}
        The ``--endian`` the file was written with.
    raw : bool, keyword-only
        If true, return the **zero-copy** on-disk samples as an ``(N, 2)``
        array
        (``[:, 0]`` = I, ``[:, 1]`` = Q) in the file's own dtype, with no
        rescale. Otherwise return a complex array (see Returns).

    Returns
    -------
    NDArray
        With ``raw=False`` (default): ``complex128`` for ``cf64``, else
        ``complex64`` — a zero-copy view for the float types, a rescaled copy
        for the integer types. With ``raw=True``: an ``(N, 2)`` array in the
        on-disk dtype.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.wfm.readback import read_iq
    >>> # round-trip a ci16 capture back to unit-scale complex64
    >>> iq = read_iq("capture.iq", "ci16")  # doctest: +SKIP
    >>> iq.dtype  # doctest: +SKIP
    dtype('complex64')
    """
    bo = "<" if endian == "le" else ">"
    if sample_type in _CPLX:
        cchar, rchar = _CPLX[sample_type]
        if raw:
            return np.fromfile(path, dtype=bo + rchar).reshape(-1, 2)
        # interleaved float32/float64 reinterpreted as complex — no
        # deinterleave
        return np.fromfile(path, dtype=bo + cchar)
    if sample_type in _INT:
        ichar, scale, conv = _INT[sample_type]
        ints = np.fromfile(path, dtype=bo + ichar)
        if raw:
            return ints.reshape(-1, 2)
        native = ints.astype(np.dtype(ichar))  # to native byte order for cvt
        flt = np.asarray(conv(scale=scale).steps(native))
        return (flt[0::2] + 1j * flt[1::2]).astype(np.complex64)
    raise ValueError(f"unknown sample_type {sample_type!r}")
