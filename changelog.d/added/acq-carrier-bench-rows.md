- **The acq and pool benchmarks time a search with a carrier set.** There
    are three new acq rows (`op5M_U50k_D150_t1_fc`, `op2M_U50k_D60_t1_fc`
    and `op5M_U5k_D150_t1_fc`) and two new pool rows (`push[full,…,carrier]`),
    all at a 2.2 GHz carrier, so every side tile runs its code-rate ramp. On
    one dev box the ramp costs 16–19% of each block-coherent search. The
    rows exist so main measures them before the one delay-Doppler map
    replaces that ramp (#2825, #2752).
