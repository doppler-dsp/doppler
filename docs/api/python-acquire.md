# Python Acquire API

The `doppler.acquire` module provides **`CarrierAcquisition`** — a PSDMF
(power-spectral-density matched-filter) residual-carrier frequency
estimator. It runs after `doppler.dsss.Acquisition`'s own coarse Doppler
search as a one-shot refinement stage: non-coherently average the
incoming stream's power spectrum, then circularly correlate that average
against a known power spectrum shape (the default is the average PSD of
a random rectangular-pulse BPSK stream, a sinc²; `psd_template` overrides
it for a different pulse shape or modulation) to find the residual
carrier offset.

Source:
[`src/doppler/acquire/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/acquire/__init__.py)

See the [CarrierAcquisition: RRC Pulse Shaping gallery page](../gallery/carrier-acq-rrc.md)
for a worked example showing why the template matters.

______________________________________________________________________

## `CarrierAcquisition` — PSDMF residual-carrier estimation

Composes `doppler.spectral.PSD` (FFT + window + non-coherent power
averaging), `doppler.spectral.CorrDetector` (FFT-based correlation of the
averaged power against the known template, plus a noise-referenced test
statistic), and `doppler.detection`'s Pfa/Pd statistics (the same ones
`Acquisition` itself is built on) for the detection gate. `sequential`
(test every block, adaptive) vs. non-sequential (a fixed `dwell_target`
wait) mirror `~/legacy-commz`'s own `FrequencyAcquisition` reference —
`max_n_blocks` is sequential mode's own give-up cap, deliberately
independent of `dwell_target`.

::: doppler.acquire.CarrierAcquisition

## Related pages

<!-- related-pages:start -->

**Gallery** — [CarrierAcquisition: RRC Pulse Shaping](../gallery/carrier-acq-rrc.md), [Gallery](../gallery/index.md)
**Design** — [DsssReceiver Specifications](../design/async-dsss-spec.md)

<!-- related-pages:end -->
