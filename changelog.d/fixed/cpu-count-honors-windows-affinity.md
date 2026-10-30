- **On Windows, `nthreads=0` honors the process affinity mask too.**
    `start /affinity` or `SetProcessAffinityMask` now shrinks
    `dp_cpu_count()` on a machine with one processor group (64 CPUs or
    fewer), as `taskset` does on Linux. (#2684)
