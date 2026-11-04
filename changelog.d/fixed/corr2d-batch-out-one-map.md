- **`execute`, `execute_coherent` and `execute_power_sum` take an `out=` of
    exactly the result.** They asked for as many elements as the input had, so
    an `out=` sized by `*_max_out()` was refused. See [#2818](https://github.com/doppler-dsp/doppler/issues/2818).
