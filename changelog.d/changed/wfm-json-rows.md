- **Scene JSON reads and writes its fields from the surface table.** A
    source's and a segment's keys now come out in table order: the same
    keys and values as before, reordered once. Each key's JSON rules (when
    it is omitted, when it applies) are declared in the manifest, and a
    record's bytes are pinned by a test (#853).
