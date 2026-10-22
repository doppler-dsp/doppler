# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# awgn_core: the bench feeds the pool the shipped noise generator, and
# nothing else here uses it. Linked onto the bench alone, as the receiver's
# own extra cmake does, so awgn's header stays out of the pool's public one.
target_link_libraries(bench_async_dsss_pool_core PRIVATE awgn_core)
