- **The release freshness gate asks each gallery script for its own plot.**
    One re-rendered PNG used to clear every changed script, including ones that
    never ran; a byte-identical re-render is now waived in writing (#2609). A
    waiver line with no reason on its own line fails the gate by name instead of
    lending the lines below it to the previous entry (#2608).
