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
    constellation : Literal["bpsk", "qpsk", "qam16", "qam64", "auto"], default "auto"
        The constellation: BPSK, QPSK, 16-QAM or 64-QAM, one to six bits per
        subcarrier. `auto` (the default) takes it from `mbps`, or the 36 Mb/s
        pair (16-QAM, 3/4) when no rate is given; it is given with `code_rate`,
        or neither is.
        One of ``"bpsk"``, ``"qpsk"``, ``"qam16"``, ``"qam64"``, ``"auto"``.
    code_rate : Literal["r12", "r23", "r34", "r56", "auto"], default "auto"
        The BCC rate: `r12` is 1/2, `r23` is 2/3, `r34` is 3/4 and `r56` is
        5/6. `auto` (the default) as for `constellation`.
        One of ``"r12"``, ``"r23"``, ``"r34"``, ``"r56"``, ``"auto"``.
    mbps : int, default 0
        The data rate at 20 MHz in Mb/s, as Table 17-6 prints it: 6, 9, 12, 18,
        24, 36, 48 or 54, a second spelling of `constellation` and `code_rate`,
        which are then left at `auto`. 0 (the default) gives none, and with no
        pair either the rate is 36 Mb/s. The object holds no clocking: the same
        PPDU played at 10 or 5 MHz carries the rate halved or quartered.
    window : int, default WLAN_WINDOW_STANDARD
        The default is the standard's 100 ns transition, `2 * oversample`
        samples; 0 is no window. Otherwise the window length in samples at the
        OUTPUT rate, 0 to 80 times `oversample`: the cross-fade at each OFDM
        symbol boundary of the PPDUs `ppdu` renders. Above the `16 *
        oversample` guard interval the cross-fade reaches into the symbol body.
    oversample : int, default 1
        `M`, 1 to 128: the output rate over the 20 MHz clock. The PPDU is
        rendered by a transform `M` times longer, the same subcarriers, not
        resampled.

    Raises
    ------
    ValueError
        If construction fails. The exception message is the reason the
        constructor gives, or ``the rate does not resolve (half a pair, a rate
        beside a pair, or a rate Table 17-6 does not list), constellation is
        not one of bpsk, qpsk, qam16, qam64, code_rate is not one of r12, r23,
        r34, r56, window is not 0 to 80 * oversample, or oversample is not 1 to
        128`` when it gives none.

    Examples
    --------
    >>> from doppler.wlan import Wlan
    >>> w = Wlan(constellation="qam16", code_rate="r34")  # 36 Mb/s at 20 MHz
    >>> w.n_bpsc, w.n_cbps, w.n_dbps
    (4, 192, 144)
    >>> Wlan(constellation="qam256")
    Traceback (most recent call last):
        ...
    ValueError: constellation must be one of "bpsk", "qpsk", "qam16", "qam64", "auto", got 'qam256'

    """

    def __init__(
        self,
        constellation: Literal["bpsk", "qpsk", "qam16", "qam64", "auto"] = "auto",
        code_rate: Literal["r12", "r23", "r34", "r56", "auto"] = "auto",
        mbps: int = 0,
        window: int = ...,
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
            Receives dp_wlan_ppdu_max_out() samples.

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
        >>> x.dtype, x.shape  # 160 + 160 + 80 + 80 * 6, plus the 2-sample window
        (dtype('complex64'), (882,))
        >>> w2 = Wlan(constellation="qam16", code_rate="r34", oversample=2)
        >>> w2.ppdu(psdu, 93).shape  # twice as fine, and a 4-sample window
        (1764,)
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

    def signal_bits(
        self,
        psdu_len: int,
        out: npt.NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """The SIGNAL field's 24 bits for a PSDU of `psdu_len` octets at this
        rate: RATE, a reserved 0, LENGTH least significant bit first, an
        even-parity bit and six zero tail bits (Figure 17-5). One bit per
        element. Refuses with `ValueError` naming why.

        Parameters
        ----------
        psdu_len : int
            PSDU octets, 1 to WLAN_PSDU_MAX.
        out : npt.NDArray[np.uint8] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[np.uint8]
            The bits written, or -1 if the rate has no SIGNAL RATE bits, the
            length is out of range or max_out is short.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            the reason the C call gives, or ``signal_bits refused``, with the
            return code appended (gh-869), when it gives none.

        Examples
        --------
        >>> from doppler.wlan import Wlan
        >>> w = Wlan(constellation="qam16", code_rate="r34")  # 36 Mb/s
        >>> w.signal_bits(100).tolist()  # RATE, 0, LENGTH lsb first, parity, tail
        [1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        >>> w.signal_bits(0)
        Traceback (most recent call last):
            ...
        ValueError: the PSDU is 1 to 4095 octets

        """

    def signal_bits_max_out(self) -> int:
        """The capacity dp_wlan_signal_bits_why() needs: WLAN_SIG_BITS.

        Returns
        -------
        int
            WLAN_SIG_BITS.
        """

    def data_bits(
        self,
        x: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        scrambler_state: int,
        out: npt.NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """The Data field's bits for a PSDU at this rate: SERVICE, the PSDU
        (each octet least significant bit first), tail and pad, scrambled from
        `scrambler_state` except the tail. One bit per element, `n_sym *
        n_dbps` of them. Refuses with `ValueError` naming why.

        Parameters
        ----------
        x : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            The PSDU octets.
        scrambler_state : int
            The scrambler register's initial state, 1 to 127.
        out : npt.NDArray[np.uint8] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[np.uint8]
            The bits written, or -1 if the length or the state is out of range
            or max_out is short.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            the reason the C call gives, or ``data_bits refused``, with the
            return code appended (gh-869), when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wlan import Wlan
        >>> w = Wlan(constellation="qam16", code_rate="r34")
        >>> psdu = np.arange(100, dtype=np.uint8)
        >>> w.data_bits(psdu, 93).shape  # n_sym 6 times n_dbps 144
        (864,)
        >>> w.data_bits(psdu, 128)
        Traceback (most recent call last):
            ...
        ValueError: the scrambler state is 1 to 127

        """

    def data_bits_max_out(self, n_in: int, /) -> int:
        """The capacity dp_wlan_data_bits_why() needs: `n_sym * n_dbps`.

        Parameters
        ----------
        n_in : int
            PSDU octets.

        Returns
        -------
        int
            Bits, or 0 for a length outside 1 to WLAN_PSDU_MAX.
        """

    def map(
        self,
        x: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """Map interleaved bits (one per element, a whole number of groups of
        `n_bpsc`) to this rate's constellation points: unit average power, with
        the standard's bit encodings. Refuses with `ValueError` naming why.

        Parameters
        ----------
        x : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            Unpacked bits.
        out : npt.NDArray[np.complex64] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[np.complex64]
            The points written, or -1 for a partial group, no bits or a short
            max_out.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            the reason the C call gives, or ``map refused``, with the return
            code appended (gh-869), when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wlan import Wlan
        >>> w = Wlan(constellation="qam16", code_rate="r34")
        >>> pts = w.map(np.array([0, 0, 0, 0, 1, 1, 1, 1], dtype=np.uint8))
        >>> bool(np.allclose(pts, [-0.9487 - 0.9487j, 0.3162 + 0.3162j], atol=1e-3))
        True
        >>> w.map(np.zeros(7, dtype=np.uint8))
        Traceback (most recent call last):
            ...
        ValueError: the bits are a whole number of groups of n_bpsc

        """

    def map_max_out(self, n_in: int, /) -> int:
        """The capacity dp_wlan_map_why() needs: `n_in / n_bpsc` points.

        Parameters
        ----------
        n_in : int
            Bits.

        Returns
        -------
        int
            Points.
        """

    def points(
        self,
        x: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        scrambler_state: int,
        out: npt.NDArray[np.complex64] | None = None,
    ) -> NDArray[np.complex64]:
        """The constellation points a PSDU's layout takes, in layout order: the
        SIGNAL field's 48, then the Data field's `48 * n_sym`, as complex64.
        The input to `Ofdm`, and what `ppdu` modulates. Refuses with
        `ValueError` naming why.

        Parameters
        ----------
        x : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            The PSDU octets.
        scrambler_state : int
            The scrambler register's initial state, 1 to 127.
        out : npt.NDArray[np.complex64] | None
            Optional pre-allocated output buffer. When given, the result is
            written into it and the returned array is a view of exactly the
            samples produced; when omitted, a fresh array is allocated.

        Returns
        -------
        NDArray[np.complex64]
            The points written, or -1 if the rate has no SIGNAL RATE bits, the
            length or the state is out of range or max_out is short.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            the reason the C call gives, or ``points refused``, with the return
            code appended (gh-869), when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wlan import Wlan
        >>> w = Wlan(constellation="qam16", code_rate="r34")
        >>> psdu = np.arange(100, dtype=np.uint8)
        >>> w.points(psdu, 93).shape  # 48 SIGNAL + 48 * 6 Data
        (336,)
        >>> w.points_max_out(100)
        336
        >>> w.points(psdu, 0)
        Traceback (most recent call last):
            ...
        ValueError: the scrambler state is 1 to 127

        """

    def points_max_out(self, n_in: int, /) -> int:
        """The capacity dp_wlan_points_why() needs: `48 + 48 * n_sym`.

        Parameters
        ----------
        n_in : int
            PSDU octets.

        Returns
        -------
        int
            Points, or 0 for a length outside 1 to WLAN_PSDU_MAX.
        """

    def mpdu(
        self,
        x: npt.NDArray[np.uint8] | bytes | bytearray | memoryview,
        header: npt.NDArray[np.uint8] | bytes | bytearray | memoryview = ...,
        out: npt.NDArray[np.uint8] | None = None,
    ) -> NDArray[np.uint8]:
        """A data frame's PSDU from its body and, optionally, its MAC header:
        the `header` octets, then the `body`, then the FCS (CRC-32/ISO-HDLC
        over everything before it, least significant octet first). Without a
        header, or with an empty one, the body is taken as an MPDU that lacks
        its FCS and the result is the body and its FCS. There is no spelling
        that returns the body unchanged: that is the body itself. 1 to 4095
        octets in all. Refuses with `ValueError` naming why.

        With no header the body is taken as an MPDU that lacks its FCS and the
        result is the body and its FCS, which is also what an empty header
        gives in data mode, so "no header" and "an empty header" are one
        result. There is no way to ask for the body unchanged: WLAN_MAC_NONE is
        the body itself. The MPDU does not depend on the rate; it is on the
        object so a caller builds a frame and renders it with one handle.

        Parameters
        ----------
        x : npt.NDArray[np.uint8] | bytes | bytearray | memoryview
            The body octets (the whole MPDU without its FCS when there is no
            header).
        header : npt.NDArray[np.uint8] | bytes | bytearray | memoryview, default None
            Header octets, or NULL.
        out : npt.NDArray[np.uint8] | None
            Receives the header, the body and the FCS.

        Returns
        -------
        NDArray[np.uint8]
            The octets written, or -1 if the MPDU is 0 or above WLAN_PSDU_MAX
            octets or max_out is short.

        Raises
        ------
        ValueError
            If the C call returns a negative value. The exception message is
            the reason the C call gives, or ``mpdu refused``, with the return
            code appended (gh-869), when it gives none.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.wlan import Wlan
        >>> w = Wlan()
        >>> header = np.arange(24, dtype=np.uint8)
        >>> body = np.arange(10, dtype=np.uint8)
        >>> psdu = w.mpdu(body, header)  # 24 + 10 + the 4-octet FCS
        >>> psdu.shape
        (38,)
        >>> bytes(psdu[-4:]).hex()  # CRC-32/ISO-HDLC, least significant first
        'c0958e66'
        >>> w.mpdu(body).shape  # no header: the body and its FCS
        (14,)

        """

    def mpdu_max_out(self, n_in: int, /) -> int:
        """The capacity dp_wlan_mpdu_why() needs: WLAN_PSDU_MAX, the longest
        PSDU.

        Parameters
        ----------
        n_in : int
            Unused.

        Returns
        -------
        int
            WLAN_PSDU_MAX.
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
