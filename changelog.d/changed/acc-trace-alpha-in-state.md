- **AccTrace's state blob carries `alpha`, and refuses another mode's
    blob** (#2000). A resume after a runtime `alpha` change used to continue
    with the alpha from `create`, and a mean trace's blob restored into an
    exp instance went on as an EMA. `set_state` refuses an `alpha` the setter
    would refuse and a blob from another mode, and version-1 blobs are no
    longer accepted -- including those nested in `PSD`'s, `CarrierAcquisition`'s
    and `Specan`'s state.
