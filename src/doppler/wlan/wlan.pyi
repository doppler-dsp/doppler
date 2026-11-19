# wlan/wlan.pyi — type stubs for the wlan C extension.
from typing import Literal, final

@final
class Wlan:
    """Create a rate holder, by the index of the constellation and of the code
    rate.

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

    Raises
    ------
    ValueError
        If construction fails. The exception message is ``constellation is not
        one of bpsk, qpsk, qam16, qam64, code_rate is not one of r12, r23, r34,
        r56``.

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
