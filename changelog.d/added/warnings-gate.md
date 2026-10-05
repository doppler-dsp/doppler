- **`make warnings-check` gates doppler's C at zero `-Wall -Wextra` warnings**
    (#1658). The tree compiled with no warning flag at all; now a fresh Release
    build of every target, Python extensions included, under gcc and clang
    fails on any warning outside `vendor/` and the 100 jm glue files on
    `scripts/.warnings-exempt`. That list may only shrink: an added entry
    fails `make lint`, and a listed file that has gone clean fails the gate.
    It found one more unused parameter, in `pocketfft.c`, now removed.
