- **The release validator's `cross-on` row watches the crossing.** Its
    watch opened when symbol lock settled, which is after the crossing on
    29 of 30 seeds, so #2495's Dll level moved the window rather than the
    receiver. The row now starts at T_OFF_S with the neighbour 1.9 chips
    away; code lock holds 20 vs 21 of 30 at the slow level against the old
    divisor (#2513, record §12.29).
