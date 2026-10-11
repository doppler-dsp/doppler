- **A C test's diagnostics land in order in a redirected log.** `dp_test.h`
    wrote failures to unbuffered stderr while verbose PASS lines sat in
    stdout's buffer, so a `> log 2>&1` capture could put a FAIL inside a PASS
    line and undercount `^FAIL`. Every diagnostic now goes through
    `DP_TEST_ERR`, which flushes stdout first, and `make tests-ssot` refuses a
    raw `fprintf(stderr` in `native/tests` (#2050).
