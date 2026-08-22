- **The release-notes renderer could exit 141 and publish nothing.** The
    `### Highlights` extraction exits at the next `### ` heading; fed from a
    pipe, the writer then takes SIGPIPE, and `set -o pipefail` turns that into
    a failed release — after emitting a **zero-byte** body, with every visible
    step looking fine. Found by running the renderer over a 132 KB section
    rather than by reading it. It reads a here-string now, and
    `test_extraction_does_not_die_of_sigpipe` asserts the exit code
    specifically, because the symptom was otherwise indistinguishable from an
    ordinary failure.
