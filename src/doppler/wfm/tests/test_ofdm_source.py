"""The ``ofdm`` source type on every face: the refusals, and one source.

``type = "ofdm"`` sends one 802.11 PPDU per frame of a data source (the
bridge is ``wfm_synth_bridge.c``, the design ``docs/design/wlan.md``, section
5).
Every face asks the one validator, ``dp_wfm_source_error`` and, for the
rules that need ``fs``, ``dp_wfm_ofdm_fs_error``, so a refusal is ONE
sentence on the JSON scene, the public ``Synth``, a ``Composer`` and the CLI.
The C side -- the render against ``wlan``, the keys, the range checks -- is
``native/tests/test_wfm_ofdm_bridge.c``.

These tests pin:

- each refusal reaches every face with the same words;
- one source renders the same samples from Python keywords, its own JSON
  record and the CLI, and those samples are ``wlan``'s PPDU for the same
  octets (the identity, not a second implementation of it).
"""

from __future__ import annotations

import json
import subprocess
import zlib

import numpy as np
import pytest

from doppler.wfm import Composer, Segment, Synth, cli, field_bits
from doppler.wlan import Wlan

FS = 20e6
#: A fixed 100-octet payload, as the 800 bits of a PN Field (the CLI's
#: ``--data pn:800:15``), and the octets those bits are, first bit most
#: significant (the order a data file's bytes and a hex Field unpack in).
BITS = field_bits("pn:800:15")
OCTETS = np.packbits(BITS).ravel()

_OK = {
    "type": "ofdm",
    "fs": FS,
    "wlan": 36,
    "data": "pn:800:15",
    "data_len": 800,
}

#: name -> (the scene segment, the CLI flags beside --type ofdm, the words
#: every face's sentence carries).
REFUSALS = {
    "wlan-not-a-rate": (
        {**_OK, "wlan": 7},
        ["--wlan", "7"],
        "wlan is the 20 MHz data rate",
    ),
    "wlan-with-constellation": (
        {**_OK, "constellation": "qam16"},
        ["--constellation", "qam16"],
        "wlan is a spelling",
    ),
    "wlan-with-code-rate": (
        {**_OK, "code_rate": "3/4"},
        ["--code-rate", "3/4"],
        "wlan is a spelling",
    ),
    "wlan-with-another-bandwidth": (
        {**_OK, "bandwidth": 10e6},
        ["--bandwidth", "10e6"],
        "wlan is the data rate at 20 MHz",
    ),
    "pair-without-a-rate-row": (
        {
            **{k: v for k, v in _OK.items() if k != "wlan"},
            "constellation": "bpsk",
            "code_rate": "2/3",
        },
        ["--constellation", "bpsk", "--code-rate", "2/3"],
        "no SIGNAL RATE bits",
    ),
    "esno": (
        {**_OK, "snr_mode": "esno"},
        ["--snr-mode", "esno"],
        "snr_mode ebno and esno",
    ),
    "ebno": (
        {**_OK, "snr_mode": "ebno"},
        ["--snr-mode", "ebno"],
        "snr_mode ebno and esno",
    ),
    "fs-not-a-multiple": (
        {**_OK, "fs": 30e6},
        ["--fs", "30e6"],
        "fs must be an integer multiple",
    ),
    "window-too-long": (
        {**_OK, "window": 81},
        ["--window", "81"],
        "window is 0 to 80 times",
    ),
    "scrambler-out-of-range": (
        {**_OK, "scrambler_init": 128},
        ["--scrambler-init", "128"],
        "scrambler_init is the 7-bit",
    ),
    "mac-data": (
        {**_OK, "mac": "data"},
        ["--mac", "data"],
        "mac data needs a mac_header",
    ),
    "header-too-short": (
        {**_OK, "wlan": 36, "mac": "data", "mac_header": "0x" + "00" * 20},
        ["--mac", "data", "--mac-header", "0x" + "00" * 20],
        "mac_header is a data frame's header",
    ),
    "header-not-whole-octets": (
        {**_OK, "mac": "data", "mac_header": "10101"},
        ["--mac", "data", "--mac-header", "10101"],
        "mac_header is whole octets",
    ),
    "header-with-mac-none": (
        {**_OK, "mac_header": "0x" + "00" * 24},
        ["--mac-header", "0x" + "00" * 24],
        "mac_header is the MAC header",
    ),
    "header-with-mac-fcs": (
        {**_OK, "mac": "fcs", "mac_header": "0x" + "00" * 24},
        ["--mac", "fcs", "--mac-header", "0x" + "00" * 24],
        "mac_header is the MAC header",
    ),
    "data-len-not-octets": (
        {**_OK, "data_len": 12},
        ["--data-len", "12"],
        "data_len must be a multiple of 8",
    ),
}


