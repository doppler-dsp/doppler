- **`wfmgen --detached --repeat` is refused**, like `--continuous`: a detached
    BLUE header is written when the run ends, and a looping run never ends — it
    wrote until the disk filled and left no `.hdr` (#1591).
