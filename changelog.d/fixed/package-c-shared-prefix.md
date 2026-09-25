- **`make package-c` installs into a shared prefix**
    ([#1543](https://github.com/doppler-dsp/doppler/issues/1543)). The
    headers-under-`include/doppler/` check now reads CMake's install
    manifest, so other packages' headers in `~/.local` or `/usr/local` no
    longer fail the install.