def _kwargs(seg: dict) -> dict:
    """A scene segment as Python keywords: the data Field becomes bits."""
    return {
        k: field_bits(v) if k in ("data", "mac_header") else v
        for k, v in seg.items()
    }


def _cli(args: list[str], tmp_path):
    out = tmp_path / "out.cf32"
    p = subprocess.run(
        [cli._runnable(), *args, "--output", str(out)],
        capture_output=True,
        text=True,
        timeout=120,
    )
    return p, out


@pytest.mark.parametrize(
    ("seg", "flags", "words"), REFUSALS.values(), ids=REFUSALS.keys()
)
def test_every_face_refuses_with_one_sentence(seg, flags, words, tmp_path):
    with pytest.raises(ValueError) as scene:
        Composer.from_json(json.dumps({"version": 1, "segments": [seg]}))
    why = str(scene.value)
    assert words in why, why

    kw = _kwargs(seg)
    with pytest.raises(ValueError) as synth:
        Synth(**kw).steps(8)
    assert str(synth.value) == why
    with pytest.raises(ValueError) as comp:
        Composer([Segment(**kw)])
    assert str(comp.value) == why

    # The CLI: the accepted source's flags, less any the row gives itself.
    given = {f for f in flags if f.startswith("--")}
    base = ["--type", "ofdm", "--fs", repr(seg["fs"])]
    if "--wlan" not in given and "wlan" in seg:
        base += ["--wlan", str(seg["wlan"])]
    base += ["--data", "pn:800:15"]
    if "--data-len" not in given:
        base += ["--data-len", "800"]
    p, _ = _cli([*base, *flags], tmp_path)
    assert p.returncode == 2, p.stderr
    assert f"error: {why}" in p.stderr, p.stderr


def test_an_ofdm_field_on_another_type_is_refused_on_every_face(tmp_path):
    seg = {"type": "bpsk", "fs": FS, "num_samples": 64, "window": 4}
    with pytest.raises(ValueError) as scene:
        Composer.from_json(json.dumps({"version": 1, "segments": [seg]}))
    why = str(scene.value)
    assert "are fields of an ofdm source" in why, why
    with pytest.raises(ValueError) as synth:
        Synth(type="bpsk", fs=FS, window=4).steps(8)
    assert str(synth.value) == why
    p, _ = _cli(["--type", "bpsk", "--count", "64", "--window", "4"], tmp_path)
    assert p.returncode == 2, p.stderr
    assert f"error: {why}" in p.stderr, p.stderr


def test_the_engine_face_does_not_offer_the_type():
    """A bare engine has no renderer: it would play silence, no error."""
    from doppler.wfm import _SynthEngine

    with pytest.raises(ValueError, match="type must be one of"):
        _SynthEngine(type="ofdm", fs=FS)


def _ppdu_from_wlan() -> np.ndarray:
    """What ``wlan`` builds from the same octets, by its own Python face."""
    w = Wlan(constellation="qam16", code_rate="r34")
    return w.ppdu(OCTETS, 93)


