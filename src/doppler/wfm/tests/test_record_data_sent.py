"""A record carries what each data source SENT, and replays it by hash.

``docs/design/payload-data-source.md`` §4.8: ``to_json()`` (the record) and
``to_sigmf()`` carry the truth for scoring -- the frames sent, the fill
bits padding the last, the idle frames -- with the source bits read and,
for a file, its ``dp_hash64``. A replay identifies a file by that content,
not by its name: one that changed is refused, naming both hashes. The C
tests pin the mechanism (``test_wfm_compose.c``,
``test_a_record_replays_its_data_by_hash``); these pin the Python face of
it, which is ``Composer.to_json`` / ``to_sigmf`` / ``from_json``.
"""

from __future__ import annotations

import json
import re
from typing import TYPE_CHECKING

import numpy as np
import pytest

from doppler.wfm import Composer, Segment

if TYPE_CHECKING:
    from pathlib import Path

#: 40 bits in 16-bit frames: three frames, the last padded with 8 fill bits.
BITS = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 5, np.uint8)


def _field_composer() -> Composer:
    return Composer(
        [
            Segment(
                type="bits",
                modulation="none",
                sps=1,
                snr=200.0,
                data=BITS,
                data_len=16,
                fill=np.array([0], np.uint8),
            )
        ]
    )


def _source(record: str) -> dict:
    return json.loads(record)["segments"][0]


def test_a_record_written_before_the_run_is_the_scene_alone() -> None:
    assert "data_sent" not in _source(_field_composer().to_json())


def test_the_record_and_sigmf_carry_what_the_data_sent() -> None:
    c = _field_composer()
    assert c.compose().size == 48
    sent = _source(c.to_json())["data_sent"]
    assert sent == {"bits": 40, "frames": 3, "pad_bits": 8, "idle_frames": 0}
    ann = json.loads(c.to_sigmf(fs=1e6))["annotations"][0]
    assert ann["wfmgen:frames"] == 3
    assert ann["wfmgen:pad_bits"] == 8
    assert ann["wfmgen:idle_frames"] == 0
    assert ann["wfmgen:data_bits"] == 40
    assert "wfmgen:data_hash" not in ann, "a Field reads no octets"


def _file_scene(path: Path) -> str:
    return json.dumps(
        {
            "segments": [
                {
                    "type": "bits",
                    "modulation": "none",
                    "sps": 1,
                    "snr": 200.0,
                    "data_len": 16,
                    "data_from_file": str(path),
                }
            ]
        }
    )


def _recorded(path: Path) -> tuple[str, np.ndarray]:
    path.write_bytes(b"F7F7")
    c = Composer.from_json(_file_scene(path))
    x = c.compose()
    return c.to_json(), x


def test_a_file_replays_by_its_hash(tmp_path: Path) -> None:
    rec, x = _recorded(tmp_path / "f7.bin")
    sent = _source(rec)["data_sent"]
    assert sent["bits"] == 32 and sent["frames"] == 2
    assert re.fullmatch(r"0x[0-9a-f]{16}", sent["hash"])
    assert np.array_equal(Composer.from_json(rec).compose(), x)


def test_a_changed_file_is_refused_naming_both_hashes(tmp_path: Path) -> None:
    data = tmp_path / "f7.bin"
    rec, _ = _recorded(data)
    want = _source(rec)["data_sent"]["hash"]
    data.write_bytes(b"F7F8")  # the same length: a length check would pass
    with pytest.raises(ValueError, match=want) as e:
        Composer.from_json(rec)
    assert re.search(
        r"the file's is 0x[0-9a-f]{16} over 32 bits", str(e.value)
    )
    assert str(data) in str(e.value)


def test_a_record_of_stdin_needs_its_file_given_again(tmp_path: Path) -> None:
    rec = json.loads(_recorded(tmp_path / "f7.bin")[0])
    rec["segments"][0]["data_from_file"] = "-"
    with pytest.raises(ValueError, match="--data-from-file"):
        Composer.from_json(json.dumps(rec))
