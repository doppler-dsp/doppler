- **A `Dll` that refuses a state blob is left as it was.** `set_state()`
    read the whole blob into the object before checking it, so a refused
    blob of the right size (a 4-segment, symbol-aided Dll's, restored into a
    49-segment one) left NULL code and buffer pointers behind, and the next
    call crashed. It now checks the blob whole first. It also refuses a blob
    whose code length, samples per chip or aid hypotheses do not fit this
    instance (#2092).
