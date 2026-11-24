- **A frame description reads a named field's received value back:
    `FrameDesc.read_field()`, `read_field_bits()`.** Most significant bit
    first, from the place the description gives the field, so a header is
    read by name with no parser of its own; a CRC trailer reads as
    received. C: `dp_wfm_frame_desc_field()`, `_field_bits()`
    ([#3309](https://github.com/doppler-dsp/doppler/issues/3309)).
