- **The delay-Doppler map's stages balance across workers.** Each stage cut
    one chunk per worker, so it ran as slow as its slowest worker. It now
    cuts 4 per worker, and each chunk claims whichever scratch slot is free.
    `op5M_U50k_D150_t8` measured 5% faster; the result is bit-identical as
    before (#3047).
