- **A refused send is no longer silent.** wfmgen exits 1 when the stream
    refused a block or the end-of-stream, and `dp_tlm_sink_lost()` counts
    dropped telemetry (#2011).
