# wlan/wlan.pyi — type stubs for the wlan C extension.
from typing import Literal, final

import numpy as np
import numpy.typing as npt
from numpy.typing import NDArray

@final
class Wlan:
    """dp_wlan_create(), and the reason when it refuses. The binding calls this
    one.

    Parameters
    ----------
    constellation : Literal["bpsk", "qpsk", "qam16", "qam64"], default "qam16"
        The constellation: BPSK, QPSK, 16-QAM or 64-QAM, one to six bits per
        subcarrier.
        One of ``"bpsk"``, ``"qpsk"``, ``"qam16"``, ``"qam64"``.
    code_rate : Literal["r12", "r23", "r34", "r56"], default "r12"
        The BCC rate: `r12` is 1/2, `r23` is 2/3, `r34` is 3/4 and `r56` is
        5/6.
        One of ``"r12"``, ``"r23"``, ``"r34"``, ``"r56"``.
    window : int, default 0
        The window length in samples at the OUTPUT rate, 0 to 80 times
        `oversample`: the cross-fade at each OFDM symbol boundary of the PPDUs
        `ppdu` renders. The standard's 100 ns transition is `2 * oversample`
        samples; above the `16 * oversample` guard interval the cross-fade
        reaches into the symbol body.
    oversample : int, default 1
        `M`, 1 to 128: the output rate over the 20 MHz clock. The PPDU is
        rendered by a transform `M` times longer, the same subcarriers, not
        resampled.

    Raises
    ------
    ValueError
        If construction fails. The exception message is the reason the
        constructor gives, or ``constellation is not one of bpsk, qpsk, qam16,
        qam64, code_rate is not one of r12, r23, r34, r56, window is not 0 to
        80 * oversample, or oversample is not 1 to 128`` when it gives none.

    Examples
    --------
    >>> from doppler.wlan import Wlan
    >>> w = Wlan(constellation="qam16", code_rate="r34")  # 36 Mb/s at 20 MHz
    >>> w.n_bpsc, w.n_cbps, w.n_dbps
    (4, 192, 144)
    >>> Wlan(constellation="qam256")
    Traceback (most recent call last):
        ...
    ValueError: constellation must be one of "bpsk", "qpsk", "qam16", "qam64", got 'qam256'

    """

    def __init__(
        self,
        constellation: Literal["bpsk", "qpsk", "qam16", "qam64"] = "qam16",
        code_rate: Literal["r12", "r23", "r34", "r56"] = "r12",
        window: int = 0,
        oversample: int = 1,
    ) -> None: ...
    def reset(self) -> None:
        """Reset a rate holder: nothing to reset, the rate is configuration.

        Examples
        --------
        >>> from doppler.wlan import Wlan
        >>> w = Wlan(constellation="qpsk", code_rate="r34")
        >>> w.reset()                      # nothing runs, so nothing changes
        >>> w.n_dbps
        72

        """

    def ppdu(
        self,
        x: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        scrambler_state: int,
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Render a PSDU to a whole PPDU (L-STF, L-LTF, SIGNAL, Data) at this
        rate, window and oversampling, as complex64 samples. Refuses with
        `ValueError` naming why.

        The standard's tables are the library's (dp_wlan_tables_standard());
        the PSDU is the caller's octets as given, FCS included. The result is
        the dp_wlan_ppdu() output for the same arguments, with the refusal's
        reason.

        Parameters
        ----------
        x : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            The PSDU octets.
        scrambler_state : int
            The scrambler register's initial state, 1 to 127.
        out : npt.NDArray[np.complex64] | None
            Receives dp_wlan_ppdu_why_max_out() samples.

        Returns
        -------
        NDArray[np.complex64]
            The samples written, or -1 if the rate has no SIGNAL RATE bits, the
            PSDU length or the scrambler state is out of range, or max_out is
            short; out is then untouched. On the Python face a short `out=` is
            refused before the other arguments are, by the binding.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            the reason the C call gives, or ``ppdu refused``, with the return
            code appended (gh-869), when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wlan import Wlan
        >>> w = Wlan(constellation="qam16", code_rate="r34")  # 36 Mb/s
        >>> psdu = np.arange(100, dtype=np.uint8)
        >>> x = w.ppdu(psdu, 93)
        >>> x.dtype, x.shape  # 160 + 160 + 80 + 80 * 6 samples
        (dtype('complex64'), (880,))
        >>> w2 = Wlan(constellation="qam16", code_rate="r34", oversample=2)
        >>> w2.ppdu(psdu, 93).shape
        (1760,)
        >>> w.ppdu(psdu, 0)
        Traceback (most recent call last):
            ...
        ValueError: the scrambler state is 1 to 127

        """

    def ppdu_max_out(self, n_in: int, /) -> int:
        """The samples a PPDU of n_in PSDU octets occupies at this object's
        rate, window and oversampling: the capacity dp_wlan_ppdu_why() needs,
        exactly.

        Parameters
        ----------
        n_in : int
            PSDU octets, 1 to WLAN_PSDU_MAX.

        Returns
        -------
        int
            Samples, or 0 for a length outside that range or for a rate with no
            SIGNAL RATE bits (Table 17-6 lists eight of the sixteen pairs; the
            others cannot be sent, and `rate_mbps` is 0 for them).
        """

    def n_sym(self, psdu_len: int) -> int:
        """The Data OFDM symbols a PSDU of `psdu_len` octets needs at this
        rate, `ceil((16 + 8*psdu_len + 6) / n_dbps)`. 0 for a length of 0 or
        above 4095.

        Parameters
        ----------
        psdu_len : int
            Octets, 1 to WLAN_PSDU_MAX.

        Returns
        -------
        int
            The symbol count, or 0 for a length of 0 or above WLAN_PSDU_MAX.

        Examples
        --------
        >>> from doppler.wlan import Wlan
        >>> w = Wlan(constellation="qam16", code_rate="r34")
        >>> w.n_sym(100)
        6
        >>> w.n_sym(0)
        0

        """

    @property
    def n_bpsc(self) -> int:
        """Bits per subcarrier: 1 for BPSK, 2, 4 or 6."""

    @property
    def n_cbps(self) -> int:
        """Coded bits per OFDM symbol, 48 times `n_bpsc`."""

    @property
    def n_dbps(self) -> int:
        """Data bits per OFDM symbol: `n_cbps` times the code rate."""

    @property
    def rate_mbps(self) -> int:
        """The data rate at 20 MHz in Mb/s, as Table 17-6 prints it: 6, 9, 12,
        18, 24, 36, 48 or 54, and 0 for the eight pairs the table does not
        list.
        """

    @property
    def window(self) -> int:
        """The window length in samples at the output rate."""

    @property
    def oversample(self) -> int:
        """`M`: the output rate over the 20 MHz clock."""

    def destroy(self) -> None:
        """Release the underlying C resources immediately.

        Ordinarily unnecessary: the resources are freed when the object is
        garbage-collected. Call this to release them at a definite point
        instead, or use the object as a context manager, which calls it on
        exit.

        Idempotent: calling it again on an already-released object does
        nothing. Every other method raises ``RuntimeError`` once it has run.
        """

    def __enter__(self) -> "Wlan":
        """Enter a context manager, returning this object.

        Lets a Wlan be used in a `with` statement so its C resources are
        released deterministically on exit rather than at collection time.

        Returns
        -------
        Wlan
            This same object, not a copy.
        """

    def __exit__(
        self,
        exc_type: object | None = ...,
        exc: object | None = ...,
        tb: object | None = ...,
        /,
    ) -> None:
        """Exit a context manager, releasing the Wlan.

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
