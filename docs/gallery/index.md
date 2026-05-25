# Gallery

Output plots from the example scripts in `examples/python/`.
Run `make gallery` to regenerate all images.

<div class="grid cards" markdown>

-   **[AGC — Step Response](agc.md)**

    ---

    [![AGC convergence](../assets/agc_convergence.png)](agc.md)

    20 dB power step tracked within ~350 samples.  All three
    decimation settings converge identically.

    [:octicons-arrow-right-24: Walkthrough](agc.md)

-   **[CIC Decimation Filter](cic.md)**

    ---

    [![CIC decimation spectrum](../assets/cic_demo_spectrum.png)](cic.md)

    Wideband IQ → CIC → narrowband slice.  Jammer alias visible at
    48 kHz, ~71 dB below the wanted tone.

    [:octicons-arrow-right-24: Walkthrough](cic.md)

-   **[Correlation and Detection](corr.md)**

    ---

    [![Corr / Corr2D / Detector demo](../assets/corr_demo.png)](corr.md)

    Coherent integration, 2-D template matching, and streaming
    CFAR detection in one run.

    [:octicons-arrow-right-24: Walkthrough](corr.md)

-   **[Detection Theory Curves](detection-curves.md)**

    ---

    [![Detection theory curves](../assets/detection_curves.png)](detection-curves.md)

    Pd vs SNR and Pd vs dwell from closed-form Marcum Q.  Every
    3 dB SNR halves the required dwell.

    [:octicons-arrow-right-24: Walkthrough](detection-curves.md)

-   **[Monte Carlo vs Marcum Q](detection-sim.md)**

    ---

    [![Monte Carlo vs theory](../assets/detection_sim.png)](detection-sim.md)

    30 000 trials per SNR point.  Empirical survival functions and
    Pd vs SNR match theory throughout.

    [:octicons-arrow-right-24: Walkthrough](detection-sim.md)

-   **[2-D Acquisition Grid](detection2d.md)**

    ---

    [![2-D acquisition demo](../assets/detection2d_demo.png)](detection2d.md)

    GPS/CDMA-style 16 × 16 Doppler × code-phase search in one
    FFT2 call.  Bonferroni-corrected CFAR.

    [:octicons-arrow-right-24: Walkthrough](detection2d.md)

</div>
