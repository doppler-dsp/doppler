- **The Doppler-rate validator checks a PN preamble too.** Beside Zadoff-Chu
    and LFM, `validate_acq_rate_pd` repeats its capture claims on one period
    of a 127-chip m-sequence (`dp_mls_poly(7)`) from the shipped generator:
    the capture delivers its `pd_burst`, compensation beats the capped depth
    3 dB under the design point, and Pfa holds (#2219).
