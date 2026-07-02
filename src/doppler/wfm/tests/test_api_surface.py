"""Exhaustive API-surface coverage for ``doppler.wfm`` / ``wfmgen``.

Where :mod:`test_dsp_correctness` validates the *math*, this module validates
the *surface*: every public name in ``doppler.wfm.__all__`` is importable and
exercised, every handle class round-trips its lifecycle (construct → use → read
properties → context-manager → ``close``/``destroy``), the ``Segment`` /
``Timeline`` / ``Composer`` graph composes and serialises, the live ``ZmqSink``
+ ``SampleClock`` path actually moves samples, and the ``wfmgen`` CLI honours
its full argument map with **face parity** (CLI bytes == Python ``Composer``
bytes == ``--from-file`` JSON replay).

A **completeness gate** (:class:`TestSurfaceCompleteness`) fails loudly if a
new public symbol is added to ``__all__`` without a matching entry in the
coverage registry below — so the surface can't silently grow past its tests.

Bugs uncovered here are ``xfail(strict=True)`` with pointers into
``docs/dev/wfm-validation-findings.md``.
"""

from __future__ import annotations

import json
import shutil
import subprocess

import numpy as np
import pytest

import doppler.wfm as w

# --------------------------------------------------------------------------- #
# Coverage registry — every name in __all__ must appear here, mapped to the
# test (class or function) that exercises it. The completeness gate enforces
# it.
# --------------------------------------------------------------------------- #
COVERAGE: dict[str, str] = {
    # generators / engine
    "PN": "TestPNLifecycle",
    "_SynthEngine": "TestSynthEngineLifecycle",
    "Synth": "TestSynthLifecycle",
    "Segment": "TestComposerGraph",
    "Timeline": "TestComposerGraph",
    "Composer": "TestComposerGraph",
    # factories
    "tone": "TestFactories",
    "noise": "TestFactories",
    "pn": "TestFactories",
    "bpsk": "TestFactories",
    "qpsk": "TestFactories",
    "chirp": "TestFactories",
    "bits": "TestFactories",
    # module functions
    "bpsk_map": "TestModuleFunctions",
    "qpsk_map": "TestModuleFunctions",
    "wfm_awgn_amplitude": "TestModuleFunctions",
    "wfm_ebno_to_snr_db": "TestModuleFunctions",
    "mls_poly": "TestModuleFunctions",
    "rrc_taps": "TestModuleFunctions",
    "dsss_spread": "TestModuleFunctions",
    "write_blue_header": "TestModuleFunctions",
    # stimulus engine (component cache) — dedicated suite in test_plan.py
    "Plan": "test_plan.py",
    "prepare": "test_plan.py",
    # transport / IO handles
    "Writer": "TestReaderWriter",
    "Reader": "TestReaderWriter",
    "ZmqSink": "TestZmqSinkAndClock",
    "SampleClock": "TestZmqSinkAndClock",
    "read_iq": "TestReaderWriter",
}

ENUMS = {
    "type": ["tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits"],
    "snr_mode": ["auto", "fs", "ebno", "esno"],
    "lfsr": ["galois", "fibonacci"],
    "modulation": ["none", "bpsk", "qpsk"],
    "pulse": ["rect", "rrc"],
    "sample_type": ["cf32", "cf64", "ci32", "ci16", "ci8"],
    "file_type": ["raw", "csv", "blue", "sigmf"],
    "endian": ["le", "be"],
}


def _find_wfmgen() -> str | None:
    """Locate the ``wfmgen`` C binary. Prefer the one the package bundles
    (``doppler/wfm/_bin/wfmgen``, resolved + made executable by the console
    shim) so the CLI face-parity tests run from any install — venv, wheel, or
    another machine — not just when ``wfmgen`` happens to be on ``PATH``."""
    try:
        from doppler.wfm.cli import _runnable

        return _runnable()
    except (ImportError, FileNotFoundError, OSError):
        return shutil.which("wfmgen")


