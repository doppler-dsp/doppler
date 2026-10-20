- **`AsyncDsssReceiver` takes a ranked, filtered peak, so searchers on one
    stream split the signals.** `set_rank(r)` takes the r-th strongest peak
    of a dwell and `set_exclusions(phases)` drops code phases other
    receivers already hold; peaks within a chip of a stronger one count
    once. Two searchers at ranks 0 and 1 seed on two emitters in the same
    dwell. Rank 0 with no exclusions picks as before. A search that goes on
    after a refused or unranked dwell no longer re-feeds the engine's carry
    (the state blob is v13).
