"""The 802.11 preset's zero-config defaults, on both faces (#3269).

The lead's decision: with nothing given, ``Wlan()`` and ``wfmgen --type
ofdm`` are 36 Mb/s (16-QAM at 3/4), the source's MAC appends the FCS, its
payload is 96 octets of PN15 (a 100-octet PSDU with the FCS, Annex I's
length), and its ``fs`` is the bandwidth. The pair is resolved whole by
one C function both faces call (``dp_wlan_rate_resolve``); a record and a
read-back hold the resolved value, never ``auto``.

Each test names the case it rejects. What v0.85.0's records read back as is
held against records that v0.85.0's own writer produced, committed as
``fixtures/v085_ofdm_records.json`` by ``fixtures/write_v085_ofdm_records.py``
run on a build of the v0.85.0 tag (commit 1b077b607).
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

import numpy as np
import pytest

from doppler.tests._repo import repo_root
from doppler.wfm import Composer, Segment, Synth, cli, field_bits
from doppler.wlan import Wlan

if sys.version_info >= (3, 11):
    import tomllib
else:
    import tomli as tomllib

ROOT = repo_root(__file__)

FIXTURES = Path(__file__).parent / "fixtures" / "v085_ofdm_records.json"
#: The default payload, as the source's data field spells it.
PAYLOAD = np.packbits(field_bits("pn:768:15")).ravel()


def _zero_config_ppdu(scrambler: int) -> np.ndarray:
    """Wlan's PPDU at zero config, of the default payload with its FCS."""
    w = Wlan()
    return w.ppdu(w.mpdu(PAYLOAD), scrambler)


def test_zero_config_is_36_on_both_faces(tmp_path):
    """``Wlan()`` and a bare ``--type ofdm`` give the same signal.

    Rejects either face's default left behind: v0.85.0's ``Wlan()`` was 24
    Mb/s and its CLI 6 Mb/s, so either renders another L-SIG RATE and the
    sample comparison fails.
    """
    assert Wlan().rate_mbps == Wlan(mbps=36).rate_mbps == 36
    want = _zero_config_ppdu(93)
    assert len(PAYLOAD) + 4 == 100  # the PSDU is Annex I's length

    out = tmp_path / "x.cf32"
    p = subprocess.run(
        [
            cli._runnable(),
            "--type",
            "ofdm",
            "--scrambler-init",
            "93",
            "--output",
            str(out),
        ],
        capture_output=True,
        text=True,
        timeout=120,
    )
    assert p.returncode == 0, p.stderr
    got = np.fromfile(out, np.complex64)
    np.testing.assert_allclose(got[: len(want)], want, atol=1e-5)
    # the Python faces render the same samples
    syn = Synth(type="ofdm", scrambler_init=93).steps(len(want))
    np.testing.assert_allclose(syn, want, atol=1e-5)
    seg = Composer([Segment(type="ofdm", scrambler_init=93)]).compose()
    np.testing.assert_array_equal(seg[: len(want)], got[: len(want)])
    # the control: v0.85.0's CLI default pair is another signal
    old = Wlan(constellation="bpsk", code_rate="r12")
    alt = old.ppdu(old.mpdu(PAYLOAD), 93)
    assert not np.allclose(got[: len(alt)], alt[: len(got)], atol=1e-3)


def test_mbps_is_the_tables_pair_for_all_eight():
    """``Wlan(mbps=N)`` is the object of Table 17-6's pair for N.

    Rejects a hand-typed rate map in the binding: the eight N come from the
    table (``rate_mbps`` over every pair), never from a list here.
    """
    pairs = [
        (c, r)
        for c in ("bpsk", "qpsk", "qam16", "qam64")
        for r in ("r12", "r23", "r34", "r56")
    ]
    rows = {
        w.rate_mbps: w
        for c, r in pairs
        if (w := Wlan(constellation=c, code_rate=r)).rate_mbps
    }
    assert len(rows) == 8
    for mbps, by_pair in rows.items():
        by_rate = Wlan(mbps=mbps)
        assert (by_rate.n_bpsc, by_rate.n_dbps, by_rate.rate_mbps) == (
            by_pair.n_bpsc,
            by_pair.n_dbps,
            mbps,
        )


