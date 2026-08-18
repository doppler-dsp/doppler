- **The lock thresholds a decision actually used are readable.**
    `MpskReceiver` and its views gain `lock_drop_thresh`, `sync_lock_thresh`
    and `sync_lock_drop_thresh`, joining the `lock_thresh` that was already
    there. Anything reading a lock statistic against its decision needs both
    edges of the pair, and the carrier drop level (`0.8 ×` declare) and the
    timing loop's levels were previously reachable only by retyping them —
    a second copy of a rule the object owns, free to drift from the decision
    it claims to describe.

    The timing pair reading `0.311 / 0.311` is information rather than a
    defect: the timing loop carries no *level* hysteresis, its hysteresis
    living in the verify counts instead. Its threshold is also not the
    carrier's number and is not derived the same way — symsync sizes block
    length and threshold together from (rolloff, esno_min, pfa, pd).
