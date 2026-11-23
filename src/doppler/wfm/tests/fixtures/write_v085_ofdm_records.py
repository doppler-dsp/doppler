"""Write the ofdm scene records that v0.85.0's own writer produced.

This script is the provenance of ``v085_ofdm_records.json``. It is NOT
run by the suite and must NOT be run against a newer build: the fixture
exists to hold what a v0.85.0 user's records say, and regenerating it
from the code under test would make the test compare that code with
itself.

How the committed file was produced (2026-11-23)::

    git worktree add --detach <dir> v0.85.0
    cd <dir> && make pyext
    <dir>/.venv/bin/python <this file> > v085_ofdm_records.json

``Composer.to_json`` calls the same C scene writer as ``wfmgen``'s
record, so these are the records either face wrote at v0.85.0. Each case
is a source as a v0.85.0 user spelled it; what the record holds is
whatever that writer kept. At v0.85.0 the writer omits
``constellation``, ``code_rate`` and ``mac`` at their defaults (bpsk,
1/2, none), which is why a rate given by one field writes one key, and
why a default caller and an explicit bpsk 1/2 write the same record.
"""

from __future__ import annotations

import json

import doppler
from doppler.wfm import Composer, Segment, field_bits

# One 100-octet payload, as the guide's examples spell it.
DATA = field_bits("pn:800:15")
BASE = {"type": "ofdm", "fs": 20e6, "data": DATA, "data_len": 800}

# name -> the keywords a v0.85.0 user gave, beyond BASE
CASES = {
    "rate6_default": {},
    "rate6_explicit": {
        "constellation": "bpsk",
        "code_rate": "1/2",
        "mac": "none",
    },
    "rate9": {"constellation": "bpsk", "code_rate": "3/4"},
    "rate12": {"constellation": "qpsk", "code_rate": "1/2"},
    "rate24": {"constellation": "qam16", "code_rate": "1/2"},
    "rate18": {"constellation": "qpsk", "code_rate": "3/4"},
    "rate36_mac_none": {"constellation": "qam16", "code_rate": "3/4"},
    "rate36_mac_fcs": {
        "constellation": "qam16",
        "code_rate": "3/4",
        "mac": "fcs",
    },
    "rate48": {"constellation": "qam64", "code_rate": "2/3", "mac": "fcs"},
    "rate54": {"constellation": "qam64", "code_rate": "3/4", "mac": "fcs"},
    "wlan36_mac_fcs": {"wlan": 36, "mac": "fcs"},
}


def main() -> None:
    """Print the records, keyed by case, with the writer's version."""
    records = {
        name: json.loads(Composer([Segment(**BASE, **kw)]).to_json())
        for name, kw in CASES.items()
    }
    out = {"written_by": doppler.__version__, "records": records}
    print(json.dumps(out, indent=1, sort_keys=True))


if __name__ == "__main__":
    main()
