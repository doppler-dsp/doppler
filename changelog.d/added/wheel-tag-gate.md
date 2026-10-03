- **A release refuses a wheel pip would never install.** v0.60.0 and v0.61.0
    shipped Windows 3.13/3.14 wheels tagged `cp313-cpwin_amd64`, which pip skips
    for the sdist. `make check-wheel-tags` now gates `publish-python` on every
    built wheel, and `make release-watch` re-checks what PyPI and the GitHub
    Release serve ([#1817](https://github.com/doppler-dsp/doppler/issues/1817)).
