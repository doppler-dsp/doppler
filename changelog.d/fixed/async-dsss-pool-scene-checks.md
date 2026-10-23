- **The `AsyncDsssPool` scene's release and masking checks can fail.**
    `async_dsss_pool_scene.py` checked "released after it leaves" over
    epochs it never matched, and asserted "F waits for C" on its table
    rather than on the run. Both now read the run, and the page states the
    near-far limit as the design's table does (doppler#2560).
