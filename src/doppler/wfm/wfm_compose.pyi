# wfm_compose.pyi — composer OO types (jm; gh-287).
from __future__ import annotations
from typing import Any, Iterator
from typing_extensions import disjoint_base
import numpy as np
import numpy.typing as npt
from numpy.typing import NDArray
from doppler.wfm import FrameDesc

@disjoint_base
class Synth:
    """Synth.

    Parameters
    ----------
    type : str, default ``"tone"``
        Waveform type. tone: complex sinusoid. noise: AWGN. pn: PN sequence
        (LFSR). bpsk/qpsk: PN-driven modulation. chirp: linear-FM sweep. bits:
        a caller's bit pattern, with selectable modulation. symbols: a caller's
        complex constellation stream. dsss: spread spectrum -- a two-code burst
        (repeated preamble + data-code-spread frame) by default, or a
        continuous asynchronous stream when symbol_rate is set.
        One of ``"tone"``, ``"noise"``, ``"pn"``, ``"bpsk"``, ``"qpsk"``,
        ``"chirp"``, ``"bits"``, ``"symbols"``, ``"dsss"``.
    freq : float | tuple[float, float], default 0.0
        Carrier or offset frequency in Hz; for chirp, the sweep start. With fs
        = 1 it is in normalised cycles per sample.
    snr : float | tuple[float, float], default 100.0
        Signal-to-noise ratio in dB, interpreted per snr_mode. 100 or more is
        clean: no AWGN is added.
    snr_mode : str, default ``"auto"``
        How snr is interpreted. auto: Es/N0 for bpsk/qpsk/dsss, and relative to
        full scale for tone/noise/pn/chirp/bits/symbols -- bits included,
        because a bits frame has no symbol rate the engine can infer. fs: dB
        relative to full scale. ebno: Eb/N0, per bit. esno: Es/N0, per symbol;
        for a dsss burst the outer data symbol of len(data_code) chips x sps
        samples, for a continuous dsss stream the fs/symbol_rate samples the
        async symbol spans.
        One of ``"auto"``, ``"fs"``, ``"ebno"``, ``"esno"``.
    seed : int, default 0
        PRNG and LFSR seed for the noise and PN streams. Deterministic: vary it
        for run-to-run change. For a PN-sourced type it starts the
        pn_length-bit register, masked: a seed whose low pn_length bits are
        zero starts the register at 1, as seed 0 does.
    sps : int, default 1
        Samples per symbol (PSK) or per chip (PN): the oversampling factor.
        Unused by noise, which records it as 0.
    pn_length : int, default 15
        PN LFSR register length in bits, 2 to 64 for the PN-bearing types; the
        sequence period is 2^pn_length - 1. Unused by noise, which records it
        as 0.
    pn_poly : int, default 0
        PN generator polynomial, in the Galois bit-vector convention; 0 selects
        a maximal-length (MLS) polynomial for pn_length. It must fit the
        pn_length-bit register: a bit at or above it is refused, not masked. A
        polynomial above 2^53 does not survive a JSON number, so a scene file
        needs 0 (auto) for such a register.
    lfsr : str, default ``"galois"``
        PN LFSR realisation. Both give the same period; fibonacci's chips are
        galois's in reverse order.
        One of ``"galois"``, ``"fibonacci"``.
    level : float | tuple[float, float], default 0.0
        Source power in dBFS (<= 0; 0 is unit power). Applies when summed in a
        Segment or Composer, as a gain of 10^(level/20); a standalone
        Synth.steps() ignores it.
    background : int, default 0
        Mark this source as part of the static background field (0/1).
        Plan.prepare() folds a contiguous leading run of background sources
        into ONE pre-summed cache entry instead of caching each separately, so
        a scene of many fixed emitters costs one buffer rather than hundreds.
        The composite is overridable as a unit: it takes a single slot in
        gains/phases/enable and counts as one in n_sources(), so scaling it
        trims the whole field while its members keep their relative levels.
        Background sources must come first in the segment (a non-prefix
        ordering is rejected by prepare, since the fold would no longer
        reproduce compose bit-for-bit). Ignored by compose() and by standalone
        Synth.steps().
    f_end : float | tuple[float, float], default 0.0
        Chirp end frequency in Hz; ignored by other types.
    span : int, default 0
        Chirp sweep length in samples: the frequency ramps from freq to f_end
        over this many samples, then holds at f_end. 0 means the enclosing
        Segment's num_samples. A standalone chirp (f_end != freq) must declare
        it, so step(), steps(N) and any chunking of reads produce the same
        waveform; generating one without it raises. Ignored by non-chirp types.
    doppler : float | tuple[float, float], default 0.0
        Clock Doppler in ppm: the received time base is rescaled by 1 +
        doppler*1e-6, so the symbol and chip rates move with the carrier and a
        timing loop sees the error a carrier-only `freq` offset hides. Accepts
        a (lo, hi) tuple drawn uniformly per repeat, like freq/snr. Zero
        doppler AND zero doppler_rate means no channel is built at all, so a
        source that does not ask for Doppler renders exactly as it always did.
    doppler_rate : float | tuple[float, float], default 0.0
        Linear ramp on `doppler`, in ppm per second of elapsed stream time. The
        channel runs through a segment's gaps as well as its on-time -- an
        emitter does not stop moving because its burst ended -- so this is per
        second, not per second of on-time. Accepts a (lo, hi) tuple drawn
        uniformly per repeat.
    carrier_hz : float, default 0.0
        RF carrier in Hz that the ppm figures are referred to. It gives the
        coherent carrier rotation that accompanies the time-base warp; 0 warps
        the clock alone, with no carrier rotation -- a legitimate scene, not an
        unset field. Independent of doppler/doppler_rate.
    doppler_lifetime : str, default ``"per_instance"``
        How long this source's Doppler channel lives. per_instance: the channel
        dies with each `repeats` instance, so the geometry restarts -- the
        repeated-trial shape, which composes with a ranged doppler re-drawn per
        instance. persist: one continuous pass carries across the segment's
        gaps and repeat instances, keyed by (segment, source) position -- the
        only lifetime under which doppler_rate accumulates across a multi-burst
        scene. Plan.prepare() REFUSES a persist source, because its cache
        renders each source independently and concurrently; compose() and
        stream() honour both.
        One of ``"per_instance"``, ``"persist"``.
    modulation : str, default ``"bpsk"``
        Symbol mapping of a bits pattern. none: the pattern shaped and output
        as-is (NRZ). bpsk: +/-1 symbols. qpsk: Gray-coded symbols from pairs of
        bits.
        One of ``"none"``, ``"bpsk"``, ``"qpsk"``.
    pulse : str, default ``"rect"``
        Pulse shape per symbol or chip, for pn/bpsk/qpsk/bits/symbols/dsss.
        rect: rectangular, no ISI filtering. rrc: root-raised cosine; see
        rrc_beta and rrc_span.
        One of ``"rect"``, ``"rrc"``.
    rrc_beta : float, default 0.35
        RRC roll-off factor, in (0, 1], when pulse=rrc.
    rrc_span : int, default 8
        RRC filter support in symbols when pulse=rrc, ONE-SIDED: the filter has
        2*rrc_span*sps + 1 taps, unit energy (sum of h^2 = 1).
    symbols : npt.NDArray[np.complex64] | None, default None
        For type=symbols: a complex constellation stream. Each element is the
        output point itself, oversampled by sps, cycled, and RRC-shaped with
        pulse=rrc, which generalises any modulation (pi/4-QPSK, QAM, ...).
    acq_code : bytes | None, default None
        The preamble code, sent acq_reps times at the head of the frame (a
        Field's *REPS on the command line and in a scene) -- the coherent
        pull-in target BurstDespreader.set_acq and BurstDemod.set_preamble lock
        to. For type=dsss it is unmodulated chips ahead of the spread frame;
        for type=bits it is the head of the bit pattern. Setting it is what
        makes a source FRAMED.
    acq_reps : int, default 1
        Preamble repetitions: periods of acq_code before the sync word. On the
        command line and in a scene it is acq_code's *REPS.
    data_code : bytes | None, default None
        For type=dsss: the payload spreading code, a second code distinct from
        acq_code. Every frame bit (sync, payload, crc) is XOR-spread across its
        full length, so len(data_code) is the spreading factor.
    sync : bytes | None, default None
        RETIRED (doppler#1617): nothing reads it but the refusal. The
        frame-sync word is a field of the frame DESCRIPTION, so sync= is
        refused naming frame=; the CLI's --sync builds that description for
        you, and a scene refuses the "sync" key.
    crc : bytes | None, default None
        RETIRED (doppler#1617): nothing reads it but the refusal. A CRC is a
        stage of the frame DESCRIPTION, so crc= is refused naming frame=,
        whatever its value (crc="none" included); the CLI's --crc builds that
        description for you, and a scene refuses the "crc" key.
    symbol_rate : float, default 0.0
        For type=dsss: > 0 selects CONTINUOUS asynchronous mode. The spreading
        code repeats endlessly and data rides on it at this symbol rate (Hz),
        independent of the code-epoch rate (chips/symbol = fs/sps/symbol_rate,
        non-integer). No preamble/sync/CRC frame; data comes from the payload
        when supplied, else a seeded PN a receiver regenerates. Absent/0 =
        burst.
    dsss_code_only : int, default 0
        Continuous dsss data source: 1 = code-only (the pure spreading code, no
        data modulation); 0 = data-modulated (the payload when supplied, else
        the seeded PN). Ignored for burst dsss and non-dsss types.
    frame : FrameDesc | str | None, default None
        A frame DESCRIPTION, the whole frame: fields in wire order, and stages
        that each name the span they cover (crc16, rs, randomise, interleave,
        conv, or a kind of your own). It is the only way a source says anything
        but the common frame: a coding stage, a field of the caller's own bits
        at a position of their choosing, a stage covering a span they name.
        `wfmgen --frame FILE`, a scene's `frame` key and Python's `frame=` (a
        FrameDesc or a Frame) all land here. When it is set it IS the frame,
        and an unspread acq_code beside it is refused. NULL means the common
        frame, `[preamble x reps | data]`, which `dp_wfm_frame_fixed()` builds
        from acq_code and the data source; a sync word or a CRC is a field or a
        stage of a description, never a flat field. A C caller's description is
        borrowed, exactly as `wfm_seq_t` is borrowed elsewhere here, so it must
        outlive the source. The composer and a Python source hold their own
        copy (`dp_wfm_frame_copy()`), so a later change to the FrameDesc does
        not reach them. On the Python face `frame=` is an input: read it back
        from the composer's JSON (its getter is jm's, pending removal:
        doppler#1694). KERNELS stay in C by design. A description names a
        stage's KIND; the code that runs it is a `wfm_frame_ops_t` entry, and a
        caller adding a genuinely new transform (convolutional interleaving,
        say) writes that kernel in C and hands it to `dp_wfm_frame_assemble`
        directly.
    bits : bytes | None, default None
        RETIRED (doppler#1718): nothing reads it but the refusal. A payload is
        drawn from a data source, so bits=, and its aliases payload= and
        pattern=, are refused naming data=; the CLI and a scene refuse --bits
        and "payload" the same way.
    data : bytes | None, default None
        A frame's payload drawn from a data source: a Field on the command line
        and in a scene, a bit array in Python. The source is split into
        data_len-bit frames, one chunk per frame, and its last chunk is padded
        from fill. For type=bits, bpsk/qpsk/pn framed, a dsss burst (one burst
        per frame), and continuous dsss (one bit per data symbol, no frame);
        not with data_from_file.
    data_len : int, default 0
        Bits of the data source per frame: the data:LEN of the common frame
        [preamble x reps | sync | data:LEN | crc]. 0 takes a finite source
        whole, as one frame. A carried frame names its own data field, and this
        is then 0 or that field's LEN. Continuous dsss has no frame, and
        refuses it.
    fill : bytes | None, default None
        The bits that pad a data source's last frame when it does not divide
        into data_len-bit frames, tiled from their first bit; stdin on a framed
        source always needs them. Without them such a source is refused before
        the first sample. A Field on the command line and in a scene, a bit
        array in Python. Continuous dsss has no frame to pad, and refuses it.
    fs : float, default 1.0
        Sample rate in Hz, one per segment and shared by all its sources. At
        the default 1.0 every frequency is normalised (cycles per sample);
        state it whenever a scene is in real Hz.
    """

    def __init__(
        self,
        type: str = ...,
        freq: float | tuple[float, float] = ...,
        snr: float | tuple[float, float] = ...,
        snr_mode: str = ...,
        seed: int = ...,
        sps: int = ...,
        pn_length: int = ...,
        pn_poly: int = ...,
        lfsr: str = ...,
        level: float | tuple[float, float] = ...,
        background: int = ...,
        f_end: float | tuple[float, float] = ...,
        span: int = ...,
        doppler: float | tuple[float, float] = ...,
        doppler_rate: float | tuple[float, float] = ...,
        carrier_hz: float = ...,
        doppler_lifetime: str = ...,
        modulation: str = ...,
        pulse: str = ...,
        rrc_beta: float = ...,
        rrc_span: int = ...,
        symbols: npt.NDArray[np.complex64] | None = ...,
        acq_code: bytes | None = ...,
        acq_reps: int = ...,
        data_code: bytes | None = ...,
        sync: bytes | None = ...,
        crc: bytes | None = ...,
        symbol_rate: float = ...,
        dsss_code_only: int = ...,
        frame: FrameDesc | str | None = ...,
        bits: bytes | None = ...,
        data: bytes | None = ...,
        data_len: int = ...,
        fill: bytes | None = ...,
        fs: float = ...,
    ) -> None: ...
    type: str
    freq: float | tuple[float, float]
    snr: float | tuple[float, float]
    snr_mode: str
    seed: int
    sps: int
    pn_length: int
    pn_poly: int
    lfsr: str
    level: float | tuple[float, float]
    background: int
    f_end: float | tuple[float, float]
    span: int
    doppler: float | tuple[float, float]
    doppler_rate: float | tuple[float, float]
    carrier_hz: float
    doppler_lifetime: str
    modulation: str
    pulse: str
    rrc_beta: float
    rrc_span: int
    symbols: npt.NDArray[np.complex64] | None
    acq_code: bytes | None
    acq_reps: int
    data_code: bytes | None
    sync: bytes | None
    crc: bytes | None
    symbol_rate: float
    dsss_code_only: int
    @property
    def frame(self) -> str | None: ...
    @frame.setter
    def frame(self, value: FrameDesc | str | None) -> None: ...
    bits: bytes | None
    data: bytes | None
    data_len: int
    fill: bytes | None
    fs: float
    def steps(self, n: int) -> NDArray[np.complex64]:
        """Generate the next *n* samples of this source on its own.

        The first call builds the generator from this configuration, through
        `dp_wfm_source_to_synth`; later calls continue it.

        Parameters
        ----------
        n : int
            How many samples; 0 returns an empty array.

        Returns
        -------
        NDArray[np.complex64]
            The samples, in order.

        Raises
        ------
        ValueError
            If `n` is negative. If `dp_wfm_source_to_synth` refuses this
            configuration; the message is its reason.
        RuntimeError
            If `dp_wfm_source_to_synth` fails and gives no reason.
        """
    def step(self) -> complex:
        """Generate the next sample of this source on its own.

        The first call builds the generator from this configuration, through
        `dp_wfm_source_to_synth`; later calls continue it.

        Returns
        -------
        complex
            The sample.

        Raises
        ------
        ValueError
            If `dp_wfm_source_to_synth` refuses this configuration; the message
            is its reason.
        RuntimeError
            If `dp_wfm_source_to_synth` fails and gives no reason.
        """
    def reset(self) -> None:
        """Rewind the generator to sample 0.

        A no-op before the first `steps()`/`step()`, which starts from sample 0
        anyway.
        """

