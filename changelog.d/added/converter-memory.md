- **`dp_RateConverter_get_memory()`: when a converter's output stops
    depending on how it started.** The number of inputs after create or
    reset until an output carries nothing but the input, summed from each
    stage, so a caller that measures the output knows how many outputs to
    skip, including a CIC plan's startup burst
    ([#3365](https://github.com/doppler-dsp/doppler/issues/3365)).
