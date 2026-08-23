- **`docs/design/streaming.md` — the transport contract, which had no home.**
    Every comparable subsystem has a design page; streaming had an archived
    roadmap and an archived migration note, so the wire format existed only
    as a comment in `stream.h` — and that comment is incomplete (chunking)
    and in one place wrong (the `version` value). The new page owns what is
    on the wire byte by byte, how a frame too large for the broker is
    chunked and reassembled, the subjects and JetStream objects derived from
    an endpoint (`iq.<base>.<type>`, `work.<base>.<type>`, `DP_WORK_<base>`,
    `DP_PULL_<base>`), who owns which buffer, and what the layer does not
    promise. Writing it produced #956, #958 and #959; §10 is the map.

    Two hazards it surfaces that were written down nowhere: chunk
    reassembly requires **one publisher per subject** (a frame from a second
    publisher arriving mid-reassembly fails the sequence check and drops the
    whole frame), and a **CF128 frame is not portable between x86-64 and
    aarch64** — both are 32 bytes, but one is 80-bit extended and the other
    IEEE binary128, so nothing rejects the frame and the values decode to
    nonsense. doppler publishes multi-arch images.
