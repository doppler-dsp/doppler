"""The per-frame code-only window of a continuous dsss stream, on every face.

Of every ``frame_symbols`` symbols the first ``code_only_symbols`` carry the
spreading code alone, and the data pauses through them. The C synth has the
window (``dp_wfm_synth_set_dsss_window``); a source carries it as two fields,
so the public ``Synth``, a ``Segment`` scene, the CLI and a ``Plan`` all
reach it. These tests pin three properties:

- the public ``Synth`` IS the engine with the window set on it, sample for
  sample, over the seeded PN and over a payload, clean and noisy -- the
  bridge adds no behaviour of its own;
- one windowed scene renders byte for byte from Python kwargs, its own JSON
  record, the CLI replaying that record and the CLI's bare flags;
- every rule is refused with ONE sentence on every face, because every face
  asks the same question of a source (``dp_wfm_source_error``).

The C side -- the run length a data source derives through the windows, the
SigMF keys, the source-to-synth hand-off -- is ``test_wfm_compose.c`` and
``test_wfm_writer_core.c``; the Plan case is in ``test_plan.py``.
"""

from __future__ import annotations

import json
import subprocess

import numpy as np
import pytest

from doppler.wfm import Composer, Segment, Synth, _SynthEngine, cli, field_bits

FS, SPS = 1e6, 2
#: Chips per symbol, deliberately not a whole number: the symbol edges, and
#: so the frame edges, land at any chip phase.
CPS = 12.7
RATE = FS / SPS / CPS
CODE = np.random.default_rng(3).integers(0, 2, 31, dtype=np.uint8)
#: Three symbols of code alone opening every eight-symbol frame.
W, F = 3, 8
#: Four frames' worth of samples.
N = int(4 * F * CPS) * SPS
#: A payload shorter than the four frames' data symbols.
PAYLOAD = np.random.default_rng(4).integers(0, 2, 20, dtype=np.uint8)


def _synth(snr: float, payload, **window) -> Synth:
    """The public face: a continuous dsss Synth, the window as kwargs."""
    extra = {} if payload is None else {"data": payload}
    return Synth(
        type="dsss",
        fs=FS,
        sps=SPS,
        seed=7,
        snr=snr,
        snr_mode="fs",
        symbol_rate=RATE,
        data_code=CODE,
        **extra,
        **window,
    )


def _engine(like: Synth, payload) -> _SynthEngine:
    """The raw engine configured as `like` is, then the window set on it.

    Its PN register length is read off the Synth rather than restated, so
    the two can only differ by what the test is about."""
    e = _SynthEngine(
        type="dsss",
        fs=FS,
        freq=0.0,
        snr=like.snr,
        snr_mode="fs",
        seed=like.seed,
        sps=SPS,
        pn_length=like.pn_length,
    )
    cps = (FS / SPS) / RATE
    if payload is None:
        e.set_dsss_cont(CODE, cps, data="prbs")
    else:
        e.set_dsss_cont(CODE, cps, payload=payload)
    e.set_dsss_window(code_only_symbols=W, frame_symbols=F)
    return e


@pytest.mark.parametrize("snr", [100.0, 20.0], ids=["clean", "noisy"])
@pytest.mark.parametrize("payload", [None, PAYLOAD], ids=["prbs", "payload"])
def test_the_synths_window_is_the_engines(snr, payload):
    """Sample for sample, and not the windowless stream."""
    s = _synth(snr, payload, code_only_symbols=W, frame_symbols=F)
    assert (s.code_only_symbols, s.frame_symbols) == (W, F)
    x = s.steps(N)
    np.testing.assert_array_equal(x, _engine(s, payload).steps(N))
    assert not np.array_equal(x, _synth(snr, payload).steps(N))


def test_the_window_carries_the_code_alone():
    """Read off the public face alone: every chip in a window is the bare
    code, and the data section carries data. (The engine's own test reads
    the payload running on across frames; parity above makes it this
    face's too.)"""
    x = _synth(100.0, None, code_only_symbols=W, frame_symbols=F).steps(N)
    nchips = N // SPS
    bits = (x.real[::SPS] < 0).astype(np.uint8) ^ np.resize(CODE, nchips)
    in_window = (np.arange(nchips) / CPS).astype(np.int64) % F < W
    assert not bits[in_window].any()
    assert bits[~in_window].any()


@pytest.mark.parametrize(
    "n_bits", [2 * (F - W), 2 * (F - W) + 2], ids=["on-a-frame", "mid-frame"]
)
def test_the_stream_ends_where_its_run_does(n_bits):
    """A payload's stream is silent from its run's end, for good. The case
    that matters is a payload that fills its frames' data symbols exactly:
    the next symbol opens a window, and no data follows it, so it is not
    sent. The run is the Composer's, the stream the public Synth's."""
    bits = PAYLOAD[:n_bits]
    run = (
        Composer(
            [
                Segment(
                    type="dsss",
                    fs=FS,
                    sps=SPS,
                    seed=7,
                    snr=100.0,
                    symbol_rate=RATE,
                    data_code=CODE,
                    data=bits,
                    code_only_symbols=W,
                    frame_symbols=F,
                )
            ]
        )
        .compose()
        .size
    )
    x = _synth(100.0, bits, code_only_symbols=W, frame_symbols=F).steps(N)
    assert run < N
    assert np.flatnonzero(np.abs(x) > 0.5)[-1] + 1 == run


def _cli(args, tmp_path, name="out.cf32"):
    out = tmp_path / name
    p = subprocess.run(
        [cli._runnable(), *args, "--output", str(out)],
        capture_output=True,
        text=True,
        timeout=120,
    )
    return p, out


