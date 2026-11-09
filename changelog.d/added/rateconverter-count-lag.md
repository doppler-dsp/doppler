- **`dp_RateConverter_get_count_lag()`**: how far a cascade's output count
    can trail its input, in input samples, from its plan (about one output
    a stage). Held to the measured worst lag at 15 rates covering every
    plan, reached within 0.01 outputs.
