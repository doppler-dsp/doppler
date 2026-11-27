- **A matched plan's response, derived.** `dp_RateConverter_response()` writes
    each output's filter on the input (halfbands, CIC, compensator and the
    bank's arm, composed at the input rate) and the input index it ends at,
    from the stages' definitions and with nothing run through the converter.
    It is what the noise at the output, and so a threshold on a statistic of
    it, is derived from. C only.
