- **AccTrace's state blob carries `alpha`** (#2000). A resume after a
    runtime `alpha` change used to continue with the alpha from `create`.
    `set_state` refuses an `alpha` the setter would refuse, and version-1
    blobs (PSD's included, which nests AccTrace's) are no longer accepted.
