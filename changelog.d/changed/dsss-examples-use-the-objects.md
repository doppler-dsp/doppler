- **The DSSS examples measure with doppler's objects instead of hand-rolled
    numpy.** Four of them carried private reimplementations of kernels the
    library already ships, and two of those were producing the wrong answer.

    - **`dsss_burst_demo`'s repetition sweep is
        `BurstAcquisition`** — the object that actually integrates coherently
        across the preamble and applies the CFAR threshold. It was a hand
        sliding correlator matched to **one** code period no matter how many
        were transmitted, so it measured **+0.6 dB across an 8x change in
        `acq_reps`** while the docstring claimed +9 and the plot drew four
        identical curves under four separated theory lines
        ([#980](https://github.com/doppler-dsp/doppler/issues/980)). The panel
        is now Pd vs input SNR per `acq_reps`, and the threshold moves
        **2.26 / 2.50 / 3.12 dB per doubling** against the ideal 3.01. The
        shortfall is real and named rather than tuned away: more coherent depth
        is more search cells, so the Bonferroni threshold rises with it
        (3.98 → 4.30 across these arms).

        The old assertion could not have caught this — it compared each arm to
        a floor that one period of a 127-chip code clears on its own. The new
        one is a **ratio between arms**, which is what the claim was. Sabotage:
        pinning the coherent depth to 1 collapses the shifts to 0.25 / 1.22 /
        0.83 dB and it fires.

        Its spectrogram is `spectral.FFT` + `hann_window` +
        `magnitude_db_cf32` rather than `np.fft` + `np.hanning`, so it cannot
        disagree with the `PSD` panel beside it about what a Hann window is.

    - **`async_dsss_receiver_spec_demo`'s EVM is `ber.ber_evm_db`.** Its
        private `self_evm_db()` was a line-for-line twin of that kernel —
        self-referenced, rotation-from-data, no truth and no lag — and returns
        the identical −10.1 dB. Its AWGN level now comes from
        `wfm_awgn_amplitude` (reproducing the hand sigma to seven digits) and
        the draw from a `type="noise"` Synth.

    - **`dsss_receiver_demo`'s capture is one `Segment`.** The pre-signal
        silence is the segment's own `delay_samples` with `gap_noise="auto"`
        running the floor through it, so the hand-built zeros, the hand sigma
        and the second noise realisation are all gone.

- **The async DSSS example measures its acquisition rate instead of asserting
    one lucky draw.** Scored over eight fixed noise draws, the receiver
    acquires on **5 of 8** (TCA) and **6 of 8** (±50 kHz) at the Es/N0 the file
    documents as its reliable point — and decodes cleanly on every draw where
    it does. The single-seed assertion it used to make was a coin flip that
    happened to be green.

    Not caused by the noise-source change above: the same 6/8 comes out with
    the noise drawn by numpy or by doppler's own source, at an amplitude
    identical to seven significant figures. Nor is it pull-in time (the same
    seeds fail at `N_SYM = 6000`) or a mis-told operating point (the receiver
    is given the true C/N0). Why the rate is what it is:
    [#982](https://github.com/doppler-dsp/doppler/issues/982).