WFMGEN = _find_wfmgen()
needs_cli = pytest.mark.skipif(WFMGEN is None, reason="wfmgen not found")


# --------------------------------------------------------------------------- #
class TestSurfaceCompleteness:
    def test_all_symbols_importable(self) -> None:
        for name in w.__all__:
            assert hasattr(w, name), f"{name} in __all__ but not importable"

    def test_every_symbol_has_coverage(self) -> None:
        missing = sorted(set(w.__all__) - set(COVERAGE))
        assert not missing, (
            f"public symbols added without a coverage-registry entry: "
            f"{missing}"
        )

    def test_no_stale_registry_entries(self) -> None:
        stale = sorted(set(COVERAGE) - set(w.__all__))
        assert not stale, (
            f"coverage registry references dropped symbols: {stale}"
        )


# --------------------------------------------------------------------------- #
class TestPNLifecycle:
    def test_construct_generate_reset_destroy(self) -> None:
        p = w.PN(poly=w.mls_poly(7), seed=1, length=7)
        a = p.generate(16).copy()
        p.reset()
        b = p.generate(16).copy()
        assert np.array_equal(a, b)
        p.destroy()

    @pytest.mark.parametrize("lfsr", ENUMS["lfsr"])
    def test_both_lfsr_forms(self, lfsr: str) -> None:
        chips = w.PN(poly=w.mls_poly(7), seed=1, length=7, lfsr=lfsr).generate(
            127
        )
        assert set(np.unique(chips)).issubset({0, 1})

    def test_context_manager(self) -> None:
        with w.PN(poly=w.mls_poly(5), seed=1, length=5) as p:
            assert p.generate(31).shape == (31,)


class TestSynthEngineLifecycle:
    def test_getters_setters(self) -> None:
        e = w._SynthEngine(type="tone", fs=1e6, freq=0.0, snr=100.0)
        assert e.get_wtype() == 0
        assert e.get_nsps() == 8
        e.reset()
        assert isinstance(e.step(), complex)
        assert e.steps(8).dtype == np.complex64
        e.destroy()

    def test_context_manager(self) -> None:
        with w._SynthEngine(type="qpsk", sps=4, snr=100.0) as e:
            assert e.steps(16).shape == (16,)


class TestSynthLifecycle:
    @pytest.mark.parametrize("wtype", ENUMS["type"])
    def test_every_type_generates(self, wtype: str) -> None:
        kw: dict = {"type": wtype, "snr": 100.0}
        if wtype == "bits":
            kw["bits"] = bytes([1, 0, 1, 1])
        x = w.Synth(**kw).steps(64)
        assert x.shape == (64,)
        assert x.dtype == np.complex64
        assert np.all(np.isfinite(x.view(np.float32)))

    @pytest.mark.parametrize("mode", ENUMS["snr_mode"])
    def test_every_snr_mode(self, mode: str) -> None:
        x = w.Synth(type="bpsk", snr=10.0, snr_mode=mode, sps=4).steps(256)
        assert np.all(np.isfinite(x.view(np.float32)))

    @pytest.mark.parametrize("pulse", ENUMS["pulse"])
    def test_pulse_shapes(self, pulse: str) -> None:
        x = w.Synth(
            type="bpsk",
            pulse=pulse,
            rrc_beta=0.35,
            rrc_span=8,
            sps=4,
            snr=100.0,
        ).steps(256)
        assert x.shape == (256,)

    def test_step_and_reset(self) -> None:
        s = w.Synth(type="tone", freq=1e5, snr=100.0)
        assert isinstance(s.step(), complex)
        a = s.steps(32).copy()
        s.reset()
        # reset rewinds to post-create; step() above advanced one sample, so
        # compare a fresh instance instead.
        t = w.Synth(type="tone", freq=1e5, snr=100.0)
        assert np.array_equal(t.steps(33)[1:], a)


