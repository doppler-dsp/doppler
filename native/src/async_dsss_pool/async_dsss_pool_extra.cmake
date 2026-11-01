# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# awgn_core and gold_core: the bench feeds the pool the shipped noise
# generator and the CCSDS default Gold code, and nothing else here uses
# them. Linked onto the bench alone, as the receiver's own extra cmake does,
# so their headers stay out of the pool's public one.
target_link_libraries(bench_async_dsss_pool_core PRIVATE awgn_core gold_core)
