- **A one-tile search with D > 1 now uses the threads asked for.**
    `set_threads()` left it on one worker whatever the depth, though its
    map fans by epoch, capture bin and row as a tiled search's does. The
    request now stands at any tile count when D > 1; one tile at D = 1
    stays serial, and the result is bit-identical at every thread count.
    A search also starts its workers at its first push, not at create, so
    a receiver set to one thread never builds them
    ([the split by stage](docs/design/correlator.md), #3055).
