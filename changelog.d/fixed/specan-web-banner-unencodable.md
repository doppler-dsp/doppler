- **`doppler-specan --web` no longer exits at startup when its stdout cannot encode the
    banner arrow.** A redirected stdout under cp1252, as on Windows, crashed it with
    `UnicodeEncodeError` before the server listened; the arrow now prints as an escape
    ([#2225](https://github.com/doppler-dsp/doppler/issues/2225)).
