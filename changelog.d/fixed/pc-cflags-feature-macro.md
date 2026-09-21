- **`pkg-config --cflags doppler` is enough to compile the installed headers.**
    It lacked the feature-test macro the CMake target carries, so at a strict
    `-std=c99` on glibc 2.28, 33 of 159 headers — `doppler.h` included — failed
    on `MAP_ANONYMOUS` for a pkg-config consumer while `find_package` worked.
    The macro is now declared once and feeds the build, the exported target
    and `doppler.pc`; the package smoke compiles every installed header from
    pkg-config's flags alone
    ([#1451](https://github.com/doppler-dsp/doppler/issues/1451)).
