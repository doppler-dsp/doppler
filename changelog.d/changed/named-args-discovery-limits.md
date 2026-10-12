- **The named-args gate reads a quote inside a comment as a comment.**
    Blanked strings-first, `/* 5" */` hid every helper and call up to the
    next quote (#2227).
