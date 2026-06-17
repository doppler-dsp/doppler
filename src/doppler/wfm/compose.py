"""Multi-segment waveform composition, file writers, and a ZMQ sink.

This is the Python face of the C ``wfmgen`` composer subsystem — the same engine
behind the ``wfmgen`` CLI, exposed here as classes. A :class:`Composer` strings
:class:`Segment` specs (tone / noise / PN / BPSK / QPSK, each with its own
on-time and trailing gap) into one stream, optionally looping (``repeat``) or
running forever (``continuous``). :class:`Writer` serialises samples to the same
containers as the CLI (raw interleaved I/Q, CSV, BLUE type-1000, SigMF), and
:class:`ZmqSink` publishes them over ZeroMQ. The composer's resolved spec
round-trips through JSON (:meth:`Composer.to_json` / :meth:`Composer.from_json`),
so a capture is fully reproducible.

The samples come back as ``complex64`` arrays; pair :class:`Writer` with
:func:`doppler.wfm.readback.read_iq` to round-trip a file.

The ``Synth`` / ``Segment`` / ``Timeline`` / ``Composer`` ergonomic types are the
**jm-generated** CPython types in ``doppler.wfm.wfm_compose`` (the composer lives
in the ``.so``; ``jm`` owns the binding). The classes here are thin Python
**subclasses** that add only what the generic generator cannot express: standalone
sample generation (:meth:`Synth.steps`), the ``pattern`` / ``f_start`` input sugar,
the flat single-source :class:`Segment` view, and :meth:`Composer.stream`. The
container writers/readers (BLUE / SigMF / ZMQ / sample-clock) stay hand-written
here, over the transport binding ``_wfmcompose``.

Examples
--------
>>> from doppler.wfm.compose import Composer, Segment
>>> spec = [Segment("pn", num_samples=127, pn_length=7),
...         Segment("tone", freq=1e5, num_samples=256, off_samples=64)]
>>> x = Composer(spec).compose()
>>> x.dtype, len(x)  # 127 (pn) + 256 (tone) + 64 (gap)
(dtype('complex64'), 447)
"""

from __future__ import annotations

import math
import os
from typing import Sequence

import numpy as np
from numpy.typing import NDArray

# _wfmcompose: transport binding (writers/readers/sink/clock/sigmf/DSP helpers).
# wfm_compose: the jm-generated composer OO types (Synth/Segment/…), in the .so.
from . import _wfmcompose as _c
from . import wfm_compose as _g
from .wfm import _SynthEngine  # internal C engine; Synth uses it standalone

# ── string-enum ↔ C-int tables (must match native/src/app/wfmgen.c) ──────────
_TYPES = ("tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits")
_MODES = ("auto", "fs", "ebno", "esno")
_PULSES = ("rect", "rrc")  # PSK pulse shape → C: rect (hold) / rrc (FIR)
_BITMODS = ("none", "bpsk", "qpsk")  # bits-pattern modulation → C bit_mod
_STYPES = ("cf32", "cf64", "ci32", "ci16", "ci8")
_FTYPES = ("raw", "csv", "blue", "sigmf")
_ENDIANS = ("le", "be")
_LFSRS = ("galois", "fibonacci")


def _parse_bits(pattern) -> np.ndarray:
    """Coerce a bit pattern to a uint8 array of 0/1.

    Accepts a binary string (``"10110101"``), a hex string prefixed ``0x``
    (``"0xAA55"`` → 16 bits, MSB first), or any array-like of 0/1.
    """
    if isinstance(pattern, str):
        s = pattern.strip()
        if s[:2].lower() == "0x":
            n = len(s) - 2
            val = int(s, 16)
            bits = [(val >> (4 * n - 1 - i)) & 1 for i in range(4 * n)]
            return np.array(bits, dtype=np.uint8)
        if set(s) - {"0", "1"}:
            raise ValueError(
                "bit string must be 0/1 (or '0x..' hex), got " + repr(pattern)
            )
        return np.frombuffer(s.encode(), dtype=np.uint8) - ord("0")
    arr = np.asarray(pattern, dtype=np.uint8).ravel()
    return (arr != 0).astype(np.uint8)


def _idx(name: str, table: tuple[str, ...], what: str) -> int:
    try:
        return table.index(name)
    except ValueError:
        raise ValueError(
            f"{what} must be one of {table!r}, got {name!r}"
        ) from None


def _bits_to_pattern(b):
    """A generated source's ``bits`` buffer (``bytes`` | ``None``) → pattern."""
    return np.frombuffer(b, dtype=np.uint8) if b else None


