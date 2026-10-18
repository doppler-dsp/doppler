- **`check_site_links` refuses a built site with no page to check.** A
    docs build that rendered nothing (theme assets and an unrendered
    sitemap template, no page) used to read "OK — 0 pages" and pass
    `make docs-check`. Zero pages checked now fails.
