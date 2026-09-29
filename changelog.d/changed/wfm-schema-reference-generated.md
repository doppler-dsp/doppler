- **The scene schema's field properties and a new field reference are
    generated.** `docs/schema/wfmgen.schema.json` takes each source and
    segment field from the surface table (hand-owned keys untouched). A
    `background` source, which the reader always accepted, no longer fails
    validation. The new `guide/wfmgen/options.md` lists every field's flag,
    scene key, Python keyword, type, default and meaning (#853).
