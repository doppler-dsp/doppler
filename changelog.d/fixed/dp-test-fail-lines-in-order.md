- **A C test's FAIL lines land in order in a redirected log.** `dp_test.h`
    wrote failures to unbuffered stderr while verbose PASS lines sat in
    stdout's buffer, so in a `> log 2>&1` capture a FAIL could land inside a
    PASS line and a count of `^FAIL` came up short. Every stderr line now
    flushes stdout first (#2050).