@disjoint_base
class Segment:
    """Segment.

    Parameters
    ----------
    type : str, default ``"tone"``
        Waveform type. tone: complex sinusoid. noise: AWGN. pn: PN sequence
        (LFSR). bpsk/qpsk: PN-driven modulation. chirp: linear-FM sweep. bits:
        a caller's bit pattern, with selectable modulation. symbols: a caller's
        complex constellation stream. dsss: spread spectrum -- a two-code burst
        (repeated preamble + data-code-spread frame) by default, or a
        continuous asynchronous stream when symbol_rate is set.
        One of ``"tone"``, ``"noise"``, ``"pn"``, ``"bpsk"``, ``"qpsk"``,
        ``"chirp"``, ``"bits"``, ``"symbols"``, ``"dsss"``.
    freq : float | tuple[float, float], default 0.0
        Carrier or offset frequency in Hz; for chirp, the sweep start. With fs
        = 1 it is in normalised cycles per sample.
    snr : float | tuple[float, float], default 100.0
        Signal-to-noise ratio in dB, interpreted per snr_mode. 100 or more is
        clean: no AWGN is added.
    snr_mode : str, default ``"auto"``
        How snr is interpreted. auto: Es/N0 for bpsk/qpsk/dsss, and relative to
        full scale for tone/noise/pn/chirp/bits/symbols -- bits included,
        because a bits frame has no symbol rate the engine can infer. fs: dB
        relative to full scale. ebno: Eb/N0, per bit. esno: Es/N0, per symbol;
        for a dsss burst the outer data symbol of len(data_code) chips x sps
        samples, for a continuous dsss stream the fs/symbol_rate samples the
        async symbol spans.
        One of ``"auto"``, ``"fs"``, ``"ebno"``, ``"esno"``.
    seed : int, default 0
        PRNG and LFSR seed for the noise and PN streams. Deterministic: vary it
        for run-to-run change. For a PN-sourced type it starts the
        pn_length-bit register, masked: a seed whose low pn_length bits are
        zero starts the register at 1, as seed 0 does.
    sps : int, default 1
        Samples per symbol (PSK) or per chip (PN): the oversampling factor.
        Unused by noise, which records it as 0.
    pn_length : int, default 15
        PN LFSR register length in bits, 2 to 64 for the PN-bearing types; the
        sequence period is 2^pn_length - 1. Unused by noise, which records it
        as 0.
    pn_poly : int, default 0
        PN generator polynomial, in the Galois bit-vector convention; 0 selects
        a maximal-length (MLS) polynomial for pn_length. It must fit the
        pn_length-bit register: a bit at or above it is refused, not masked. A
        polynomial above 2^53 does not survive a JSON number, so a scene file
        needs 0 (auto) for such a register.
    lfsr : str, default ``"galois"``
        PN LFSR realisation. Both give the same period; fibonacci's chips are
        galois's in reverse order.
        One of ``"galois"``, ``"fibonacci"``.
    level : float | tuple[float, float], default 0.0
        Source power in dBFS (<= 0; 0 is unit power). Applies when summed in a
        Segment or Composer, as a gain of 10^(level/20); a standalone
        Synth.steps() ignores it.
    background : int, default 0
        Mark this source as part of the static background field (0/1).
        Plan.prepare() folds a contiguous leading run of background sources
        into ONE pre-summed cache entry instead of caching each separately, so
        a scene of many fixed emitters costs one buffer rather than hundreds.
        The composite is overridable as a unit: it takes a single slot in
        gains/phases/enable and counts as one in n_sources(), so scaling it
        trims the whole field while its members keep their relative levels.
        Background sources must come first in the segment (a non-prefix
        ordering is rejected by prepare, since the fold would no longer
        reproduce compose bit-for-bit). Ignored by compose() and by standalone
        Synth.steps().
    f_end : float | tuple[float, float], default 0.0
        Chirp end frequency in Hz; ignored by other types.
    span : int, default 0
        Chirp sweep length in samples: the frequency ramps from freq to f_end
        over this many samples, then holds at f_end. 0 means the enclosing
        Segment's num_samples. A standalone chirp (f_end != freq) must declare
        it, so step(), steps(N) and any chunking of reads produce the same
        waveform; generating one without it raises. Ignored by non-chirp types.
    doppler : float | tuple[float, float], default 0.0
        Clock Doppler in ppm: the received time base is rescaled by 1 +
        doppler*1e-6, so the symbol and chip rates move with the carrier and a
        timing loop sees the error a carrier-only `freq` offset hides. Accepts
        a (lo, hi) tuple drawn uniformly per repeat, like freq/snr. Zero
        doppler AND zero doppler_rate means no channel is built at all, so a
        source that does not ask for Doppler renders exactly as it always did.
    doppler_rate : float | tuple[float, float], default 0.0
        Linear ramp on `doppler`, in ppm per second of elapsed stream time. The
        channel runs through a segment's gaps as well as its on-time -- an
        emitter does not stop moving because its burst ended -- so this is per
        second, not per second of on-time. Accepts a (lo, hi) tuple drawn
        uniformly per repeat.
    carrier_hz : float, default 0.0
        RF carrier in Hz that the ppm figures are referred to. It gives the
        coherent carrier rotation that accompanies the time-base warp; 0 warps
        the clock alone, with no carrier rotation -- a legitimate scene, not an
        unset field. Independent of doppler/doppler_rate.
    doppler_lifetime : str, default ``"per_instance"``
        How long this source's Doppler channel lives. per_instance: the channel
        dies with each `repeats` instance, so the geometry restarts -- the
        repeated-trial shape, which composes with a ranged doppler re-drawn per
        instance. persist: one continuous pass carries across the segment's
        gaps and repeat instances, keyed by (segment, source) position -- the
        only lifetime under which doppler_rate accumulates across a multi-burst
        scene. Plan.prepare() REFUSES a persist source, because its cache
        renders each source independently and concurrently; compose() and
        stream() honour both.
        One of ``"per_instance"``, ``"persist"``.
    modulation : str, default ``"bpsk"``
        Symbol mapping of a bits pattern. none: the pattern shaped and output
        as-is (NRZ). bpsk: +/-1 symbols. qpsk: Gray-coded symbols from pairs of
        bits.
        One of ``"none"``, ``"bpsk"``, ``"qpsk"``.
    pulse : str, default ``"rect"``
        Pulse shape per symbol or chip, for pn/bpsk/qpsk/bits/symbols/dsss.
        rect: rectangular, no ISI filtering. rrc: root-raised cosine; see
        rrc_beta and rrc_span.
        One of ``"rect"``, ``"rrc"``.
    rrc_beta : float, default 0.35
        RRC roll-off factor, in (0, 1], when pulse=rrc.
    rrc_span : int, default 8
        RRC filter support in symbols when pulse=rrc, ONE-SIDED: the filter has
        2*rrc_span*sps + 1 taps, unit energy (sum of h^2 = 1).
    symbols : npt.NDArray[np.complex64] | None, default None
        For type=symbols: a complex constellation stream. Each element is the
        output point itself, oversampled by sps, cycled, and RRC-shaped with
        pulse=rrc, which generalises any modulation (pi/4-QPSK, QAM, ...).
    acq_code : bytes | None, default None
        The preamble code, sent acq_reps times at the head of the frame (a
        Field's *REPS on the command line and in a scene) -- the coherent
        pull-in target BurstDespreader.set_acq and BurstDemod.set_preamble lock
        to. For type=dsss it is unmodulated chips ahead of the spread frame;
        for type=bits it is the head of the bit pattern. Setting it is what
        makes a source FRAMED.
    acq_reps : int, default 1
        Preamble repetitions: periods of acq_code before the sync word. On the
        command line and in a scene it is acq_code's *REPS.
    data_code : bytes | None, default None
        For type=dsss: the payload spreading code, a second code distinct from
        acq_code. Every frame bit (sync, payload, crc) is XOR-spread across its
        full length, so len(data_code) is the spreading factor.
    sync : bytes | None, default None
        RETIRED (doppler#1617): nothing reads it but the refusal. The
        frame-sync word is a field of the frame DESCRIPTION, so sync= is
        refused naming frame=; the CLI's --sync builds that description for
        you, and a scene refuses the "sync" key.
    crc : bytes | None, default None
        RETIRED (doppler#1617): nothing reads it but the refusal. A CRC is a
        stage of the frame DESCRIPTION, so crc= is refused naming frame=,
        whatever its value (crc="none" included); the CLI's --crc builds that
        description for you, and a scene refuses the "crc" key.
    symbol_rate : float, default 0.0
        For type=dsss: > 0 selects CONTINUOUS asynchronous mode. The spreading
        code repeats endlessly and data rides on it at this symbol rate (Hz),
        independent of the code-epoch rate (chips/symbol = fs/sps/symbol_rate,
        non-integer). No preamble/sync/CRC frame; data comes from the payload
        when supplied, else a seeded PN a receiver regenerates. Absent/0 =
        burst.
    dsss_code_only : int, default 0
        Continuous dsss data source: 1 = code-only (the pure spreading code, no
        data modulation); 0 = data-modulated (the payload when supplied, else
        the seeded PN). Ignored for burst dsss and non-dsss types.
    frame : FrameDesc | str | None, default None
        A frame DESCRIPTION, the whole frame: fields in wire order, and stages
        that each name the span they cover (crc16, rs, randomise, interleave,
        conv, or a kind of your own). It is the only way a source says anything
        but the common frame: a coding stage, a field of the caller's own bits
        at a position of their choosing, a stage covering a span they name.
        `wfmgen --frame FILE`, a scene's `frame` key and Python's `frame=` (a
        FrameDesc or a Frame) all land here. When it is set it IS the frame,
        and an unspread acq_code beside it is refused. NULL means the common
        frame, `[preamble x reps | data]`, which `dp_wfm_frame_fixed()` builds
        from acq_code and the data source; a sync word or a CRC is a field or a
        stage of a description, never a flat field. A C caller's description is
        borrowed, exactly as `wfm_seq_t` is borrowed elsewhere here, so it must
        outlive the source. The composer and a Python source hold their own
        copy (`dp_wfm_frame_copy()`), so a later change to the FrameDesc does
        not reach them. On the Python face `frame=` is an input: read it back
        from the composer's JSON (its getter is jm's, pending removal:
        doppler#1694). KERNELS stay in C by design. A description names a
        stage's KIND; the code that runs it is a `wfm_frame_ops_t` entry, and a
        caller adding a genuinely new transform (convolutional interleaving,
        say) writes that kernel in C and hands it to `dp_wfm_frame_assemble`
        directly.
    bits : bytes | None, default None
        RETIRED (doppler#1718): nothing reads it but the refusal. A payload is
        drawn from a data source, so bits=, and its aliases payload= and
        pattern=, are refused naming data=; the CLI and a scene refuse --bits
        and "payload" the same way.
    data : bytes | None, default None
        A frame's payload drawn from a data source: a Field on the command line
        and in a scene, a bit array in Python. The source is split into
        data_len-bit frames, one chunk per frame, and its last chunk is padded
        from fill. For type=bits, bpsk/qpsk/pn framed, a dsss burst (one burst
        per frame), and continuous dsss (one bit per data symbol, no frame);
        not with data_from_file.
    data_len : int, default 0
        Bits of the data source per frame: the data:LEN of the common frame
        [preamble x reps | sync | data:LEN | crc]. 0 takes a finite source
        whole, as one frame. A carried frame names its own data field, and this
        is then 0 or that field's LEN. Continuous dsss has no frame, and
        refuses it.
    fill : bytes | None, default None
        The bits that pad a data source's last frame when it does not divide
        into data_len-bit frames, tiled from their first bit; stdin on a framed
        source always needs them. Without them such a source is refused before
        the first sample. A Field on the command line and in a scene, a bit
        array in Python. Continuous dsss has no frame to pad, and refuses it.
    fs : float, default 1.0
        Sample rate in Hz, one per segment and shared by all its sources. At
        the default 1.0 every frequency is normalised (cycles per sample);
        state it whenever a scene is in real Hz.
    num_samples : int | tuple[int, int], default 0
        Segment on-time in samples, before the trailing gap: 0 derives it from
        the sources, or 1024 when they set none. A finite data source sets its
        frames, a lone dsss burst one burst, and a stream runs to its end. A
        count beside a finite data source or a lone dsss burst is refused,
        since they set the length; give repeats for more.
    off_samples : int | tuple[int, int], default 0
        Trailing gap after the on-time, in samples. It carries the noise floor
        or hard zeros, per gap_noise.
    repeats : int, default 1
        Play the segment this many times back-to-back (each instance = delay +
        on-time + trailing gap) before advancing. Ranged fields re-draw and the
        AWGN is fresh per instance; the signal (codes, payload, PN phase) stays
        fixed. 0 and 1 both mean one instance.
    delay_samples : int | tuple[int, int], default 0
        Leading gap before the on-time, in samples: the burst arrives after
        this delay, and the gap carries the noise floor like off_samples.
        Ranged like off_samples and re-drawn per repeats instance, so a (lo,
        hi) delay is per-burst arrival jitter. Use off_samples for inter-burst
        spacing, delay_samples for arrival jitter.
    gap_noise : str, default ``"auto"``
        Gap policy for this segment's delay and trailing gap. auto: gaps carry
        the segment's noise floor -- the sources' AWGN keeps running while the
        signal stops (clean scenes still get exact-zero gaps). off: gaps are
        hard zeros.
        One of ``"auto"``, ``"off"``.
    """

    sources: list[Synth]
    fs: float
    num_samples: int | tuple[int, int]
    off_samples: int | tuple[int, int]
    repeats: int
    delay_samples: int | tuple[int, int]
    gap_noise: str
    type: str
    freq: float
    snr: float
    snr_mode: str
    seed: int
    sps: int
    pn_length: int
    pn_poly: int
    lfsr: str
    level: float
    background: int
    f_end: float
    span: int
    doppler: float
    doppler_rate: float
    carrier_hz: float
    doppler_lifetime: str
    modulation: str
    pulse: str
    rrc_beta: float
    rrc_span: int
    symbols: npt.NDArray[np.complex64] | None
    acq_code: bytes | None
    acq_reps: int
    data_code: bytes | None
    sync: bytes | None
    crc: bytes | None
    symbol_rate: float
    dsss_code_only: int
    frame: str | None
    bits: bytes | None
    data: bytes | None
    data_len: int
    fill: bytes | None
    def __init__(
        self,
        type: str = ...,
        freq: float | tuple[float, float] = ...,
        snr: float | tuple[float, float] = ...,
        snr_mode: str = ...,
        seed: int = ...,
        sps: int = ...,
        pn_length: int = ...,
        pn_poly: int = ...,
        lfsr: str = ...,
        level: float | tuple[float, float] = ...,
        background: int = ...,
        f_end: float | tuple[float, float] = ...,
        span: int = ...,
        doppler: float | tuple[float, float] = ...,
        doppler_rate: float | tuple[float, float] = ...,
        carrier_hz: float = ...,
        doppler_lifetime: str = ...,
        modulation: str = ...,
        pulse: str = ...,
        rrc_beta: float = ...,
        rrc_span: int = ...,
        symbols: npt.NDArray[np.complex64] | None = ...,
        acq_code: bytes | None = ...,
        acq_reps: int = ...,
        data_code: bytes | None = ...,
        sync: bytes | None = ...,
        crc: bytes | None = ...,
        symbol_rate: float = ...,
        dsss_code_only: int = ...,
        frame: FrameDesc | str | None = ...,
        bits: bytes | None = ...,
        data: bytes | None = ...,
        data_len: int = ...,
        fill: bytes | None = ...,
        fs: float = ...,
        num_samples: int | tuple[int, int] = ...,
        off_samples: int | tuple[int, int] = ...,
        repeats: int = ...,
        delay_samples: int | tuple[int, int] = ...,
        gap_noise: str = ...,
    ) -> None: ...
    @classmethod
    def sum(
        cls,
        *sources: Synth,
        fs: float = ...,
        num_samples: int | tuple[int, int] = ...,
        off_samples: int | tuple[int, int] = ...,
        repeats: int = ...,
        delay_samples: int | tuple[int, int] = ...,
        gap_noise: str = ...,
    ) -> Segment:
        """Combine *sources* into a single Segment."""
    def add(self, *others: Segment) -> Timeline:
        """Append segments; return a Timeline."""

