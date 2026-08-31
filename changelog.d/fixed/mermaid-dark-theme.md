- **Diagrams are legible in the dark theme, which is the default one** — three
    pinned `fill:#ede7f6,color:#000`, so in `slate` they drew near-white text on
    a near-white box; no stylesheet could fix it, because each diagram renders
    into a closed shadow root. Fill and text now come from the palette, and
    `scripts/check_mermaid_theme.py` fails any diagram that pins either
    ([#1149][gh1149]).
- **The wfmgen guide's model is one timeline** — the index drew the
    source/segment distinction and the object ladder as two pictures that
    repeated each other; it is now a single left-to-right read of the sample
    clock ([#1149][gh1149]).
- **The 13 wfmgen flags the docs described but never ran now have runnable
    lines**, each on the page already explaining it and each carrying the number
    it produces. Flag exercise goes **53 → 66 of 67**; the last entry, `-o`, is
    unexercisable by construction, not undocumented ([#1149][gh1149]).

[gh1149]: https://github.com/doppler-dsp/doppler/issues/1149
