- **`dp_fmod_pos` stays in `[0, m)`, and a NaN stays NaN.** It could return
    exactly its period, and the DLL's replica tap then read one chip past
    its code (#2110).
