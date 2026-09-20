- **Headers install under `include/doppler/`, not `include/` itself.** The
    old layout put 138 generic names (`fft/`, `util/`, `pocketfft/`) at the
    prefix root. `find_package` and pkg-config consumers change nothing —
    the `-I` moves with the files, and `#include <lo/lo_core.h>` still
    resolves. Only a hand-written `-I<prefix>/include` must become
    `-I<prefix>/include/doppler`
    ([#1408](https://github.com/doppler-dsp/doppler/issues/1408)).
