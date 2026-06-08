# Python Streaming API

ZMQ-backed streaming — PUB/SUB, PUSH/PULL, and REQ/REP socket pairs,
all backed by the native C `dp_*` streaming functions. All socket
types support the context manager protocol.

Source:
[`src/doppler/stream/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/stream/__init__.py)

______________________________________________________________________

::: doppler.stream
options:
members:
\- get_timestamp_ns
\- Publisher
\- Subscriber
\- Push
\- Pull
\- Requester
\- Replier
