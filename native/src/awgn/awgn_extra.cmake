# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# dp_once_core: the sine table is built under dp_once() on the first
# dp_awgn_create (doppler#2328). PUBLIC so the tests, the bench and every
# extension module that links awgn_core link dp_once and its -pthread too.
target_link_libraries(awgn_core PUBLIC dp_once_core)
