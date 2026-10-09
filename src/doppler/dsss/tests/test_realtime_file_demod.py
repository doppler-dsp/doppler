"""Integration test for the continuous wfmgen -> follow -> demod pipeline.

The wfmgen CLI streams scene.json (--continuous) to a growing file; the reader
follows it and decodes each burst (CRC + bits) as its samples land, alongside
the writer. The scene uses wfmgen's ranged-field interface, so each repeat
draws a fresh Doppler (``freq: [lo, hi]``) and a fresh arrival jitter (trailing
``off_samples: [lo, hi]`` → varying code phase) on top of a fresh noise
realization (``seed_advance="noise"``); the code/payload are fixed, so every
burst still decodes to the same frame. Run non-paced (realtime=False) so the
writer races and the reader drains it fast. Skips when the CLI isn't built.
"""

import json

import pytest

from doppler.examples import dsss_realtime_file_demod as demo

pytestmark = [
    pytest.mark.skipif(
        demo.wfmgen_available() is None,
        reason="wfmgen CLI not built / on PATH",
    ),
]


def test_tailing_pipeline_decodes_every_burst():
    demo.check(demo.run_streaming(4, realtime=False), 4)


def test_a_refused_scene_raises_with_the_writers_reason(tmp_path):
    """#1797: a writer that refuses its scene used to leave an empty results
    list, and the example exited 0 having demonstrated nothing."""
    scene = demo.write_scene(tmp_path / "scene.json")
    spec = json.loads(scene.read_text(encoding="utf-8"))
    spec["segments"][0]["sync"] = demo._bitstr(demo.SYNC)  # retired flat key
    scene.write_text(json.dumps(spec), encoding="utf-8")

    with pytest.raises(RuntimeError, match=r'status 2 .*"sync" is retired'):
        demo.run_streaming(2, realtime=False, scene_path=scene)


def test_check_refuses_a_short_run():
    """The example's own exit path: fewer bursts than asked for fails."""
    with pytest.raises(AssertionError, match="0 of 6 bursts arrived"):
        demo.check([], 6)


def test_ranged_fields_vary_burst_to_burst():
    """The whole point of the ranged scene: Doppler and code phase shift from
    burst to burst (a fixed scene would repeat them identically)."""
    results = demo.run_streaming(5, realtime=False)
    dopplers = [r["est_freq_hz"] for r in results]
    phases = [r["code_phase"] for r in results]
    # At least three distinct Doppler draws and three distinct code phases out
    # of five bursts (uniform draws practically never collide this much).
    assert len({round(f) for f in dopplers}) >= 3
    assert len(set(phases)) >= 3
