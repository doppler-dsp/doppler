- **`DsssReceiver` and `AsyncDsssReceiver` no longer drop a code period of
    despread partials.** Each runs its `Dll` a code period at a time, and a
    period's samples hold two epoch ends when the boundary sits on its
    edge. `AsyncDsssReceiver`'s refine path had room for one period's
    partials, so one period went missing from the fine hand-over and
    `CarrierAcquisition` (a 6 Hz hand-over error at a design sd of
    0.58 Hz). `DsssReceiver` sized its scratch at the call's own length, so
    a call of a few samples that completed a carried period kept only as
    many of the period's partials as it had samples. Both are now sized
    for the most a call can emit (#3281).
