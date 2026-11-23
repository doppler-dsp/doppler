- **`AsyncDsssReceiver` refuses a hand-over whose start is older than its
    hand-over ring holds.** It used to start the live chain at the oldest
    sample the ring held instead. That condition came only from the refine
    dropping a code period of partials (#3281); with every partial kept,
    none of 1,089 noise geometries reaches it. If it does occur, the hand-over is
    refused like a hit outside the domain: counted in `refused_hits`, with the
    search restarted on the oldest sample held.