@disjoint_base
class Timeline:
    """Timeline."""

    segments: list[Segment]
    def __init__(self, segments: list[Segment]) -> None: ...
    def add(self, *segments: Segment) -> Timeline:
        """Append and return self."""
    def __iter__(self) -> Iterator[Segment]: ...
    def __len__(self) -> int: ...
    def __getitem__(self, i: int, /) -> Segment: ...

@disjoint_base
class Composer:
    """Composer.

    Parameters
    ----------
    segments : Segment | Timeline | list[Segment] | None, default None
        Initial segment list.
    repeat : bool, default False
        Loop the sequence after the last segment.
    continuous : bool, default False
        Never finish; execute always returns the requested count.
    """

    segments: list[Segment]
    repeat: bool
    continuous: bool
    def __init__(
        self,
        segments: Segment | Timeline | list[Segment] | None = ...,
        *,
        repeat: bool = ...,
        continuous: bool = ...,
        **segment_kwargs,
    ) -> None: ...
    def execute(self, n: int) -> NDArray[np.complex64]:
        """Execute for *n* samples."""
    def compose(self, block: int = ...) -> NDArray[np.complex64]:
        """Compose the full sequence into one array."""
    def stream(
        self,
        block: int = ...,
        realtime: float = ...,
    ) -> Iterator[NDArray[np.complex64]]:
        """Iterate the sequence in blocks.

        Parameters
        ----------
        block : int
            Samples per yielded array.
        realtime : float
            Sample rate in **Hz** to pace against — pass
            your ``fs`` (e.g. ``realtime=10e6``), not a
            speed multiplier. ``0`` (the default) streams
            as fast as possible.
        """
    def to_dict(self) -> dict:
        """Serialise the composer state to a dict."""
    def _draws_json(self) -> str:
        """Serialise as _draws_json."""
    def to_sigmf(
        self,
        sample_type: str = ...,
        endian: str = ...,
        fs: float = ...,
        fc: float = ...,
        t0: float = ...,
    ) -> str:
        """Serialise as to_sigmf."""
    def close(self) -> None:
        """Release native resources."""
    def __enter__(self) -> Composer: ...
    def __exit__(self, *exc) -> None: ...
    @classmethod
    def from_json(cls, json: str) -> Composer: ...
    @classmethod
    def from_file(cls, path: str) -> Composer: ...
    def to_json(self) -> str: ...

def tone(**kw: Any) -> Synth:
    """Return a Synth configured as a *tone* source."""
def noise(**kw: Any) -> Synth:
    """Return a Synth configured as a *noise* source."""
def pn(**kw: Any) -> Synth:
    """Return a Synth configured as a *pn* source."""
def bpsk(**kw: Any) -> Synth:
    """Return a Synth configured as a *bpsk* source."""
def qpsk(**kw: Any) -> Synth:
    """Return a Synth configured as a *qpsk* source."""
def chirp(**kw: Any) -> Synth:
    """Return a Synth configured as a *chirp* source."""
def bits(**kw: Any) -> Synth:
    """Return a Synth configured as a *bits* source."""
