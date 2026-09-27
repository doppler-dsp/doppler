- **`cvt.bytes_to_bin` unpacks octets to bits.** Packed data (a binary
    file, a byte stream) now reaches a frame field through one named
    conversion, and `wfmgen --bits-file` uses it instead of a private copy
    (#853).
