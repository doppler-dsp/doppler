- **`AsyncDsssReceiver` and `AsyncDsssPool` take a telemetry context.**
    `set_telemetry(tlm, prefix)` reports every loop's discriminator and
    filter output and, on the pool, the searcher's dwells, every slot and the
    table: 105 probes for four slots, kept across the re-seeds that rebuild
    a receiver's loops. A context holds 512 probes, not 64, so the public
    `dp_tlm_t` grows to about 25 KiB and the fields after its probe table
    move: C code built against an earlier doppler must be rebuilt. A refused
    attach changes nothing, a full table included.
    `make pool-telemetry` times both on one wfm Gold capture and files the
    captures; `scripts/plot_capture.py --time` plots any capture in seconds.
