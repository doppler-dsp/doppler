- **The named-args gate finds forwarding helpers itself.** A `static`
    function that passes a parameter, bare, into a constructor's window or
    mode slot (or into another such helper) is held like the constructor;
    the hand-kept table is gone, and two of its slots that reached only a
    compare drop out (#2197).
