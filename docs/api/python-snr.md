# Python SNR API

The `doppler.snr` module provides stateless SNR / Es-N0 estimators over a
block of complex baseband samples, shared across any receiver that needs
one rather than each object growing its own ad hoc formula. Two
independent algorithms, each with a sliding-window `_series` sibling for
visualizing drift vs time/index instead of reading one block-average
scalar:

- **`snr_data_aided_db`** — known-symbol estimator. Strip the known
    transmitted sign, then `Es/N0 = (mean signal amplitude)^2 / (mean residual power)`. Needs ground truth (or trusted decisions), but is
    simple, unbiased, scale-invariant, and polarity-invariant (a global sign
    flip changes nothing, since the amplitude is squared).
- **`snr_m2m4_db`** — non-data-aided (blind), moment-based estimator
    (Pauluzzi & Beaulieu, "A comparison of SNR estimation techniques for the
    AWGN channel", IEEE Trans. Commun. 48(10), 2000) for a constant-modulus
    signal (BPSK/QPSK/M-PSK) in circular complex AWGN. No known symbols
    needed at all.

Source:
[`src/doppler/snr/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/snr/__init__.py)

See the
[5-Burst DSSS Link gallery page](../gallery/dsss-burst-pipeline.md#esn0-db-not-snr_est-now-a-standalone-dopplersnr-module)
for both estimators used against a real despread burst, including the
`_series` sliding-window plot.

```pycon
>>> import numpy as np
>>> from doppler.snr import snr_data_aided_db, snr_m2m4_db
>>> rng = np.random.default_rng(0)
>>> bits = (rng.random(5000) > 0.5).astype(np.uint8)
>>> sign = np.where(bits, -1.0, 1.0).astype(np.complex64)
>>> noise = (0.3 * (rng.standard_normal(5000)
...          + 1j * rng.standard_normal(5000))).astype(np.complex64)
>>> soft = (sign + noise).astype(np.complex64)
>>> round(float(snr_data_aided_db(soft, bits)), 1)  # known symbols
7.5
>>> round(float(snr_m2m4_db(soft)), 1)  # blind -- agrees, no `bits` needed
7.4

```

______________________________________________________________________

## `snr_data_aided_db` — known-symbol Es/N0

::: doppler.snr.snr_data_aided_db

______________________________________________________________________

## `snr_m2m4_db` — blind (M2M4) Es/N0

::: doppler.snr.snr_m2m4_db

______________________________________________________________________

## `snr_data_aided_db_series` — sliding-window known-symbol Es/N0

::: doppler.snr.snr_data_aided_db_series

______________________________________________________________________

## `snr_m2m4_db_series` — sliding-window blind Es/N0

::: doppler.snr.snr_m2m4_db_series

## Related pages

<!-- related-pages:start -->

**Gallery** — [Async DSSS Receiver: the SPEC waveform through coupled Doppler](../gallery/async-dsss-receiver-spec.md), [A 5-Burst DSSS Link — wfmgen's Three Faces, the Full Receiver Chain](../gallery/dsss-burst-pipeline.md)

<!-- related-pages:end -->
