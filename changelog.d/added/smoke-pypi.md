- **The published wheel is now smoke-tested from PyPI**
    ([#1347](https://github.com/doppler-dsp/doppler/issues/1347)). `smoke-wheel`
    installs a local file before the upload, so nothing in a release ever
    exercised the index — the C library had a post-publish smoke and Python did
    not. `smoke-pypi` installs `doppler-dsp==<version>` on x86_64 and aarch64
    and runs the same end-to-end, retiring the release runbook's manual step 8.
