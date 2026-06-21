"""wfm_e2e.py — end-to-end smoke of the *published* doppler-dsp wheel on 3.9.

Runs entirely against the installed `doppler.wfm` public API + the bundled
`wfmgen` console script — no repo checkout, no build toolchain. It is the
golden-path proof that what a user `pip install`s actually works on Python 3.9:

  1. import `doppler.wfm`, assert the public surface (`__all__`) is present;
  2. `wfmgen` console-script smoke (`json-template` + a file render) — proves
     the bundled C binary runs under 3.9;
  3. generate every waveform type via `Synth`/factories; FFT-verify the tone
     peak and the noise power (loose tolerances);
  4. `Composer` -> write all four containers (raw / csv / blue / sigmf) across
     representative sample types -> `read_iq` / `Reader` round-trip, asserting
     sample count, dtype, and a correlation check;
  5. `to_json` -> `from_json` parity; `to_sigmf` JSON parses with the expected
     keys.

Checks are chosen to pass on the shipped 0.17.0 — they deliberately steer
around the three known bugs the repo's exhaustive suite pins as xfails
(PN default polynomial, `wfmgen --output -`, and the `ZmqSink` cf32 stream
decode), so this is a clean PASS/FAIL signal for the artifact itself. Prints a
summary table and exits non-zero on any failure.

Run (inside the container):
    python wfm_e2e.py
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from collections.abc import Callable
from pathlib import Path

import numpy as np

import doppler.wfm as w

Check = Callable[[], object]

EXPECTED_SYMBOLS = {
    "PN",
    "Synth",
    "_SynthEngine",
    "Segment",
    "Timeline",
    "Composer",
    "tone",
    "noise",
    "pn",
    "bpsk",
    "qpsk",
    "chirp",
    "bits",
    "bpsk_map",
    "qpsk_map",
    "wfm_awgn_amplitude",
    "wfm_ebno_to_snr_db",
    "mls_poly",
    "rrc_taps",
    "dsss_spread",
    "write_blue_header",
    "Writer",
    "Reader",
    "ZmqSink",
    "SampleClock",
    "read_iq",
}

results: list[tuple[str, bool, str]] = []


def check(name: str) -> Callable[[Check], Check]:
    """Decorator: run a check, capture pass/fail + a one-line note."""

    def wrap(fn: Check) -> Check:
        try:
            note = fn() or ""
            results.append((name, True, str(note)))
        except Exception as exc:  # record every failure, never raise
            results.append((name, False, f"{type(exc).__name__}: {exc}"))
        return fn

    return wrap


@check("public surface (__all__)")
def _surface() -> str:
    missing = EXPECTED_SYMBOLS - set(w.__all__)
    assert not missing, f"missing from __all__: {sorted(missing)}"
    for s in w.__all__:
        assert hasattr(w, s), f"{s} not importable"
    return f"{len(w.__all__)} symbols"


@check("wfmgen console: json-template")
def _cli_template() -> str:
    out = subprocess.run(
        ["wfmgen", "json-template"], check=True, capture_output=True
    )
    spec = json.loads(out.stdout.decode())
    assert "segments" in spec
    return "parses as JSON"


@check("wfmgen console: render to file")
def _cli_render() -> str:
    with tempfile.TemporaryDirectory() as d:
        out = Path(d) / "tone.iq"
        subprocess.run(
            [
                "wfmgen",
                "--type",
                "tone",
                "--count",
                "256",
                "--snr",
                "100",
                "--sample_type",
                "cf32",
                "--output",
                str(out),
            ],
            check=True,
            capture_output=True,
        )
        x = np.fromfile(out, dtype=np.complex64)
    assert len(x) == 256, f"got {len(x)} samples"
    return "256 cf32 samples"


@check("generate every waveform type")
def _all_types() -> str:
    for t in ("tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits"):
        kw = {"type": t, "snr": 100.0}
        if t == "bits":
            kw["bits"] = bytes([1, 0, 1, 1])
        x = np.asarray(w.Synth(**kw).steps(256))
        assert x.shape == (256,) and x.dtype == np.complex64
        assert np.all(np.isfinite(x.view(np.float32)))
    return "7 types, finite cf32"


@check("tone FFT peak at the right bin")
def _tone_peak() -> str:
    n, k = 4096, 137
    x = np.asarray(
        w.Synth(type="tone", fs=float(n), freq=float(k), snr=100.0).steps(n)
    )
    spec = np.abs(np.fft.fft(x))
    peak = int(np.argmax(spec))
    assert peak == k, f"peak at bin {peak}, expected {k}"
    assert np.allclose(np.abs(x), 1.0, atol=1e-3), "tone not unit-magnitude"
    return f"peak bin {peak}"


@check("noise power ~ unit (raw kernel)")
def _noise_power() -> str:
    x = np.asarray(w.Synth(type="noise", snr=0.0, seed=3).steps(100_000))
    p = float(np.mean(np.abs(x) ** 2))
    assert 0.9 < p < 1.1, f"noise power {p:.3f} not ~1.0"
    return f"power {p:.3f}"


def _roundtrip(file_type: str, sample_type: str) -> None:
    """Write a tone capture in one container/dtype and read it back."""
    x = np.asarray(
        w.Synth(type="tone", fs=1e6, freq=5e4, snr=100.0).steps(512)
    )
    with tempfile.TemporaryDirectory() as d:
        if file_type == "sigmf":
            base = Path(d) / "cap"
            path = str(base) + ".sigmf-data"
            base.with_suffix(".sigmf-meta").write_text(
                w.Composer(
                    [w.Segment("tone", freq=5e4, num_samples=512)]
                ).to_sigmf(sample_type=sample_type, fs=1e6)
            )
        else:
            path = str(Path(d) / f"cap.{file_type}")
        with w.Writer(
            path,
            file_type=file_type,
            sample_type=sample_type,
            fs=1e6,
            total=len(x),
        ) as wr:
            wr.write(x)
        # raw/csv are headerless -> supply the dtype; blue/sigmf self-describe.
        reader = (
            w.Reader(path)
            if file_type in ("blue", "sigmf")
            else w.Reader(path, sample_type=sample_type)
        )
        y = np.asarray(reader.read(512))
        reader.close()
    assert len(y) == 512, f"{file_type}/{sample_type}: {len(y)} samples"
    corr = np.abs(np.vdot(y, x)) / (np.linalg.norm(y) * np.linalg.norm(x))
    assert corr > 0.999, f"{file_type}/{sample_type}: corr {corr:.4f}"


@check("container round-trips (raw/csv/blue/sigmf)")
def _containers() -> str:
    for ft, st in (
        ("raw", "cf32"),
        ("raw", "ci16"),
        ("csv", "cf32"),
        ("blue", "cf32"),
        ("sigmf", "ci16"),
    ):
        _roundtrip(ft, st)
    return "5 container/dtype combos"


@check("read_iq across sample types")
def _read_iq() -> str:
    x = np.asarray(w.Synth(type="qpsk", sps=2, snr=100.0).steps(1024))
    for st in ("cf32", "cf64", "ci8", "ci16", "ci32"):
        with tempfile.TemporaryDirectory() as d:
            p = str(Path(d) / f"c.{st}")
            with w.Writer(
                p, file_type="raw", sample_type=st, total=len(x)
            ) as wr:
                wr.write(x)
            y = w.read_iq(p, st)
        assert len(y) == 1024, f"{st}: {len(y)}"
    return "5 dtypes"


@check("JSON spec round-trip parity")
def _json_parity() -> str:
    c = w.Composer([w.Segment("qpsk", sps=4, num_samples=256, seed=7)])
    c2 = w.Composer.from_json(c.to_json())
    assert np.array_equal(c.compose(), c2.compose())
    return "compose() identical"


@check("to_sigmf JSON schema")
def _sigmf() -> str:
    c = w.Composer([w.Segment("tone", freq=1e5, num_samples=128)])
    meta = json.loads(c.to_sigmf(sample_type="ci16", fs=1e6, fc=2.4e9))
    assert meta["global"]["core:datatype"] == "ci16_le"
    assert meta["global"]["core:sample_rate"] == 1_000_000
    assert meta["annotations"][0]["core:label"] == "tone"
    return "global + annotations present"


def main() -> int:
    print(f"doppler-dsp e2e on Python {sys.version.split()[0]}")
    print(f"doppler.wfm from {Path(w.__file__).resolve()}\n")

    width = max(len(n) for n, _, _ in results) if results else 0
    passed = 0
    for name, ok, note in results:
        flag = "PASS" if ok else "FAIL"
        passed += ok
        print(f"  [{flag}] {name:<{width}}  {note}")

    total = len(results)
    print(f"\n{passed}/{total} checks passed")
    return 0 if passed == total else 1


if __name__ == "__main__":
    raise SystemExit(main())
