- **An example suite for the ring buffer, on both faces.** Seven Python
    scripts (start at `buffers_demo`: three widths, one shape) and six C
    programs cover every call — lifecycle and lent views, block-size
    conversion, the single-threaded surface, 16-bit I/Q as a record, every
    refusal, stopping a blocked `wait()`, the element-typed face, and a
    file-backed ring. Each checks its own results, and
    [Ring Buffers](docs/examples/python-buffers.md) is built from their
    regions, so the page is what was executed.
