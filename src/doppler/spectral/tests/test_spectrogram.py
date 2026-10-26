"""Spectrogram: chunking, resume, levels, flush and refusals."""

import numpy as np
import pytest

from doppler.spectral import PSD, Spectrogram

NFFT = 64


def _tone(n: int, k: int, amp: float = 1.0) -> np.ndarray:
    """A complex tone on bin k of an NFFT-point frame, repeated to length n."""
    t = np.arange(n)
    return (amp * np.exp(2j * np.pi * k * t / NFFT)).astype(np.complex64)


def _push_all(sp: Spectrogram, x: np.ndarray, chunk: int) -> np.ndarray:
    """Push x in chunks of chunk samples and collect the rows produced."""
    rows = []
    for i in range(0, len(x), chunk):
        block = sp.push(x[i : i + chunk])
        if block is not None:
            rows.append(block)
    return np.concatenate(rows) if rows else np.empty((0, NFFT), np.float32)


class TestChunkInvariance:
    """Rows do not depend on how the input is chunked."""

    @pytest.mark.parametrize("chunk", [1, 7, 33, 64, 200, 1000])
    def test_rows_do_not_depend_on_the_chunking(self, chunk):
        """Odd chunk sizes straddle frame edges, and the rows must not move."""
        rng = np.random.default_rng(1)
        x = (
            rng.standard_normal(1000) + 1j * rng.standard_normal(1000)
        ).astype(np.complex64)
        whole = _push_all(Spectrogram(nfft=NFFT, hop=NFFT), x, len(x))
        pieces = _push_all(Spectrogram(nfft=NFFT, hop=NFFT), x, chunk)
        assert pieces.shape == whole.shape
        assert np.array_equal(pieces, whole)

    def test_a_trailing_partial_frame_is_dropped_without_flush(self):
        """A partial frame at the end is not emitted until flush()."""
        x = np.ones(1000, np.complex64)
        rows = _push_all(Spectrogram(nfft=NFFT, hop=NFFT), x, 1000)
        assert rows.shape[0] == 1000 // NFFT

    def test_zero_length_push_returns_none(self):
        """A push that completes no row returns None."""
        sp = Spectrogram(nfft=NFFT, hop=NFFT)
        assert sp.push(np.zeros(0, np.complex64)) is None