class TestFactories:
    def test_all_factories_return_synth(self) -> None:
        facs = {
            "tone": w.tone(freq=1e5),
            "noise": w.noise(),
            "pn": w.pn(pn_length=7),
            "bpsk": w.bpsk(sps=4),
            "qpsk": w.qpsk(sps=4),
            "chirp": w.chirp(freq=0.0, f_end=1e5),
            "bits": w.bits(bits=bytes([1, 0, 1])),
        }
        for name, syn in facs.items():
            assert type(syn).__name__ == "Synth", name
            assert syn.steps(16).shape == (16,)


class TestModuleFunctions:
    def test_bpsk_map(self) -> None:
        out = np.asarray(w.bpsk_map(np.array([0, 1, 0, 1], np.uint8)))
        assert np.allclose(out.real, [1, -1, 1, -1])

    def test_qpsk_map(self) -> None:
        out = np.asarray(w.qpsk_map(np.array([0, 1, 2, 3], np.uint8)))
        assert np.allclose(np.abs(out), 1.0, atol=1e-4)
        assert len(np.unique(np.round(out, 4))) == 4

    def test_awgn_amplitude(self) -> None:
        assert w.wfm_awgn_amplitude(0.0, 1.0) == pytest.approx(
            np.sqrt(0.5), rel=1e-5
        )

    def test_ebno_to_snr(self) -> None:
        assert w.wfm_ebno_to_snr_db(10.0, 1, 8.0) == pytest.approx(
            10.0 - 10.0 * np.log10(8.0), abs=1e-4
        )

    @pytest.mark.parametrize("n", [3, 5, 7, 9, 15])
    def test_mls_poly_gives_maximal_sequence(self, n: int) -> None:
        chips = w.PN(poly=w.mls_poly(n), seed=1, length=n).generate(
            (1 << n) - 1
        )
        assert int(chips.sum()) == (1 << (n - 1))

    def test_rrc_taps(self) -> None:
        h = w.rrc_taps(0.35, 8, 8)
        assert len(h) == 2 * 8 * 8 + 1

    def test_dsss_spread(self) -> None:
        syms = np.array([1, -1, 1], np.complex64)
        code = np.array([1, 0, 1], np.uint8)
        chips = np.asarray(w.dsss_spread(syms, code, 3))
        assert len(chips) == 9

    def test_write_blue_header(self, tmp_path) -> None:
        p = tmp_path / "cap.hdr"
        w.write_blue_header(str(p), sample_type="cf32", fs=1e6, total=512)
        head = p.read_bytes()
        assert head[:4] == b"BLUE"
        assert len(head) == 512


