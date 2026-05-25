# Gallery

Output plots from the example scripts in `examples/python/`.  Run `make gallery` to regenerate all images.

---

## AGC step response

![AGC convergence](assets/agc_convergence.png)

20 dB power step tracked within ~350 samples at `loop_bw = 0.00125`.  All three decimation settings converge identically — only the loop tick rate differs.

```bash
python examples/python/agc_demo.py
```

---

## CIC decimation filter

![CIC decimation spectrum](assets/cic_demo_spectrum.png)

Top: wideband input spectrum (2.048 Msps) with the CIC magnitude response overlaid.  The jammer at 600 kHz falls outside the output Nyquist boundary and is suppressed by 40+ dB.  Bottom: decimated output at 128 ksps — the 15 kHz wanted tone survives while the jammer alias is buried in the noise floor.

```bash
python examples/python/cic_demo.py
```

---

## Correlation and detection

![Corr / Corr2D / Detector demo](assets/corr_demo.png)

Left: coherent integration (`dwell=8`) lifts a BPSK PN signal out of the noise floor.  Centre: `Corr2D` locates a 2-D template shift in one FFT2 call.  Right: `Detector.push()` stream — signal dwells sit well above the threshold while noise-only dwells stay below.

```bash
python examples/python/corr_demo.py
```

---

## Detection theory curves

![Detection theory curves](assets/detection_curves.png)

Pd vs SNR and Pd vs dwell from the closed-form Marcum Q functions.  Every 3 dB of extra SNR roughly halves the required dwell to reach Pd = 0.9 at Pfa = 1e-5.

```bash
python examples/python/detection_curves.py
```

---

## Monte Carlo vs Marcum Q theory

![Monte Carlo vs theory](assets/detection_sim.png)

30 000 independent trials per SNR point.  Empirical survival functions (left) and Pd vs SNR (right) match the closed-form Marcum Q predictions throughout.

```bash
python examples/python/detection_sim.py
```

---

## 2-D acquisition grid

![2-D acquisition demo](assets/detection2d_demo.png)

GPS/CDMA-style acquisition: 16 × 16 Doppler × code-phase search grid evaluated in one FFT2 call.  Bonferroni correction applied across all 256 cells.  Left: acquisition surface.  Centre: Pd vs dwell theory + MC.  Right: ROC curve.

```bash
python examples/python/detection2d_demo.py
```
