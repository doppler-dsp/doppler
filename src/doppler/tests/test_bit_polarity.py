"""Every bit decision in the library follows ONE polarity (doppler#2654).

The rule's home is ``mpsk_core.h``: bit ``b`` is sent as ``1 - 2b`` -- bit 0
is +1, bit 1 is -1 -- so a decision maps a NEGATIVE value to 1 and zero or
positive to 0. ``Despreader.bits()`` and ``BurstDespreader.bits()`` once
mapped a positive prompt to 1 instead, and their doctests scored
``min(e, 1 - e)``, which cannot tell a correct stream from its complement.
Both objects were wrong for as long as that score was the only check.

This gate runs one known-phase BPSK stream, mapped by ``bin_to_nrz``,
through every object that turns samples into bits and requires each to hand
back the sent bits RAW. Agreeing with the same truth is agreeing with each
other, so a new emitter with the opposite rule cannot land quietly.

Registration-free
-----------------
The emitters are DISCOVERED from the manifests, not listed: a method (views
included) or module function that returns ``uint8_t`` bytes from a real or
complex sample input. Each discovered emitter must have a driver below, or an
exemption with its reason; a new one with neither fails
``test_every_emitter_is_driven``, and so does a driver whose emitter is gone.
The table is the stimulus, not the census.
"""

from __future__ import annotations

import sys
from typing import TYPE_CHECKING

import numpy as np
import pytest

from doppler.tests._repo import repo_root

if TYPE_CHECKING:
    from collections.abc import Callable, Iterator

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib

ROOT = repo_root(__file__)

#: Element types that are SAMPLES: a uint8 out of one of these is a decision.
_SAMPLE = {"float", "double", "float _Complex", "double _Complex"}


def _is_sample(t: str | None) -> bool:
    return t is not None and t.removesuffix("[]").strip() in _SAMPLE


def _method_emits(m: dict, obj_in: str | None) -> bool:
    """A uint8 out of a sample input: the method's own arg, a ``params``
    array, or (``arg_type = "void"`` and no params) the object's input."""
    if m.get("return_type") != "uint8_t":
        return False
    ins = [p.get("type") for p in m.get("params", [])]
    at = m.get("arg_type")
    if at and at != "void":
        ins.append(at)
    if at == "void" and not m.get("params"):
        ins.append(obj_in)
    return any(_is_sample(t) for t in ins)


def _function_emits(f: dict) -> bool:
    """A module function writing uint8 from a sample array."""
    params = f.get("params", [])
    out_u8 = f.get("out_type") == "uint8_t" or any(
        p.get("out") and p.get("type") == "uint8_t[]" for p in params
    )
    return out_u8 and any(
        _is_sample(p.get("type")) and not p.get("out") for p in params
    )


def _discover() -> Iterator[str]:
    """``Class.method`` for objects and their views, ``module.fn`` for
    module functions -- every place a sample becomes a bit."""
    for path in sorted((ROOT / "objects").glob("*.toml")):
        for comp, cfg in tomllib.loads(path.read_text("utf-8")).items():
            if not isinstance(cfg, dict):
                continue
            obj_in = cfg.get("arg_type")
            cls = cfg.get("class_name") or "".join(
                p[:1].upper() + p[1:] for p in comp.split("_")
            )
            methods = {m["name"]: m for m in cfg.get("methods", [])}
            for name, m in methods.items():
                if _method_emits(m, obj_in):
                    yield f"{cls}.{name}"
            for view in cfg.get("views", []):
                own = {m["name"]: m for m in view.get("methods", [])}
                skip = set(view.get("exclude_methods", []))
                for name, m in {**methods, **own}.items():
                    if name not in skip and _method_emits(m, obj_in):
                        yield f"{view['class_name']}.{name}"
    jm = tomllib.loads((ROOT / "just-makeit.toml").read_text("utf-8"))
    for mod, mcfg in jm.get("module", {}).items():
        for f in mcfg.get("functions", []) if isinstance(mcfg, dict) else []:
            if _function_emits(f):
                yield f"{mod}.{f['name']}"


# ── the stimulus: one bit stream, sent as 1 - 2b by bin_to_nrz ──────────────

