- **`libdoppler.so` carries an ABI version: the SONAME is now
    `libdoppler.so.MAJOR.MINOR`** (`libdoppler.so.0.52`), and likewise
    `libdoppler_stream.so`. A binary linked against one minor can no longer
    have the next one's ABI swapped underneath it by an upgrade. Relink
    consumers once; `-ldoppler`, `find_package` and pkg-config are unchanged.
    `abi-check` asserts it
    ([#1407](https://github.com/doppler-dsp/doppler/issues/1407)).
