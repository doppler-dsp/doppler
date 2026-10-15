# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# dp_once_core: the sine table is built under dp_once() on the first
# dp_lo_create (doppler#2328). PUBLIC so the tests, the bench and every
# extension module that links lo_core link dp_once and its -pthread too.
target_link_libraries(lo_core PUBLIC dp_once_core)