class TestComposerGraph:
    def test_segment_flat_view_and_sum(self) -> None:
        seg = w.Segment("tone", freq=1e5, num_samples=128, off_samples=16)
        assert seg.type == "tone"
        assert seg.num_samples == 128
        assert seg.off_samples == 16
        multi = w.Segment.sum(
            w.tone(freq=1e5), w.tone(freq=-1e5), num_samples=128
        )
        assert len(multi.sources) == 2

    def test_segment_add_makes_timeline(self) -> None:
        tl = w.Segment("pn", num_samples=64, pn_length=7).add(
            w.Segment("tone", freq=1e5, num_samples=64)
        )
        assert type(tl).__name__ == "Timeline"
        assert len(tl) == 2
        assert type(tl[0]).__name__ == "Segment"
        assert len(list(tl)) == 2  # __iter__

    def test_timeline_add(self) -> None:
        tl = w.Timeline([w.Segment("tone", freq=1e5, num_samples=32)])
        tl2 = tl.add(w.Segment("noise", num_samples=32))
        assert len(tl2) == 2

    def test_compose_execute_lengths(self) -> None:
        spec = [
            w.Segment("pn", num_samples=127, pn_length=7),
            w.Segment("tone", freq=1e5, num_samples=256, off_samples=64),
        ]
        x = w.Composer(spec).compose()
        assert len(x) == 127 + 256 + 64
        # execute(n) is the low-level block API.
        y = w.Composer(spec).execute(100)
        assert len(y) == 100

    def test_stream_blocks(self) -> None:
        c = w.Composer([w.Segment("tone", freq=1e5, num_samples=256)])
        blocks = list(c.stream(block=64))
        assert sum(len(b) for b in blocks) == 256

    def test_to_dict_and_json_roundtrip(self) -> None:
        # repeat/continuous are timeline flags with no bounded length, so
        # compose() (which materialises the whole stream) must not be called on
        # a repeating spec — it would loop forever (see findings
        # #compose-repeat-unbounded). Here we only assert the flag survives the
        # JSON round-trip; sample equality is checked on the finite spec below.
        c = w.Composer(
            [w.Segment("tone", freq=1e5, num_samples=128)],
            repeat=True,
        )
        d = c.to_dict()
        assert set(d) >= {"repeat", "continuous", "segments"}
        assert d["repeat"] is True
        assert w.Composer.from_json(c.to_json()).to_dict()["repeat"] is True

        # Finite spec: the JSON round-trip reproduces identical samples.
        fin = w.Composer([w.Segment("tone", freq=1e5, num_samples=128)])
        assert np.array_equal(
            fin.compose(), w.Composer.from_json(fin.to_json()).compose()
        )

    def test_from_file(self, tmp_path) -> None:
        c = w.Composer([w.Segment("qpsk", sps=4, num_samples=128)])
        p = tmp_path / "spec.json"
        p.write_text(c.to_json())
        c2 = w.Composer.from_file(str(p))
        assert np.array_equal(c.compose(), c2.compose())

    def test_to_sigmf_valid_json(self) -> None:
        # Two segments -> two annotations; documents the sidecar schema in
        # docs/guide/wfmgen.md (#sigmf-sidecar-schema).
        c = w.Composer(
            [
                w.Segment(
                    "qpsk", sps=8, snr=20.0, snr_mode="esno", num_samples=4096
                ),
                w.Segment("tone", freq=1e5, num_samples=2048),
            ]
        )
        meta = json.loads(c.to_sigmf(sample_type="ci16", fs=1e6, fc=2.4e9))

        g = meta["global"]
        assert g["core:datatype"] == "ci16_le"
        assert g["core:sample_rate"] == 1_000_000
        assert g["core:version"] == "1.0.0"

        # Single capture carries the RF centre.
        assert meta["captures"][0]["core:sample_start"] == 0
        assert meta["captures"][0]["core:frequency"] == 2.4e9

        # One annotation per source, in order, with the wfmgen:* ground truth.
        anns = meta["annotations"]
        assert len(anns) == 2
        assert anns[0]["core:label"] == "qpsk"
        assert anns[1]["core:label"] == "tone"
        assert anns[0]["core:sample_start"] == 0
        assert anns[0]["core:sample_count"] == 4096
        assert anns[1]["core:sample_start"] == 4096
        for a in anns:
            assert a["core:freq_lower_edge"] <= a["core:freq_upper_edge"]
            for k in (
                "wfmgen:snr",
                "wfmgen:snr_mode",
                "wfmgen:sps",
                "wfmgen:seed",
            ):
                assert k in a
        assert anns[0]["wfmgen:snr_mode"] == "esno"

    def test_context_manager_and_close(self) -> None:
        with w.Composer([w.Segment("tone", freq=1e5, num_samples=64)]) as c:
            assert c.compose().shape == (64,)


