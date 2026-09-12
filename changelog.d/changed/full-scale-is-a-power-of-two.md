- **A wire format's full scale is `2^(N-1)`, from `dp_format_full_scale()`
    alone.** It was `2^(N-1)-1`, restated in four files. The power of two is
    the grid a converter actually has, so dyadic values round-trip exactly and
    `dp_mean_power()` reports true dBFS; `+1.0` now saturates to the type's
    maximum by construction, which the writer's `clipped` reports honestly
    ([#1117](https://github.com/doppler-dsp/doppler/issues/1117)).
    `make lint-full-scale` fails on a reintroduced private table.
