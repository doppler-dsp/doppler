- **`make bench-ab ALIAS=old=new`** judges a benchmark family renamed
    between the two commits, pairing A's `old::<row>` with B's `new::<row>`
    suffix for suffix. A row on one side only fails as UNPAIRED, and every
    alias in force is printed with the verdict (#2752 step 3.2).