_TX = np.random.default_rng(2654).integers(0, 2, 240).astype(np.uint8)


def _nrz(bits: np.ndarray) -> np.ndarray:
    """The library's transmit map, not a restatement of it."""
    from doppler.cvt import bin_to_nrz

    out = np.zeros(bits.size, np.float32)
    assert bin_to_nrz(bits, out) == bits.size
    return out


def _spread(code: np.ndarray, sym: np.ndarray, sps: int) -> np.ndarray:
    """Each symbol times the code's chips (0 -> +1, 1 -> -1), held sps."""
    chips = _nrz(code)
    return np.concatenate([np.repeat(s * chips, sps) for s in sym])


def _settled_errors(got: np.ndarray, sent: np.ndarray, lags: int) -> int:
    """Raw mismatches over a settled window, at the pipeline's own delay.

    A demodulator with a matched filter and a timing loop delivers bit i a
    few outputs late, and that delay is the pipeline's, not the decision's.
    The window is aligned at whichever lag in ``[-lags, lags]`` fits best --
    an inverted stream disagrees with ~half of a random stream at EVERY lag,
    so the search cannot rescue a wrong polarity.
    """
    lo, n = 100, 100
    t = sent[lo : lo + n]
    return min(
        int(np.sum(got[lo + k : lo + k + n] != t))
        for k in range(-lags, lags + 1)
    )


Result = tuple[np.ndarray, np.ndarray, int]  # (decided, sent, lag search)


def _nrz_to_bin() -> Result:
    from doppler.cvt import nrz_to_bin

    out = np.zeros(_TX.size, np.uint8)
    nrz_to_bin(_nrz(_TX), out)
    return out, _TX, 0


def _mpsk_demap() -> Result:
    from doppler.mpsk import mpsk_demap

    return mpsk_demap(_nrz(_TX).astype(np.complex64), 2), _TX, 0


def _despreader() -> Result:
    from doppler.dsss import Despreader

    code = np.random.default_rng(1).integers(0, 2, 31).astype(np.uint8)
    x = _spread(code, _nrz(_TX), 4).astype(np.complex64)
    return Despreader(code, 4).bits(x), _TX, 0


def _burst_despreader() -> Result:
    from doppler.dsss import BurstDespreader

    code = np.random.default_rng(1).integers(0, 2, 31).astype(np.uint8)
    x = _spread(code, _nrz(_TX), 4).astype(np.complex64)
    return BurstDespreader(code, sf=31, sps=4).bits(x), _TX, 0


def _frame_burst() -> tuple[
    np.ndarray, object, np.ndarray, np.ndarray, np.ndarray
]:
    """sync | payload | CRC-16 behind an unmodulated preamble, at phase 0.

    Returns the burst, its description, the frame bits, and the acq and
    data codes.
    """
    from doppler.wfm import Frame

    rng = np.random.default_rng(0)
    acq = rng.integers(0, 2, 31).astype(np.uint8)
    dcode = rng.integers(0, 2, 8).astype(np.uint8)
    sync = rng.integers(0, 2, 13).astype(np.uint8)
    desc = Frame(sync=sync, payload=_TX[:32], crc="crc16")
    frame = np.asarray(desc.bits(), np.uint8)
    pre = np.tile(np.repeat(_nrz(acq), 4), 4)
    bb = np.concatenate([pre, _spread(dcode, _nrz(frame), 4)])
    return bb.astype(np.complex64), desc, frame, acq, dcode


def _burst_demod() -> Result:
    from doppler.dsss import BurstDemod

    x, desc, frame, acq, dcode = _frame_burst()
    d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
    d.set_preamble(acq, 4)
    d.set_prior(0.0, 0)
    return np.asarray(d.demod(x)), frame, 0


def _dsss_burst_receiver() -> Result:
    from doppler.dsss import DsssBurstReceiver

    x, desc, frame, acq, dcode = _frame_burst()
    rng = np.random.default_rng(3)
    cap = 0.02 * (rng.standard_normal(20000) + 1j * rng.standard_normal(20000))
    cap = cap.astype(np.complex64)
    cap[5000 : 5000 + x.size] += x
    r = DsssBurstReceiver(
        acq, dcode, desc, reps=4, spc=4, chip_rate=1.0e6, cn0_dbhz=50.0
    )
    return np.asarray(r.push(cap)), frame, 0