def test_one_source_on_every_face(tmp_path):
    """Keywords == the JSON record == the CLI, and all are wlan's PPDU."""
    want = _ppdu_from_wlan()
    seg = {**_OK, "scrambler_init": 93}
    kw = _kwargs(seg)
    got = Synth(**kw).steps(len(want))
    np.testing.assert_allclose(got, want, atol=1e-5)
    # the source renders no more than the one PPDU its data holds
    assert not np.any(Synth(**kw).steps(len(want) + 40)[len(want) :])

    again = Composer.from_json(
        json.dumps({"version": 1, "segments": [seg]})
    ).compose()
    np.testing.assert_array_equal(again, got)

    p, out = _cli(
        [
            "--type",
            "ofdm",
            "--fs",
            "20e6",
            "--wlan",
            "36",
            "--scrambler-init",
            "93",
            "--data",
            "pn:800:15",
            "--data-len",
            "800",
        ],
        tmp_path,
    )
    assert p.returncode == 0, p.stderr
    np.testing.assert_array_equal(np.fromfile(out, np.complex64), got)


@pytest.mark.parametrize(
    "flags",
    [["--sync", "1011"], ["--crc", "crc16"], ["--acq-code", "0x9"]],
    ids=["sync", "crc", "acq-code"],
)
def test_the_frame_flags_are_refused_for_ofdm(flags, tmp_path):
    """The PHY builds its own fields: a preamble, a sync word or a CRC from
    flags would frame a waveform that carries no bit stream."""
    p, _ = _cli(
        [
            "--type",
            "ofdm",
            "--fs",
            "20e6",
            "--wlan",
            "36",
            "--data",
            "pn:800:15",
            "--data-len",
            "800",
            *flags,
        ],
        tmp_path,
    )
    assert p.returncode == 2, p.stderr
    assert "ofdm renders finished samples" in p.stderr, p.stderr


#: A 24-octet data frame header: Frame Control, Duration, three addresses,
#: Sequence Control.
HEADER = bytes.fromhex("08020000ffffffffffff0200000000010200000000020000")


def test_mac_data_is_header_body_and_fcs_on_every_face(tmp_path):
    """mac data sends the PPDU of header + the frame's octets + the FCS,
    the FCS computed here (zlib's CRC-32, least significant octet first), not
    by the code under test, and the three faces agree."""
    body = bytes(OCTETS)
    psdu = HEADER + body
    psdu += zlib.crc32(psdu).to_bytes(4, "little")
    want = Wlan(constellation="qam16", code_rate="r34").ppdu(
        np.frombuffer(psdu, np.uint8), 93
    )
    seg = {
        **_OK,
        "scrambler_init": 93,
        "mac": "data",
        "mac_header": "0x" + HEADER.hex(),
    }
    kw = _kwargs(seg)
    got = Synth(**kw).steps(len(want))
    np.testing.assert_allclose(got, want, atol=1e-5)
    again = Composer.from_json(
        json.dumps({"version": 1, "segments": [seg]})
    ).compose()
    np.testing.assert_array_equal(again, got)
    p, out = _cli(
        [
            "--type",
            "ofdm",
            "--fs",
            "20e6",
            "--wlan",
            "36",
            "--scrambler-init",
            "93",
            "--mac",
            "data",
            "--mac-header",
            "0x" + HEADER.hex(),
            "--data",
            "pn:800:15",
            "--data-len",
            "800",
        ],
        tmp_path,
    )
    assert p.returncode == 0, p.stderr
    np.testing.assert_array_equal(np.fromfile(out, np.complex64), got)


# --- the default window (#3190) -------------------------------------------
#
# A source nobody gave a window renders with the standard's 100 ns
# transition, ``2 * fs / bandwidth`` samples, as ``Wlan``'s default is at the
# same oversampling. ``window=0`` is the rectangular window, asked for. A
# scene or a record holds the count the source rendered with, never the
# absence, so it renders the same samples under any later default.

_RATES = [(20e6, 1), (40e6, 2), (80e6, 4)]


def _wlan(m: int, **kw) -> Wlan:
    return Wlan(constellation="qam16", code_rate="r34", oversample=m, **kw)


