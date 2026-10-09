- **The `corr` Python benchmark now correlates the frame it is credited
    with.** It handed a 64-point `Corr` 65,536 samples; the kernel read 64 and
    ignored the rest, so `docs/benchmarks.md` printed 206 GSa/s (portable) and
    262 GSa/s (native) for roughly 1/1000 of the work. Those published figures
    stay until the next representative-machine run; a new gate fails any
    published throughput above 100 GSa/s (#1918).