class TestReaderWriter:
    @pytest.mark.parametrize("file_type", ENUMS["file_type"])
    @pytest.mark.parametrize("sample_type", ["cf32", "ci16"])
    def test_write_read_roundtrip(
        self, tmp_path, file_type: str, sample_type: str
    ) -> None:
        x = w.Synth(type="tone", fs=1e6, freq=5e4, snr=100.0).steps(512)
        # SigMF keys off the .sigmf-data extension + a .sigmf-meta sidecar.
        if file_type == "sigmf":
            base = tmp_path / "cap"
            path = str(base) + ".sigmf-data"
            (base.with_suffix(".sigmf-meta")).write_text(
                w.Composer(
                    [w.Segment("tone", freq=5e4, num_samples=512)]
                ).to_sigmf(sample_type=sample_type, fs=1e6)
            )
        else:
            path = str(tmp_path / f"cap.{file_type}")
        with w.Writer(
            path,
            file_type=file_type,
            sample_type=sample_type,
            fs=1e6,
            total=len(x),
        ) as wr:
            wr.write(x)

        # Raw/CSV are headerless: the reader cannot infer the sample type, so
        # it must be supplied. BLUE/SigMF carry it in the header/sidecar.
        r = (
            w.Reader(path)
            if file_type in ("blue", "sigmf")
            else w.Reader(path, sample_type=sample_type)
        )
        assert r.file_type in (file_type, "raw")  # sigmf-data detects as sigmf
        y = np.asarray(r.read(512))
        r.close()
        assert len(y) == 512
        # Correlation check (integer/CSV quantisation tolerated).
        corr = np.abs(np.vdot(y, x)) / (np.linalg.norm(y) * np.linalg.norm(x))
        assert corr > 0.999

    def test_writer_clip_properties(self, tmp_path) -> None:
        x = 3.0 * w.Synth(type="tone", freq=1e4, snr=100.0).steps(512)
        p = tmp_path / "clip.iq"
        with w.Writer(
            str(p), file_type="raw", sample_type="ci16", total=len(x)
        ) as wr:
            wr.track_clipping(1)
            wr.write(x)
            assert wr.clipped is True
            assert 0.0 < wr.clip_fraction <= 1.0
            assert isinstance(wr.peak_dbfs, float)

    def test_reader_metadata(self, tmp_path) -> None:
        x = w.Synth(type="tone", freq=5e4, snr=100.0).steps(256)
        p = tmp_path / "cap.blue"
        with w.Writer(
            str(p),
            file_type="blue",
            sample_type="cf32",
            fs=2e6,
            fc=1e6,
            total=len(x),
        ) as wr:
            wr.write(x)
        r = w.Reader(p)
        # BLUE carries a header -> metadata recovered.
        assert r.fs == 2e6
        assert r.num_samples == 256
        assert r.sample_type == "cf32"
        r.close()

    @pytest.mark.parametrize(
        "sample_type", ["cf32", "cf64", "ci8", "ci16", "ci32"]
    )
    def test_read_iq_all_types(self, tmp_path, sample_type: str) -> None:
        x = w.Synth(type="qpsk", sps=2, snr=100.0).steps(1024)
        p = tmp_path / f"c.{sample_type}"
        with w.Writer(
            str(p), file_type="raw", sample_type=sample_type, total=len(x)
        ) as wr:
            wr.write(x)
        y = w.read_iq(str(p), sample_type)
        assert len(y) == 1024
        raw = w.read_iq(str(p), sample_type, raw=True)
        assert raw.shape == (1024, 2)


