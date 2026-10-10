- **`BurstDespreader.set_state()` refuses a blob from a differently
    configured despreader instead of crashing the next `steps()`.** It copied
    every field from the blob but kept its own code buffers, so a blob taken
    with `set_acq()` active, restored into a despreader without it, left a
    NULL acq code to read. The acq code now travels in the blob, which
    restores only into a despreader with the same `sf`, `sps` and acq-code
    length; a refused blob changes nothing (#2041).
