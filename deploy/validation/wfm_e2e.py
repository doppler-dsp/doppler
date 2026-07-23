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
     representative sample types -> `Reader` round-trip, asserting
     sample count, dtype, and a correlation check;
  5. `to_json` -> `from_json` parity; `to_sigmf` JSON parses with the expected
     keys.

Checks are chosen to pass on the shipped 0.17.0 — they deliberately steer
around the three known bugs the repo's exhaustive suite pins as xfails
(PN default polynomial, `wfmgen --output -`, and the `ZmqSink`/`StreamSink`
cf32 stream decode), so this is a clean PASS/FAIL signal for the artifact
itself. Features
that landed *after* 0.17.0 (the `Synth(bits=...)` kwarg, `Composer.to_sigmf`)
are **feature-detected** — exercised when the wheel has them, gracefully
degraded (SigMF via the `wfmgen` CLI) otherwise — so this same script validates
the current release and the next one. Prints a summary table and exits non-zero
on any failure.

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
    "StreamSink",
    "SampleClock",
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
                "--sample-type",
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
    # The six core types are present in every release.
    for t in ("tone", "noise", "pn", "bpsk", "qpsk", "chirp"):
        x = np.asarray(w.Synth(type=t, snr=100.0).steps(256))
        assert x.shape == (256,) and x.dtype == np.complex64
        assert np.all(np.isfinite(x.view(np.float32)))
    # The `bits` waveform + its `Synth(bits=...)` kwarg landed after 0.17.0;
    # exercise it only when the installed wheel supports it.
    try:
        xb = np.asarray(
            w.Synth(type="bits", bits=bytes([1, 0, 1, 1]), snr=100.0).steps(
                256
            )
        )
        assert xb.shape == (256,)
        return "7 types, finite cf32"
    except TypeError:
        return "6 core types (bits kwarg not in this wheel)"


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


def _write_sigmf(
    base: Path, sample_type: str, fs: float, freq: float, n: int
) -> None:
    """Lay down a SigMF pair (`.sigmf-data` + `.sigmf-meta`) using whichever
    sidecar writer the installed wheel provides: ``Composer.to_sigmf`` if
    present (post-0.17.0), else the shipped ``wfmgen --file-type sigmf``."""
    if hasattr(w.Composer, "to_sigmf"):
        x = np.asarray(
            w.Synth(type="tone", fs=fs, freq=freq, snr=100.0).steps(n)
        )
        with w.Writer(
            str(base) + ".sigmf-data",
            file_type="sigmf",
            sample_type=sample_type,
            fs=fs,
            total=n,
        ) as wr:
            wr.write(x)
        Path(str(base) + ".sigmf-meta").write_text(
            w.Composer([w.Segment("tone", freq=freq, num_samples=n)]).to_sigmf(
                sample_type=sample_type, fs=fs
            )
        )
    else:
        subprocess.run(
            [
                "wfmgen",
                "--type",
                "tone",
                "--freq",
                str(freq),
                "--fs",
                str(fs),
                "--count",
                str(n),
                "--snr",
                "100",
                "--sample-type",
                sample_type,
                "--file-type",
                "sigmf",
                "--output",
                str(base),
            ],
            check=True,
            capture_output=True,
        )


def _roundtrip(file_type: str, sample_type: str) -> None:
    """Write a tone capture in one container/dtype and read it back."""
    n = 512
    with tempfile.TemporaryDirectory() as d:
        if file_type == "sigmf":
            base = Path(d) / "cap"
            _write_sigmf(base, sample_type, 1e6, 5e4, n)
            reader = w.Reader(str(base) + ".sigmf-data")
            y = np.asarray(reader.read(n))
            reader.close()
            # CLI- or to_sigmf-generated: validate the round-trip is sane
            # (count, finite, non-trivial power) rather than bit-exact.
            assert len(y) == n, f"sigmf/{sample_type}: {len(y)} samples"
            assert np.all(np.isfinite(y.view(np.float32)))
            assert float(np.mean(np.abs(y) ** 2)) > 1e-6
            return
        x = np.asarray(
            w.Synth(type="tone", fs=1e6, freq=5e4, snr=100.0).steps(n)
        )
        path = str(Path(d) / f"cap.{file_type}")
        with w.Writer(
            path, file_type=file_type, sample_type=sample_type, fs=1e6, total=n
        ) as wr:
            wr.write(x)
        # raw/csv are headerless -> supply the dtype; blue self-describes.
        reader = (
            w.Reader(path)
            if file_type == "blue"
            else w.Reader(path, sample_type=sample_type)
        )
        y = np.asarray(reader.read(n))
        reader.close()
        assert len(y) == n, f"{file_type}/{sample_type}: {len(y)} samples"
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


@check("Reader across sample types")
def _reader_dtypes() -> str:
    x = np.asarray(w.Synth(type="qpsk", sps=2, snr=100.0).steps(1024))
    for st in ("cf32", "cf64", "ci8", "ci16", "ci32"):
        with tempfile.TemporaryDirectory() as d:
            p = str(Path(d) / f"c.{st}")
            with w.Writer(
                p, file_type="raw", sample_type=st, total=len(x)
            ) as wr:
                wr.write(x)
            with w.Reader(p, sample_type=st) as r:
                y = r.read(1024)
        assert len(y) == 1024, f"{st}: {len(y)}"
    return "5 dtypes"


@check("JSON spec round-trip parity")
def _json_parity() -> str:
    c = w.Composer([w.Segment("qpsk", sps=4, num_samples=256, seed=7)])
    c2 = w.Composer.from_json(c.to_json())
    assert np.array_equal(c.compose(), c2.compose())
    return "compose() identical"


@check("SigMF sidecar schema")
def _sigmf() -> str:
    # Validate the .sigmf-meta the shipped wheel emits — via Composer.to_sigmf
    # if present, else the wfmgen CLI (see _write_sigmf).
    with tempfile.TemporaryDirectory() as d:
        base = Path(d) / "cap"
        _write_sigmf(base, "ci16", 1e6, 1e5, 128)
        meta = json.loads(Path(str(base) + ".sigmf-meta").read_text())
    assert meta["global"]["core:datatype"] == "ci16_le"
    assert meta["global"]["core:sample_rate"] == 1_000_000
    assert meta["annotations"][0]["core:label"] == "tone"
    via = "to_sigmf" if hasattr(w.Composer, "to_sigmf") else "wfmgen CLI"
    return f"global + annotations present (via {via})"


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