@pytest.mark.parametrize(
    ("kw", "words"),
    [
        ({"constellation": "qam16"}, "are one rate: give both, or neither"),
        ({"code_rate": "r34"}, "are one rate: give both, or neither"),
        ({"mbps": 36, "code_rate": "r34"}, "is a spelling of constellation"),
        (
            {"mbps": 36, "constellation": "qam16", "code_rate": "r34"},
            "is a spelling of constellation",
        ),
        ({"mbps": 7}, "the data rate is one of 6, 9, 12"),
    ],
    ids=[
        "constellation-alone",
        "code-rate-alone",
        "mbps-beside-half",
        "mbps-beside-pair",
        "mbps-not-a-row",
    ],
)
def test_wlan_refuses_in_the_sources_words(kw, words):
    """``Wlan`` and the source refuse a rate in the same sentence.

    Rejects a second copy of the rule on either face: the sentence is the
    one resolver's, so the source's refusal of the same spelling is equal.
    """
    with pytest.raises(ValueError) as w:
        Wlan(**kw)
    assert words in str(w.value)
    src = {k: v for k, v in kw.items() if k != "mbps"}
    if "mbps" in kw:
        src["wlan"] = kw["mbps"]
    src = {k: ("3/4" if v == "r34" else v) for k, v in src.items()}
    with pytest.raises(ValueError) as s:
        Composer([Segment(type="ofdm", **src)])
    assert str(s.value) == str(w.value)


def test_a_rate_off_20_mhz_is_refused():
    """``wlan N`` is the 20 MHz rate; the zero-config pair is not ``wlan``.

    Rejects dropping the 20 MHz check: ``wlan=36`` at 10 MHz would render an
    18 Mb/s signal labeled 36. The zero-config source at 10 MHz is accepted:
    it names the pair, which carries 18 Mb/s there, as the help says.
    """
    with pytest.raises(ValueError, match="wlan is the data rate at 20 MHz"):
        Composer([Segment(type="ofdm", wlan=36, bandwidth=10e6)])
    seg = json.loads(
        Composer([Segment(type="ofdm", bandwidth=10e6)]).to_json()
    )["segments"][0]
    assert (seg["constellation"], seg["code_rate"]) == ("qam16", "3/4")
    assert seg["fs"] == 10e6


def test_auto_is_never_read_or_written():
    """An ofdm source's read-back and record hold the resolved value.

    Scoped to ofdm: these three are an ofdm source's fields, and another
    type reads back what was given (``auto`` unless set; decided with #3273's
    review), pinned on its own below. Rejects a getter or a writer that
    passes ``auto`` through for an ofdm source, or a tone that claims an
    ofdm source's 16-QAM.
    """
    s = Synth(type="ofdm")
    assert (s.constellation, s.code_rate, s.mac) == ("qam16", "3/4", "fcs")
    w = Synth(type="ofdm", wlan=54)
    assert (w.constellation, w.code_rate) == ("qam64", "3/4")
    for seg in (Segment(type="ofdm"), Segment(type="ofdm", wlan=6)):
        c = Composer([seg])
        rec = json.loads(c.to_json())["segments"][0]
        ann = json.loads(c.to_sigmf())["annotations"][0]
        for k in ("constellation", "code_rate", "mac"):
            assert rec.get(k) != "auto" and ann.get(f"wfmgen:{k}") != "auto"
        assert rec["mac"] == "fcs" and ann["wfmgen:mac"] == "fcs"
        if "wlan" not in rec:  # the resolved pair is written, not omitted
            assert (rec["constellation"], rec["code_rate"]) == ("qam16", "3/4")
            assert ann["wfmgen:constellation"] == "qam16"
        # the record composes again, to the same samples
        np.testing.assert_array_equal(
            Composer.from_json(c.to_json()).compose(), c.compose()
        )
    # a wlan source writes wlan alone: one spelling, one reading
    rec = json.loads(Composer([Segment(type="ofdm", wlan=6)]).to_json())
    assert "constellation" not in rec["segments"][0]


def test_a_tone_reads_back_what_was_given():
    """Another type has no constellation: it reads ``auto`` unless set, and
    its record carries none of the three. Rejects a tone claiming 16-QAM
    (an ofdm source's default) or BPSK (the struct's zero)."""
    t = Synth(type="tone")
    assert (t.constellation, t.code_rate, t.mac) == ("auto", "auto", "auto")
    rec = json.loads(Composer([Segment(type="tone")]).to_json())
    assert not {"constellation", "code_rate", "mac"} & set(rec["segments"][0])


def test_an_explicit_zero_is_written_and_read_back():
    """bpsk, 1/2 and none are index 0 of their choices, and a record keeps
    them. Rejects the writer's omit-at-zero rule reaching these rows: an
    explicit 6 Mb/s without the FCS would be written as nothing and reread
    as 36 Mb/s with it."""
    kw = {"constellation": "bpsk", "code_rate": "1/2", "mac": "none"}
    c = Composer([Segment(type="ofdm", **kw)])
    rec = json.loads(c.to_json())["segments"][0]
    assert {k: rec.get(k) for k in kw} == kw
    again = Composer.from_json(c.to_json())
    np.testing.assert_array_equal(again.compose(), c.compose())


