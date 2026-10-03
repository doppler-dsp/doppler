- **`Specan` drops `window`; span and RBW 0 mean auto.** `dp_specan_create`
    loses its `window` argument (always Kaiser). `span`/`rbw` 0 are auto
    (`fs/1.28`, `span/100`); an oversize span or RBW is clamped, and `.span` and
    `.rbw` report what was realised.
