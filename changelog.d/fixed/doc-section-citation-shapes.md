- **The section-citation gate checked one spelling of three, over seven
    directories.** A continuation (`§7.1, §8`) and a markdown link whose path
    follows the number both went unexamined, and the scan set omitted
    `native/benchmarks` — so nine citations pointed at nothing after
    `async-dsss-receiver.md` moved its §12 to the measurements page, and
    `CHANGELOG.md` still cited two pages folded into `mpsk.md`. All nine
    retargeted, scan set is now `git ls-files`, 208 citations checked, and the
    gate has a test that seeds each spelling and requires it to go red.