def test_a_segments_rate_does_not_depend_on_source_order():
    """Every ofdm source at a default fs names its bandwidth, whatever its
    place in the segment. Rejects the first source alone deciding: [tone,
    ofdm] would keep fs 1.0 and refuse its ofdm source."""
    for order in (("tone", "ofdm"), ("ofdm", "tone")):
        seg = Segment.sum(*(Synth(type=t) for t in order))
        rec = json.loads(Composer([seg]).to_json())["segments"][0]
        assert rec["fs"] == 20e6, order
    # the refusal names the two sources and their bandwidths
    with pytest.raises(ValueError) as e:
        Composer(
            [
                Segment.sum(
                    Synth(type="ofdm"), Synth(type="ofdm", bandwidth=10e6)
                )
            ]
        )
    assert "ofdm sources 0 (20 MHz) and 1 (10 MHz) differ in bandwidth" in (
        str(e.value)
    )


def _pair(s: Synth) -> tuple[str, str]:
    return (s.constellation, s.code_rate)


def _recorded_pair(s: Synth) -> tuple[str, str]:
    """The pair a record of @p s holds, through Segment.sum and to_json."""
    rec = json.loads(Composer([Segment.sum(s)]).to_json())["segments"][0]
    src = rec["sources"][0] if "sources" in rec else rec
    return (src["constellation"], src["code_rate"])


@pytest.mark.parametrize(
    "half, value, other, fixed",
    [
        ("constellation", "bpsk", "code_rate", "3/4"),
        ("code_rate", "1/2", "constellation", "qam16"),
    ],
)
def test_a_live_half_fixes_the_other_at_its_read(half, value, other, fixed):
    """Either order: setting one half beside an auto other fixes the other
    at the value it read, so the pair is whole and a record carries it.
    Rejects a setter that stores the half alone (the source would then be
    refused at build as half a pair)."""
    s = Synth(type="ofdm")
    setattr(s, half, value)
    assert getattr(s, half) == value and getattr(s, other) == fixed
    s.steps(8)  # builds: the pair is whole
    want = _pair(s)
    assert _recorded_pair(s) == want


def test_auto_returns_the_whole_pair():
    """auto is the pair's: one half set to auto returns both, and the next
    half set fixes the other at its read again. Rejects a setter that makes
    one half auto beside a given other (setting qpsk then would keep the
    old 1/2, not fix the default's 3/4)."""
    s = Synth(type="ofdm", constellation="bpsk", code_rate="1/2")
    s.code_rate = "auto"
    assert _pair(s) == ("qam16", "3/4")  # both auto: the default pair
    s.wlan = 54  # accepted only because the pair is auto again
    s.wlan = 0
    s.constellation = "qpsk"
    assert _pair(s) == ("qpsk", "3/4")
    assert _recorded_pair(s) == ("qpsk", "3/4")


def test_a_second_spelling_is_refused_either_way():
    """A half beside wlan, and wlan beside a given pair: each refused with
    its sentence, the object untouched. Rejects either setter storing."""
    w = Synth(type="ofdm", wlan=54)
    with pytest.raises(ValueError, match="the rate is set by wlan"):
        w.constellation = "bpsk"
    assert (w.wlan, *_pair(w)) == (54, "qam64", "3/4")
    g = Synth(type="ofdm", constellation="bpsk", code_rate="1/2")
    with pytest.raises(
        ValueError, match="the rate is set by constellation and code_rate"
    ):
        g.wlan = 54
    assert (g.wlan, *_pair(g)) == (0, "bpsk", "1/2")


def test_a_pair_the_table_lacks_is_written_then_refused_at_build():
    """(qam16, 5/6) is a given pair Table 17-6 lacks: the setter writes it
    and the source is refused when it builds, naming the pair and the
    table. Rejects a setter that refuses it (the doc says the build does)
    and a build that accepts it."""
    s = Synth(type="ofdm", constellation="qam16", code_rate="3/4")
    s.code_rate = "5/6"
    assert _pair(s) == ("qam16", "5/6")
    with pytest.raises(ValueError, match="no SIGNAL RATE bits"):
        s.steps(8)


