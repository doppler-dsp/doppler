- **`StreamSink.send()` raises `OSError` on a refused block.** It returned
    the C status as an `int`, which both in-tree callers dropped; it now
    returns `None`, as `Publisher.send` does. Drop the status check (#2011).
