- **`nthreads=0` uses the CPUs the process may run on, not every CPU on
    the machine.** `dp_cpu_count()` now reads the affinity mask on Linux, so
    `taskset`, a container cpuset or a cgroup cpuset no longer oversubscribes
    Corr2D, DelayDoppler, AsyncDsssPool or `dp_parallel` (20 workers on 8
    pinned CPUs in the v0.77.0 bench). (#2679)
