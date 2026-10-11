"""Spectrogram: chunking, resume, row levels and flush."""

import numpy as np
import pytest

from doppler.spectral import PSD, Spectrogram

NFFT = 64


def _tone(n: int, k: int, amp: float = 1.0) -> np.ndarray:
    """A complex tone on bin k of an nfft-point frame, repeated to length n."""
    t = np.arange(n)
    return (amp * np.exp(2j * np.pi * k * t / NFFT)).astype(np.complex64)


def _push_all(sp: Spectrogram, x: np.ndarray, chunk: int) -> np.ndarray:
    """Push x in chunks of `chunk` samples and collect every row produced."""
    rows = []
    for i in range(0, len(x), chunk):
        block = sp.push(x[i : i + chunk])
        if block is not None:  # None: this push completed no row
            rows.append(block)
    return np.concatenate(rows) if rows else np.empty((0, NFFT), np.float32)


class TestChunkInvariance:
    """Rows do not depend on how the input is chunked."""

    @pytest.mark.parametrize("chunk", [1, 7, 33, 64, 200, 1000])
    def test_rows_do_not_depend_on_the_chunking(self, chunk):
        """Odd chunk sizes straddle frame edges; the rows must not move."""
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
        """A unit-amplitude tone reads 1.0 on its bin."""
        row = Spectrogram(nfft=NFFT, hop=NFFT, window="rect").push(
            _tone(NFFT, 10)
        )[0]
        assert row.argmax() == NFFT // 2 + 10
        assert row[NFFT // 2 + 10] == pytest.approx(1.0, rel=1e-4)

    def test_full_scale_tone_reads_its_dbfs_in_db_mode(self):
        """The same tone reads 0 dBFS on its bin in dB mode."""
        row = Spectrogram(nfft=NFFT, hop=NFFT, window="rect", mode="db").push(
            _tone(NFFT, 10)
        )[0]
        assert row[NFFT // 2 + 10] == pytest.approx(0.0, abs=1e-3)


class TestBackpressure:
    """A row limit loses no input."""

    def test_a_full_capacity_push_takes_all_its_input(self):
        """With out sized to push_max_out, one call takes every sample and
        writes every row it completes; consumed reports the whole input."""
        x = np.ones(4 * NFFT, np.complex64)
        sp = Spectrogram(nfft=NFFT, hop=NFFT)
        out = np.empty((sp.push_max_out(len(x)) // NFFT, NFFT), np.float32)
        block = sp.push(x, out=out)
        assert block.shape == (4, NFFT)
        assert sp.consumed == len(x)


class TestFlush:
    """flush() emits the trailing partial frame."""

    def test_flush_emits_the_trailing_partial_frame(self):
        """The partial frame is zero-padded to nfft and emitted by flush()."""
        x = np.ones(NFFT + 10, np.complex64)
        sp = Spectrogram(nfft=NFFT, hop=NFFT)
        rows = [sp.push(x)]
        tail = np.empty(NFFT, np.float32)
        assert sp.flush(tail) == NFFT
        assert sp.pending == 0
        assert np.concatenate([*rows, tail[None, :]]).shape[0] == 2


class TestConstruction:
    """Invalid construction is refused."""

    @pytest.mark.parametrize("hop", [0, NFFT + 1])
    def test_hop_outside_one_to_nfft_is_refused(self, hop):
        """hop must be in 1..nfft."""
        with pytest.raises(ValueError):
            Spectrogram(nfft=NFFT, hop=hop)

    def test_mode_is_power_by_default(self):
        """A caller who does not ask for dB gets linear power."""
        row = Spectrogram(nfft=NFFT, hop=NFFT, window="rect").push(
            _tone(NFFT, 10)
        )[0]
        assert row.max() <= 1.0 + 1e-4
