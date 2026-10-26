- **The guides, install, examples, CLI and API pages match the code.** About
    45 wrong commands, outputs and claims are fixed, among them `wfmgen`'s
    `--crc` default, the whole-archive link `nats://` needs, and a stream
    header documented as 96-byte "SIGS" rather than 64-byte `DPSTREAM`
    ([#2581](https://github.com/doppler-dsp/doppler/issues/2581)).
- **A doc example that warns fails its gate.** `make test-stubs` and
    `make test-snippets` treat a `UserWarning` as an error, so an example
    cannot show an under-powered `BurstAcquisition` again
    ([#2581](https://github.com/doppler-dsp/doppler/issues/2581)).