class TestResume:
    """A mid-frame state blob resumes in a fresh object."""

    def test_a_mid_frame_blob_resumes_into_a_fresh_object(self):
        """The second half matches an unbroken run, bit for bit."""
        rng = np.random.default_rng(2)
        x = (rng.standard_normal(700) + 1j * rng.standard_normal(700)).astype(
            np.complex64
        )
        cut = 333  # not a multiple of NFFT: the carry is non-empty
        expect = _push_all(Spectrogram(nfft=NFFT, hop=NFFT), x, 700)

        a = Spectrogram(nfft=NFFT, hop=NFFT)
        head = _push_all(a, x[:cut], cut)
        b = Spectrogram(nfft=NFFT, hop=NFFT)
        b.set_state(a.get_state())
        tail = _push_all(b, x[cut:], 700 - cut)
        assert np.array_equal(np.concatenate([head, tail]), expect)

    @pytest.mark.parametrize(
        "src, dst",
        [
            ({"window": "rect"}, {"window": "rect", "mode": "db"}),
            ({"window": "rect"}, {"window": "rect", "hop": NFFT // 2}),
            ({"window": "rect"}, {"window": "rect", "nfft": 2 * NFFT}),
            ({"window": "rect"}, {"window": "blackman-harris"}),
            (
                {"window": "kaiser", "beta": 2.0},
                {"window": "kaiser", "beta": 9.0},
            ),
        ],
        ids=["mode", "hop", "nfft", "window", "kaiser-beta"],
    )
    def test_a_blob_from_another_configuration_is_refused(self, src, dst):
        """A blob from another mode, hop, nfft, window or beta is refused."""
        base = {"nfft": NFFT, "hop": NFFT}
        s = Spectrogram(**{**base, **src})
        s.push(np.ones(40, np.complex64))
        blob = s.get_state()
        d = Spectrogram(**{**base, **dst})
        d.push(np.ones(7, np.complex64))
        before = d.get_state()
        with pytest.raises(ValueError):
            d.set_state(blob)
        assert d.get_state() == before
        assert d.pending == 7

    def test_a_hann_object_takes_a_blob_with_another_beta(self):
        """Beta shapes only the Kaiser window, so a Hann object restores it."""
        src = Spectrogram(nfft=NFFT, hop=NFFT, window="hann", beta=0.0)
        dst = Spectrogram(nfft=NFFT, hop=NFFT, window="hann", beta=5.0)
        dst.set_state(src.get_state())


class TestRowIsOnePsdFrame:
    """A row is the PSD of its frame."""

    def test_a_row_equals_the_psd_of_that_frame(self):
        """Row 0 matches PSD.power_twosided() of the same frame."""
        rng = np.random.default_rng(3)
        frame = (
            rng.standard_normal(NFFT) + 1j * rng.standard_normal(NFFT)
        ).astype(np.complex64)
        row = Spectrogram(nfft=NFFT, hop=NFFT, window="hann").push(frame)[0]
        psd = PSD(n=NFFT, fs=1.0, window="hann", mode="mean")
        psd.accumulate(frame)
        np.testing.assert_allclose(row, psd.power_twosided(), rtol=1e-5)


class TestLevels:
    """Full-scale tone levels in power and dB mode."""

    def test_full_scale_tone_reads_one_in_power_mode(self):
        """A unit-amplitude tone reads 1.0 on its bin, DC-centred."""
        row = Spectrogram(nfft=NFFT, hop=NFFT, window="rect").push(
            _tone(NFFT, 10)
        )[0]
        assert row.argmax() == NFFT // 2 + 10
        assert row[NFFT // 2 + 10] == pytest.approx(1.0, rel=1e-4)

    def test_full_scale_tone_reads_zero_dbfs_in_db_mode(self):
        """A unit-amplitude tone reads 0 dBFS on its bin in dB mode."""
        row = Spectrogram(nfft=NFFT, hop=NFFT, window="rect", mode="db").push(
            _tone(NFFT, 10)
        )[0]
        assert row[NFFT // 2 + 10] == pytest.approx(0.0, abs=1e-3)

    def test_half_amplitude_tone_reads_minus_6_02_db_in_db_mode(self):
        """A 0.5-amplitude tone is a quarter of the power: -6.02 dBFS."""
        row = Spectrogram(nfft=NFFT, hop=NFFT, window="rect", mode="db").push(
            _tone(NFFT, 10, amp=0.5)
        )[0]
        assert row[NFFT // 2 + 10] == pytest.approx(-6.0206, abs=1e-3)


class TestBackpressure:
    """The out buffer's shape and size are checked on the Python face."""

    def test_a_short_out_takes_what_fits_and_resumes_bit_exact(self):
        """A short out writes the whole rows that fit; the rest resumes.

        Pushing 4 rows' worth through a 2-row out takes 2 rows and reports
        consumed below the input length. Offering x[consumed:] again yields
        the remaining rows, and the two halves equal one full push bit for
        bit (gh-2230, #2592).
        """
        x = np.arange(4 * NFFT).astype(np.complex64)
        ref = Spectrogram(nfft=NFFT, hop=NFFT).push(x)
        sp = Spectrogram(nfft=NFFT, hop=NFFT)
        first = sp.push(x, out=np.empty((2, NFFT), np.float32))
        assert first.shape == (2, NFFT)
        assert 0 < sp.consumed < len(x)
        rest = sp.push(x[sp.consumed :])
        np.testing.assert_array_equal(np.vstack([first, rest]), ref)

    def test_a_full_capacity_push_takes_all_its_input(self):
        """With out sized to push_max_out, one call takes every sample."""
        x = np.ones(4 * NFFT, np.complex64)
        sp = Spectrogram(nfft=NFFT, hop=NFFT)
        out = np.empty((sp.push_max_out(len(x)) // NFFT, NFFT), np.float32)
        block = sp.push(x, out=out)
        assert block.shape == (4, NFFT)
        assert sp.consumed == len(x)

    def test_an_out_of_the_wrong_dtype_is_a_type_error(self):
        """A float64 out is refused with TypeError."""
        sp = Spectrogram(nfft=8, hop=8)
        with pytest.raises(TypeError):
            sp.push(
                np.ones(16, np.complex64), out=np.empty((2, 8), np.float64)
            )

    def test_a_one_dimensional_out_is_refused(self):
        """A 1-D out is refused: it must be (rows, nfft)."""
        sp = Spectrogram(nfft=8, hop=8)
        with pytest.raises(ValueError):
            sp.push(np.ones(16, np.complex64), out=np.empty(16, np.float32))

    def test_an_out_with_the_wrong_columns_is_refused(self):
        """An out whose columns are not nfft is refused."""
        sp = Spectrogram(nfft=8, hop=8)
        with pytest.raises(ValueError):
            sp.push(
                np.ones(16, np.complex64), out=np.empty((2, 4), np.float32)
            )


class TestFlush:
    """flush() returns the owed row as push returns rows, or None (#2181)."""

    def test_flush_returns_the_owed_row_on_the_hop_grid_with_hop_below_nfft(
        self,
    ):
        """With hop 4 the owed row starts at sample 4 and is its PSD frame."""
        x = (np.arange(11) + 1j * np.arange(11)).astype(np.complex64)
        sp = Spectrogram(nfft=8, hop=4, window="rect")
        sp.push(x)
        assert sp.flush_max_out() == 8
        row = sp.flush()
        assert row.shape == (1, 8)
        assert row.dtype == np.float32
        padded = np.zeros(8, np.complex64)
        padded[:7] = x[4:]
        psd = PSD(n=8, fs=1.0, window="rect", mode="mean")
        psd.accumulate(padded)
        np.testing.assert_allclose(row[0], psd.power_twosided(), rtol=1e-5)

    def test_out_is_written_in_place(self):
        """A (1, nfft) out is the row's storage, and holds what () returns."""
        x = np.ones(11, np.complex64)
        want = Spectrogram(nfft=8, hop=8, window="rect")
        want.push(x)
        sp = Spectrogram(nfft=8, hop=8, window="rect")
        sp.push(x)
        buf = np.zeros((1, 8), np.float32)
        got = sp.flush(out=buf)
        assert np.shares_memory(got, buf)
        np.testing.assert_array_equal(got, want.flush())

    @pytest.mark.parametrize("shape", [(1, 7), (1, 9), (2, 8), (8,)])
    def test_an_out_of_the_wrong_shape_is_refused_and_the_frame_stays_owed(
        self, shape
    ):
        """A short, long, two-row or 1-D out raises ValueError, losing
        nothing: the (2, 8) case reaches the kernel's exact-length refusal."""
        sp = Spectrogram(nfft=8, hop=8, window="rect")
        sp.push(np.ones(11, np.complex64))
        with pytest.raises(ValueError):
            sp.flush(out=np.empty(shape, np.float32))
        assert sp.pending == 3
        assert sp.flush().shape == (1, 8)

    @pytest.mark.parametrize("with_out", [False, True])
    def test_a_second_flush_returns_none(self, with_out):
        """A flush restarts the stream, so a second flush owes nothing."""
        sp = Spectrogram(nfft=8, hop=8, window="rect")
        sp.push(np.ones(11, np.complex64))
        kw = {"out": np.empty((1, 8), np.float32)} if with_out else {}
        assert sp.flush(**kw).shape == (1, 8)
        assert sp.flush(**kw) is None


class TestConstruction:
    """Invalid construction is refused, and the mode defaults to power."""

    def test_hop_past_nfft_is_refused(self):
        """A hop above nfft would leave samples no row covers."""
        with pytest.raises(ValueError):
            Spectrogram(nfft=NFFT, hop=NFFT + 1)

    @pytest.mark.parametrize("nfft", [8, 128, 1024])
    def test_hop_0_means_nfft_at_any_nfft(self, nfft):
        """The default hop is 0, PSD's rule: it tiles the stream at any nfft.

        With a fixed default, Spectrogram(nfft=128) was refused and nfft 1024
        silently overlapped its frames 4 to 1.
        """
        sp = Spectrogram(nfft=nfft)
        assert sp.hop == nfft
        rows = sp.push(np.ones(3 * nfft, np.complex64))
        assert rows.shape == (3, nfft)
        tiled = Spectrogram(nfft=nfft, hop=nfft).push(
            np.ones(3 * nfft, np.complex64)
        )
        assert np.array_equal(rows, tiled)

    def test_hop_0_is_hop_nfft_in_the_blob_too(self):
        """hop 0 is resolved at construction, so its blob is hop = nfft's
        byte for byte and each restores into the other."""
        x = np.arange(37, dtype=np.float32).astype(np.complex64)
        zero, tiled = Spectrogram(nfft=8, hop=0), Spectrogram(nfft=8, hop=8)
        zero.push(x)
        tiled.push(x)
        assert zero.get_state() == tiled.get_state()
        zero.set_state(tiled.get_state())
        tiled.set_state(zero.get_state())

    def test_hop_0_at_nfft_0_is_refused(self):
        """hop 0 resolves to nfft, which is 0: the range still refuses it."""
        with pytest.raises(ValueError):
            Spectrogram(nfft=0, hop=0)

    def test_mode_is_power_by_default(self):
        """A caller who does not ask for dB gets linear power."""
        row = Spectrogram(nfft=NFFT, hop=NFFT, window="rect").push(
            _tone(NFFT, 10)
        )[0]
        assert row.max() <= 1.0 + 1e-4


class TestConfiguration:
    """The configuration reads back as built, read-only, by the same names."""

    def test_every_argument_reads_back(self):
        """window and mode come back as the strings that made them."""
        sp = Spectrogram(nfft=64, hop=16, window="kaiser", beta=7.5, mode="db")
        assert (sp.nfft, sp.hop, sp.window, sp.beta, sp.mode) == (
            64,
            16,
            "kaiser",
            7.5,
            "db",
        )
        assert isinstance(sp.nfft, int) and isinstance(sp.hop, int)
        assert isinstance(sp.window, str) and isinstance(sp.mode, str)
        assert isinstance(sp.beta, float)

    def test_beta_reads_back_as_the_float32_it_keeps(self):
        """beta is stored as float32, so a value float32 cannot hold exactly
        reads back widened, not as typed: 0.1 is 0.10000000149011612."""
        sp = Spectrogram(nfft=16, hop=16, window="kaiser", beta=0.1)
        assert sp.beta == float(np.float32(0.1))
        assert sp.beta != 0.1

    @pytest.mark.parametrize(
        "window", ["hann", "kaiser", "blackman-harris", "rect"]
    )
    def test_every_window_reads_back(self, window):
        """Every name the constructor takes is a name the property returns."""
        assert Spectrogram(nfft=16, hop=16, window=window).window == window

    def test_the_defaults_read_back(self):
        """The defaults are the documented ones."""
        sp = Spectrogram()
        assert (sp.nfft, sp.hop, sp.window, sp.beta, sp.mode) == (
            256,
            256,
            "hann",
            0.0,
            "power",
        )

    def test_consumed_and_pending_follow_the_stream(self):
        """pending is the uncovered tail; consumed is the whole chunk."""
        sp = Spectrogram(nfft=8, hop=8, window="rect")
        sp.push(np.ones(20, np.complex64))
        assert (sp.consumed, sp.pending) == (20, 4)
        assert isinstance(sp.consumed, int) and isinstance(sp.pending, int)

    @pytest.mark.parametrize(
        "name",
        ["nfft", "hop", "window", "beta", "mode", "consumed", "pending"],
    )
    def test_the_configuration_is_read_only(self, name):
        """A setter would change what an existing object's rows mean."""
        sp = Spectrogram(nfft=8, hop=8)
        with pytest.raises(AttributeError):
            setattr(sp, name, getattr(sp, name))
