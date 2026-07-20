#!/usr/bin/env python3
"""
Render a Markdown document into a themed HTML page that matches the VIBE site.

    python3 tools/gen_docs.py <input.md> <output.html> "<Title>" "<Description>"

Used by `make docs` to keep docs/specification.html and docs/Stability_Paradox.html
in lockstep with SPECIFICATION.md and docs/Stability_Paradox.md, so the website
can never drift from the source Markdown again.

Requires: python3 + the `markdown` package (pip install markdown).
"""

import re
import sys
import markdown

TEMPLATE = """<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <link rel="shortcut icon" type="image/x-icon" href="favicon.ico?" />
    <title>__TITLE__</title>
    <meta name="description" content="__DESC__" />
    <link
      href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@300;400;500;600;700&display=swap"
      rel="stylesheet"
    />
    <link
      rel="stylesheet"
      href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css"
    />
    <style>
      * { margin: 0; padding: 0; box-sizing: border-box; }
      :root {
        --primary: #6366f1; --primary-light: #818cf8; --accent: #06b6d4;
        --bg-primary: #0f172a; --bg-secondary: #1e293b; --bg-tertiary: #334155;
        --border-color: #475569; --code-bg: #0b1120;
        --text-primary: #f1f5f9; --text-secondary: #cbd5e1; --text-muted: #94a3b8;
      }
      [data-theme="light"] {
        --bg-primary: #ffffff; --bg-secondary: #f8fafc; --bg-tertiary: #f1f5f9;
        --border-color: #e2e8f0; --code-bg: #f6f8fa;
        --text-primary: #0f172a; --text-secondary: #334155; --text-muted: #64748b;
      }
      html { scroll-behavior: smooth; overflow-x: hidden; -webkit-text-size-adjust: 100%; }
      body {
        font-family: "IBM Plex Mono", ui-monospace, monospace;
        background: var(--bg-primary); color: var(--text-secondary);
        line-height: 1.7; -webkit-font-smoothing: antialiased;
        overflow-x: hidden;
      }
      a { color: var(--primary-light); text-decoration: none; }
      a:hover { text-decoration: underline; }

      header {
        position: sticky; top: 0; z-index: 50;
        background: rgba(15, 23, 42, 0.85); backdrop-filter: blur(12px);
        border-bottom: 1px solid var(--border-color);
      }
      [data-theme="light"] header { background: rgba(255, 255, 255, 0.85); }
      nav {
        max-width: 1200px; margin: 0 auto; padding: 0.9rem 1.5rem;
        display: flex; align-items: center; gap: 1.5rem;
      }
      .brand { font-weight: 700; color: var(--text-primary); font-size: 1.1rem; }
      .brand span { color: var(--primary-light); }
      nav .spacer { flex: 1; }
      nav a { color: var(--text-secondary); font-size: 0.9rem; }
      nav a:hover { color: var(--primary-light); text-decoration: none; }
      .theme-btn {
        cursor: pointer; background: var(--bg-tertiary); border: 1px solid var(--border-color);
        color: var(--text-primary); border-radius: 8px; padding: 0.35rem 0.6rem; font-size: 0.85rem;
      }

      .layout {
        max-width: 1200px; margin: 0 auto; padding: 2rem 1.5rem;
        display: grid; grid-template-columns: 260px 1fr; gap: 2.5rem; align-items: start;
      }
      .toc {
        position: sticky; top: 80px; max-height: calc(100vh - 100px); overflow-y: auto;
        background: var(--bg-secondary); border: 1px solid var(--border-color);
        border-radius: 12px; padding: 1.25rem; font-size: 0.85rem;
      }
      .toc-title {
        text-transform: uppercase; letter-spacing: 0.08em; font-size: 0.72rem;
        color: var(--text-muted); margin-bottom: 0.75rem; font-weight: 600;
      }
      .toc ul { list-style: none; }
      .toc ul ul { padding-left: 0.9rem; }
      .toc li { margin: 0.15rem 0; }
      .toc a { color: var(--text-secondary); }
      .toc a:hover { color: var(--primary-light); text-decoration: none; }

      article { min-width: 0; max-width: 60rem; }
      article h1, article h2, article h3, article h4 {
        color: var(--text-primary); line-height: 1.25; font-weight: 700;
        margin: 2.2rem 0 1rem; scroll-margin-top: 80px;
      }
      article h1 { font-size: 2.2rem; margin-top: 0; }
      article h2 {
        font-size: 1.6rem; padding-bottom: 0.4rem; border-bottom: 1px solid var(--border-color);
      }
      article h3 { font-size: 1.25rem; }
      article h4 { font-size: 1.05rem; color: var(--text-secondary); }
      article p, article ul, article ol, article blockquote, article table, article pre {
        margin: 0.9rem 0;
      }
      article ul, article ol { padding-left: 1.5rem; }
      article li { margin: 0.35rem 0; }
      article strong { color: var(--text-primary); }
      article hr { border: none; border-top: 1px solid var(--border-color); margin: 2rem 0; }

      code {
        font-family: "IBM Plex Mono", monospace; font-size: 0.88em;
        background: var(--bg-tertiary); color: var(--primary-light);
        padding: 0.15em 0.4em; border-radius: 5px;
      }
      pre {
        background: var(--code-bg); border: 1px solid var(--border-color);
        border-radius: 10px; padding: 1.1rem 1.25rem; overflow-x: auto;
      }
      pre code { background: none; color: var(--text-secondary); padding: 0; font-size: 0.85rem; }
      blockquote {
        border-left: 3px solid var(--primary); background: var(--bg-secondary);
        padding: 0.75rem 1.1rem; border-radius: 0 8px 8px 0; color: var(--text-secondary);
      }
      table { width: 100%; border-collapse: collapse; font-size: 0.88rem; }
      th, td { border: 1px solid var(--border-color); padding: 0.6rem 0.8rem; text-align: left; }
      th { background: var(--bg-tertiary); color: var(--text-primary); font-weight: 600; }
      tr:nth-child(even) td { background: var(--bg-secondary); }

      footer {
        border-top: 1px solid var(--border-color); margin-top: 3rem;
        padding: 2rem 1.5rem; text-align: center; color: var(--text-muted); font-size: 0.85rem;
      }
      @media (max-width: 900px) {
        nav { flex-wrap: wrap; gap: 1rem; padding: 0.75rem 1rem; }
        .layout { grid-template-columns: 1fr; padding: 1.5rem 1.15rem; gap: 1.5rem; }
        .toc { position: static; max-height: 42vh; order: -1; }
        article { max-width: 100%; }
        article h1 { font-size: 1.8rem; }
        article h2 { font-size: 1.35rem; }
        article table { display: block; overflow-x: auto; -webkit-overflow-scrolling: touch; }
        article pre { padding: 0.9rem 1rem; }
      }
      @media (max-width: 560px) {
        nav .hide-sm { display: none; }
        nav a, .theme-btn { font-size: 0.82rem; }
        .brand { font-size: 1rem; }
        article h1 { font-size: 1.55rem; }
        article h2 { font-size: 1.25rem; }
        article pre code { font-size: 0.8rem; }
        pre { padding: 0.85rem 0.9rem; }
      }
    </style>
  </head>
  <body>
    <header>
      <nav>
        <a class="brand" href="index.html">VIBE<span>//</span></a>
        <span class="spacer"></span>
        <a href="index.html" class="hide-sm">Home</a>
        <a href="specification.html">Spec</a>
        <a href="Stability_Paradox.html" class="hide-sm">Philosophy</a>
        <a href="https://github.com/1ay1/vibe">GitHub</a>
        <button class="theme-btn" onclick="toggleTheme()" aria-label="Toggle theme">
          <i class="fas fa-circle-half-stroke"></i>
        </button>
      </nav>
    </header>

    <div class="layout">
      <aside class="toc">
        <div class="toc-title">On this page</div>
        __TOC__
      </aside>
      <article>
        __CONTENT__
      </article>
    </div>

    <footer>
      Generated from Markdown by <code>tools/gen_docs.py</code> ·
      <a href="https://github.com/1ay1/vibe">github.com/1ay1/vibe</a> ·
      Keep calm and VIBE on 🌊
    </footer>

    <script>
      function applyTheme(t) {
        if (t === "light") document.documentElement.setAttribute("data-theme", "light");
        else document.documentElement.removeAttribute("data-theme");
      }
      function toggleTheme() {
        const light = document.documentElement.getAttribute("data-theme") === "light";
        const next = light ? "dark" : "light";
        localStorage.setItem("theme", next);
        applyTheme(next);
      }
      applyTheme(localStorage.getItem("theme"));
    </script>
  </body>
</html>
"""


