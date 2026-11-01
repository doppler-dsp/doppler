- **`AsyncDsssPool` runs one search, not one per idle receiver.** Every idle
    receiver used to search for itself, building the same surface on the
    same samples, so a pool of N cost N searches (3.4 to 4.0 for four on
    the pinned bench). The controller now owns one search and seeds idle
    receivers from its picks, strongest first; the search runs only while a
    receiver is idle. A pool of four idle receivers now costs what one
    does. A pooled receiver's idle state reads `"idle"` (state 3, formerly
    reserved) where it read `"searching"`.
