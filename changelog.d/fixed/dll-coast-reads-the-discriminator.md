- **A coasting `Dll` reads its discriminator, on both paths, and takes a
    phase correction** (#1280, design §12.22): the header promised the hold
    filters nothing and steers nothing, but `steer()` returned before the
    discriminator was computed, so a coasting loop's `.e` probe read 0, and
    the header's `dll_update()`, the `segments == 1` path, had no hold at
    all and kept steering. Both hold after `last_error` now, pinned in
    `test_dll_core`. `set_code_phase(chips)` is the other half of the coast:
    a holder on another clock puts the loop back where that clock says.
