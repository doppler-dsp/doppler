"""Multi-segment waveform composition, file writers, and a ZMQ sink.

This is the Python face of the C ``wfmgen`` composer subsystem — the same
engine behind the ``wfmgen`` CLI, exposed here as classes. A
:class:`Composer` strings :class:`Segment` specs (tone / noise / PN / BPSK /
QPSK, each with its own on-time and trailing gap) into one stream, optionally
looping (``repeat``) or running forever (``continuous``). :class:`Writer`
serialises samples to the same containers as the CLI (raw interleaved I/Q,
CSV, BLUE type-1000, SigMF), and :class:`ZmqSink` publishes them over
ZeroMQ. The composer's resolved spec round-trips through JSON
(:meth:`Composer.to_json` / :meth:`Composer.from_json`), so a capture is
fully reproducible.

The samples come back as ``complex64`` arrays; pair :class:`Writer` with
:func:`doppler.wfm.readback.read_iq` to round-trip a file.

The ``Synth`` / ``Segment`` / ``Timeline`` / ``Composer`` ergonomic types
are the **jm-generated** CPython types in ``doppler.wfm.wfm_compose`` (the
composer lives entirely in the ``.so``; ``jm`` owns the binding). They are
**re-exported verbatim** below — there is no Python wrapper layer: standalone
sample generation
(:meth:`Synth.steps`), the ``pattern`` / ``f_start`` input sugar, the flat
single-source :class:`Segment` view, :meth:`Composer.stream`, and the resolved
:meth:`Composer.to_dict` are all generated. Only the container writers/readers
(BLUE / SigMF / ZMQ / sample-clock) stay hand-written here, over the transport
binding ``_wfmcompose``.

Examples
--------
>>> from doppler.wfm.compose import Composer, Segment
>>> spec = [
...     Segment("pn", num_samples=127, pn_length=7),
...     Segment("tone", freq=1e5, num_samples=256, off_samples=64),
... ]
>>> x = Composer(spec).compose()
>>> x.dtype, len(x)  # 127 (pn) + 256 (tone) + 64 (gap)
(dtype('complex64'), 447)
"""

from __future__ import annotations

import json as _json
import os
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Iterator, Sequence

    import numpy as np
    from numpy.typing import NDArray

# _wfmcompose: transport binding (sigmf/DSP helpers — the transport classes are
# now the generated kind="handle" types below).
from . import _wfmcompose as _c

# The transport surface is now the generated kind="handle" types — re-export
# them through compose so `doppler.wfm.compose.Writer` (etc.) stays the import
# path. (Realtime pacing now lives in C as Composer.stream(realtime=fs); the
# hand-written paced() helper is retired.)
from .sample_clock import SampleClock  # noqa: F401  (re-export)

# The composer OO surface IS the generated .so type — re-export it verbatim, no
# Python wrapper. Synth/Segment/Timeline/Composer carry standalone generation,
# the pattern/f_start aliases, the flat Segment view, stream() and to_dict().
from .wfm_compose import (  # noqa: F401  (re-export)
    Composer,
    Segment,
    Synth,
    Timeline,
    bits,
    bpsk,
    chirp,
    noise,
    pn,
    qpsk,
    tone,
)
from .wfm_plan import Plan as _Plan  # the generated handle; wrapped below
from .wfm_reader import Reader  # noqa: F401  (re-export)
from .wfm_sink import ZmqSink  # noqa: F401  (re-export)
from .wfm_writer import Writer  # noqa: F401  (re-export)

# string-enum ↔ C-int tables for the remaining hand binding
# (write_blue_header); must match native/src/app/wfmgen.c and the manifest
# [[enum]] stype/endian.
#
# This is a third copy of that enum SSOT (doppler#179 review #8). It
# survives because write_blue_header is the last unmigrated hand binding:
# turning it into a generated `jm function` would let the enum strings
# resolve against the single manifest [[enum]] SSOT, deleting these tables
# — but that needs `path` + `enum` arg support in jm's module-function
# generator, which it does not yet have (tracked as just-makeit#353). Until
# then, the mapping has to live here.
_STYPES = ("cf32", "cf64", "ci32", "ci16", "ci8")
_ENDIANS = ("le", "be")


def _idx(name: str, table: tuple[str, ...], what: str) -> int:
    try:
        return table.index(name)
    except ValueError:
        raise ValueError(
            f"{what} must be one of {table!r}, got {name!r}"
        ) from None


