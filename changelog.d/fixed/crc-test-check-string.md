- **`make warnings-check` passes under gcc 15 and clang 21.** The CRC
    test's nine-byte check message is written as nine characters, so no
    terminating NUL is implied and
    `-Wunterminated-string-initialization` has nothing to report.