class TestZmqSinkAndClock:
    @staticmethod
    def _sink_to_sub(stream, sample_type: str):
        """Bind a ``ZmqSink``, connect a ``Subscriber``, and round-trip one
        block over ``ipc://``. Returns the received samples (or raises the
        decode error). ZMQ PUB/SUB is a slow-joiner — warm up, then re-send a
        few times in case the first frame races the subscription."""
        import tempfile
        import time

        ep = f"ipc://{tempfile.mkdtemp()}/feed"
        sink = w.ZmqSink(ep, sample_type=sample_type)
        sub = stream.Subscriber(ep)
        time.sleep(0.1)
        x = w.Synth(type="tone", fs=1e6, freq=1e5, snr=100.0).steps(1024)
        try:
            got = None
            for _ in range(10):
                sink.send(x, 1e6, 2.4e9)
                samples, _hdr = sub.recv(timeout_ms=500)
                if samples is not None and len(samples):
                    got = np.asarray(samples)
                    break
        finally:
            sink.close()
            sub.close()
        return x, got

    def test_zmqsink_to_subscriber(self) -> None:
        # Live PUB->SUB over ipc:// proves ZmqSink moves samples to a
        # doppler.stream Subscriber. cf64 round-trips bit-exactly.
        stream = pytest.importorskip("doppler.stream")
        x, got = self._sink_to_sub(stream, "cf64")
        assert got is not None, "no frame received from ZmqSink"
        assert np.allclose(got, x.astype(np.complex128), atol=1e-9)

    def test_zmqsink_cf32_decodes_in_stream(self) -> None:
        # cf32 is wfm's DEFAULT sample type + the most common ZmqSink path;
        # doppler.stream now decodes all six dp_sample_type_t types (#193).
        stream = pytest.importorskip("doppler.stream")
        x, got = self._sink_to_sub(stream, "cf32")
        assert got is not None, "no cf32 frame received from ZmqSink"
        assert np.allclose(got, x, atol=1e-4)

    def test_zmqsink_clip_properties(self) -> None:
        import tempfile

        ep = f"ipc://{tempfile.mkdtemp()}/feed2"
        sink = w.ZmqSink(ep, sample_type="ci16")
        sink.track_clipping(1)
        sink.send(3.0 * np.ones(64, np.complex64), 1e6, 0.0)
        assert sink.clipped is True
        assert sink.clip_fraction > 0.0
        sink.close()

    def test_sampleclock_pace(self) -> None:
        import time

        fs = 100_000.0
        sc = w.SampleClock(fs=fs)
        n = 5000
        t0 = time.perf_counter()
        sc.pace(n)
        elapsed = time.perf_counter() - t0
        # Paced wait ~ n/fs = 50 ms; allow generous slack for CI jitter.
        assert elapsed == pytest.approx(n / fs, abs=0.1)
        assert sc.samples >= n
        assert sc.underruns >= 0
        assert isinstance(sc.max_lateness, float)
        sc.reset()
        sc.close()


