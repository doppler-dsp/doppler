- **The DLL's code-lock detector integrates over a symbol once it knows
    the symbol period.** `Dll.set_symbol_period` lifts the per-epoch
    max-power look-back to the symbol scale: the hypothesis whose
    transition-free windows carry the most power is the symbol timing, and
    its windows are the detector's looks — 7.8 dB more per look at the
    async-DSSS operating point, never across a transition.
    `Dll.set_lock_verify` sizes the drop hysteresis from a budget.
    `AsyncDsssReceiver` applies both from its configuration; before this its
    detector read "unlocked" 96% of the time at Es/N0 5.7 dB on a loop that
    never lost the code (`docs/design/async-dsss-receiver.md` §3.7, §12.4).
