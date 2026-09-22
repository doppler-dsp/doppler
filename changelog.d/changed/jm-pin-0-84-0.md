- **just-makeit pin 0.83.0 → 0.84.0.** No generated file changes. It
    brings `fragment = "generated"`, which lets a binding fragment become
    fully jm-owned
    ([#1446](https://github.com/doppler-dsp/doppler/issues/1446)). A first
    render is refused while the fragment still holds hand-written code:
    checked on `wfm_writer` and `pn`, where `apply` refused and wrote
    nothing. `jm apply`'s false `out=` contiguity warnings are gone: 76
    warnings on 0.83.0, 36 now, and the 2 contiguity warnings left are
    real (`HalfbandDecimator`, `hbdecim_q15`).