# Cross-links between the docs point at the SOURCE Markdown (correct when
# browsing the repo on GitHub), but the website serves generated HTML. Rewrite
# intra-repo *.md links to their generated *.html siblings so a live page never
# links out to raw Markdown. Absolute/external URLs are left untouched, and any
# leading directory is dropped because the generated pages all live in docs/.
_DOC_HTML = {
    "specification.md": "specification.html",
    "stability_paradox.md": "Stability_Paradox.html",
}


def rewrite_md_links(html):
    def repl(m):
        target, frag = m.group(1), m.group(2) or ""
        if target.lower().startswith(("http://", "https://", "//", "mailto:")):
            return m.group(0)
        base = target.rsplit("/", 1)[-1]
        html_name = _DOC_HTML.get(base.lower(), base[:-3] + ".html")
        return 'href="%s%s"' % (html_name, frag)

    return re.sub(r'href="([^"#]+\.md)(#[^"]*)?"', repl, html, flags=re.I)


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: gen_docs.py <input.md> <output.html> [title] [description]")
    src, out = sys.argv[1], sys.argv[2]
    title = sys.argv[3] if len(sys.argv) > 3 else "VIBE"
    desc = sys.argv[4] if len(sys.argv) > 4 else "VIBE documentation."

    with open(src, "r", encoding="utf-8") as f:
        text = f.read()

    md = markdown.Markdown(
        extensions=["extra", "tables", "fenced_code", "sane_lists", "toc", "attr_list"],
        extension_configs={"toc": {"toc_depth": "2-3", "permalink": False}},
    )
    body = rewrite_md_links(md.convert(text))
    toc = md.toc or "<ul></ul>"

    page = (
        TEMPLATE.replace("__TITLE__", title)
        .replace("__DESC__", desc)
        .replace("__TOC__", toc)
        .replace("__CONTENT__", body)
    )
    with open(out, "w", encoding="utf-8") as f:
        f.write(page)
    print(f"wrote {out}  ({len(page):,} bytes)")


if __name__ == "__main__":
    main()
