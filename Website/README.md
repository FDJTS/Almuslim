# Almuslim Website

This is a minimal static site for Almuslim downloads and installation instructions. It is framework-free (plain HTML/CSS/JS) and can be hosted on any static hosting provider (GitHub Pages, Netlify, Cloudflare Pages, etc.).

## Customize

- Edit `config.json` to set:
  - `name`, `tagline`
  - `repo`: link to your repository
  - `downloads`: URLs, file labels, and optional SHA256 checksums
- Assets are in `assets/`:
  - `favicon.svg` and `hero-illustration.svg` can be replaced with your branding

## Checksums

Generate SHA256 checksums for release artifacts and paste them into `config.json`.

Windows (PowerShell):

```
Get-FileHash .\Almuslim-Setup.exe -Algorithm SHA256
```

Linux/macOS:

```
shasum -a 256 ./Almuslim-*.AppImage
shasum -a 256 ./Almuslim.dmg
```

## Develop locally

You can open `index.html` directly in a browser. If your browser blocks `fetch` for local files, serve the folder:

PowerShell:

```
# From the Website/ directory
python -m http.server 8080
```

Then browse: http://localhost:8080

## Deploy

- GitHub Pages (project site): push the `Website/` contents to the `gh-pages` branch or use a GitHub Action to publish from `/Website`.
- Any static host: deploy the `Website/` directory as-is.

## Notes

- The primary Download button adapts to the visitor OS (Windows, macOS, Linux, Android) when a matching URL is provided in `config.json`.
- All styles are in `assets/styles.css`; tweak colors and spacing there.

---

MIT License — see repository `LICENSE`.
