- **`AsyncDsssPool` joins its workers once every 16 code periods.** The
    search and every receiver still step one period a call, on a grid
    counted from reset; the controller runs once a group, so its seeds and
    releases land up to 15 periods later. A pushed sample reaches the
    receivers when its group is whole. Nine receivers tracking with the
    search on 16 threads at the 415.1-B-1 point: 54 to 43 ns a sample at
    2.2 GHz, 66 to 43 at 2.8 GHz.
