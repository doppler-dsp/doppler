- **`DP_ERR_UNSUPPORTED` (-11): the object's flavor does not serve the
    call.** The correlator returns it from every member a flavor does not
    honor ([design §18](docs/design/correlator-python.md)), so
    `DP_ERR_INVALID` keeps meaning a bad argument to a call the object
    does serve, and the binding can say which one happened (#2752).
