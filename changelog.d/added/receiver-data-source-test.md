- **A receiver test over a `data:LEN` description** — `DsssBurstReceiver`
    built from `[sync | data:LEN | crc]` returns, burst by burst through
    `FrameDesc.deframe`, the chunk the source sent (padded last chunk
    included), and a corrupted burst fails its check.
    ([#1620](https://github.com/doppler-dsp/doppler/issues/1620))