# ── module-level helpers ─────────────────────────────────────────────────────

# sigmf_meta is now the generated Composer.to_sigmf() method (delegated
# serializer over the resolved segments) — call Composer(spec).to_sigmf(...).


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


# rrc_taps / dsss_spread are now generated `variable_output` module functions
# (doppler.wfm.rrc_taps / .dsss_spread, over
# native/src/wfm/{rrc_taps,dsss_spread}.c).


# ── Plan: "prepare once, materialize many" stimulus engine ──────────────────


def _spec_json(scene: Composer | str | bytes) -> str:
    """Coerce a scene to its spec-JSON string (the wfm_plan ctor argument).

    Accepts a :class:`Composer` (or anything with a ``to_json()``, e.g. a
    :class:`Timeline`) or an already-serialized JSON ``str``/``bytes`` — so
    ``Plan(composer)`` and ``Plan(open(f).read())`` both work.
    """
    if isinstance(scene, (str, bytes)):
        return scene.decode() if isinstance(scene, bytes) else scene
    # A Composer (the common case) or any object exposing to_json (e.g. a
    # Timeline) serializes itself; reject anything else with a clear message.
    if not isinstance(scene, Composer) and not hasattr(scene, "to_json"):
        raise TypeError(
            f"cannot prepare a Plan from {type(scene).__name__}; "
            "pass a Composer or a spec JSON string"
        )
    return scene.to_json()


