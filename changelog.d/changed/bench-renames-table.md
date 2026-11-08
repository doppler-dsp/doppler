- **A renamed benchmark row keeps its history.** `benchmarks/renames.toml`
    maps an old row family to the one it became, `c:acq` to
    `c:correlator` for one. The benchmarks page joins the two histories,
    the label gate wants one label and one exempt line per row under its
    new ID, and `make bench-ab` pairs A's old rows with B's new ones from
    the same table. `make bench-ab`'s `ALIAS=` is gone, and it keys every
    row by the page's row ID, so moving a bench file to another module no
    longer unpairs it. The step-3 correlator renames are the first
    entries: the page's 58 correlator rows now carry the history their old
    IDs recorded ([#2900](https://github.com/doppler-dsp/doppler/issues/2900)).