def test_another_type_stores_a_half_as_given():
    """On a tone the rule does not apply: a half is stored as written and
    the other stays auto. Rejects the ofdm rule leaking onto every type."""
    t = Synth(type="tone")
    t.constellation = "bpsk"
    assert _pair(t) == ("bpsk", "auto")


def _readback(s: Synth) -> dict:
    """Every property the Synth reads back, by name: what a refused write
    must leave exactly as it was."""
    names = [
        n
        for n in dir(type(s))
        if isinstance(getattr(type(s), n), property)
        or type(getattr(type(s), n)).__name__ == "getset_descriptor"
    ]
    return {n: repr(getattr(s, n)) for n in names if not n.startswith("_")}


def _tone_with(**given) -> Synth:
    s = Synth(type="tone")
    for name, value in given.items():
        setattr(s, name, value)
    return s


@pytest.mark.parametrize(
    "given, words",
    [
        ({"constellation": "bpsk"}, "are one rate: give both, or neither"),
        ({"code_rate": "1/2"}, "are one rate: give both, or neither"),
        (
            {"constellation": "bpsk", "code_rate": "1/2", "wlan": 36},
            "is a spelling of constellation and code_rate",
        ),
        ({"wlan": 7}, "the data rate is one of 6, 9"),
        (
            {"wlan": 36, "bandwidth": 10e6},
            "wlan is the data rate at 20 MHz",
        ),
    ],
)
def test_a_type_write_to_ofdm_refuses_what_the_build_would(given, words):
    """A tone holds the rate fields as given; the write that makes it ofdm
    is refused with the build's sentence, and every property reads back as
    before. Rejects a type setter that stores and leaves the refusal to
    steps() (#3273's case: ('bpsk', '3/4') read back, then refused)."""
    s = _tone_with(**given)
    before = _readback(s)
    with pytest.raises(ValueError, match=words):
        s.type = "ofdm"
    assert _readback(s) == before


def test_a_type_write_with_a_whole_pair_builds():
    """The accepted case beside the refusals: a whole pair carried into
    ofdm is written and renders."""
    s = _tone_with(constellation="bpsk", code_rate="1/2")
    s.type = "ofdm"
    assert _pair(s) == ("bpsk", "1/2")
    assert s.steps(8).shape == (8,)


@pytest.mark.parametrize(
    "start, name, value, words",
    [
        ({}, "wlan", 7, "the data rate is one of 6, 9"),
        ({"wlan": 36}, "bandwidth", 10e6, "wlan is the data rate at 20 MHz"),
        ({"bandwidth": 10e6}, "wlan", 36, "wlan is the data rate at 20 MHz"),
    ],
)
def test_a_live_ofdm_write_refuses_what_the_build_would(
    start, name, value, words
):
    """A wlan no row names, and wlan off 20 MHz from either side, are
    refused at the write, every property unchanged. Rejects a setter that
    stores a source reading as a valid pair that steps() then refuses."""
    s = Synth(type="ofdm", **start)
    before = _readback(s)
    with pytest.raises(ValueError, match=words):
        setattr(s, name, value)
    assert _readback(s) == before


@pytest.mark.xfail(
    strict=True,
    reason="the constructor calls no check until just-makeit#2856's "
    "check_fn; #3273 pins it at that release",
)
def test_construction_with_half_a_pair_is_refused():
    """The constructor route of the same rule (#3273)."""
    with pytest.raises(ValueError, match="are one rate: give both"):
        Synth(type="ofdm", constellation="bpsk")


def _synth_fs_default() -> float:
    """``WFM_SYNTH_FS_DEFAULT``, the constant the manifest names.

    The Synth's constructor takes it by ``default_raw`` (just-makeit
    0.136.0, gh-2747) and the ofdm rule reads it, so the two are one
    constant. Rejects the manifest going back to a literal beside it.
    """
    hdr = (ROOT / "native/inc/doppler/wfm_synth/wfm_synth_core.h").read_text(
        encoding="utf-8"
    )
    named = float(
        re.search(r"#define WFM_SYNTH_FS_DEFAULT (\S+)", hdr).group(1)
    )
    man = tomllib.loads(
        (ROOT / "objects/wfm_synth.toml").read_text(encoding="utf-8")
    )
    fs = next(p for p in man["wfm_synth"]["init_params"] if p["name"] == "fs")
    assert fs["default_raw"] == "WFM_SYNTH_FS_DEFAULT" and "default" not in fs
    return named


