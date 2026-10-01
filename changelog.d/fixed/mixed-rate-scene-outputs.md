- **wfmgen no longer labels or paces a mixed-rate scene at its first
    segment's rate**
    ([#1733](https://github.com/doppler-dsp/doppler/issues/1733)).
    `--realtime` and `nats://` frames follow each segment's own `fs`, SigMF
    leaves the rate out, and BLUE refuses by name.
