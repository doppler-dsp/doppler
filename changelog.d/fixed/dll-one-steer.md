- **`Dll`: one discriminator and steer for both correlation paths** (#1280).
    The full-epoch path's inline `dll_update()` and the partial-correlation
    core's private `steer()` each carried the discriminator, the clamp, the
    probe and the coast hold, and had drifted (design §12.22 fixed the
    coast on one before finding the other). Both now call `dll_steer()`;
    the two paths' separately pinned loop gains stay as a table the steer
    reads. Also: a held loop now takes a new `set_rate_aid()` at once
    instead of keeping the aid it was held with.