def test_an_fs_at_a_default_is_the_bandwidth():
    """For ofdm an fs at a face's default is read as unset, on every face.

    The faces: a Synth's ``fs``, a Segment's ``fs``, a Composer's segment,
    its record, and the CLI's record (``wfmgen_flag_matrix.json``'s
    ``ofdm_zero_config`` row, fs 20000000, held by ctest's
    ``wfmgen_flag_matrix``).
    Rejects a read of the stored default: before just-makeit 0.136.0 a
    Synth read 1e6 and a Segment 1.0 while both rendered at 20e6. Rejects
    the rule reaching any other fs: 2.0 and 1.5e6 are still refused, a tone
    and an ofdm source at a given rate read as given, and a segment whose
    sources disagree reads the 1.0 it was given.
    """
    synth_default = _synth_fs_default()
    assert Synth().fs == synth_default
    assert Synth(type="tone", fs=1.0).fs == 1.0
    for fs in (1.0, synth_default):
        assert Synth(type="ofdm", fs=fs).fs == 20e6
        assert Synth(type="ofdm", fs=fs, bandwidth=10e6).fs == 10e6
        assert Segment(type="ofdm", fs=fs).fs == 20e6
        c = Composer([Segment(type="ofdm", fs=fs)])
        assert json.loads(c.to_json())["segments"][0]["fs"] == 20e6
        assert c.segments[0].fs == 20e6
    assert Synth(type="ofdm").fs == 20e6
    assert Segment(type="ofdm").fs == 20e6
    assert Synth(type="ofdm", fs=40e6).fs == 40e6
    assert Segment(type="ofdm", fs=40e6).fs == 40e6
    # the rate needs no pair: a live half pair reads it too
    s = Synth(type="ofdm")
    s.constellation = "bpsk"
    assert s.fs == 20e6
    # every source is asked, in any order; a disagreement reads as given
    for order in (("tone", "ofdm"), ("ofdm", "tone")):
        assert Segment.sum(*(Synth(type=t) for t in order)).fs == 20e6
    two = Segment.sum(Synth(type="ofdm"), Synth(type="ofdm", bandwidth=10e6))
    assert two.fs == 1.0
    a = Synth(type="ofdm", scrambler_init=5).steps(500)
    b = Synth(type="ofdm", scrambler_init=5, fs=20e6).steps(500)
    np.testing.assert_array_equal(a, b)
    for fs in (2.0, 1.5e6):
        with pytest.raises(ValueError, match="fs must be an integer multiple"):
            Composer([Segment(type="ofdm", fs=fs)])


def _v085() -> dict:
    data = json.loads(FIXTURES.read_text(encoding="utf-8"))
    assert data["written_by"] == "0.85.0"
    return data["records"]


#: The records a v0.85.0 user wrote, and how each reads now, as decided:
#: (constellation, code_rate, mac) after a re-record, or the refusal.
RULED = {
    "rate6_default": ("qam16", "3/4", "fcs"),
    "rate6_explicit": ("qam16", "3/4", "fcs"),
    "rate9": "are one rate",
    "rate12": "are one rate",
    "rate24": "are one rate",
    "rate18": ("qpsk", "3/4", "fcs"),
    "rate36_mac_none": ("qam16", "3/4", "fcs"),
    "rate36_mac_fcs": ("qam16", "3/4", "fcs"),
    "rate48": ("qam64", "2/3", "fcs"),
    "rate54": ("qam64", "3/4", "fcs"),
    "wlan36_mac_fcs": None,  # wlan alone, as written
}


@pytest.mark.parametrize("name", sorted(RULED))
def test_a_v085_record_reads_as_decided(name):
    """A v0.85.0 record reads back as the declared break says.

    Rejects a silent reread: under per-field resolution the 24 Mb/s record
    would compose as 36 with no error. What does not change -- fs, the
    payload, a wlan source's rate, and the pair of every record that wrote
    both keys -- is held equal to the record as written.
    """
    old = _v085()[name]
    want = RULED[name]
    if isinstance(want, str):
        with pytest.raises(ValueError, match=want):
            Composer.from_json(json.dumps(old))
        return
    new = json.loads(Composer.from_json(json.dumps(old)).to_json())
    o, n = old["segments"][0], new["segments"][0]
    assert n["fs"] == o["fs"] and n["data"] == o["data"]
    if want is None:
        assert n.get("wlan") == o["wlan"] and "constellation" not in n
        assert n["mac"] == o["mac"]
        return
    assert (n["constellation"], n["code_rate"], n["mac"]) == want
    # a pair the record wrote whole is the pair it reads
    if "constellation" in o and "code_rate" in o:
        assert (n["constellation"], n["code_rate"]) == (
            o["constellation"],
            o["code_rate"],
        )
