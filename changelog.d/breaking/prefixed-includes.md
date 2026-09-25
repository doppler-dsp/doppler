- **C consumers include doppler's headers as `<doppler/...>`**
    ([#1546](https://github.com/doppler-dsp/doppler/issues/1546)):
    `#include <doppler/lo/lo_core.h>`, was `<lo/lo_core.h>`. `pkg-config   --cflags doppler` and the exported CMake targets now hand out
    `-I<prefix>/include`, so no component name (`fft/`, `util/`,
    `clib_common.h`) sits at the root of a consumer's search path; a bare
    `cc` line changes `-I <prefix>/include/doppler` to `-I <prefix>/include`.
    The installed files do not move.