class Synth(_g.Synth):
    """One waveform — the single object that both **generates** and **composes**.

    A ``Synth`` *is* the configuration of a waveform (tone / noise / PN / BPSK /
    QPSK). Use it two ways with the same object:

    - **Generate now:** :meth:`steps` / :meth:`step` lazily spin the C engine and
      return samples. :meth:`reset` rewinds it.
    - **Compose:** pass one or more into :meth:`Segment.sum` to mix them over a
      shared noise floor. Composition reads the config only — it spins **no**
      engine, so summing N synths allocates nothing extra.

    The composition half (the field marshalling, the ``Composer`` that drives the
    C kernel) is the jm-generated :class:`doppler.wfm.wfm_compose.Synth` this
    subclasses; the standalone-generation half (:meth:`steps`) and the
    ``pattern`` / ``f_start`` input sugar are added here.

    Build with the :func:`tone` / :func:`qpsk` / :func:`bpsk` / :func:`pn` /
    :func:`noise` helpers, or construct directly.

    Parameters
    ----------
    type : {"tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits"}
        Waveform kind (``"noise"`` is a bare AWGN floor at ``level`` dBFS;
        ``"chirp"`` is a linear-FM sweep — see ``f_end``).
    fs : float
        Sample rate (Hz) for standalone generation. Ignored when the synth is
        used in a :class:`Segment` — the segment owns the rate.
    freq : float
        Carrier/offset frequency (Hz). For a ``chirp`` this is the **start**
        frequency f_start; ``f_start=`` is accepted as an alias.
    f_end : float
        Chirp **end** frequency (Hz); used only when ``type="chirp"``.
    f_start : float, optional
        Readable alias for ``freq`` (chirp start frequency); folded into
        ``freq`` at construction when given.
    snr : float
        SNR in dB under ``snr_mode``; ``100`` (the default) is numerically clean.
    snr_mode : {"auto", "fs", "ebno", "esno"}
        How ``snr`` is interpreted; ``auto`` resolves per ``type``.
    seed : int
        PRNG / LFSR seed.
    sps, pn_length, pn_poly, lfsr
        PN/PSK data-source parameters (see :class:`Segment`).
    level : float
        Level in dBFS (``<= 0``) applied **in composition**; standalone
        :meth:`steps` generates at unit power, byte-identical to the bare engine.
    pattern : str or array-like, optional
        For ``type="bits"``: a binary string (``"10110101"``), a hex string
        (``"0xAA55"``, MSB first), or any array-like of 0/1.

    Examples
    --------
    >>> from doppler.wfm import Synth
    >>> Synth(type="tone", fs=1.0, freq=0.0).steps(4).tolist()
    [(1+0j), (1+0j), (1+0j), (1+0j)]
    >>> # an up-chirp sweeping 100 kHz → 300 kHz over 4096 samples
    >>> Synth(type="chirp", fs=1e6, f_start=1e5, f_end=3e5).steps(4096).shape
    (4096,)
    """

    def __init__(
        self,
        type: str = "tone",
        fs: float = 1e6,
        freq: float = 0.0,
        f_end: float = 0.0,
        snr: float = 100.0,
        snr_mode: str = "auto",
        seed: int = 1,
        sps: int = 8,
        pn_length: int = 7,
        pn_poly: int = 0,
        lfsr: str = "galois",
        level: float = 0.0,
        f_start: float | None = None,
        pattern: object = None,
        modulation: str = "bpsk",
        pulse: str = "rect",
        rrc_beta: float = 0.35,
        rrc_span: int = 8,
    ) -> None:
        # ``f_start`` is sugar for ``freq`` (a chirp's start frequency *is* the
        # carrier offset at t=0); fold it in so there is one stored value.
        if f_start is not None:
            freq = f_start
        if type == "bits" and pattern is None:
            raise ValueError("type='bits' needs a pattern")
        # ``pattern`` (str / hex / array) → the generated type's ``bits`` bytes.
        bits = (
            _parse_bits(pattern).tobytes()
            if (type == "bits" and pattern is not None)
            else None
        )
        # The generated tp_init validates the enum fields and populates the C
        # source struct that Composer reads.
        super().__init__(
            type=type,
            freq=freq,
            f_end=f_end,
            snr=snr,
            snr_mode=snr_mode,
            seed=seed,
            sps=sps,
            pn_length=pn_length,
            pn_poly=pn_poly,
            lfsr=lfsr,
            level=level,
            bits=bits,
            modulation=modulation,
            pulse=pulse,
            rrc_beta=rrc_beta,
            rrc_span=rrc_span,
            fs=fs,
        )
        self._eng = None

    @property
    def n_samples(self) -> int:
        """One full pass of a ``bits`` pattern, in samples (``n_bits*sps``,
        halved for qpsk). 0 for the streaming types (no natural length)."""
        if self.type != "bits":
            return 0
        nb = len(self.bits) if self.bits else 0
        per_sym = 2 if self.modulation == "qpsk" else 1
        return (nb // per_sym) * self.sps

    def _engine(self):
        """Build the backing C engine (no ``level`` — that is composition)."""
        eng = _SynthEngine(
            type=self.type,
            fs=self.fs,
            freq=self.freq,
            f_end=self.f_end,
            snr=self.snr,
            snr_mode=self.snr_mode,
            seed=self.seed,
            sps=self.sps,
            pn_length=self.pn_length,
            pn_poly=self.pn_poly,
            lfsr=self.lfsr,
        )
        if self.type == "bits":
            eng.set_bits(
                np.frombuffer(self.bits, dtype=np.uint8),
                _idx(self.modulation, _BITMODS, ""),
            )
        # RRC pulse shaping (pn/bpsk/qpsk only): pass the raw unit-energy taps;
        # the C set_rrc scales them by sqrt(sps) for unit transmit power, so the
        # standalone and composer faces are byte-identical.
        if self.pulse == "rrc" and self.type in ("pn", "bpsk", "qpsk"):
            eng.set_rrc(rrc_taps(self.rrc_beta, self.sps, self.rrc_span))
        return eng

    def _lazy(self):
        eng = getattr(self, "_eng", None)
        if eng is None:
            eng = self._eng = self._engine()
        return eng

    def steps(self, n: int) -> NDArray[np.complex64]:
        """Generate ``n`` cf32 samples (spins the engine on first use)."""
        return self._lazy().steps(n)

    def step(self):
        """Generate one cf32 sample (spins the engine on first use)."""
        return self._lazy().step()

    def reset(self) -> None:
        """Rewind the engine so generation repeats from sample 0 (no-op if it
        has not generated yet)."""
        eng = getattr(self, "_eng", None)
        if eng is not None:
            eng.reset()


def tone(freq: float = 0.0, **kw) -> Synth:
    """A continuous-wave tone :class:`Synth` at ``freq`` Hz."""
    return Synth(type="tone", freq=freq, **kw)


def bpsk(**kw) -> Synth:
    """A BPSK :class:`Synth`."""
    return Synth(type="bpsk", **kw)


def qpsk(**kw) -> Synth:
    """A QPSK :class:`Synth`."""
    return Synth(type="qpsk", **kw)


def pn(**kw) -> Synth:
    """A PN/MLS chip :class:`Synth`."""
    return Synth(type="pn", **kw)


def noise(level: float = 0.0, **kw) -> Synth:
    """An AWGN noise-floor :class:`Synth` at ``level`` dBFS (the segment floor)."""
    return Synth(type="noise", level=level, **kw)


def chirp(f_start: float = 0.0, f_end: float = 1e5, **kw) -> Synth:
    """A linear-FM (LFM) chirp :class:`Synth` sweeping ``f_start`` → ``f_end``.

    The instantaneous frequency rises (or falls, if ``f_end < f_start``)
    linearly across the generated length — ``steps(n)`` standalone, or the
    segment's ``num_samples`` in composition — then holds at ``f_end``.

    Examples
    --------
    >>> from doppler.wfm import chirp
    >>> s = chirp(f_start=100e3, f_end=200e3, fs=1e6)
    >>> s.steps(10000).shape
    (10000,)
    """
    return Synth(type="chirp", freq=f_start, f_end=f_end, **kw)


def bits(pattern, modulation: str = "bpsk", **kw) -> Synth:
    """A user-bit-pattern :class:`Synth` (preambles / sync words / test vectors).

    ``pattern`` is a binary string (``"10110101"``), a hex string
    (``"0xAA55"``, MSB first), or any array-like of 0/1. ``modulation`` maps the
    bits to symbols — ``"none"`` (0/1 amplitude), ``"bpsk"`` (±1), or ``"qpsk"``
    (two bits per symbol, Gray-coded). Each bit is held ``sps`` samples and the
    pattern **cycles** to fill the requested length; one pass is
    :attr:`Synth.n_samples`.

    Examples
    --------
    >>> from doppler.wfm import bits
    >>> s = bits("10110101", sps=4, modulation="bpsk")
    >>> s.n_samples
    32
    >>> s.steps(s.n_samples).shape
    (32,)
    """
    return Synth(type="bits", pattern=pattern, modulation=modulation, **kw)


class Segment(_g.Segment):
    """One waveform segment of a composed stream.

    Fields mirror the C ``wfm_segment_t`` and the ``wfmgen`` CLI flags; the
    string fields (``type``, ``snr_mode``, ``lfsr``) are mapped to the C enums.
    Defaults match the CLI: a clean 1024-sample baseband tone at 1 MS/s.

    A single-source segment carries its one source's fields inline (so
    ``segment.freq`` / ``segment.f_end`` / … read through to the source); a
    multi-source segment is built with :meth:`sum`. This subclasses the
    jm-generated :class:`doppler.wfm.wfm_compose.Segment`, adding the flat
    single-source view plus the ``pattern`` / ``f_start`` input sugar.

    Parameters
    ----------
    type : {"tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits"}
        Waveform kind.
    fs : float
        Sample rate (Hz).
    freq, f_end, snr, snr_mode, seed, sps, pn_length, pn_poly, lfsr
        Source parameters (see :class:`Synth`).
    num_samples : int
        On-time length of the segment (samples).
    off_samples : int
        Trailing zero gap after the segment (samples).
    level : float
        Segment level in dBFS (``<= 0``); the output is scaled by
        ``10 ** (level / 20)``. Default ``0`` is a bit-exact no-op.
    pattern : str or array-like, optional
        For ``type="bits"`` (see :class:`Synth`).
    """

    def __init__(
        self,
        type: str = "tone",
        fs: float = 1e6,
        freq: float = 0.0,
        f_end: float = 0.0,
        snr: float = 100.0,
        snr_mode: str = "auto",
        seed: int = 1,
        sps: int = 8,
        pn_length: int = 7,
        pn_poly: int = 0,
        lfsr: str = "galois",
        num_samples: int = 1024,
        off_samples: int = 0,
        level: float = 0.0,
        sources: list | None = None,
        f_start: float | None = None,
        pattern: object = None,
        modulation: str = "bpsk",
        pulse: str = "rect",
        rrc_beta: float = 0.35,
        rrc_span: int = 8,
    ) -> None:
        if sources is not None:
            raise TypeError(
                "build a multi-source segment with Segment.sum(), not sources="
            )
        if f_start is not None:
            freq = f_start
        if type == "bits" and pattern is None:
            raise ValueError("type='bits' needs a pattern")
        bits = (
            _parse_bits(pattern).tobytes()
            if (type == "bits" and pattern is not None)
            else None
        )
        # The generated single-source ctor forwards the source fields to build
        # one internal Synth and stores the segment scalars (fs/on/off).
        super().__init__(
            type=type,
            freq=freq,
            f_end=f_end,
            snr=snr,
            snr_mode=snr_mode,
            seed=seed,
            sps=sps,
            pn_length=pn_length,
            pn_poly=pn_poly,
            lfsr=lfsr,
            level=level,
            bits=bits,
            modulation=modulation,
            pulse=pulse,
            rrc_beta=rrc_beta,
            rrc_span=rrc_span,
            fs=fs,
            num_samples=num_samples,
            off_samples=off_samples,
        )

    def __getattr__(self, name):
        # Flat back-compat: a single-source segment exposes its source's fields
        # (type / freq / f_end / pulse / …) directly. Only fires for names not
        # on the base type (fs / num_samples / off_samples / sources resolve
        # normally, so no recursion).
        srcs = self.sources
        if len(srcs) == 1:
            return getattr(srcs[0], name)
        raise AttributeError(name)

    @classmethod
    def sum(
        cls,
        *sources: Synth,
        num_samples: int,
        off: int = 0,
        fs: float = 1e6,
    ) -> _g.Segment:
        """Mix several :class:`Synth` together over the same ``num_samples``.

        The synths mix at the same time (one receiver), so they share one
        sample rate ``fs`` and one noise floor. ``off`` is the trailing gap.

        Examples
        --------
        >>> from doppler.wfm import Segment, qpsk, tone
        >>> seg = Segment.sum(
        ...     qpsk(snr=15), tone(freq=2e5, level=-12), num_samples=4096
        ... )
        >>> len(seg.sources)
        2
        """
        if not sources:
            raise ValueError("Segment.sum needs at least one synth")
        return _g.Segment.sum(
            *sources, fs=fs, num_samples=num_samples, off_samples=off
        )


# The generated Timeline (an ordered, iterable run of segments built by
# Segment.add / Timeline.add) is already complete — re-export it directly.
Timeline = _g.Timeline


def _flat_segment(seg):
    """Wrap a resolved generated segment in a flat (single-source) Segment.

    A single-source segment becomes a :class:`Segment` whose source fields read
    inline (``.f_end``, ``.pulse``, …); a multi-source segment is returned as-is
    (it has no single waveform to flatten).
    """
    srcs = list(seg.sources)
    if len(srcs) != 1:
        return seg
    s = srcs[0]
    return Segment(
        type=s.type,
        fs=seg.fs,
        freq=s.freq,
        f_end=s.f_end,
        snr=s.snr,
        snr_mode=s.snr_mode,
        seed=s.seed,
        sps=s.sps,
        pn_length=s.pn_length,
        pn_poly=s.pn_poly,
        lfsr=s.lfsr,
        num_samples=seg.num_samples,
        off_samples=seg.off_samples,
        level=s.level,
        modulation=s.modulation,
        pulse=s.pulse,
        rrc_beta=s.rrc_beta,
        rrc_span=s.rrc_span,
        pattern=_bits_to_pattern(s.bits),
    )


def _src_tuple(s) -> tuple:
    """A source's 16-field ``_SOURCE_FMT`` tuple for the transport marshal."""
    return (
        _idx(s.type, _TYPES, "type"),
        float(s.freq),
        float(s.snr),
        _idx(s.snr_mode, _MODES, "snr_mode"),
        int(s.seed),
        int(s.sps),
        int(s.pn_length),
        int(s.pn_poly),
        _idx(s.lfsr, _LFSRS, "lfsr"),
        float(s.level),
        float(s.f_end),
        _idx(s.modulation, _BITMODS, "modulation"),
        s.bits,
        _idx(s.pulse, _PULSES, "pulse"),
        float(s.rrc_beta),
        int(s.rrc_span),
    )


def _seg_tuple(seg) -> tuple:
    """A segment's transport tuple: flat ``_SEG_FMT`` (1 source) else nested.

    Mirrors the C round-trip (``wfmcompose_py.c``): ``n_sources == 1`` is the
    flat single-source form, otherwise ``(num, off, fs, [source-tuples])``.
    """
    srcs = list(seg.sources)
    if len(srcs) == 1:
        s = srcs[0]
        return (
            _idx(s.type, _TYPES, "type"),
            float(seg.fs),
            float(s.freq),
            float(s.snr),
            _idx(s.snr_mode, _MODES, "snr_mode"),
            int(s.seed),
            int(s.sps),
            int(s.pn_length),
            int(s.pn_poly),
            _idx(s.lfsr, _LFSRS, "lfsr"),
            int(seg.num_samples),
            int(seg.off_samples),
            float(s.level),
            float(s.f_end),
            _idx(s.modulation, _BITMODS, "modulation"),
            s.bits,
            _idx(s.pulse, _PULSES, "pulse"),
            float(s.rrc_beta),
            int(s.rrc_span),
        )
    return (
        int(seg.num_samples),
        int(seg.off_samples),
        float(seg.fs),
        [_src_tuple(s) for s in srcs],
    )


class Composer(_g.Composer):
    """A multi-segment waveform generator over a list of :class:`Segment`.

    Construct from an explicit segment list, from a single segment's keyword
    arguments, or from a JSON spec (:meth:`from_json` / :meth:`from_file`). Pull
    samples a block at a time with :meth:`execute`, or drain a finite spec to one
    array with :meth:`compose`. Usable as a context manager.

    The composer itself (the C kernel driver, ``execute`` / ``compose`` /
    ``to_json``) is the jm-generated :class:`doppler.wfm.wfm_compose.Composer`
    this subclasses; :meth:`stream` and the flat :attr:`segments` view are added
    here.

    Parameters
    ----------
    segments : Sequence[Segment] or None
        The segments to compose. If ``None``, a single segment is built from the
        remaining keyword arguments (see :class:`Segment`).
    repeat : bool, optional
        Loop the whole sequence after the last segment.
    continuous : bool, optional
        Never finish (implies ``repeat``); :meth:`compose` then raises.
    **segment_kwargs
        Used to build a single :class:`Segment` when ``segments`` is ``None``.

    Examples
    --------
    >>> from doppler.wfm.compose import Composer
    >>> with Composer(type="noise", snr=10.0, num_samples=4096) as c:
    ...     blk = c.execute(1024)
    >>> blk.dtype, len(blk)
    (dtype('complex64'), 1024)
    """

    # __init__ / execute / compose / to_json / close / __enter__ / __exit__ /
    # repeat / continuous are inherited from the generated Composer unchanged.

    @property
    def segments(self) -> list:
        """The resolved segment list (defaults filled in), as flat Segments."""
        return [_flat_segment(s) for s in _g.Composer.segments.__get__(self)]

    @classmethod
    def from_json(cls, json: str) -> "Composer":
        """Build a composer from a JSON spec string (the ``--from-file`` schema)."""
        base = _g.Composer.from_json(json)
        return cls(
            list(_g.Composer.segments.__get__(base)),
            repeat=base.repeat,
            continuous=base.continuous,
        )

    @classmethod
    def from_file(cls, path: str | os.PathLike) -> "Composer":
        """Build a composer from a JSON spec file."""
        base = _g.Composer.from_file(os.fspath(path))
        return cls(
            list(_g.Composer.segments.__get__(base)),
            repeat=base.repeat,
            continuous=base.continuous,
        )

    def stream(self, block: int = 4096, *, realtime: bool | float = False):
        """Yield successive blocks — a generator over :meth:`execute`.

        Turns the ``while len(b := c.execute(n)):`` boilerplate into
        ``for b in c.stream(n):``. A finite spec ends when ``execute`` drains;
        a ``continuous`` spec streams forever.

        This is the Python equivalent of the ``wfmgen --realtime`` flag: pass
        ``realtime`` to pace each block to real time (the same `timing_core`
        clock the CLI uses). ``realtime=True`` paces at the first segment's
        ``fs``; pass a float to override the rate.

        Parameters
        ----------
        block : int
            Samples per yielded array (the last finite block may be shorter).
        realtime : bool or float, optional
            Pace to real time. ``True`` uses ``segments[0].fs``; a float sets
            the sample-clock rate explicitly. Default ``False``.

        Yields
        ------
        NDArray[np.complex64]
            One block per iteration.

        Examples
        --------
        >>> from doppler.wfm.compose import Composer
        >>> c = Composer(type="tone", freq=1e5, num_samples=1000)
        >>> total = sum(len(b) for b in c.stream(256))
        >>> total
        1000
        """
        clk = None
        if realtime:
            fs = self.segments[0].fs if realtime is True else float(realtime)
            clk = SampleClock(fs)
        while True:
            blk = self.execute(block)
            if len(blk) == 0:
                break
            yield blk
            if clk is not None:
                clk.pace(len(blk))  # sleep to the next block's deadline


class Writer:
    """Stream ``complex64`` samples to a container file.

    Wraps the C writer for the four containers the CLI supports. For ``"sigmf"``
    the samples land in ``path`` (use ``<name>.sigmf-data``) and the companion
    metadata is produced by :func:`sigmf_meta`; for detached BLUE use
    :func:`write_blue_header`.

    Parameters
    ----------
    path : str or PathLike
        Output file (opened in binary mode; truncated).
    file_type : {"raw", "csv", "blue", "sigmf"}
        Output container.
    sample_type : {"cf32", "cf64", "ci32", "ci16", "ci8"}
        On-disk element type; integer types are quantised from unit-scale.
    endian : {"le", "be"}
        Byte order for multi-byte integer types (ignored for CSV).
    fs, fc : float
        Sample rate and centre frequency (Hz), recorded in BLUE/SigMF metadata.
    total : int, optional
        Expected sample count (lets seekable BLUE headers be patched on close).

    Examples
    --------
    >>> import tempfile, os
    >>> from doppler.wfm.compose import Composer, Writer
    >>> from doppler.wfm.readback import read_iq
    >>> x = Composer(type="tone", freq=1e5, num_samples=512).compose()
    >>> p = os.path.join(tempfile.mkdtemp(), "cap.cf32")
    >>> with Writer(p, sample_type="cf32") as w:
    ...     _ = w.write(x)
    >>> import numpy as np
    >>> bool(np.allclose(read_iq(p, "cf32"), x))
    True
    """

    def __init__(
        self,
        path: str | os.PathLike,
        *,
        file_type: str = "raw",
        sample_type: str = "cf32",
        endian: str = "le",
        fs: float = 1e6,
        fc: float = 0.0,
        total: int = 0,
        headroom: float = 0.0,
    ) -> None:
        self._stype = _idx(sample_type, _STYPES, "sample_type")
        self._cap = _c.writer_open(
            os.fspath(path),
            _idx(file_type, _FTYPES, "file_type"),
            self._stype,
            _idx(endian, _ENDIANS, "endian"),
            float(fs),
            float(fc),
            int(total),
        )
        # headroom backs the composite off to -H dBFS so peaks fit; a single
        # gain, so SNR is invariant. 0 dB is a bit-exact no-op.
        if headroom:
            _c.writer_set_gain(self._cap, 10.0 ** (-float(headroom) / 20.0))

    def write(self, iq: NDArray[np.complex64]) -> int:
        """Write a block of samples; returns the number written."""
        return _c.writer_write(self._cap, iq)

    def track_clipping(self, on: bool = True) -> None:
        """Enable the per-component clip *counter* (off by default; the peak is
        always tracked). Call before writing if you want :attr:`clip_fraction`.
        """
        _c.writer_track_clipping(self._cap, bool(on))

    @property
    def peak_dbfs(self) -> float:
        """Largest sample magnitude written, in dBFS (``0`` dB = full scale).
        Positive means an integer wire type clipped; the value is the headroom
        it would need. Readable while open or after :meth:`close`."""
        peak, _ = _c.writer_stats(self._cap)
        return 20.0 * math.log10(peak) if peak > 0.0 else float("-inf")

    @property
    def clip_fraction(self) -> float:
        """Fraction (0..1) of I/Q components that saturated. Always ``0`` unless
        :meth:`track_clipping` was enabled; only meaningful for integer types.
        """
        _, frac = _c.writer_stats(self._cap)
        return frac

    @property
    def clipped(self) -> bool:
        """True if an integer capture ran past full scale (``peak > 1``)."""
        peak, _ = _c.writer_stats(self._cap)
        return self._stype >= 2 and peak > 1.0

    def close(self) -> None:
        """Flush, patch any header, and close the file (idempotent)."""
        _c.writer_close(self._cap)

    def __enter__(self) -> "Writer":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


class Reader:
    """Read a capture back to ``complex64`` — the dual of :class:`Writer`.

    The container is **auto-detected** from the file (BLUE ``"BLUE"`` magic, a
    ``.sigmf-meta`` sidecar, the ``.csv`` extension, else raw), and
    self-describing containers (BLUE, SigMF) recover the sample type, byte
    order, sample rate and centre frequency from their metadata. Headerless raw
    / CSV take the ``sample_type`` / ``endian`` hints. All detection, header
    parsing and wire→unit conversion happen in C; this class is thin glue.

    Parameters
    ----------
    path : str or PathLike
        Capture to read. A BLUE ``.det`` or SigMF ``.sigmf-data`` data file
        resolves its ``.hdr`` / ``.sigmf-meta`` sidecar automatically.
    sample_type : {"cf32", "cf64", "ci32", "ci16", "ci8"}
        Wire type for headerless raw / CSV (ignored once BLUE/SigMF metadata is
        parsed).
    endian : {"le", "be"}
        Byte order for headerless raw.

    Examples
    --------
    >>> import tempfile, os, numpy as np
    >>> from doppler.wfm.compose import Composer, Writer, Reader
    >>> x = Composer(type="tone", freq=1e5, num_samples=512).compose()
    >>> p = os.path.join(tempfile.mkdtemp(), "cap.blue")
    >>> with Writer(p, file_type="blue", fs=1e6) as w:
    ...     _ = w.write(x)
    >>> with Reader(p) as r:            # BLUE self-describes — no hints needed
    ...     y = r.read_all()
    ...     print(r.file_type, int(r.fs), bool(np.allclose(y, x)))
    blue 1000000 True
    """

    def __init__(
        self,
        path: str | os.PathLike,
        *,
        sample_type: str = "cf32",
        endian: str = "le",
    ) -> None:
        self._cap = _c.reader_open(
            os.fspath(path),
            _idx(sample_type, _STYPES, "sample_type"),
            _idx(endian, _ENDIANS, "endian"),
        )
        # Metadata is fixed for the open capture; resolve it once.
        self._info = _c.reader_info(self._cap)

    @property
    def file_type(self) -> str:
        """Detected container: ``"raw"`` / ``"csv"`` / ``"blue"`` / ``"sigmf"``."""
        return _FTYPES[self._info[0]]

    @property
    def sample_type(self) -> str:
        """Resolved wire sample type."""
        return _STYPES[self._info[1]]

    @property
    def endian(self) -> str:
        """Resolved byte order."""
        return _ENDIANS[self._info[2]]

    @property
    def fs(self) -> float:
        """Sample rate (Hz); ``0.0`` if the container doesn't carry it."""
        return self._info[3]

    @property
    def fc(self) -> float:
        """Centre frequency (Hz); ``0.0`` if not recorded."""
        return self._info[4]

    @property
    def num_samples(self) -> int:
        """Total complex samples available; ``0`` if unknown (a stream)."""
        return self._info[5]

    def read(self, n: int) -> NDArray[np.complex64]:
        """Read up to ``n`` samples; a shorter (or empty) array marks EOF."""
        return _c.reader_read(self._cap, int(n))

    def read_all(self, block: int = 65536) -> NDArray[np.complex64]:
        """Drain the whole capture into one ``complex64`` array."""
        chunks = []
        while True:
            blk = self.read(block)
            if len(blk) == 0:
                break
            chunks.append(blk)
        if not chunks:
            return np.empty(0, dtype=np.complex64)
        return np.concatenate(chunks)

    def close(self) -> None:
        """Close the file (idempotent)."""
        _c.reader_close(self._cap)

    def __enter__(self) -> "Reader":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


class ZmqSink:
    """Publish ``complex64`` samples over a ZeroMQ PUB socket (POSIX only).

    Each :meth:`send` frames the block with its sample rate and centre frequency
    and the chosen ``sample_type`` wire format — the same framing the C CLI's
    ``--output zmq://…`` uses.

    Parameters
    ----------
    endpoint : str
        ZeroMQ endpoint, e.g. ``"tcp://0.0.0.0:5555"``.
    sample_type : {"cf32", "cf64", "ci32", "ci16", "ci8"}
        Wire element type.
    """

    def __init__(self, endpoint: str, *, sample_type: str = "cf32") -> None:
        if not hasattr(_c, "sink_open"):
            raise NotImplementedError(
                "ZmqSink is not available on this platform"
            )
        self._cap = _c.sink_open(
            endpoint, _idx(sample_type, _STYPES, "sample_type")
        )

    def send(
        self, iq: NDArray[np.complex64], fs: float, fc: float = 0.0
    ) -> None:
        """Publish a block tagged with its sample rate and centre frequency."""
        _c.sink_send(self._cap, iq, float(fs), float(fc))

    def close(self) -> None:
        """Close the publisher (idempotent)."""
        _c.sink_close(self._cap)

    def __enter__(self) -> "ZmqSink":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


class SampleClock:
    """Pace and timestamp a stream against an ideal ``fs``-Hz clock (POSIX).

    A :class:`SampleClock` mimics a hardware sample clock in software. Off one
    drift-free timeline anchored at construction it does two things:

    - :meth:`pace` — sleep so each block leaves at its real-time deadline
      ``epoch + n/fs``, throttling a producer (e.g. a :class:`Composer` feeding
      a :class:`ZmqSink`) to real time.
    - :meth:`stamp` — return the ideal UNIX-epoch-ns time of the next sample,
      for reproducible capture metadata (SigMF ``core:datetime``, records).

    The schedule is anchored, not incremental: every deadline is recomputed
    from the cumulative sample count against a fixed epoch, so an over- or
    under-sleep on one block is corrected on the next — the long-run rate is
    exactly ``fs``, with only bounded per-block jitter.

    Parameters
    ----------
    fs : float
        Sample rate (Hz). Must be > 0.
    resync : bool, optional
        Re-anchor the timeline to "now" on each underrun (default: keep the
        absolute schedule and let the average rate self-heal if it catches up).

    Examples
    --------
    >>> from doppler.wfm.compose import SampleClock
    >>> clk = SampleClock(fs=1e6)
    >>> slack = clk.pace(1000)        # advance 1000 samples (~1 ms) and wait
    >>> clk.samples
    1000
    >>> isinstance(clk.stamp(), int)  # ideal ns timestamp of the next sample
    True
    """

    def __init__(self, fs: float, *, resync: bool = False) -> None:
        if not hasattr(_c, "clock_create"):
            raise NotImplementedError(
                "SampleClock is not available on this platform"
            )
        self._cap = _c.clock_create(float(fs), bool(resync))

    def pace(self, count: int) -> float:
        """Advance ``count`` samples and sleep to that block's deadline.

        Returns the slack in seconds measured before sleeping: ``>= 0`` means
        the block was early (and it slept that long); ``< 0`` means it arrived
        late — an underrun (no sleep, counted).
        """
        return _c.clock_pace(self._cap, int(count))

    def stamp(self) -> int:
        """Ideal UNIX-epoch-ns timestamp of the next sample (index ``n``).

        Call before :meth:`pace` to tag the block you're about to emit, or
        after to tag the following one.
        """
        return _c.clock_stamp(self._cap)

    def reset(self) -> None:
        """Re-anchor to now and zero the counters — a fresh clock at ``n=0``."""
        _c.clock_reset(self._cap)

    def resync(self) -> None:
        """Drop accumulated lateness; pace forward from now (keeps ``n``)."""
        _c.clock_resync(self._cap)

    @property
    def samples(self) -> int:
        """Cumulative samples advanced through :meth:`pace`."""
        return _c.clock_stats(self._cap)[0]

    @property
    def underruns(self) -> int:
        """Number of :meth:`pace` calls that arrived past their deadline."""
        return _c.clock_stats(self._cap)[1]

    @property
    def max_lateness(self) -> float:
        """Worst lateness observed (seconds); ``0.0`` if never behind."""
        return _c.clock_stats(self._cap)[2] / 1e9


# ── module-level helpers ─────────────────────────────────────────────────────


def sigmf_meta(
    *,
    sample_type: str = "cf32",
    endian: str = "le",
    fs: float = 1e6,
    fc: float = 0.0,
    segments: Sequence[Segment],
) -> str:
    """Build the SigMF ``.sigmf-meta`` JSON for a composed capture.

    The capture's segments become per-segment SigMF annotations; pair this with
    a ``Writer(..., file_type="sigmf")`` writing the ``.sigmf-data`` companion.
    """
    return _c.sigmf_meta_json(
        _idx(sample_type, _STYPES, "sample_type"),
        _idx(endian, _ENDIANS, "endian"),
        float(fs),
        float(fc),
        [_seg_tuple(s) for s in segments],
    )


def write_blue_header(
    path: str | os.PathLike,
    *,
    sample_type: str = "cf32",
    endian: str = "le",
    fs: float = 1e6,
    fc: float = 0.0,
    total: int,
    data_start: float = 0.0,
    detached: bool = True,
) -> None:
    """Write a standalone BLUE type-1000 HCB header (the detached ``.hdr``).

    The 512-byte header carries the ``"BLUE"`` magic, byte order, ``data_size``
    (``total`` × bytes-per-sample), the type-1000 tag and ``xdelta = 1/fs``;
    pair it with a detached ``.det`` body of raw interleaved I/Q.

    Examples
    --------
    >>> import os, tempfile
    >>> from doppler.wfm.compose import write_blue_header
    >>> p = os.path.join(tempfile.mkdtemp(), "cap.hdr")
    >>> write_blue_header(p, sample_type="cf32", fs=1e6, total=512)
    >>> with open(p, "rb") as f:
    ...     head = f.read()
    >>> head[:4], len(head)
    (b'BLUE', 512)
    """
    _c.blue_write_hcb(
        os.fspath(path),
        _idx(sample_type, _STYPES, "sample_type"),
        _idx(endian, _ENDIANS, "endian"),
        float(fs),
        float(fc),
        float(data_start),
        int(total),
        bool(detached),
    )


def rrc_taps(beta: float, sps: int, span: int) -> NDArray[np.float32]:
    """Root-raised-cosine pulse-shaping taps.

    Returns ``2*span*sps + 1`` float32 taps for roll-off ``beta``, ``sps``
    samples per symbol, and a ``±span``-symbol support.

    Examples
    --------
    >>> from doppler.wfm.compose import rrc_taps
    >>> t = rrc_taps(0.35, 4, 6)
    >>> t.dtype, len(t)
    (dtype('float32'), 49)
    """
    return _c.rrc_taps(float(beta), int(sps), int(span))


def dsss_spread(
    syms: NDArray[np.complex64], code: NDArray[np.uint8], sf: int
) -> NDArray[np.complex64]:
    """Direct-sequence spread ``syms`` by the ``±1`` chip ``code`` (length ≥ ``sf``).

    Each symbol is repeated against the first ``sf`` chips, yielding
    ``len(syms) * sf`` output chips.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.wfm.compose import dsss_spread
    >>> syms = np.array([1+0j, -1+0j], dtype=np.complex64)
    >>> code = np.array([0, 1, 0, 1], dtype=np.uint8)
    >>> dsss_spread(syms, code, 4).shape
    (8,)
    """
    return _c.dsss_spread(syms, code, int(sf))


def mls_poly(n: int) -> int:
    """Maximal-length-sequence primitive polynomial for an LFSR of length ``n``.

    Mirrors the table the synth/PN engine uses for ``pn_poly=0``; valid for
    ``n`` in 2..64 (returns 0 otherwise).

    Examples
    --------
    >>> from doppler.wfm.compose import mls_poly
    >>> hex(mls_poly(7))
    '0x41'
    """
    return _c.mls_poly(int(n))
