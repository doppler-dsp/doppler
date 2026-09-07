- **`AsyncDsssPool` is certified** (design §12.17): its validation report
    renders the soak's own regression subset (`--check --emit`, 66 s on
    twenty cores) so the per-push gates and the evidence are one run, and
    the population soaks — ten emitters for 120 s and for 600 s, and the
    whole population timed behind the shipped DDC (`--budget`) — are the
    design page's. The soak's heap watch runs with glibc's tcache off (it
    re-execs itself with the tunable): the +53 KiB the 600 s run read was
    the cache's accounting, not the pool's, and the heap is flat to a page
    with it off. `async_dsss_pool_demo` in C and Python, with a gallery
    page.
