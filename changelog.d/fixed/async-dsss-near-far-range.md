- **`AsyncDsssReceiver`'s ranked or excluded search no longer narrows its
    near-far range with every signal on the air.** The sidelobe rule
    compared a candidate against the sum of every stronger signal, so with
    six emitters up a true one 10 dB below the strongest, inside the 17 dB
    sidelobe range, was never taken while receivers sat idle. The bound is
    now the strongest signal, plus, on a deep block, the stronger signals
    on the candidate's own Doppler row, whose fixed sidelobes can stack. In
    a pool, that emitter is held 96% of its air time, from 58 ms
    (doppler#2554).
