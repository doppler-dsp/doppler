- **The alloc-helper ratchet stopped contradicting itself (#1043).** Its
    docstring and allow-file header said counts may *only shrink*; its failure
    message said to raise the count and explain in the commit message. One
    rule now: shrink freely, and a raise needs `# <reason>` **on the line**,
    compared against `origin/main` so an unexplained one fails. A commit
    message is read once by whoever is already convinced; the line is read by
    whoever comes next.
