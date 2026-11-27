- **The BER meter no longer refuses a perfect link at its alignment.** The
    test harness's marker placement (`dp_ber_marker_place()`) accepted a
    stretch of truth whose noiseless replay aligned at lag 0, and a stretch
    next to a code-only run could then refuse a link with every decision
    right (#3131: 256 of 256, 0.63 dB short). It now requires the sync to
    clear at every true lag the search allows, with the received symbols'
    measured noise and the peak at the sync's own false-alarm quantile, and
    takes the received symbols as an argument. Over a 415.1-B-1 frame at 10
    dB the old placement lost 6 starts in 1,000, the new one none.
