# CarrierAcquisition: RRC Pulse Shaping

![Measured spectrum vs. matched RRC template, and estimate error at two Es/N0 points](../assets/carrier_acq_rrc_demo.png)

[`CarrierAcquisition`](../api/python-acquire.md) is a PSDMF (power-spectral-density
matched-filter) residual-carrier estimator: it non-coherently averages the
incoming stream's power spectrum ([`PSD`](corr.md)), then circularly
correlates that average against a *known* power spectrum shape to find the
residual carrier offset. It runs as a one-shot refinement stage after
`Acquisition`'s own coarse Doppler search — see
[DSSS Acquisition: Pd/Pfa](dsss-acq-characterization.md) for that stage.

## The template is a property of the pulse shape, not a universal constant

The default known shape (`psd_template` left empty) is the average PSD of a
random rectangular-pulse (plain NRZ) BPSK stream — a sinc². An RRC
(root-raised-cosine) pulse-shaped stream's average PSD is a **different**
shape entirely: the squared magnitude of the RRC filter's own frequency
response, a raised-cosine roll-off with no sidelobes past `(1+beta)/(2*sps)`
of the symbol rate. `psd_template` exists precisely so a caller running a
different pulse shape or modulation can supply the *correct* known shape —
mirroring `~/legacy-commz`'s own `FrequencyAcquisition.power_spectrum`
override, the reference this object's design descends from.

## How it works

```python
--8<-- "src/doppler/examples/carrier_acq_rrc_demo.py:signal"
```

```python
--8<-- "src/doppler/examples/carrier_acq_rrc_demo.py:templates"
```

```python
--8<-- "src/doppler/examples/carrier_acq_rrc_demo.py:compare"
```

A linear filter applied to a white bipolar sequence has average PSD
proportional to `|H(f)|^2` — the direct RRC analogue of the default
template's rectangular-pulse sinc². [`doppler.wfm.rrc_taps`](wfm-io.md)
already generates the filter; the template is just its own zero-padded,
DC-centred squared frequency response.

## What you're seeing

Same RRC-shaped BPSK stream, same true 137 Hz residual, two Es/N0 points:

**Left — measured power spectrum vs. the matched RRC template**, at 5 dB
Es/N0. The measured spectrum's own roll-off (blue) lines up with the
template's shape (dashed green) — this is the shape the correlation is
actually matching against, not an arbitrary constant.

**Right — estimate error, wrong vs. matched template, at two Es/N0
points**. At 10 dB both templates land within a few Hz of the true
residual — at generous margin, template shape barely matters. At 5 dB the
**default (rectangular-pulse) template never confidently detects at all**
— the CFAR gate never fires — while the **matched RRC template still lands
within a few Hz**. The wrong shape doesn't just cost precision; it can cost
the detection outright.

## A known open question, not swept under the rug

`CarrierAcquisition`'s detection gate reuses `doppler.detection`'s
`det_threshold_noncoherent`/`det_n_noncoh` — the same Pfa/Pd statistics
`Acquisition` itself is built on, derived for classic complex-correlator
(Rayleigh/Rician) detection. Whether those statistics transfer cleanly to
gating a *power-spectrum-vs-template* correlation (a different,
non-negative, non-Gaussian regime) hasn't been verified — this page's own
`design_snr`/`sequential=False` choice was picked empirically to get a
converged, multi-look comparison, not derived from first principles. See
`FINISHING_PLAN.md`'s own `CarrierAcquisition` section for the open item.

Source: `src/doppler/examples/carrier_acq_rrc_demo.py`. See also
[Correlation](corr.md) (the `PSD`/`Corr`/`CorrDetector` primitives this
object composes), [2-D Acquisition](detection2d.md) (the same Pfa/Pd
statistics applied to `Acquisition`'s own code-phase × Doppler search), and
[wfm I/O](wfm-io.md) (`rrc_taps` and pulse shaping in general).
