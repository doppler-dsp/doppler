- **Three spellings changed with the merge.** A search's `fs` is
    `sample_rate`; a map's `n_out` is `execute_max_out()` (`nx_out` on the
    1-D flavor, whose padding knob it also is); and `DelayDoppler`'s
    refusal of `ny=0` has no spelling, since `doppler_rows=0` is "no map"
    and builds the 1-D flavor
    ([design §17, §18](docs/design/correlator-python.md)).
