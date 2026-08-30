- **All three wfmgen faces are asserted bit-identical again.** The CLI was
    compared to a float32 tolerance while [#1003](https://github.com/doppler-dsp/doppler/issues/1003)
    stood — up to 32 ULP once a scene carried noise. It does not reproduce:
    bit-identical at the commit the issue was filed against, at every commit
    since, and with the CLI built `-O2 -fno-fast-math` against an
    `-O3 -ffast-math` extension, which was the obvious mechanism and is not
    the one. The strong claim is back, so a divergence anywhere reports
    itself rather than being tolerated.
