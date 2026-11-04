- **A regrid re-applies the caller's thread request.** A regrid to one
    tile left a tiled engine's pool and `threads` in place, so a three-tile
    `Acquisition` kept reading 3 workers, and a receiver's state blob carried
    that stale count. It now drops to serial, as `set_threads()` would (#2746).
    The request itself is kept apart from the count that runs, so widening
    again comes back to min(request, tiles) rather than staying at 1 (#2797).