def test_one_windowed_scene_on_every_face(tmp_path):
    """kwargs == its JSON record == the CLI replaying it == bare flags."""
    seg = Segment(
        type="dsss",
        fs=FS,
        sps=SPS,
        seed=7,
        snr=20.0,
        snr_mode="fs",
        symbol_rate=RATE,
        data_code=CODE,
        num_samples=N,
        code_only_symbols=W,
        frame_symbols=F,
    )
    comp = Composer([seg])
    x = comp.compose()
    np.testing.assert_array_equal(
        x, _synth(20.0, None, code_only_symbols=W, frame_symbols=F).steps(N)
    )

    text = comp.to_json()
    rec = json.loads(text)["segments"][0]
    assert (rec["code_only_symbols"], rec["frame_symbols"]) == (W, F)
    np.testing.assert_array_equal(Composer.from_json(text).compose(), x)

    scene = tmp_path / "scene.json"
    scene.write_text(text)
    p, out = _cli(["--from-file", str(scene)], tmp_path, "replay.cf32")
    assert p.returncode == 0, p.stderr
    np.testing.assert_array_equal(np.fromfile(out, np.complex64), x)

    p, out = _cli(
        [
            "--type",
            "dsss",
            "--fs",
            repr(FS),
            "--sps",
            str(SPS),
            "--seed",
            "7",
            "--snr",
            "20",
            "--snr-mode",
            "fs",
            "--symbol-rate",
            repr(RATE),
            "--data-code",
            "".join(map(str, CODE)),
            "--code-only-symbols",
            str(W),
            "--frame-symbols",
            str(F),
            "--count",
            str(N),
        ],
        tmp_path,
        "flags.cf32",
    )
    assert p.returncode == 0, p.stderr
    np.testing.assert_array_equal(np.fromfile(out, np.complex64), x)


# ── every rule, one sentence on every face ──────────────────────────────────

_CONT = {
    "type": "dsss",
    "sps": SPS,
    "fs": FS,
    "symbol_rate": RATE,
    "data_code": "".join(map(str, CODE)),
}
_BURST = {"type": "dsss", "sps": SPS, "fs": FS, "acq_code": "pn:31:5"}

#: (scene segment, CLI flags, the words the sentence must hold). A scene's
#: `code_only` is Python's `dsss_code_only`; everything else is spelled alike.
REFUSALS = {
    "burst": (
        {**_BURST, "frame_symbols": F, "code_only_symbols": W},
        ["--frame-symbols", str(F), "--code-only-symbols", str(W)],
        "give type dsss with a symbol_rate",
    ),
    "tone": (
        {"type": "tone", "frame_symbols": F, "code_only_symbols": W},
        ["--frame-symbols", str(F), "--code-only-symbols", str(W)],
        "give type dsss with a symbol_rate",
    ),
    "no-frame": (
        {**_CONT, "code_only_symbols": W},
        ["--code-only-symbols", str(W)],
        "give frame_symbols",
    ),
    "frame-without-window": (
        {**_CONT, "frame_symbols": F},
        ["--frame-symbols", str(F)],
        "give code_only_symbols",
    ),
    "longer-than-its-frame": (
        {**_CONT, "frame_symbols": F, "code_only_symbols": F + 1},
        ["--frame-symbols", str(F), "--code-only-symbols", str(F + 1)],
        "exceeds frame_symbols",
    ),
    "beside-code-only": (
        {
            **_CONT,
            "frame_symbols": F,
            "code_only_symbols": W,
            "code_only": True,
        },
        [
            "--frame-symbols",
            str(F),
            "--code-only-symbols",
            str(W),
            "--code-only",
        ],
        "changes nothing",
    ),
    "all-window-with-data": (
        {**_CONT, "frame_symbols": F, "code_only_symbols": F, "data": "0xAB"},
        [
            "--frame-symbols",
            str(F),
            "--code-only-symbols",
            str(F),
            "--data",
            "0xAB",
        ],
        "would never be sent",
    ),
}


def _kwargs(seg: dict) -> dict:
    """A scene segment as Python kwargs: Field strings become bit arrays."""
    fields = {"data_code", "acq_code", "data"}
    kw = {k: field_bits(v) if k in fields else v for k, v in seg.items()}
    if "code_only" in kw:
        kw["dsss_code_only"] = kw.pop("code_only")
    return kw


def _flags(seg: dict) -> list[str]:
    """The CLI flags for a segment's type, rate and codes."""
    args = ["--type", seg["type"]]
    if seg["type"] == "dsss":
        args += ["--sps", str(SPS), "--fs", repr(FS)]
        if "symbol_rate" in seg:
            args += ["--symbol-rate", repr(RATE)]
            args += ["--data-code", seg["data_code"]]
        else:
            args += ["--acq-code", seg["acq_code"]]
    return args


@pytest.mark.parametrize(
    ("seg", "flags", "words"), REFUSALS.values(), ids=REFUSALS.keys()
)
def test_every_face_refuses_with_one_sentence(seg, flags, words, tmp_path):
    count = {} if "data" in seg else {"num_samples": 64}
    with pytest.raises(ValueError) as scene:
        Composer.from_json(
            json.dumps({"version": 1, "segments": [{**seg, **count}]})
        )
    why = str(scene.value)
    assert words in why, why

    kw = _kwargs(seg)
    with pytest.raises(ValueError) as synth:
        Synth(**kw).steps(8)
    assert str(synth.value) == why
    with pytest.raises(ValueError) as comp:
        Composer([Segment(**kw, **count)])
    assert str(comp.value) == why

    tail = [] if "data" in seg else ["--count", "64"]
    p, _ = _cli([*_flags(seg), *flags, *tail], tmp_path)
    assert p.returncode == 2, p.stderr
    assert f"error: {why}" in p.stderr, p.stderr