@needs_cli
class TestCLI:
    def _run(self, args: list[str], tmp_path) -> np.ndarray:
        out = tmp_path / "o.iq"
        subprocess.run(
            [WFMGEN, *args, "--output", str(out)],
            check=True,
            capture_output=True,
        )
        return np.fromfile(out, dtype=np.complex64)

    @pytest.mark.parametrize("wtype", ENUMS["type"])
    def test_every_type(self, tmp_path, wtype: str) -> None:
        args = [
            "--type",
            wtype,
            "--count",
            "128",
            "--snr",
            "100",
            "--sample-type",
            "cf32",
        ]
        if wtype == "bits":
            args += ["--bits", "10110010"]
        x = self._run(args, tmp_path)
        assert len(x) == 128

    @pytest.mark.parametrize("stype", ENUMS["sample_type"])
    def test_every_sample_type(self, tmp_path, stype: str) -> None:
        out = tmp_path / "o.bin"
        subprocess.run(
            [
                WFMGEN,
                "--type",
                "tone",
                "--freq",
                "5e4",
                "--count",
                "256",
                "--snr",
                "100",
                "--sample-type",
                stype,
                "--output",
                str(out),
            ],
            check=True,
            capture_output=True,
        )
        y = w.read_iq(str(out), stype)
        assert len(y) == 256

    def test_face_parity_cli_vs_composer(self, tmp_path) -> None:
        cli = self._run(
            [
                "--type",
                "tone",
                "--freq",
                "100000",
                "--fs",
                "1000000",
                "--count",
                "256",
                "--snr",
                "100",
                "--sample-type",
                "cf32",
            ],
            tmp_path,
        )
        py = w.Composer(
            [w.Segment("tone", freq=1e5, fs=1e6, num_samples=256, snr=100.0)]
        ).compose()
        assert np.array_equal(cli, py)

    def test_face_parity_from_file_replay(self, tmp_path) -> None:
        spec = w.Composer([w.Segment("qpsk", sps=4, num_samples=256, seed=3)])
        spec_path = tmp_path / "spec.json"
        spec_path.write_text(spec.to_json())
        out = tmp_path / "o.iq"
        subprocess.run(
            [
                WFMGEN,
                "--from-file",
                str(spec_path),
                "--sample-type",
                "cf32",
                "--output",
                str(out),
            ],
            check=True,
            capture_output=True,
        )
        cli = np.fromfile(out, dtype=np.complex64)
        assert np.array_equal(cli, spec.compose())

    def test_bits_hex_equals_binary(self, tmp_path) -> None:
        h = self._run(
            [
                "--type",
                "bits",
                "--bits-hex",
                "A5",
                "--modulation",
                "bpsk",
                "--sps",
                "1",
                "--count",
                "8",
                "--snr",
                "100",
                "--sample-type",
                "cf32",
            ],
            tmp_path,
        )
        b = self._run(
            [
                "--type",
                "bits",
                "--bits",
                "10100101",
                "--modulation",
                "bpsk",
                "--sps",
                "1",
                "--count",
                "8",
                "--snr",
                "100",
                "--sample-type",
                "cf32",
            ],
            tmp_path,
        )
        assert np.array_equal(h, b)

    def test_json_template_subcommand(self, tmp_path) -> None:
        r = subprocess.run(
            [WFMGEN, "json-template"], check=True, capture_output=True
        )
        # Emits an editable spec that parses as JSON.
        spec = json.loads(r.stdout.decode())
        assert "segments" in spec

    def test_record_resolved_spec(self, tmp_path) -> None:
        rec = tmp_path / "rec.json"
        subprocess.run(
            [
                WFMGEN,
                "--type",
                "tone",
                "--freq",
                "5e4",
                "--count",
                "64",
                "--snr",
                "100",
                "--sample-type",
                "cf32",
                "--output",
                str(tmp_path / "o.iq"),
                "--record",
                str(rec),
            ],
            check=True,
            capture_output=True,
        )
        assert json.loads(rec.read_text())  # a resolved spec record

    def test_output_dash_is_stdout(self, tmp_path) -> None:
        # Docs (guide/wfmgen.md) say '-' prints to stdout (#192).
        r = subprocess.run(
            [
                WFMGEN,
                "--type",
                "tone",
                "--count",
                "256",
                "--sample-type",
                "cf32",
                "--output",
                "-",
            ],
            check=True,
            capture_output=True,
            cwd=str(tmp_path),
        )
        assert len(r.stdout) == 256 * 8  # 256 cf32 samples on stdout
        assert not (tmp_path / "-").exists()

    def test_output_dash_to_tty_refused(self, tmp_path) -> None:
        # The explicit '-' form must trip the same "refuse binary IQ to a
        # terminal" guard as omitting --output. Give wfmgen a pty as stdout so
        # isatty() is true; it should exit non-zero and write nothing.
        import os

        master, slave = os.openpty()
        try:
            r = subprocess.run(
                [
                    WFMGEN,
                    "--type",
                    "tone",
                    "--count",
                    "64",
                    "--sample-type",
                    "cf32",
                    "--output",
                    "-",
                ],
                stdout=slave,
                stderr=subprocess.PIPE,
                cwd=str(tmp_path),
            )
        finally:
            os.close(slave)
            os.close(master)
        assert r.returncode != 0, "writing binary IQ to a tty must be refused"
        assert b"terminal" in r.stderr.lower()
        # CSV is human-readable, so '-' to a tty is allowed.
        m2, s2 = os.openpty()
        try:
            r2 = subprocess.run(
                [
                    WFMGEN,
                    "--type",
                    "tone",
                    "--count",
                    "8",
                    "--file-type",
                    "csv",
                    "--output",
                    "-",
                ],
                stdout=s2,
                stderr=subprocess.PIPE,
                cwd=str(tmp_path),
            )
        finally:
            os.close(s2)
            os.close(m2)
        assert r2.returncode == 0, "CSV to a tty should be allowed"