class Plan:
    """A prepared scene that re-materializes parameter variations cheaply.

    A composed multi-source scene is a linear form
    ``Σ gainₖ·signalₖ + noise``; the expensive DSP (spreading, pulse shaping,
    the LO) lives entirely in the *signal* terms, which do not change when you
    sweep a level, a phase, the SNR, or the noise seed. :class:`Plan` renders
    each source once (bit-identically to a full compose), caches it, and then
    serves every variation as a cheap re-weighted sum — so a BER/Pd curve or a
    Monte-Carlo campaign that re-runs one scene hundreds of times pays the DSP
    cost *once*.

    Construct it from anything :func:`_spec_json` accepts — most often a
    :class:`Composer` (or call :func:`prepare`). The baseline ``render()`` (no
    overrides) is **bit-for-bit identical** to ``Composer(scene).compose()``.

    Parameters
    ----------
    scene : Composer or str or bytes
        The scene to prepare — a :class:`Composer` (or any object with a
        ``to_json()``) or a spec JSON string.

    Notes
    -----
    v1 supports a single finite (non-ranged, no off-gap) ``sum`` segment with
    at most one noise floor; a lone *bundled* noisy source (its private RNG is
    fused into the signal) is not separable and raises ``ValueError`` at
    construction. The overridable axes are per-source ``gains`` (dBFS levels),
    ``phases`` (radians), ``enable`` (drop a source), the global ``snr`` (the
    noise floor), and the Monte-Carlo ``seed``. Frequency (Doppler) and delay
    (multipath) are planned follow-ups.

    Examples
    --------
    >>> import numpy as np
    >>> from doppler.wfm.compose import Composer, Segment, qpsk, tone, prepare
    >>> scene = Composer(
    ...     Segment.sum(
    ...         qpsk(snr=12.0, seed=7, sps=8, pn_length=7),
    ...         tone(freq=1e5, seed=3, sps=8),
    ...         fs=1e6,
    ...         num_samples=4096,
    ...     )
    ... )
    >>> plan = prepare(scene)
    >>> len(plan), plan.n_sources
    (4096, 2)
    >>> # baseline reproduces a full compose exactly
    >>> np.array_equal(plan.render(), scene.compose())
    True
    >>> # sweep SNR with no re-synthesis; each point is a cheap re-weight
    >>> curve = {s: plan.at(s) for s in (0.0, 3.0, 6.0, 9.0)}
    >>> {s: v.shape for s, v in curve.items()}  # doctest: +ELLIPSIS
    {0.0: (4096,), 3.0: (4096,), 6.0: (4096,), 9.0: (4096,)}
    """

    __slots__ = ("_h",)

    def __init__(self, scene: Composer | str | bytes) -> None:
        try:
            self._h = _Plan(_spec_json(scene))
        except RuntimeError as exc:
            # The generated ctor returns NULL → RuntimeError for an
            # unparseable or out-of-scope spec; ValueError is the honest sign.
            raise ValueError(
                "scene cannot be prepared as a Plan: expected a single "
                "finite, non-ranged sum segment with a separable noise floor "
                "(a lone bundled noisy source, a multi-segment / continuous / "
                "repeat / ranged scene is out of v1 scope)"
            ) from exc

    def render(
        self,
        *,
        gains: Sequence[float] | None = None,
        phases: Sequence[float] | None = None,
        enable: Sequence[bool] | None = None,
        snr: float | None = None,
        seed: int | None = None,
    ) -> NDArray[np.complex64]:
        """Materialize the scene with per-axis overrides applied.

        Every argument is optional; omit them all for the baseline (identical
        to ``Composer(scene).compose()``). ``gains``/``phases``/``enable`` are
        per signal source (length :attr:`n_sources`, in scene order).

        Parameters
        ----------
        gains : sequence of float, optional
            Absolute source levels in dBFS (``0`` = unit power). Replaces each
            source's resolved level; does not move the noise floor.
        phases : sequence of float, optional
            Per-source phase rotations in radians (``0`` = identity).
        enable : sequence of bool, optional
            ``False`` drops a source (an exact ``gain=0`` term).
        snr : float, optional
            Global SNR in dB — moves only the noise floor (its convention is
            the anchor source's ``snr_mode``).
        seed : int, optional
            Noise seed for this realization (defaults to the scene's, i.e. the
            value that reproduces a full compose).

        Returns
        -------
        numpy.ndarray
            ``complex64`` samples, length :func:`len`.
        """
        ov: dict[str, object] = {}
        if gains is not None:
            ov["gains"] = [float(x) for x in gains]
        if phases is not None:
            ov["phases"] = [float(x) for x in phases]
        if enable is not None:
            ov["enable"] = [bool(x) for x in enable]
        if snr is not None:
            ov["snr"] = float(snr)
        if seed is not None:
            ov["seed"] = int(seed)
        return self._h.render(_json.dumps(ov) if ov else "{}")

    def at(self, snr: float, seed: int | None = None) -> NDArray[np.complex64]:
        """Scalar fast path: render at one ``(snr, seed)`` (no JSON parse).

        The hot loop of an SNR sweep or Monte-Carlo run. ``seed`` defaults to
        :attr:`anchor_seed` (which reproduces a full compose at the scene's
        base SNR).
        """
        return self._h.at(
            float(snr), self.anchor_seed if seed is None else int(seed)
        )

    def sweep(
        self, snrs: Sequence[float], *, seed: int | None = None
    ) -> Iterator[tuple[float, NDArray[np.complex64]]]:
        """Yield ``(snr, samples)`` across an SNR list at a fixed noise seed.

        A held seed isolates the SNR axis (same noise realization, only the
        floor moves) — the natural stimulus for a Pd/BER-vs-SNR curve.
        """
        for s in snrs:
            yield s, self.at(s, seed)

    def monte_carlo(
        self, snr: float, n: int, *, seed0: int = 0
    ) -> Iterator[NDArray[np.complex64]]:
        """Yield ``n`` independent noise realizations at a fixed SNR.

        Seeds run ``seed0 … seed0 + n − 1``; the signal is identical across
        draws, only the noise differs.
        """
        for i in range(n):
            yield self.at(snr, seed0 + i)

    @property
    def n_sources(self) -> int:
        """Number of cached signal sources (excludes the noise floor)."""
        return self._h.n_sources()

    @property
    def anchor_seed(self) -> int:
        """The noise seed that reproduces a full compose at the base SNR."""
        return self._h.anchor_seed()

    def __len__(self) -> int:
        return self._h.length()

    def __enter__(self) -> Plan:
        return self

    def __exit__(self, *exc: object) -> None:
        self._h.close()


def prepare(scene: Composer | str | bytes) -> Plan:
    """Prepare a scene into a reusable :class:`Plan` (``Plan(scene)``).

    Examples
    --------
    >>> from doppler.wfm.compose import Composer, Segment, qpsk, tone, prepare
    >>> plan = prepare(
    ...     Composer(
    ...         Segment.sum(
    ...             qpsk(snr=10.0, seed=1),
    ...             tone(freq=2e5, seed=2),
    ...             fs=1e6,
    ...             num_samples=1024,
    ...         )
    ...     )
    ... )
    >>> len(plan)
    1024
    """
    return Plan(scene)
