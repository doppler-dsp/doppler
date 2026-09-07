- **`AsyncDsssPool` is certified** (design §12.17): its validation report
    renders the soak's own regression subset (`--check --emit`, 66 s on
    twenty cores) so the per-push gates and the evidence are one run, and
    the population soaks — ten emitters for 120 s and for 600 s, and the
    whole population timed behind the shipped DDC (`--budget`) — are the
    design page's. The soak's heap watch reads the pool and nothing
    else: glibc's tcache off (it re-execs itself with the tunable) and its
    own stint records sized before the base — the +53 KiB the 600 s run
    read was those two, and the heap is flat to a page over ten minutes.
    `async_dsss_pool_demo` in C and Python, with a gallery page.
