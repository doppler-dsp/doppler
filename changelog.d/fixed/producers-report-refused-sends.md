- **A refused send is no longer silent.** wfmgen exits 1 when the stream
    refused a block or the end-of-stream, `dp_tlm_sink_lost()` counts dropped
    telemetry, and the sends, EOS, flush and drain carry `DP_NODISCARD` (#2011).
