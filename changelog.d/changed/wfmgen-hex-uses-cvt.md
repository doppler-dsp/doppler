- **wfmgen's `--acq-code-hex` / `--data-code-hex` parse through `cvt`.** The
    digit loop in `parse_hex_string` was a second statement of a conversion
    `hex_to_bin` already owns — same MSB-first order, same four bits per
    digit, same refusal on a bad char. Two copies of that is how a marker
    comes to be expanded one way by the generator and another by a receiver.
    The allocation stays at the call site, because the caller owns the array.
