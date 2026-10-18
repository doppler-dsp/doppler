- **Section citations are checked in every file and every spelling.**
    `scripts/check_doc_sections.py` read `.h .c .md .py` only and knew
    `§N` directly after a path, so 14 citations of a moved section sat
    unchecked; it now reads every tracked text file, accepts `section N`
    and a comment line break, and refuses `design §N` that names no page
    beyond a shrink-only list (#2416).
