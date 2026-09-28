- **Each wfm source and segment field has one description.** Its header
    comment and its Python docstring had drifted apart for 31 of 32 fields.
    They are now the same text, and `make lint` fails if they differ again
    (#853).
