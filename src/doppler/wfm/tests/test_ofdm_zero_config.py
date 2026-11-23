"""The 802.11 preset's zero-config defaults, on both faces (#3269).

The owner's ruling: with nothing given, ``Wlan()`` and ``wfmgen --type
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
    type reads back what was given (``auto`` unless set; ruled with #3273's
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
    assert (
        "ofdm sources 0 (2e+07 Hz) and 1 (1e+07 Hz) differ in bandwidth"
        in (str(e.value))
    )


def test_a_live_half_pair_reads_as_written():
    """A half pair on a live object reads each given half as written, and
    the default's for the other, never auto. Rejects a getter that masks a
    write with the default pair: bpsk given must not read as qam16."""
    s = Synth(type="ofdm")
    s.constellation = "bpsk"
    assert (s.constellation, s.code_rate) == ("bpsk", "3/4")
    t = Synth(type="ofdm")
    t.code_rate = "5/6"
    assert (t.constellation, t.code_rate) == ("qam16", "5/6")


def _synth_fs_default() -> float:
    """``WFM_SYNTH_FS_DEFAULT``, held equal to the manifest's literal.

    The bridge's rule reads the C name; the Synth's constructor takes the
    manifest's ``default`` (jm's source fs reads no ``default_raw``,
    just-makeit#2747). Rejects the two drifting apart.
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
    assert float(fs["default"]) == named
    return named


def test_an_fs_at_a_default_is_the_bandwidth():
    """For ofdm an fs at the face's default is read as unset.

    Rejects the rule reaching any other fs: 2.0 and 1.5e6 are still
    refused. The consequence, pinned: an explicit fs at a default is read
    the same way, and the record shows the bandwidth.
    """
    synth_default = _synth_fs_default()
    assert Synth().fs == synth_default
    for fs in (1.0, synth_default):
        c = Composer([Segment(type="ofdm", fs=fs)])
        assert json.loads(c.to_json())["segments"][0]["fs"] == 20e6
        assert c.segments[0].fs == 20e6
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


#: The records a v0.85.0 user wrote, and how each reads now, as ruled:
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
def test_a_v085_record_reads_as_ruled(name):
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
