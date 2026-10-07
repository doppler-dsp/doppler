- **just-makeit pin 0.98.3 → 0.99.0.** It carries `elements_per_sample` on a
    `variable_output` method (gh-1996), `out=` and `<m>_max_out()` for an
    array beside other params (gh-1998), `extra_methods` (gh-1997) and `rank`
    on a constructor array (gh-2004), all filed from #1446. Seven methods jm
    owns now take a keyword `out=` buffer and have a `<m>_max_out()` sizer:
    `DopplerChannel.execute_profile` and `execute_ctrl` on `DDC`,
    `MatchedDDC`, `Ddcr`, `MatchedDdcr`, `RateConverter`,
    `MatchedRateConverter`. `Resampler.execute_ctrl` and `Farrow.delay` already
    accepted `out=`; only their stubs caught up.
