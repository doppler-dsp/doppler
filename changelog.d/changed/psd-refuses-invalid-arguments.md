- **PSD, AccTrace and `obw_from_power` refuse what read wrong** (#1911): a
    zero-gain or non-finite window, a non-finite `fs`/`full_scale`, `pad = 0`,
    `bits > 64`, an oversized `n * pad`, an exp `alpha` outside
    `0 < alpha <= 1`. OBW is NaN outside `0 < frac < 1` and exact at a
    bin-boundary tie (one bin wider); `band_power()` may return `None`.
