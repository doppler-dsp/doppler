// MathJax v3 config for pymdownx.arithmatex (generic mode) under
// mkdocs-material. arithmatex wraps converted math in \(...\) / \[...\] with
// class "arithmatex"; re-typeset on instant-navigation page swaps.
window.MathJax = {
  tex: {
    inlineMath: [["\\(", "\\)"]],
    displayMath: [["\\[", "\\]"]],
    processEscapes: true,
    processEnvironments: true,
  },
  options: {
    ignoreHtmlClass: ".*|",
    processHtmlClass: "arithmatex",
  },
};

document$.subscribe(() => {
  MathJax.typesetPromise();
});