@pytest.mark.parametrize(("fs", "m"), _RATES)
def test_the_default_window_is_the_standards_100_ns(fs, m):
    """Unset is wlan's own default; ``window=0`` is wlan's rectangular PPDU."""
    seg = {**_OK, "fs": fs, "scrambler_init": 93}
    want = _wlan(m).ppdu(OCTETS, 93)
    rect = _wlan(m, window=0).ppdu(OCTETS, 93)
    assert len(want) == len(rect) + 2 * m  # the window is 2 M samples
    got = Synth(**_kwargs(seg)).steps(len(want))
    np.testing.assert_allclose(got, want, atol=1e-5)
    # the control: the rectangular PPDU is not the default one
    assert not np.allclose(got[: len(rect)], rect, atol=1e-3)
    zero = Synth(**_kwargs({**seg, "window": 0})).steps(len(rect))
    np.testing.assert_allclose(zero, rect, atol=1e-5)


@pytest.mark.parametrize(("fs", "m"), _RATES)
def test_a_scene_and_a_sigmf_hold_the_window_the_source_rendered_with(fs, m):
    """The record carries ``2 M``, never the absence; an explicit 0 is kept."""
    seg = {**_OK, "fs": fs, "scrambler_init": 93}
    for window, held in ((None, 2 * m), (0, 0), (3, 3)):
        kw = _kwargs(seg if window is None else {**seg, "window": window})
        c = Composer([Segment(**kw)])
        rec = json.loads(c.to_json())["segments"][0]
        assert rec["window"] == held, (window, rec)
        ann = json.loads(c.to_sigmf(fs=fs))["annotations"][0]
        assert ann["wfmgen:window"] == held
        # the record renders the samples the source rendered
        np.testing.assert_array_equal(
            Composer.from_json(c.to_json()).compose(), c.compose()
        )


@pytest.mark.parametrize(("fs", "m"), _RATES)
def test_a_scene_without_the_key_takes_the_default_at_load(fs, m):
    """A hand-written scene with no ``window`` is the unset one."""
    seg = {**_OK, "fs": fs, "scrambler_init": 93}
    bare = Composer.from_json(json.dumps({"version": 1, "segments": [seg]}))
    held = Composer.from_json(
        json.dumps({"version": 1, "segments": [{**seg, "window": 2 * m}]})
    )
    default = bare.compose()  # a finite source: one compose() per scene
    np.testing.assert_array_equal(default, held.compose())
    zero = Composer.from_json(
        json.dumps({"version": 1, "segments": [{**seg, "window": 0}]})
    )
    assert len(zero.compose()) == len(default) - 2 * m


def test_the_cli_omits_the_window_for_the_default_and_gives_0_for_none(
    tmp_path,
):
    base = [
        "--type", "ofdm", "--fs", "40e6", "--wlan", "36",
        "--scrambler-init", "93", "--data", "pn:800:15", "--data-len", "800",
    ]  # fmt: skip
    p, out = _cli(base, tmp_path)
    assert p.returncode == 0, p.stderr
    default = np.fromfile(out, np.complex64)
    p, out = _cli([*base, "--window", "4"], tmp_path)
    assert p.returncode == 0, p.stderr
    np.testing.assert_array_equal(np.fromfile(out, np.complex64), default)
    p, out = _cli([*base, "--window", "0"], tmp_path)
    assert p.returncode == 0, p.stderr
    rect = np.fromfile(out, np.complex64)
    assert len(rect) == len(default) - 4  # 2 M samples at M = 2
    assert not np.allclose(rect, default[: len(rect)], atol=1e-3)


def test_the_range_sentence_quotes_the_count_never_the_sentinel():
    """81 M is refused as the range says; the unset window never is."""
    with pytest.raises(ValueError, match="window is 0 to 80 times"):
        Synth(**_kwargs({**_OK, "window": 81})).steps(8)
    Synth(**_kwargs(_OK)).steps(8)  # unset: accepted at M = 1
