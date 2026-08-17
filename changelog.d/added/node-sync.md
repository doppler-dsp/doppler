- **Node synchronization is the library's job now, in `conv`.**
    `node_sync_score` decodes a window, **re-encodes the decisions**, and
    counts where the result disagrees with the received hard decisions;
    `node_sync_scan` runs that over all `n` alignments of a rate-1/n code and
    reports the winner with its margin. It references no truth, no marker and
    no training sequence — only the decoder's own input and output — so it
    works on a live capture, which is what makes it a receiver's statistic
    rather than a simulation's. Closes
    [#834](https://github.com/doppler-dsp/doppler/issues/834).

    In sync the count **is** the channel symbol error rate. Out of sync it is
    not a half, and the difference is worth stating because a half is what a
    coin-flip argument predicts: the decoder is a maximum-LIKELIHOOD search,
    so it finds whatever codeword agrees best with the misaligned stream.
    Measured on clean streams: **24 %** of symbols for CCSDS K=7 r=1/2, 23 %
    uninverted, 18 % for a K=5 r=1/3 — against 0 % for the right alignment.

    Two things the tests found rather than confirmed. The head of a window
    must be discarded by the DECODER's traceback depth, not the encoder's
    `k-1`: two cold starts overlap there and the decoder's all-zero prior is
    the larger, so skipping only `k-1` left three disagreements in 1598
    symbols on a clean stream and broke the polarity equality. And the scan
    must be scored over the window it is about to decode — at Es/N0 = +1 dB a
    slip early in a record made a whole-record scan prefer the phase that was
    right for the tail, and frame sync then found no marker at the head.

    `native/validation/rx_coding_gain.c` was picking the phase by which parity
    put an ASM where an ASM could be — the harness doing the library's job
    with a statistic that exists only because CCSDS supplies a marker.
    Swapping it onto the re-encoding metric changed **no measured number**:
    same frames, same bits, same ≥ 6.1 dB bound. That is the evidence the
    general statistic is at least as good as the special one.

- **The standard test harness gained the two helpers this needed**, rather
    than a private copy in one file: `dp_bit_distance` (bits differing between
    two packed-octet buffers — what `ber_meter` answers for a symbol stream
    and cannot be pointed at two byte arrays) and `dp_rx_duty` (the share of a
    window where a per-symbol flag is set). The standard record
    (`dp_rx_result_t`) now carries `lock_duty` and `lock_stat_duty`, printed
    with every battery row.
