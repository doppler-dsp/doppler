- **`AsyncDsssReceiver` and `AsyncDsssPool` take `carrier_window`.**
    `"code_period"` forms the pre-despread carrier loop's error from one
    coherent sum over the code-period window the code loop's look-back
    picks, instead of from each partial (`"partials"`, the default, as
    built); it needs a symbol of at least a code period. `Dll` gains
    `dp_dll_get_window_sum()`. The name is a placeholder.
