- **`StateFilter`: the header and design page say what the exact stability
    test does and what the tests run.** The test decides the decay of one sum,
    not a Cholesky pivot; the verified range is `bn*t` 1e-6 to the bound at one
    delay and gain, plus a simulation at eighteen delay-and-gain points; the
    `K_MIN` source is a requirement until the receiver's slope rows land
    ([#3135](https://github.com/doppler-dsp/doppler/issues/3135)).
