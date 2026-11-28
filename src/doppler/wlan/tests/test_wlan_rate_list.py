"""The 802.11 data-rate list is typed once, and every other spelling of it
is held equal to it.

``--wlan N`` names a row of Table 17-6 by its data rate, and its help, the
manifest doc, the headers and the design page each say which rates there
are. The table is the one source (``dp_wlan_tables_standard()``, read
through ``Wlan.rate_mbps``), so the list is DERIVED here and every hand-owned
sentence that names it is checked against it. A new table row, or a rate
retyped wrong, turns this red instead of leaving a face that lists a rate
the table does not carry.

Three checks:

* the hand-owned sources (:data:`SITES`) each contain the list, in the
  spelling that site uses;
* the faces that print it at run time (``wfmgen --help`` and the refusal of
  a rate the table lacks) say the same list;
* no other tracked file types the list: a new site must be declared here
  (or be generated from one that is) so it cannot drift unseen.
"""

import itertools
import re
import subprocess

import pytest

from doppler.tests._repo import repo_root
from doppler.wfm import Composer, Segment, cli
from doppler.wlan import Wlan

ROOT = repo_root(__file__)

_PAIRS = itertools.product(
    ("bpsk", "qpsk", "qam16", "qam64"), ("r12", "r23", "r34", "r56")
)
#: The rates the table lists at 20 MHz, from the table.
RATES = sorted(
    {
        m
        for c, r in _PAIRS
        if (m := Wlan(constellation=c, code_rate=r).rate_mbps)
    }
)
COMMA = ", ".join(map(str, RATES))  # 6, 9, ..., 54
OR = ", ".join(map(str, RATES[:-1])) + f" or {RATES[-1]}"  # ..., 48 or 54

#: file -> the spelling it uses. Hand-owned sources only: what a generator
#: copies from one of these is held by that generator's own drift gate.
SITES = {
    "modules/wfm_compose.toml": COMMA,
    "native/inc/doppler/wfm/wfm_compose.h": COMMA,
    "native/src/wlan/wlan_core.c": OR,
    "native/inc/doppler/wlan/wlan_core.h": OR,
    "objects/wlan.toml": OR,
    "docs/design/wlan.md": COMMA,
    "docs/guide/wlan.md": OR,
    "docs/guide/wfmgen/waveforms.md": OR,
}
#: Files that may say the list without being a declared site: what a
#: generator writes from a site, and the tests that hold the table itself.
COPIES = (
    "docs/c-api/",
    "docs/guide/wfmgen/options.md",
    "docs/schema/wfmgen.schema.json",
    "native/inc/doppler/wfm/wfm_surface.h",
    "native/src/wfm_compose/wfm_compose_ext.c",
    "src/doppler/wfm/wfm_compose.pyi",
    "src/doppler/wlan/wlan.pyi",
    "native/src/wlan/wlan_ext_wlan.c",
    "native/tests/",
    "src/doppler/wlan/tests/",
)


def _flat(text: str) -> str:
    """The text as one line: C string-literal joins, comment stars and
    wrapping removed, so a list wrapped across lines still matches."""
    text = re.sub(r'"\s*\n\s*"', "", text)
    text = re.sub(r"\n\s*(\*|\"|#)?\s*", " ", text)
    return re.sub(r"\s+", " ", text)


@pytest.mark.parametrize("site", sorted(SITES))
def test_a_hand_owned_site_names_the_tables_rates(site):
    text = _flat((ROOT / site).read_text(encoding="utf-8"))
    if SITES[site] not in text:
        pytest.fail(f"{site} does not say the table's rates: {SITES[site]}")


def test_wfmgen_help_names_the_tables_rates():
    p = subprocess.run(
        [cli._runnable(), "--help"],
        capture_output=True,
        text=True,
        timeout=60,
    )
    assert p.returncode == 0, p.stderr
    assert COMMA in _flat(p.stdout)


def test_the_refusal_of_a_rate_the_table_lacks_names_the_rates():
    missing = max(RATES) + 1
    with pytest.raises(ValueError) as e:
        Composer(
            [Segment(type="ofdm", fs=20e6, wlan=missing, data_len=10)]
        ).compose()
    assert OR in _flat(str(e.value))


def test_no_other_tracked_file_types_the_list():
    """The whole list, in either spelling, found anywhere else is a new
    copy of it: declare the file in SITES or generate it from one."""
    pattern = re.compile(
        "|".join(
            r"(?<![\d.])" + re.escape(x) + r"(?![\d.])" for x in (COMMA, OR)
        )
    )
    tracked = subprocess.run(
        ["git", "ls-files"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=True,
    ).stdout.split()
    found = []
    for name in tracked:
        if name in SITES or name.startswith(COPIES):
            continue
        try:
            text = (ROOT / name).read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        if pattern.search(_flat(text)):
            found.append(name)
    assert not found, (
        f"{found} spell the whole list of Table 17-6 rates ({COMMA}). If "
        "the sentence is about these rates, add the file to SITES (or "
        "generate it from a site); if it is an unrelated list that happens "
        "to equal them, add it to COPIES with its reason."
    )
