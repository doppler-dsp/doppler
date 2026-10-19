# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# awgn_core: the bench's searching row feeds the receiver the shipped noise
# generator, and nothing else here uses it. Linked onto the bench alone: a
# bare `depends_on` would also inject awgn's header into the receiver's
# public one and link it into the core and every composer of it.
target_link_libraries(bench_async_dsss_receiver_core PRIVATE awgn_core)
