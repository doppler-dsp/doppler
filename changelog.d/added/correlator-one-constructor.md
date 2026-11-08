- **`dp_correlator_create`: one constructor that picks the correlator's
    flavor from its knobs.** It builds what the seven flavor constructors
    build, byte for byte, and refuses a value the chosen flavor would not
    honor, with a sentence naming the knob (`why`). It is the entry point
    step 4's one Python class calls; a C caller still names its flavor.
    [Design §3, §17](docs/design/correlator-python.md) (#2752).
