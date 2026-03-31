# Python Streaming API

ZMQ-backed streaming — PUB/SUB, PUSH/PULL, and REQ/REP socket pairs,
all backed by the native C `dp_*` streaming functions.  All socket
types support the context manager protocol.

Source:
[`python/doppler/stream/_stream.pyi`](https://github.com/doppler-dsp/doppler/blob/main/python/doppler/stream/_stream.pyi)

---

::: doppler.stream
    options:
      members:
        - get_timestamp_ns
        - Publisher
        - Subscriber
        - Push
        - Pull
        - Requester
        - Replier
