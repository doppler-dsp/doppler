- **The hand-typed-version gate now sees a version that ends a sentence.**
    Its guard against a longer number (`X.Y.Z1`) refused every following
    dot, so "The current release is X.Y.Z." passed — the very claim it
    exists to stop. It now refuses only a dot a digit follows (#1943).
