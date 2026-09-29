- **A framed `--type bpsk`, `qpsk` or `pn` transmits its frame.** It was
    accepted and emitted the synth's own PN stream: the frame was assembled
    and then dropped, because only a `bits` synth plays a bit pattern. Such a
    source is now built as `bits` with the mapping its type names, on the
    composer and the standalone `Synth` alike, and its SNR keeps its type's
    reference (#1616).
