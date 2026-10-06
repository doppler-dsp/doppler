- **`Writer` is jm-generated, and its manifest now names the real
    vocabulary** (doppler#1446). `sample_type`, `file_type` and `endian`
    reference the project's `[[enum]]` tables instead of repeating lists. The
    `sample_type` copy had five names where the enum has ten, so the manifest
    and the type stub said `Writer(sample_type="f32")` was invalid while the
    binding (and a test) accepted it; the stub now lists all ten. The
    `-Wall -Wextra` exempt list goes from 15 to 14.
