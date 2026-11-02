- **`configure_search_raw()` re-applies the thread cap.** A regrid to one
    tile left a tiled engine's pool and `threads` in place, so a three-tile
    `Acquisition` kept reading 3 workers, and a receiver's state blob carried
    that stale count. It now drops to serial, as `set_threads()` would (#2746).
