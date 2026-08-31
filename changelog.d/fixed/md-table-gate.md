- **A markdown table that does not render is caught now**
    (`scripts/check_md_tables.py`). A GFM header and its `| --- |` separator
    must declare the same number of columns; when they disagree the block
    ships as a paragraph of literal pipes. Nothing saw it — `mdformat`
    reformats a mismatched table happily and the strict docs build has no
    opinion — so it was found by a person reading the page. The gate also
    catches a body row with the wrong cell count, which GFM drops silently.
    Runs in `make docs-check` and `make lint`; clean across 309 files.