def _mpsk_receiver(make: Callable[[], object], real: bool = False) -> Result:
    tx = np.random.default_rng(2654).integers(0, 2, 400).astype(np.uint8)
    if real:  # a real IF at the design centre fs/4, carrier phase 0
        bb = np.repeat(_nrz(tx), 32)
        x = (bb * np.cos(0.5 * np.pi * np.arange(bb.size))).astype(np.float32)
    else:
        x = np.repeat(_nrz(tx), 8).astype(np.complex64)
    return np.asarray(make().bits(x)), tx, 6  # type: ignore[attr-defined]


def _viterbi() -> Result:
    """A soft decision is the same rule: LLR > 0 is bit 0, the sign of the
    NRZ symbol the encoder's output was sent as."""
    from doppler.coding import ConvEncoder, Viterbi

    sym = ConvEncoder([0o171, 0o133], k=7).encode(_TX)
    llr = (8.0 * _nrz(np.asarray(sym, np.uint8))).astype(np.float32)
    out = np.asarray(Viterbi([0o171, 0o133], k=7, depth=35).decode(llr))
    return out, _TX, 0


def _track(name: str, **kw: object) -> Callable[[], object]:
    def make() -> object:
        import doppler.track as t

        return getattr(t, name)(**kw)

    return make


#: discovered emitter -> its driver, or the reason it has none.
EMITTERS: dict[str, Callable[[], Result] | str] = {
    "cvt.nrz_to_bin": _nrz_to_bin,
    "mpsk.mpsk_demap": _mpsk_demap,
    "mpsk.mpsk_diff_demap": (
        "differential: the bits are phase DIFFERENCES, so an absolute "
        "polarity has nothing to act on"
    ),
    "Despreader.bits": _despreader,
    "BurstDespreader.bits": _burst_despreader,
    "BurstDemod.demod": _burst_demod,
    "DsssBurstReceiver.push": _dsss_burst_receiver,
    "MpskReceiver.bits": lambda: _mpsk_receiver(_track("MpskReceiver", m=2)),
    "BpskReceiver.bits": lambda: _mpsk_receiver(
        _track("BpskReceiver", sample_rate_hz=8.0, symbol_rate_hz=1.0)
    ),
    "MpskReceiverR.bits": lambda: _mpsk_receiver(
        _track("MpskReceiverR", m=2, sps=32.0, init_norm_freq=0.25), real=True
    ),
    "Viterbi.decode": _viterbi,
}


def test_every_emitter_is_driven() -> None:
    """The census is the manifests'; this table must match it both ways."""
    found = set(_discover())
    missing = sorted(found - EMITTERS.keys())
    stale = sorted(EMITTERS.keys() - found)
    assert not missing, (
        f"bit emitters with no polarity driver: {missing} -- add one to "
        "EMITTERS (or an exemption with its reason)"
    )
    assert not stale, f"drivers for emitters that no longer exist: {stale}"


@pytest.mark.parametrize(
    "emitter", sorted(k for k, v in EMITTERS.items() if callable(v))
)
def test_decides_by_one_minus_2b(emitter: str) -> None:
    """At carrier phase 0 every emitter returns the sent bits, RAW."""
    drive = EMITTERS[emitter]
    assert callable(drive)
    got, sent, lags = drive()
    got = np.asarray(got, np.uint8)
    if lags == 0:
        n = min(got.size, sent.size)
        assert n >= 32, f"{emitter}: only {got.size} bits decided"
        assert np.array_equal(got[:n], sent[:n]), (
            f"{emitter}: {int(np.sum(got[:n] != sent[:n]))}/{n} bits differ "
            "from 1 - 2b (mpsk_core.h)"
        )
    else:
        errs = _settled_errors(got, sent, lags)
        assert errs == 0, (
            f"{emitter}: {errs}/100 settled bits differ from 1 - 2b"
        )
