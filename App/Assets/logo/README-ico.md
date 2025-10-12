# Generate .ico from SVG

This folder includes a helper PowerShell script to generate a Windows .ico file from the SVG logo.

Requirements:
- ImageMagick installed (magick/convert available in PATH)

Usage (PowerShell):

```powershell
cd App/Assets/logo
# From symbol SVG (recommended for app icon)
./make-ico.ps1 -Svg al-muslim-symbol.svg -Out al-muslim.ico
# From wordmark (not typical for .ico, but possible)
./make-ico.ps1 -Svg al-muslim-wordmark.svg -Out al-muslim-wordmark.ico
```

The script produces a multi-resolution ICO with sizes: 16, 24, 32, 48, 64, 128, 256 px.

Tip: Use the generated `al-muslim.ico` in Windows installers or executables as needed.
