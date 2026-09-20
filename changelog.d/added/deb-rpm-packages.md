- **`.deb` and `.rpm` packages on every GitHub Release**, x86_64 and
    aarch64: `libdoppler-dsp<X.Y>` / `libdoppler-dsp-dev` /
    `doppler-dsp-tools` (RPM: `libdoppler-dsp`, `-devel`). One build per
    arch covers every distro at or above glibc 2.28; CI installs them in
    four distros on every PR. See
    [Install → System packages](docs/install/c.md#system-packages-deb-rpm)
    ([#1409](https://github.com/doppler-dsp/doppler/issues/1409)).
