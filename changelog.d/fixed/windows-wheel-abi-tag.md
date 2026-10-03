- **Windows wheels for Python 3.13 and 3.14 install again.** v0.60.0 and
    v0.61.0 named them `cp313-cpwin_amd64`, so pip skipped them and built the
    sdist. The build backend floor is now just-buildit 0.6.2, which reads the
    ABI tag correctly
    ([#1817](https://github.com/doppler-dsp/doppler/issues/1817)).
