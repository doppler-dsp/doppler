- **An acquisition engine with a coherent depth above 1 now false-alarms at
    its `pfa`, not twice it.** Its statistic is the largest cell of a
    Doppler-interpolated surface, but the gate was priced over the native
    cells only, so the engine fired at 1.86–1.93× the target (1e6 noise dwells
    a row). The gate is now priced over the cells it actually searches.
    **Behavior change:** every gate at D > 1 rises about 2.5% at a 1e-3
    target, `pd_predicted` and `pd_burst` drop a little, and an auto-sized
    engine may pick a deeper D or report `underpowered` where it did not
    before (#2218).
